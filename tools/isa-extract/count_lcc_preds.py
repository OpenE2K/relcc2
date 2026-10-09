#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Count how lcc uses predicates in its own system libraries.

Same library set and disassembler as tools/isa-extract/count_lcc_ops.py:
every distinct shared library under fs/lib64 and fs/usr/lib64 of an lcc
1.29.16 cross compiler's sysroot (code MCST built with lcc), disassembled
with MCST objdump.  objdump's text gives the operations (names, channels,
destinations, printed conditions) and the function symbols; the raw
syllables of the same wide instruction, decoded with
tools/isa-decode/e2kdis.py (written from spec/isa/bundle.md), give the
fields objdump hides or prints only partly: every RLP/MRGC entry of the
CDS syllables, every ELP and CLP of the PLS syllables (objdump prints
only those feeding a %pred), the SS ctcond type and psrc, and the
alc/abn/abp bits.  lcc is used only as a black box: this reads the code
it produced, nothing else.

  python3 tools/isa-extract/count_lcc_preds.py                  # v3, v5 and v6 sysroots
  python3 tools/isa-extract/count_lcc_preds.py --target v3 --json out.json
"""

import argparse
import collections
import json
import re
import struct
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "isa-decode"))
import e2kdis  # noqa: E402

OBJDUMP = ROOT / "toolchains" / "binutils" / "bin" / "e2k-linux-objdump"
TARGETS = {"v3": "e2k-v3", "v5": "e2k-v5.8c2", "v6": "e2k-v6.2c3"}
LCC = "/opt/mcst/lcc-1.29.16.{}.linux-6.1"

FUNC_RX = re.compile(r"^([0-9a-f]{16}) <(.+)>:$")
BUNDLE_RX = re.compile(r"^\s+([0-9a-f]+):\s*$")
ALU_RX = re.compile(r"^  ([a-z][a-z0-9_]*),(\d)((?:,[a-z0-9]+)*)\s*(.*)$")
PRED_RX = re.compile(r"%pred(\d+)$")
REG_RX = re.compile(r"^%(?:d|q|qp|x)?(r\d+|b\[\d+\]|g\d+)$")

CTCOND_NAMES = {0: "never", 1: "always", 2: "%predN", 3: "~%predN",
                4: "%LOOP_END", 5: "%NOT_LOOP_END", 6: "%predN || %LOOP_END",
                7: "~%predN && %NOT_LOOP_END", 8: "%MLOCK [|| %dt_al]",
                9: "%MLOCK || %cmp/%clp", 11: "%cmp/%clp (v7)",
                14: "~%predN || %LOOP_END", 15: "%predN && %NOT_LOOP_END"}
PRED_CTCOND = {2, 3, 6, 7, 14, 15}
NEG_CTCOND = {3, 7, 14}


def libraries(fs):
    """Every distinct shared library file under the sysroot's lib dirs
    (as count_lcc_ops.py)."""
    seen, out = set(), []
    for d in (fs / "lib64", fs / "usr" / "lib64"):
        for f in sorted(d.glob("*.so*")):
            real = f.resolve()
            if real.is_file() and real not in seen and ".so" in real.name:
                seen.add(real)
                out.append(real)
    return out


def exec_sections(data):
    """(addr, offset, size) of every executable section of an ELF64 LE."""
    if data[:4] != b"\x7fELF" or data[4] != 2:
        return []
    shoff = struct.unpack_from("<Q", data, 0x28)[0]
    shentsize, shnum = struct.unpack_from("<HH", data, 0x3a)
    out = []
    for i in range(shnum):
        o = shoff + i * shentsize
        _, typ, flags, addr, off, size = struct.unpack_from("<IIQQQQ", data, o)
        if typ == 1 and flags & 4:
            out.append((addr, off, size))
    return out


def raw_bundle(data, secs, addr):
    for a, o, s in secs:
        if a <= addr < a + s:
            p = o + addr - a
            hs = data[p]
            length = (((hs >> 4) & 7) + 1) * 8
            b = e2kdis.Bundle(data[p:p + length])
            return b if b.valid else None
    return None


def split_ops(text):
    """'a, b, dst ? %pred0' -> (operands, condition string)."""
    cond = ""
    if " ? " in text:
        text, cond = text.split(" ? ", 1)
    ops = [x.strip() for x in text.split(",")] if text.strip() else []
    return ops, cond


def reg_key(op):
    m = REG_RX.match(op)
    return m.group(1) if m else None


def is_merge(name):
    return "merge" in name or name in ("pmrgp", "qpmrgp")


def cmp_class(name):
    if name.startswith("cmpand"):
        return "cmpand*"
    if name.startswith("cmp"):
        return "cmp* (integer)"
    if name.startswith("fxcmp"):
        return "fxcmp* (80-bit)"
    if name.startswith("fcmp"):
        return "fcmp* (FP)"
    if name.startswith("cctop"):
        return "cctop*"
    if "pcmp" in name:
        return "pcmp*op/ap (packed, v6)"
    return "other: " + name


def parse_objdump(lib):
    """Yield (function, [(addr, [lines])])."""
    p = subprocess.run([str(OBJDUMP), "-d", str(lib)], text=True,
                       capture_output=True)
    func, bundles, cur = None, [], None
    for line in p.stdout.splitlines():
        m = FUNC_RX.match(line)
        if m:
            if func is not None:
                yield func, bundles
            func, bundles, cur = m.group(2), [], None
            continue
        m = BUNDLE_RX.match(line)
        if m:
            cur = (int(m.group(1), 16), [])
            bundles.append(cur)
            continue
        if cur is not None and line.startswith("  ") and line.strip():
            cur[1].append(line)
    if func is not None:
        yield func, bundles


class Stats:
    def __init__(self, lib=""):
        self.c = collections.defaultdict(collections.Counter)
        self.examples = collections.defaultdict(list)
        self.lib = lib

    def ex(self, key, addr, func):
        if len(self.examples[key]) < 3:
            self.examples[key].append(f"{Path(self.lib).name}:{addr:x} <{func}>")

    def add(self, key, sub, n=1):
        self.c[key][sub] += n

    def merge(self, other):
        for k, v in other.c.items():
            self.c[k].update(v)
        for k, v in other.examples.items():
            self.examples[k].extend(v[:3 - len(self.examples[k])])


def analyse_lib(lib):
    st = Stats(lib)
    data = Path(lib).read_bytes()
    secs = exec_sections(data)
    if not secs:
        return st
    st.add("totals", "libraries")
    for func, bundles in parse_objdump(lib):
        st.add("totals", "functions")
        fpreds = set()          # %predN indices this function names
        ctpr_kind = {1: None, 2: None, 3: None}
        last_write = {}         # pred index -> (bundle index, cycle)
        cycle = 0
        uses_setbp = False
        prev_ct = None          # (transfer, cct, psrc) of the previous bundle
        last_call = -1          # bundle index of the latest call (linear)
        for bi, (addr, lines) in enumerate(bundles):
            b = raw_bundle(data, secs, addr)
            if b is None:
                st.add("totals", "not decoded (APB or invalid)")
                continue
            st.add("totals", "wide instructions")
            nop = (b.hs >> 7) & 7
            this_cycle = cycle
            cycle += 1 + nop
            features = set()
            rd = set()          # %predN read in this bundle
            wr = set()          # %predN written in this bundle
            pcnt_here = set()

            # ---- ALU operations from the objdump text ----
            ncmp = 0
            writes = []         # (reg, cond, name, chn)
            for line in lines:
                m = ALU_RX.match(line)
                if not m:
                    continue
                name, chn, mods, rest = m.groups()
                if name.startswith("mova"):
                    continue
                ops, cond = split_ops(rest)
                spec = "sm" in mods.split(",")
                if ops and PRED_RX.search(ops[-1]) and not is_merge(name):
                    k = int(PRED_RX.search(ops[-1]).group(1))
                    ncmp += 1
                    wr.add(k)
                    st.add("compare class", cmp_class(name))
                    st.add("compare mnemonic", name)
                    st.add("compare speculative", "sm" if spec else "plain")
                    st.add("compare channel", chn)
                    if cond:
                        st.add("compare predicated", cond)
                        st.add("compare idioms", "compare under RLP")
                    if len(ops) >= 3 and all(re.match(r"^(0x[0-9a-f]+|_f)", o)
                                             for o in ops[:2]):
                        st.add("compare idioms", "both operands literal ("
                               + ("true" if ops[0] == ops[1] and name.startswith("cmpe")
                                  else "other") + ")")
                    if name.startswith("cmpesb") or name.startswith("cmpedb"):
                        if ops[:2] in (["0x0", ops[1]], [ops[0], "0x0"]) or \
                                ops[:2] in (["0x1", ops[1]], [ops[0], "0x1"]):
                            st.add("compare idioms", "cmpe* against 0 or 1")
                    writes.append(("%pred" + str(k), cond, name, chn))
                    continue
                pred_cond = cond
                if is_merge(name):
                    st.add("merge ops", name)
                    if name in ("merges", "merged") and len(ops) >= 4:
                        a, bb = ops[0], ops[1]
                        if a == bb:
                            st.add("merge idioms", "a == b (predicated move)"
                                   + (" + RLP" if cond else ""))
                        elif {a, bb} == {"0x0", "0x1"}:
                            st.add("merge idioms", "0/1 (bool to register)")
                        elif a.startswith("0x") and bb.startswith("0x"):
                            st.add("merge idioms", "two literals")
                        else:
                            st.add("merge idioms", "real select")
                    if len(ops) >= 2:
                        writes.append((reg_key(ops[-2]), cond, name, chn))
                    if cond:
                        st.add("merge ops also under RLP", name)
                elif name.startswith("st") or name.startswith("aaurw"):
                    pass
                elif ops:
                    writes.append((reg_key(ops[-1]), cond, name, chn))
                if pred_cond:
                    if name.startswith("aaurw") and "unknown_aau_reg" in rest:
                        kind = "staa (printed aaurw)"
                    elif name.startswith("ld"):
                        kind = "load"
                    elif name.startswith("st"):
                        kind = "store"
                    elif is_merge(name):
                        kind = "merge"
                    elif name in ("movtd", "getpl") and ops and \
                            ops[-1].startswith("%ctpr"):
                        kind = "movtd/getpl to %ctpr"
                    else:
                        kind = "other ALU"
                    st.add("predicated op kind", kind)
                    st.add("predicated op speculative",
                           "sm" if spec else "plain")
                    st.add("predicated op kind x speculative",
                           kind + (" ,sm" if spec else " plain"))
                    st.add("predicated op condition source", kind + ": " + (
                        "%pcnt" if "%pcnt" in pred_cond else "%pred"))
                    st.add("predicated mnemonic", name)
                    if "," in pred_cond:
                        st.add("channels with several RLP conditions",
                               re.sub(r"\d+", "N", pred_cond))
            st.add("compares per instruction", ncmp)
            if ncmp:
                features.add("compare to %pred")

            # if-conversion: same register written under opposite predicates
            byreg = collections.defaultdict(list)
            bychn = collections.defaultdict(list)
            for reg, cond, name, chn in writes:
                if reg:
                    byreg[reg].append(cond)
                    bychn[reg].append((int(chn), cond))
            for reg, conds in byreg.items():
                if len(conds) < 2:
                    continue
                st.add("same register written twice or more", len(conds))
                if "" in conds:
                    plain_ch = [c for c, k in bychn[reg] if not k]
                    pred_ch = [c for c, k in bychn[reg] if k]
                    if len(plain_ch) > 1:
                        order = "two unpredicated writes"
                    elif pred_ch and max(plain_ch) < min(pred_ch):
                        order = "unpredicated in the lowest channel"
                    else:
                        order = "unpredicated NOT in the lowest channel"
                    st.add("unpredicated + predicated: channel order", order)
                    st.ex("unpredicated + predicated: " + order, addr, func)
                plain = {c for c in conds if c and not c.startswith("~")}
                neg = {c[1:] for c in conds if c.startswith("~")}
                if plain & neg:
                    kind = "%pred dst" if reg.startswith("%pred") else "register"
                    st.add("two writes, opposite predicates", kind)
                    st.ex("two writes, opposite predicates " + kind, addr, func)
                elif "" in conds:
                    st.add("two writes, one unpredicated", "count")
                    st.ex("two writes, one unpredicated", addr, func)
                else:
                    st.add("two writes, other conditions", "count")
                    st.ex("two writes, other conditions", addr, func)

            # ---- CDS: RLP and MRGC ----
            nrlp = nmrgc = 0
            for cds in b.cds:
                for e in (cds & 0xffff, cds >> 16):
                    opc, mask = (e >> 14) & 3, (e >> 10) & 0xf
                    if mask == 0:
                        continue
                    neg, pred = (e >> 7) & 7, e & 0x7f
                    kind = "RLP" if opc < 2 else "MRGC"
                    if kind == "RLP":
                        nrlp += 1
                    else:
                        nmrgc += 1
                    if pred & 0x60 == 0x60:
                        pname = "%pred"
                        rd.add(pred & 0x1f)
                    elif pred & 0x60 == 0x40:
                        pname = "%pcnt"
                        pcnt_here.add(pred & 0x1f)
                        st.add("%pcntN index (" + kind + ")", pred & 0x1f)
                    else:
                        pname = "invalid"
                    st.add("CDS entry source", f"{kind} {pname}")
                    chans = sum(1 for i in range(3) if mask >> i & 1)
                    st.add("channels per CDS entry", f"{kind} {chans}"
                           + (" +AM" if mask & 8 else ""))
                    for i in range(3):
                        if mask >> i & 1:
                            st.add("CDS channel uses", f"{kind} " + (
                                "negated" if neg >> i & 1 else "plain"))
                    if mask & 8:
                        st.add("CDS channel uses", f"{kind} AM (staa incr) "
                               + ("negated" if neg >> 2 & 1 else "plain"))
            st.add("RLP entries per instruction", nrlp)
            if nrlp:
                features.add("RLP")
            if nmrgc:
                features.add("MRGC")
                st.add("MRGC entries per instruction", nmrgc)
            if pcnt_here:
                features.add("%pcnt")

            # ---- PLS: ELP and CLP ----
            need = [0] * 7
            for i, p in enumerate(b.pls):
                if p & 0x20:
                    need[4 + i] = 1
            # CLP results read by ctcond type 9/11 %clpN
            ss = b.ss
            cct = ((ss >> 5) & 0xf) if ss is not None else 0
            psrc = (ss & 0x1f) if ss is not None else 0
            if ss is not None and cct in (9, 11) and (psrc >> 3) & 3 == 2:
                k = (psrc >> 1) & 3
                if k < len(b.pls):
                    need[4 + k] = max(need[4 + k], 1)
            again = True
            while again:
                again = False
                for i, p in enumerate(b.pls):
                    if need[4 + i] == 1:
                        for q in ((p >> 10) & 7, (p >> 6) & 7):
                            if q < 7 and need[q] == 0:
                                need[q] = 1
                                again |= q >= 4
                        need[4 + i] = 2
            nclp = 0
            depth = {}
            for i, p in enumerate(b.pls):
                if i < 2:
                    for k, elp in ((2 * i, (p >> 24) & 0x7f),
                                   (2 * i + 1, (p >> 16) & 0x7f)):
                        rnd = elp & 0x60 == 0x40 and 1 <= elp & 0x1f <= 15
                        if not need[k] and not rnd:
                            continue
                        if elp & 0x40 == 0:
                            src = "%lcntex" if elp & 0x3f == 0 else "%spred"
                        elif elp & 0x60 == 0x40:
                            src = "%bgrpred" if elp & 0x1f == 0 else "%rndpred"
                        else:
                            src = "%pred"
                            rd.add(elp & 0x1f)
                        st.add("ELP source", src)
                        features.add("PLS")
            # depth: a CLP reading only ELPs is level 1; reading a CLP, 2+
            for _ in range(3):
                for i, p in enumerate(b.pls):
                    if not need[4 + i]:
                        continue
                    d = 1
                    for q in ((p >> 10) & 7, (p >> 6) & 7):
                        if q >= 4:
                            d = max(d, depth.get(q - 4, 1) + 1)
                    depth[i] = d
            for i, p in enumerate(b.pls):
                if not need[4 + i]:
                    continue
                nclp += 1
                c = p & 0xffff
                opn = ("andp", "landp", "reserved", "movep")[c >> 14]
                st.add("CLP opcode", opn + (" +vdst" if c & 0x20 else ""))
                st.add("CLP slot", f"CLP{i}")
                st.add("CLP operand negation",
                       f"neg0={c >> 13 & 1} neg1={c >> 9 & 1}")
                if (c >> 10) & 7 == (c >> 6) & 7:
                    st.add("CLP same operand twice",
                           f"{opn} neg0={c >> 13 & 1} neg1={c >> 9 & 1}")
                for q in ((c >> 10) & 7, (c >> 6) & 7):
                    st.add("CLP operand kind",
                           "ELP @p0-3" if q < 4 else "CLP @p4-6")
                if c & 0x20:
                    wr.add(c & 0x1f)
                    st.add("CLP vdst", "writes %pred")
                    if (c & 0x1f) in rd:
                        st.add("CLP vdst", "writes a %pred it also reads")
                else:
                    st.add("CLP vdst", "no vdst (feeds CLP or ct only)")
            st.add("CLPs per instruction", nclp)
            st.add("CLP chain depth", max(depth.values()) if depth else 0)
            if nclp:
                features.add("PLS")

            # ---- SS: ctcond and loop bits ----
            if ss is not None and not ss & (1 << 20):
                for name, sh in (("alct", 16), ("alcf", 17), ("abpt", 18),
                                 ("abpf", 19), ("abnt", 21), ("abnf", 22)):
                    if ss >> sh & 1:
                        st.add("SS loop bits", name)
            ctop = (ss >> 10) & 3 if ss is not None else 0
            is_call = b.cs1 is not None and b.cs1 >> 28 == 5
            cs0kind = None
            if b.cs0 is not None:
                ctpr, ctp_opc = b.cs0 >> 30, (b.cs0 >> 28) & 3
                cs0kind = (("ibranch", "pref", "puttsd", "done"),
                           ("disp", None, "sdisp", "gettsd"),
                           ("disp", "ldisp", "sdisp", "gettsd"),
                           ("disp", None, "sdisp", "return"))[ctpr][ctp_opc]
            transfer = None
            if is_call:
                transfer = "call"
            elif ctop:
                k = ctpr_kind.get(ctop)
                transfer = {"return": "ct return", "disp": "ct branch",
                            "ldisp": "ct ldisp", "sdisp": "ct sdisp",
                            "movtd": "ct indirect"}.get(k, "ct unknown")
            elif cs0kind in ("ibranch", "done"):
                transfer = cs0kind
            cur_ct = None
            if transfer and ss is not None and cct in (2, 3):
                cur_ct = (transfer, ctop, cct, psrc)
                if prev_ct is not None and prev_ct[1] == ctop and ctop and \
                        prev_ct[0] == transfer:
                    same = prev_ct[3] == psrc
                    comp = same and {prev_ct[2], cct} == {2, 3}
                    st.add("back-to-back conditional ct through one %ctpr",
                           f"{transfer}: " + ("complementary (same pred, "
                                              "opposite polarity)" if comp else
                                              "same pred, same polarity" if same
                                              else "different preds"))
            prev_ct = cur_ct
            if transfer and ss is not None and cct != 0:
                st.add("transfers", transfer)
                st.add("ctcond type", f"{cct:2d} {CTCOND_NAMES.get(cct, '?')}")
                if cct != 1:
                    st.add("conditional transfers", transfer)
                    st.add("ctcond type by transfer", f"{transfer}: {cct}")
                    features.add("conditional transfer")
                if cct in PRED_CTCOND:
                    rd.add(psrc)
                    features.add("ctcond on %pred")
                    st.add("ctcond %pred polarity",
                           "negated" if cct in NEG_CTCOND else "plain")
                if cct == 8:
                    st.add("ctcond MLOCK form",
                           "%MLOCK || %dt_al" if psrc & 0xf else "%MLOCK")
                if cct in (9, 11):
                    t = (psrc >> 3) & 3
                    st.add("ctcond MLOCK form",
                           ("%MLOCK || %cmpN", "%MLOCK || %cmpJ || %cmpK",
                            "%MLOCK || %clpN", "?")[t])
                # predicate -> ct distance (straight-line, inferred)
                if cct in PRED_CTCOND and psrc in last_write:
                    d = this_cycle - last_write[psrc][1]
                    if d <= 16:
                        st.add("write->ct distance (cycles, linear)", d)
            # %ctpr preparations (after the transfer of this bundle)
            if cs0kind in ("disp", "ldisp", "sdisp", "return"):
                ctpr_kind[b.cs0 >> 30] = cs0kind
            for line in lines:
                m = ALU_RX.match(line)
                if m and m.group(1) in ("movtd", "getpl"):
                    ops, _ = split_ops(m.group(4))
                    if ops and ops[-1].startswith("%ctpr"):
                        ctpr_kind[int(ops[-1][5])] = "movtd"
            if is_call:
                ctpr_kind = {1: None, 2: None, 3: None}
            if b.cs1 is not None and b.cs1 >> 28 in (0, 1, 4) and \
                    b.cs1 & (1 << 27):
                uses_setbp = True
                st.add("setbp psz", (b.cs1 >> 18) & 0x1f)

            # RLP distance from the last write (straight-line, inferred)
            for cds in b.cds:
                for e in (cds & 0xffff, cds >> 16):
                    if (e >> 10) & 0xf and e & 0x60 == 0x60:
                        k = e & 0x1f
                        if k in last_write:
                            d = this_cycle - last_write[k][1]
                            if d <= 16:
                                st.add("write->RLP/MRGC distance "
                                       "(cycles, linear)", d)
            for k in rd:
                if k in last_write:
                    st.add("%pred reads vs calls (linear)",
                           "written before the latest call" if
                           last_write[k][0] <= last_call else "no call between")
            for k in wr:
                last_write[k] = (bi, this_cycle)
            if is_call:
                last_call = bi
                if wr:
                    st.add("%pred reads vs calls (linear)",
                           "call instructions that also write a %pred")

            if rd:
                st.add("distinct %pred read per instruction", len(rd))
            if rd | wr:
                st.add("distinct %pred named per instruction", len(rd | wr))
            fpreds |= rd | wr
            if features:
                st.add("totals", "instructions with any predicate use")
            for f in features:
                st.add("instructions with", f)
        if fpreds:
            st.add("highest %predN per function", max(fpreds))
            st.add("distinct %pred per function", len(fpreds))
            st.add("totals", "functions naming a %pred")
            if uses_setbp:
                st.add("totals", "functions with setbp")
        else:
            st.add("distinct %pred per function", 0)
    return st


def run(target, jobs):
    fs = Path(LCC.format(TARGETS[target])) / "fs"
    libs = libraries(fs)
    total = Stats()
    with ProcessPoolExecutor(jobs) as ex:
        for st in ex.map(analyse_lib, libs):
            total.merge(st)
    return total, libs


def fmt_counter(c, numeric):
    keys = sorted(c, key=(lambda k: (isinstance(k, str), k)) if numeric
                  else (lambda k: -c[k]))
    tot = sum(c.values())
    return [f"    {k}: {c[k]}  ({100.0 * c[k] / tot:.2f}%)" for k in keys]


NUMERIC = {"compares per instruction", "RLP entries per instruction",
           "MRGC entries per instruction", "CLPs per instruction",
           "CLP chain depth", "distinct %pred read per instruction",
           "distinct %pred named per instruction",
           "highest %predN per function", "distinct %pred per function",
           "setbp psz", "write->ct distance (cycles, linear)",
           "write->RLP/MRGC distance (cycles, linear)",
           "%pcntN index (RLP)", "%pcntN index (MRGC)"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--target", action="append", choices=sorted(TARGETS))
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()
    out = {}
    for t in args.target or ["v3", "v5", "v6"]:
        st, libs = run(t, args.jobs)
        print(f"== {t}: {LCC.format(TARGETS[t])}, {len(libs)} libraries")
        for key in sorted(st.c):
            print(f"  {key}:")
            numeric = key in NUMERIC
            if key in ("predicated mnemonic", "compare mnemonic"):
                c = collections.Counter(dict(st.c[key].most_common(25)))
                print("\n".join(fmt_counter(c, False)) + "  (top 25)")
            else:
                print("\n".join(fmt_counter(st.c[key], numeric)))
        print("  examples:")
        for k in sorted(st.examples):
            print(f"    {k}: " + "; ".join(st.examples[k]))
        out[t] = {k: {str(a): b for a, b in v.items()} for k, v in st.c.items()}
    if args.json:
        args.json.write_text(json.dumps(out, indent=1, sort_keys=True))


if __name__ == "__main__":
    main()
