#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check spec/isa/reservations.yaml against compiled code.

Usage: check_reservations.py [--data FILE] [--iset N] [--all] [-j N] [FILE...]
       check_reservations.py --selftest [--binutils DIR]

Decodes every wide instruction of each FILE (an E2K ELF object, program
or shared library; the ISA version comes from e_flags, or from "-vN" in
the name, or --iset) with tools/isa-decode/e2kdis.py and checks that it
keeps the rules in reservations.yaml: every ALU operation is in a
channel its form allows, paired operations hold both channels of a pair
(v2-v6) or leave the upper one empty (v7), no channel, syllable or field
is claimed twice (CS0, the SS transfer and condition, CS1 groups, MAS
fields, PLS slots, CDS entries, AAS channels), and the restrictions hold
(loads and stores in ALC0/2/3/5, prepare and transfer through one CTPR
before v6, %ctpr destinations in ALC0, %cmpN/%dt_alN/%clpN conditions,
literal slots, the ALES2/ALES5 word before v4, ...). A violation means
the data, the decoder or the compiler is wrong: each is printed with its
address and the instruction as e2kdis.py prints it.

With no FILE it checks the default set, where present:
  toolchains/corpus/*.o            tools/isa-decode/build_corpus.sh
                                   toolchains/corpus (Unipro clang and
                                   hand-written gas input, v3-v7)
  toolchains/bench/{lcc-O3,v3.4c-lcc-O3,v5.8c2-lcc-O3,v6.2c3-lcc-O3}/coremark
  /opt/mcst/lcc-1.29.16.e2k-{v3,v5.8c2,v6.2c3}.linux-6.1/fs/lib64/libc.so.6

It also prints, per ISA version, how often lcc's and the corpus's code
puts things in one wide instruction that the data says may share it
(a prepare and a call, setwd and a prepare, several MAS accesses, ...)
and the most any instruction uses of each resource.

--selftest composes, from syllables MCST gas encodes, one wide
instruction breaking each rule (gas refuses to write most of them) plus
some that keep them, and checks that exactly the expected rules fire.

Exit status 1 if any violation is found (or a self-test case fails).
"""

import argparse
import collections
import concurrent.futures
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "isa-decode"))
sys.path.insert(0, str(ROOT / "tools" / "isa-score"))
import e2kdis  # noqa: E402
from elf import Elf  # noqa: E402

_LOADER = getattr(yaml, "CSafeLoader", yaml.SafeLoader)
_YAML = {}


def load_yaml(text):
    if text not in _YAML:
        _YAML[text] = yaml.load(text, Loader=_LOADER)
    return _YAML[text]


e2kdis.yaml = type("yaml", (), {"safe_load": staticmethod(load_yaml)})

DEFAULT_FILES = (
    [str(p) for p in sorted((ROOT / "toolchains" / "corpus").glob("*.o"))]
    + [str(ROOT / "toolchains" / "bench" / d / "coremark")
       for d in ("lcc-O3", "v3.4c-lcc-O3", "v5.8c2-lcc-O3", "v6.2c3-lcc-O3")]
    + [f"/opt/mcst/lcc-1.29.16.e2k-{t}.linux-6.1/fs/lib64/libc.so.6"
       for t in ("v3", "v5.8c2", "v6.2c3")])

CS0_TABLE = (("ibranch", "pref", "puttsd", "done"),
             ("disp", None, "sdisp", "gettsd"),
             ("disp", "ldisp", "sdisp", "gettsd"),
             ("disp", None, "sdisp", "return"))
PREPARES = {"disp", "ldisp", "sdisp", "return", "gettsd"}
MAS_SHIFT = {0: 21, 2: 14, 3: 7, 5: 0}
CTPR_DST = (0xd1, 0xd2, 0xd3)
# Formats whose ALS low byte is a register destination (the others put a
# store's data, a predicate or a state register there).
DST_FORMATS = {"ALOPF1", "MERGE", "ALOPF11", "ALOPF11_MERGE", "ALOPF11_LIT8",
               "ALOPF21", "ALOPF21_MERGE", "ALOPF21_LT_PACK_CODE", "ALOPF2",
               "ALOPF12", "ALOPF22", "ALOPF12_PSHUFH"}
SRC1_FORMATS = {"ALOPF1", "MERGE", "ALOPF11", "ALOPF11_MERGE", "ALOPF11_LIT8",
                "ALOPF21", "ALOPF21_MERGE", "ALOPF21_LT_PACK_CODE", "ALOPF7",
                "ALOPF17", "ALOPF3", "ALOPF13"}
SRC2_FORMATS = SRC1_FORMATS | {"ALOPF2", "ALOPF12", "ALOPF22", "ALOPF12_PSHUFH",
                               "ALOPF12_IBRANCHD", "ALOPF12_ICALLD", "ALOPF8",
                               "ALOPF15"}
SRC3_ALES = {"ALOPF21", "ALOPF21_MERGE", "ALOPF21_LT_PACK_CODE"}
STORE_DATA = {"ALOPF3", "ALOPF13", "ALOPF10", "AAURW"}
# Pairs of syllable users whose co-occurrence is counted (evidence for
# what the data says may share an instruction).
SHARE_GROUPS = ("prepare", "pref", "ct", "call", "ibranch", "icall", "hcall", "done",
                "ibranchd", "icalld", "setcmd", "wait", "mas", "flush", "setei", "setsft",
                "mova", "alc", "abn", "abp", "pls", "cds", "lts")


class Data:
    """reservations.yaml, indexed."""

    def __init__(self, path):
        d = yaml.load(Path(path).read_text(), Loader=_LOADER)
        self.forms = {}
        for name, forms in d["operations"].items():
            for f in forms:
                for v in f["isets"]:
                    self.forms[(v, name)] = f
        self.control = d["control_operations"]
        self.restrictions = {r["id"]: r for r in d["restrictions"]}
        groups = d["wide_instruction"]["syllables"]["cs1"]["holds_one_group"]
        self.cs1_by_opc = collections.defaultdict(list)
        for g, v in groups.items():
            for o in v["opcodes"]:
                self.cs1_by_opc[o].append(g)
        self.clp_inputs = {int(k[3:]): [int(p[2:]) for p in v]
                           for k, v in d["pls"]["clp"]["inputs"].items()}
        self.clp_depth = d["pls"]["clp"]["max_depth"]
        self.cds_entries = d["cds"]["entries"]
        self.lts_count = d["wide_instruction"]["syllables"]["lts"]["count"]
        self.max_words = d["wide_instruction"]["max_words"]

    def applies(self, rule, iset):
        r = self.restrictions[rule]
        return "isets" not in r or iset in r["isets"]


def ales_expected(form, ch):
    a = form["ales"]
    if isinstance(a, str):
        return a
    for kind, chans in a.items():
        if ch in chans:
            return kind
    return None


class Checker:
    def __init__(self, data, iset):
        self.data, self.iset = data, iset
        self.spec = e2kdis.Spec(iset)
        self.pr = e2kdis.Printer(self.spec)
        self.viol = collections.Counter()      # rule -> count
        self.examples = collections.defaultdict(list)
        self.stats = collections.Counter()
        self.maxes = collections.Counter()
        self.share = collections.Counter()

    def bad(self, rule, addr, msg, b):
        if rule not in self.data.restrictions and rule != "data":
            raise KeyError(f"rule {rule} is not in reservations.yaml's restrictions")
        self.viol[rule] += 1
        self.examples[rule].append((addr, msg, b))

    def lts_refs(self, b, v, role, addr, name, form, out):
        """Record the LTS words an operand byte V uses; check the slot."""
        if v & 0xe0 != 0xc0:
            return
        lits = form.get("literals", {})
        if role == "src1":
            if 5 not in lits.get("src1", []):
                self.bad("literal-form", addr, f"{name}: 5-bit literal in src1", b)
            return
        if role == "src3":
            if v & 0xfc == 0xd8 and 32 in lits.get("src3", []):
                if (v & 3) >= len(b.lts):
                    self.bad("lts-room", addr, f"{name}: src3 LTS{v & 3} missing", b)
                out.append(("w", v & 3))
            else:
                self.bad("literal-form", addr, f"{name}: literal in src3 ({v:#x})", b)
            return
        size = 4 if v & 0xf0 == 0xc0 else 16 if v & 0xf8 == 0xd0 else \
            32 if v & 0xfc == 0xd8 else 64
        if size not in lits.get("src2", []):
            self.bad("literal-form", addr, f"{name}: {size}-bit literal in src2", b)
        n = v & 3
        if size == 16:
            if n > 1:
                self.bad("lts-room", addr, f"{name}: 16-bit literal in LTS{n}", b)
            elif n >= len(b.lts):
                self.bad("lts-room", addr, f"{name}: LTS{n} missing", b)
            out.append(("h", n, bool(v & 4)))
        elif size == 32:
            if n >= len(b.lts):
                self.bad("lts-room", addr, f"{name}: LTS{n} missing", b)
            out.append(("w", n))
        elif size == 64:
            if n == 3:
                self.bad("lts-room", addr, f"{name}: 64-bit literal at LTS3", b)
            elif n + 1 >= len(b.lts):
                self.bad("lts-room", addr, f"{name}: LTS{n}-{n + 1} missing", b)
            out += [("w", n), ("w", n + 1)]

    def bundle(self, b, addr):
        d, iset = self.data, self.iset
        st = self.stats
        st["bundles"] += 1
        groups = set()
        lts_uses = []

        # ALU channels -------------------------------------------------------
        ops = {}
        for ch in sorted(b.als):
            op = self.pr.match(b, ch)
            if op is None:
                opc = (b.als[ch] >> 24) & 0x7f
                other = sorted({o["name"] for o in self.spec.by_opc.get(opc, [])})
                self.bad("channel", addr, f"ALC{ch}: opcode {opc:#x} matches no operation "
                         f"allowed there (with it: {', '.join(other[:6]) or 'none'})", b)
                continue
            form = d.forms.get((iset, op["name"]))
            if form is None:
                self.bad("data", addr, f"{op['name']} has no v{iset} form", b)
                continue
            if ch not in form["channels"]:
                self.bad("channel", addr, f"{op['name']} in ALC{ch}, allowed "
                         f"{form['channels']}", b)
            ops[ch] = (op, form)
            st["alu ops"] += 1

        # ALES and the ALES2/ALES5 word
        words = 1 + (b.ss is not None) + len(b.als) + (b.cs0 is not None)
        mdl = ((b.hs & 0xf) + 1) * 4
        ales25 = mdl == 4 * words + (8 if b.cs1 is not None else 4)
        if ales25 and iset < 4:
            self.bad("ales25-before-v4", addr, "ALES2/ALES5 word before v4", b)
        for ch, (op, form) in ops.items():
            want = ales_expected(form, ch)
            marked = ch in b.ales_marked
            got = "none" if not marked else (
                "default" if b.ales.get(ch) == e2kdis.ALES25_DEFAULT else "value")
            if want != got and not (want == "value" and got == "default"):
                self.bad("ales", addr, f"{op['name']} in ALC{ch}: ALES {got}, data {want}", b)

        # Pairs, channel occupancy, loads and stores
        owner, ldst = {}, {}
        for ch in sorted(ops):
            op, form = ops[ch]
            name = op["name"]
            if ch in owner:
                continue
            owner[ch] = ch
            held = [ch]
            pair = next((p for p in form.get("pairs", []) if ch in p), None)
            if pair:
                lo, hi = pair
                if form["pair_encoding"] == "both":
                    other = hi if ch == lo else lo
                    if other in ops and ops[other][0]["name"] == name and other not in owner:
                        owner[other] = ch
                        held.append(other)
                    else:
                        self.bad("one-op-per-channel", addr,
                                 f"{name} in ALC{ch} without its other half in ALC{other}", b)
                else:
                    if ch != lo:
                        self.bad("one-op-per-channel", addr,
                                 f"{name} encoded in ALC{ch}, not ALC{lo} (v7)", b)
                    elif hi in ops:
                        self.bad("one-op-per-channel", addr,
                                 f"{name} in ALC{lo} with {ops[hi][0]['name']} in ALC{hi}", b)
                    owner[hi] = ch
                    held.append(hi)
                st["paired ops"] += 1
            dst = b.als[ch] & 0xff
            if "implicit_pair" in form and dst not in CTPR_DST:
                other = ch + form["implicit_pair"]["delta"]
                if other in ops:
                    self.bad("one-op-per-channel", addr,
                             f"{name} in ALC{ch} (into a register) with "
                             f"{ops[other][0]['name']} in ALC{other}", b)
                held.append(other)
            if "ldst" in form:
                for c in held:
                    ldst[c] = form["ldst"]
            # %ctpr destinations
            fmt = op["alopf"]
            if fmt in DST_FORMATS and dst in CTPR_DST:
                allowed = form.get("ctpr_dst_channels")
                if allowed is None:
                    st[f"other op into %ctpr: {name}"] += 1
                elif ch not in allowed:
                    self.bad("ctpr-dst-alc0", addr, f"{name} into %ctpr in ALC{ch}", b)
            # literals
            als = b.als[ch]
            if fmt in SRC1_FORMATS:
                self.lts_refs(b, (als >> 16) & 0xff, "src1", addr, name, form, lts_uses)
            if fmt in SRC2_FORMATS:
                self.lts_refs(b, (als >> 8) & 0xff, "src2", addr, name, form, lts_uses)
            if fmt in SRC3_ALES:
                self.lts_refs(b, b.ales.get(ch, 0) & 0xff, "src3", addr, name, form,
                              lts_uses)
            if fmt in STORE_DATA and fmt not in ("ALOPF10", "AAURW"):
                self.lts_refs(b, dst, "src3", addr, name, form, lts_uses)
            if fmt in ("ALOPF10", "ALOPF19"):
                lt = (als >> 8) & 3
                if lt:
                    if lt - 1 >= len(b.lts):
                        self.bad("lts-room", addr, f"{name}: AAU literal LTS{lt - 1} missing", b)
                    lts_uses.append(("w", lt - 1))
        for rid in ("store-alc2-load-alc3", "store-alc5-load-alc0"):
            if d.applies(rid, iset) and all(
                    ldst.get(c["channel"]) == c["ldst"] for c in d.restrictions[rid]["forbid"]):
                self.bad(rid, addr, "forbidden load/store channels", b)
        self.maxes["loads"] = max(self.maxes["loads"],
                                  sum(1 for c in ldst if ldst[c] == "load"))
        self.maxes["stores"] = max(self.maxes["stores"],
                                   sum(1 for c in ldst if ldst[c] == "store"))
        self.maxes["ALU channels"] = max(self.maxes["ALU channels"], len(owner))

        # CS1 ----------------------------------------------------------------
        cs1op = b.cs1 >> 28 if b.cs1 is not None else None
        cs1_group = None
        if cs1op is not None:
            gs = d.cs1_by_opc.get(cs1op, [])
            if cs1op == 2:
                gs = ["setsft" if b.cs1 & (1 << 27) else "setei"]
            if len(gs) != 1:
                self.bad("cs1-one-group", addr, f"CS1 opcode {cs1op} is no group", b)
            else:
                cs1_group = gs[0]
                groups.add(cs1_group)
        mas = {}
        if cs1_group == "mas":
            for ch, sh in MAS_SHIFT.items():
                v = (b.cs1 >> sh) & 0x7f
                if v:
                    mas[ch] = v
        for ch, v in mas.items():
            op_form = ops.get(owner.get(ch, ch))
            if op_form is None or "mas" not in op_form[1]:
                self.bad("mas", addr, f"MAS {v:#x} for ALC{ch}, which holds no MAS access", b)
        for ch, (op, form) in ops.items():
            if form.get("mas") == "always" and cs1_group != "mas":
                self.bad("cs1-one-group", addr, f"{op['name']} needs CS1's MAS", b)
        self.maxes["MAS fields"] = max(self.maxes["MAS fields"], len(mas))
        if cs1_group == "setcmd":
            if cs1op in (0, 1) and not b.lts:
                self.bad("lts-room", addr, "setwd without LTS0", b)
            if cs1op in (0, 1):
                lts_uses.append(("w", 0))

        # CS0, SS: prepares and transfers -------------------------------------
        cs0 = None
        if b.cs0 is not None:
            ctpr, ctp_opc, param = b.cs0 >> 30, (b.cs0 >> 28) & 3, b.cs0 & 7
            cs0 = CS0_TABLE[ctpr][ctp_opc]
            if cs0 == "return" and param == 1:
                cs0 = "gettsd"
            if cs0 is None:
                self.bad("cs0-one", addr, f"reserved CS0 ({ctpr}, {ctp_opc})", b)
            elif cs0 in PREPARES:
                groups.add("prepare")
            elif cs0 == "pref":
                groups.add("pref")
        ss = b.ss
        ctop = (ss >> 10) & 3 if ss is not None else 0
        cc = ss & 0x1ff if ss is not None else 0
        ctype = (cc >> 5) & 0xf
        sstype = (ss >> 20) & 1 if ss is not None else 0
        transfers = []
        if cs1_group == "call":
            if ctop:
                transfers.append("call")
            else:
                kind = {1: "icall", 2: "hcall"}.get((b.cs1 >> 7) & 7)
                if kind is None or cs0 != "ibranch":
                    self.bad("ctop-one", addr, "call without a CTPR or displacement", b)
                transfers.append(kind or "call?")
        elif ctop and ctype:
            transfers.append("ct")
        if cs0 == "ibranch" and not ctop and "icall" not in transfers \
                and "hcall" not in transfers:
            if ctype:
                transfers.append("ibranch")
            else:
                st["CS0 ibranch without a condition"] += 1
        if cs0 == "done" and ctype:
            transfers.append("done")
        for ch, (op, form) in ops.items():
            if "ss.ctop" in form.get("uses", []):
                transfers.append(op["name"])
        if len(transfers) > 1:
            self.bad("ctop-one", addr, f"transfers {transfers} share SS.ctop", b)
        if ctype and not ctop and not transfers:
            self.bad("ctcond-one", addr, f"condition type {ctype} without a transfer", b)
        # A conditional abgc/rbgc (v6+: SS type 1, SS.abg 2 or 3) keeps its
        # predicate in ctcond.psrc, so no transfer condition may share it.
        if iset >= 6 and ss is not None and (ss >> 20) & 1 and (ss >> 23) & 3 >= 2 and ctype:
            self.bad("ctcond-one", addr, f"conditional abgc/rbgc with condition type {ctype}", b)
        groups.discard("call")  # the CS1 group; the transfer is named below
        groups.update(transfers)
        if (cs0 in PREPARES and d.applies("same-ctpr", iset) and ctop
                and (b.cs0 >> 30) == ctop and transfers and transfers[0] in ("ct", "call")):
            self.bad("same-ctpr", addr, f"{cs0} and {transfers[0]} through %ctpr{ctop}", b)
        if transfers:
            st["transfers"] += 1

        # SS fields: loop, rotation, prefetch, type
        if ss is not None and not sstype:
            for name, sh in (("alc", 16), ("abp", 18), ("abn", 21)):
                if (ss >> sh) & 3:
                    groups.add(name)
        if ss is not None and sstype and iset < 6:
            st["SS of type 1 before v6"] += 1
        if iset >= 7 and ss is not None and ss & (1 << 9) and "ibranch" not in transfers:
            self.bad("dtbp-branch", addr, "dtbp without ibranch/rbranch", b)
        if iset >= 7 and ctype == 11 and not sstype:
            for name, sh in (("alc", 16), ("abp", 18), ("abn", 21)):
                v = (ss >> sh) & 3
                if v in (1, 2):
                    self.bad("ct11-flags", addr, f"ctcond 11 with {name} t != f", b)

        # PLS ----------------------------------------------------------------
        pls = b.pls
        need = set()
        clps = {}
        for i, p in enumerate(pls):
            c = p & 0xffff
            clps[i] = (c >> 14, (c >> 10) & 7, (c >> 6) & 7, bool(c & 0x20))
            if c & 0x20:
                need.add(i)
        cmp_need, dt_need = set(), set()
        if ctype in (9, 11) or (ctype == 8):
            psrc = cc & 0x1f
            if ctype == 8:
                for i, c in enumerate((0, 1, 3, 4)):
                    if psrc & (1 << i):
                        dt_need.add(c)
            else:
                kind = (psrc >> 3) & 3
                if kind == 0:
                    cmp_need.add((0, 1, 3, 4)[(psrc >> 1) & 3])
                elif kind == 1:
                    cmp_need |= {3, 4} if psrc & 4 else {0, 1}
                elif kind == 2:
                    n = (psrc >> 1) & 3
                    if n >= len(pls) or n == 3:
                        self.bad("ctcond-names-ops", addr, f"%clp{n} without CLP{n}", b)
                    else:
                        need.add(n)
        for c in cmp_need:
            op_form = ops.get(c)
            if op_form is None or not op_form[1].get("int_compare"):
                self.bad("ctcond-names-ops", addr, f"%cmp{c} without an integer compare "
                         f"in ALC{c}", b)
        for c in dt_need:
            if c not in ops or not b.als[c] >> 31:
                self.bad("ctcond-names-ops", addr, f"%dt_al{c} without a ,sm operation in "
                         f"ALC{c}", b)
        todo = list(need)
        while todo:
            i = todo.pop()
            for q in clps[i][1:3]:
                if q >= 4 and q - 4 not in need and q - 4 in clps:
                    need.add(q - 4)
                    todo.append(q - 4)
        depth = {}
        for i in sorted(need):
            opc, p0, p1, _ = clps[i]
            if opc == 2:
                self.bad("pls-slots", addr, f"CLP{i}: reserved opcode", b)
            dep = 1
            for q in (p0, p1):
                if q not in d.clp_inputs[i]:
                    self.bad("pls-slots", addr, f"CLP{i} reads @p{q}", b)
                elif q < 4 and q // 2 >= len(pls):
                    self.bad("pls-slots", addr, f"CLP{i} reads @p{q}, whose ELP is absent", b)
                elif q >= 4:
                    dep = max(dep, depth.get(q - 4, 1) + 1)
            depth[i] = dep
            if dep > d.clp_depth:
                self.bad("pls-slots", addr, f"CLP{i} at depth {dep}", b)
        if need or any((p >> 16) & 0x7f7f for p in pls[:2]):
            groups.add("pls")
        self.maxes["CLPs"] = max(self.maxes["CLPs"], len(need))

        # CDS ----------------------------------------------------------------
        entries = []
        for cds in b.cds:
            for half in (cds >> 16, cds & 0xffff):
                if (half >> 10) & 0xf:
                    entries.append(half)
        if len(entries) > d.cds_entries:
            self.bad("cds-entries", addr, f"{len(entries)} CDS entries", b)
        mrgc_chans = set()
        for e in entries:
            opc, mask, pred = e >> 14, (e >> 10) & 0xf, e & 0x7f
            cluster = opc & 1
            base = 3 * cluster
            if pred & 0x40 == 0:
                st["CDS entry reading %lcntex"] += 1
            for k in range(3):
                if mask & (1 << k):
                    ch = base + k
                    if ch not in ops and owner.get(ch) is None:
                        self.bad("cds-entries", addr, f"{'MRGC' if opc >= 2 else 'RLP'} "
                                 f"entry for empty ALC{ch}", b)
                    if opc >= 2:
                        mrgc_chans.add(ch)
            if mask & 8:
                if opc < 2:
                    ch = base + 2
                    als = b.als.get(ch)
                    op_form = ops.get(ch)
                    if op_form is None or op_form[0]["alopf"] != "ALOPF10" \
                            or not (als >> 8) & 4:
                        self.bad("cds-entries", addr, f"AM condition without a staa "
                                 f"incr in ALC{ch}", b)
                elif opc == 2:
                    if ss is None or not (ss >> 25) & 1 and not (ss >> 27) & 1:
                        self.bad("cds-entries", addr, "recovery-point condition without "
                                 "srp/crp", b)
                else:
                    self.bad("cds-entries", addr, "MRGC1 mask bit 3", b)
        for ch, (op, form) in ops.items():
            if form.get("mrgc") and ch not in mrgc_chans:
                self.bad("cds-entries", addr, f"{op['name']} in ALC{ch} without an MRGC "
                         f"entry", b)
        if entries:
            groups.add("cds")
        self.maxes["CDS entries"] = max(self.maxes["CDS entries"], len(entries))

        # AAS ----------------------------------------------------------------
        movas = sorted(i - 2 for i in b.aas if i >= 2)
        upper = set()
        for j in movas:
            if j in upper:
                continue        # the second half of a v2-v6 movaq
            opc = (b.aas[j + 2] >> 12) & 7
            if opc in (0, 6):
                self.bad("movaq-even", addr, f"mova channel {j}: reserved opcode {opc}", b)
            if opc == 5:
                if j % 2:
                    self.bad("movaq-even", addr, f"movaq in mova channel {j}", b)
                elif iset < 7 and (j + 1 not in movas or (b.aas[j + 3] >> 12) & 7 != 5):
                    self.bad("movaq-even", addr, f"movaq in {j} without its pair", b)
                elif iset < 7:
                    upper.add(j + 1)
                elif iset >= 7 and j + 1 in movas:
                    self.bad("movaq-even", addr, f"movaq in {j} with mova {j + 1}", b)
        if movas:
            groups.add("mova")
        self.maxes["movas"] = max(self.maxes["movas"], len(movas))

        # LTS ----------------------------------------------------------------
        if lts_uses:
            groups.add("lts")
        used = {u[1] for u in lts_uses}
        if len(b.lts) > d.lts_count:
            self.bad("lts-room", addr, f"{len(b.lts)} LTS", b)
        if b.lts and len(used) < len(b.lts):
            st["LTS words nothing reads (padding)"] += len(b.lts) - len(used)
        self.maxes["LTS words used"] = max(self.maxes["LTS words used"], len(used))
        self.maxes["words"] = max(self.maxes["words"], b.length // 4)
        if b.length // 4 > d.max_words:
            self.bad("size", addr, f"{b.length // 4} words", b)

        # The data's control_operations.uses: no two operations found here
        # may claim one exclusive field. The encoding allows one of each, so
        # a clash means the data names the wrong syllable for one of them.
        names = set(transfers) - {"ibranchd", "icalld", "call?"}
        if cs0 in PREPARES or cs0 in ("pref", "puttsd"):
            names.add(cs0)
        if cs1_group == "setcmd":
            names |= {"setwd"} if cs1op == 0 else {"setwd", "vfrpsz"} if cs1op == 1 else set()
            names |= {n for n, bit in (("setbn", 26), ("setbp", 27)) if b.cs1 >> bit & 1}
        elif cs1_group == "flush":
            names |= {n for n, bit in (("flushr", 0), ("flushc", 1), ("fillr", 2),
                                       ("fillc", 3)) if b.cs1 >> bit & 1}
        elif cs1_group in ("setei", "setsft", "wait"):
            names.add(cs1_group)
        if ss is not None and not sstype:
            names |= {n for n, sh in (("alc", 16), ("abp", 18), ("abn", 21)) if ss >> sh & 3}
            if iset <= 5 and ss >> 23 & 3:
                names.add("abg")
            names |= {n for n, bit in (("bap", 28), ("eap", 29), ("vfdi", 26))
                      if ss >> bit & 1}
            rp = (ss >> 27 & 1) << 1 | ss >> 25 & 1
            if rp:
                names.add(("", "crp", "srp", "slrp")[rp])
        if movas:
            names.add("movad")
        if need:
            names.add("andp")
        fields = collections.defaultdict(set)
        for n in names:
            for u in d.control.get(n, {}).get("uses", ["?" + n]):
                fields[u.split(".")[0] if u.startswith("cs1.") else u].add(
                    u if u.startswith("cs1.") else n)
        if cs1_group == "mas":
            fields["cs1"].add("cs1.mas")
        for ch, (op, form) in ops.items():
            for u in form.get("uses", []):
                fields[u].add(op["name"])
        for f, who in fields.items():
            if f.startswith("?"):
                self.bad("uses", addr, f"{f[1:]} is not in control_operations", b)
            elif f in ("cs0", "ss.ctop", "ss.ctcond", "cs1") and len(who) > 1:
                self.bad("uses", addr, f"{', '.join(sorted(who))} all claim {f}", b)

        # What shares an instruction ------------------------------------------
        g = sorted(x for x in groups if x in SHARE_GROUPS)
        for i, x in enumerate(g):
            for y in g[i + 1:]:
                self.share[(x, y)] += 1
        if len(mas) > 1:
            self.share[(f"{len(mas)} MAS accesses", "")] += 1


def walk(buf, starts, programs):
    """Yield (offset, Bundle) for each wide instruction in BUF, (offset,
    "apb") for each APB instruction of a program starting at an offset in
    PROGRAMS, and (offset, None) for 16 undecodable bytes, after which the
    walk resynchronises at the next symbol in STARTS if one is closer."""
    off, si = 0, 0
    while off + 8 <= len(buf):
        if off in programs:
            for _ in range(32):
                yield off, "apb"
                last = int.from_bytes(buf[off:off + 4], "little") >> 31
                off += 16
                if last:
                    break
            continue
        length = (((buf[off] >> 4) & 7) + 1) * 8
        b = e2kdis.Bundle(buf[off:off + length]) if off + length <= len(buf) else None
        if b is None or not b.valid:
            yield off, None
            nxt = off + 16
            while si < len(starts) and starts[si] <= off:
                si += 1
            if si < len(starts) and starts[si] < nxt:
                nxt = starts[si]
            off = nxt
            continue
        yield off, b
        off += length


def decode_file(path, iset_override, data_path):
    try:
        elf = Elf(path)
    except ValueError as e:
        return dict(path=path, error=str(e))
    iset = iset_override or elf.iset()
    if iset is None:
        m = re.findall(r"-v(\d)", Path(path).name)
        iset = int(m[-1]) if m else None
    if iset is None:
        raise SystemExit(f"{path}: unknown ISA version; give --iset")
    data = Data(data_path)
    chk = Checker(data, iset)
    pr = chk.pr
    invalid = apb = 0
    for sec in elf.sections:
        buf = sec.data
        starts = sorted(a - sec.addr for a in elf.symbols if sec.addr <= a < sec.addr + len(buf))
        # Array prefetch programs are 16-byte APB instructions, not wide
        # instructions, though they may parse as some (apb.md): find them
        # from the ldisp that loads each, and skip them.
        programs = set()
        for off, b in walk(buf, starts, ()):
            if b is not None and b.cs0 is not None and b.cs0 >> 28 == 0x9:
                disp = ((b.cs0 & 0x0fffffff) ^ 0x08000000) - 0x08000000
                programs.add(off + 8 * disp)
        for off, b in walk(buf, starts, programs):
            if b is None:
                invalid += 1
            elif b == "apb":
                apb += 1
            else:
                chk.bundle(b, sec.addr + off)
    examples = {}
    for rule, ex in chk.examples.items():
        examples[rule] = [(a, m, pr.bundle(b, a)) for a, m, b in ex[:200]]
    return dict(path=path, iset=iset, invalid=invalid, apb=apb, viol=chk.viol, examples=examples,
                stats=chk.stats, maxes=chk.maxes, share=chk.share)


# Self-test: wide instructions that break each rule, composed from
# syllables gas encodes (it refuses to write most of them whole), and the
# rules the checker must report for them. Each part is one wide
# instruction; parts are named by ISA version and position.
PARTS = {
    3: ["ldd,0 %dr0, 0x8, %dr1\n std,2 %dr2, 0x10, %dr3",       # 0
        "ldd,3 %dr0, 0x8, %dr1",                                 # 1
        "std,5 %dr2, 0x10, %dr3",                                # 2
        "sdivd,5 %dr0, %dr1, %dr2",                              # 3
        "movtd,1 %dr0, %dr1",                                    # 4
        "disp %ctpr1, .Lt",                                      # 5
        "ct %ctpr1",                                             # 6
        "cmpedb,0 %dr0, %dr1, %pred0\n ibranch .Lt ? %MLOCK || %cmp0",  # 7
        "addd,0 %dr0, %dr1, %dr2",                               # 8
        "pass %pred0, @p0\n pass %pred1, @p1\n andp @p0, @p1, @p4\n pass @p4, %pred2",
        "addd,0 %dr0, %dr1, %dr2 ? %pred0",                      # 10
        "addd,1 %dr0, %dr1, %dr2",                               # 11
        "merged,0 %dr0, %dr1, %dr2, %pred0",                     # 12
        "addd,0 %dr0, 0x1234, %dr1",                             # 13
        "addd,0 %dr0, 0x12345678, %dr1",                         # 14
        "ldd,0 %dr0, 0x8, %dr1, mas=0x4",                        # 15
        "setwd wsz = 0x8, nfx = 0x1, dbl = 0x0\n setbn rbs = 0x4, rsz = 0x3, rcur = 0x0\n"
        " disp %ctpr2, .Lt"],                                    # 16
    4: ["fadds,2 %r0, %r1, %r2"],
    5: ["stq,2 %dr0, 0x0, %qr2",
        "movaq,0 area = 0, ind = 0, am = 1, be = 0, %qb[0]"],
    6: ["ibranchd,0 %dr0, %empty",
        "ct %ctpr1",
        "disp %ctpr1, .Lt\n ct %ctpr1",
        "abgc 2\n ct %ctpr1",
        "abgc 1 ? %pred3"],
    7: ["cmpedb,0 %dr0, %dr1, %pred0\n ibranch .Lt ? %cmp0",
        "alc alcf = 1, alct = 1\n ct %ctpr1"],
}


def compose(als=None, ales=None, ss=None, cs0=None, cs1=None, aas=None, lts=(),
            pls=(), cds=()):
    """Bytes of a wide instruction with these syllables (bundle.md,
    "Overall layout"): ALES2/ALES5 values other than the default get the
    shared word; aas maps AAS index to half, in the order SS.aa gives."""
    als, ales, aas = als or {}, ales or {}, aas or {}
    hs, pre = 0, []
    if ss is not None:
        hs |= 1 << 12
        pre.append(ss)
    for ch in sorted(als):
        hs |= 1 << (26 + ch)
        pre.append(als[ch])
    if cs0 is not None:
        hs |= 1 << 14
        pre.append(cs0)
    for ch in ales:
        hs |= 1 << (20 + ch)
    dflt = e2kdis.ALES25_DEFAULT
    if ales.get(2, dflt) != dflt or ales.get(5, dflt) != dflt:
        pre.append(ales.get(2, dflt) << 16 | ales.get(5, dflt))
    if cs1 is not None:
        hs |= 1 << 15
        pre.append(cs1)
    halves = [ales[ch] for ch in (0, 1, 3, 4) if ch in ales]
    if ss is not None and not ss & (1 << 20):
        aa = [i for i in range(4) if ss & (1 << (12 + i))]
        halves += [aas[i] for i in sorted({i >> 1 for i in aa}) + [2 + i for i in aa]]
    if len(halves) % 2:
        halves.append(0)
    mid = [halves[k] << 16 | halves[k + 1] for k in range(0, len(halves), 2)]
    tail = list(reversed(lts)) + list(reversed(pls)) + list(reversed(cds))
    words = [0] + pre + mid
    if (len(words) + len(tail)) % 2:
        words.append(0)
    words += tail
    hs |= len(pre) | (len(words) // 2 - 1) << 4 | len(cds) << 16 | len(pls) << 18
    words[0] = hs
    return b"".join(w.to_bytes(4, "little") for w in words)


def selftest_cases(p):
    """(name, ISA version to check as, bytes, expected rules[, a change to
    the data made first])."""
    v3 = p[3]
    ldd0, std2 = v3[0].als[0], v3[0].als[2]
    ldd3, std5 = v3[1].als[3], v3[2].als[5]
    lit16 = v3[13]
    addd0 = v3[8].als[0]
    ibr = v3[7]
    pls_ok = v3[9].pls
    pls_bad = [pls_ok[0] & ~(7 << 6) | 2 << 6]           # CLP0 reads @p2
    elp = pls_ok[0] & 0x7f7f0000
    deep = [elp | 0x0000 | 0 << 10 | 1 << 6,               # CLP0 = @p0 & @p1
            0 | 4 << 10 | 0 << 6,                          # CLP1 = @p4 & @p0
            5 << 10 | 0 << 6 | 1 << 5 | 3]                 # CLP2 = @p5 & @p0 -> %pred3
    movtd_ctpr = v3[4].als[1] & ~0xff | 0xd1
    st_lit = std2 & ~0xff | 0xd8
    stq = p[5][0]
    movaq = p[5][1]
    aas1 = {0: movaq.aas[0], 3: movaq.aas[2]}       # mova 1 = gas's mova 0
    ss_aa1 = movaq.ss & ~(0xf << 12) | 1 << 13
    v7 = p[7]
    return [
        ("gas's own instruction", 3, compose(als=v3[0].als, lts=v3[0].lts), set()),
        ("setwd, setbn and disp", 3, compose(cs0=v3[16].cs0, cs1=v3[16].cs1, lts=v3[16].lts),
         set()),
        ("store ALC2 + load ALC3", 3, compose(als={2: std2, 3: ldd3}, lts=v3[0].lts),
         {"store-alc2-load-alc3"}),
        ("store ALC5 + load ALC0", 3, compose(als={0: ldd0, 5: std5}, lts=v3[0].lts),
         {"store-alc5-load-alc0"}),
        ("sdivd's opcode in ALC2", 3, compose(als={2: v3[3].als[5]}), {"channel"}),
        ("movtd into %ctpr1 in ALC1", 3, compose(als={1: movtd_ctpr}), {"ctpr-dst-alc0"}),
        ("disp + ct through %ctpr1, v3", 3, compose(ss=v3[6].ss, cs0=v3[5].cs0),
         {"same-ctpr"}),
        ("disp + ct through %ctpr1, v6", 6, compose(ss=p[6][2].ss, cs0=p[6][2].cs0), set()),
        ("%cmp0 without a compare", 3, compose(ss=ibr.ss, als={0: addd0}, cs0=ibr.cs0),
         {"ctcond-names-ops"}),
        ("condition without a transfer", 3, compose(ss=ibr.ss, als={0: ibr.als[0]}),
         {"ctcond-one"}),
        ("abgc + ct from gas", 6, compose(ss=p[6][3].ss), set()),
        ("conditional abgc + ct", 6, compose(ss=p[6][4].ss | 1 << 10 | 1 << 5),
         {"ctcond-one"}),
        ("ibranchd + ct", 6, compose(ss=p[6][1].ss, als=p[6][0].als, ales=p[6][0].ales),
         {"ctop-one", "uses"}),
        ("data saying disp takes CS1", 3,
         compose(cs0=v3[16].cs0, cs1=v3[16].cs1, lts=v3[16].lts), {"uses"},
         lambda d: d.control["disp"].update(uses=["cs1.wait"])),
        ("CLP0 reads @p2", 3, compose(pls=pls_bad), {"pls-slots"}),
        ("CLPs three deep", 3, compose(pls=deep), {"pls-slots"}),
        ("RLP for an empty channel", 3, compose(als={1: v3[11].als[1]}, cds=v3[10].cds),
         {"cds-entries"}),
        ("merge without MRGC", 3, compose(als=v3[12].als), {"cds-entries"}),
        ("16-bit literal in LTS2", 3,
         compose(als={0: lit16.als[0] & ~0xff00 | 0xd2 << 8}, lts=lit16.lts[:1] * 3),
         {"lts-room"}),
        ("32-bit literal in a missing LTS1", 3,
         compose(als={0: v3[14].als[0] & ~0xff00 | 0xd9 << 8, 1: v3[11].als[1]},
                 lts=v3[14].lts[:1]),
         {"lts-room"}),
        ("literal store data before v7", 3, compose(als={2: st_lit}, lts=v3[0].lts),
         {"literal-form"}),
        ("MAS for an empty channel", 3, compose(als={0: ldd0}, cs1=6 << 28 | 0x4 << 7),
         {"mas"}),
        ("ALES2 word on v3", 3, compose(als=p[4][0].als, ales=p[4][0].ales),
         {"ales25-before-v4", "channel"}),
        ("stq without its ALC5 half", 5, compose(als={2: stq.als[2]}, ales={2: stq.ales[2]},
                                                lts=v3[0].lts),
         {"one-op-per-channel"}),
        ("movaq pair from gas", 5, compose(ss=movaq.ss, aas=movaq.aas), set()),
        ("movaq in mova channel 1", 5, compose(ss=ss_aa1, aas=aas1), {"movaq-even"}),
        ("dtbp without ibranch", 7, compose(ss=v7[1].ss | 1 << 9), {"dtbp-branch"}),
        ("ctcond 11 with alct != alcf", 7,
         compose(ss=v7[0].ss | 1 << 16, als=v7[0].als, cs0=v7[0].cs0), {"ct11-flags"}),
    ]


def selftest(binutils, data_path):
    import subprocess
    import tempfile
    data = Data(data_path)
    parts = {}
    with tempfile.TemporaryDirectory(dir=ROOT / "toolchains") as tmp:
        for iset, asm in PARTS.items():
            src, obj, raw = (Path(tmp) / f"p{iset}.{x}" for x in ("s", "o", "bin"))
            src.write_text("\t.ignore ld_st_style\n\t.text\n.Lt:\n"
                           + "".join(f"{{\n {a}\n}}\n" for a in asm))
            subprocess.run([binutils / "e2k-linux-as", f"-mcpu=elbrus-v{iset}", "-o", obj,
                            src], check=True)
            subprocess.run([binutils / "e2k-linux-objcopy", "-O", "binary", "-j", ".text",
                            obj, raw], check=True)
            buf = raw.read_bytes()
            parts[iset] = [b for _, b in walk(buf, [], ())]
            if len(parts[iset]) != len(asm) or None in parts[iset]:
                sys.exit(f"selftest: v{iset} parts did not assemble one instruction each")
            # compose() must lay out syllables as gas does.
            off = 0
            for b in parts[iset]:
                again = compose(als=b.als, ales={c: b.ales[c] for c in b.ales_marked},
                                ss=b.ss, cs0=b.cs0, cs1=b.cs1, aas=b.aas, lts=b.lts,
                                pls=b.pls, cds=b.cds)
                if again != buf[off:off + b.length]:
                    sys.exit(f"selftest: compose() does not reproduce v{iset} part at "
                             f"{off:#x}")
                off += b.length
    failed = 0
    for name, iset, raw, want, *mutate in selftest_cases(parts):
        dd = data
        if mutate:
            dd = Data(data_path)
            mutate[0](dd)
        chk = Checker(dd, iset)
        b = e2kdis.Bundle(raw)
        if not b.valid:
            got = {"(undecodable)"}
        else:
            chk.bundle(b, 0)
            got = set(chk.viol)
        ok = got == want
        failed += not ok
        print(f"{'ok  ' if ok else 'FAIL'} {name}: {', '.join(sorted(got)) or 'no violation'}"
              + ("" if ok else f" (expected {', '.join(sorted(want)) or 'none'})"))
        if not ok and b.valid:
            for line in chk.pr.bundle(b, 0):
                print(f"      {line}")
            for rule, ex in chk.examples.items():
                for _, m, _ in ex:
                    print(f"      {rule}: {m}")
    print(f"selftest: {failed} failed")
    sys.exit(1 if failed else 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("files", nargs="*")
    ap.add_argument("--data", default=str(ROOT / "spec" / "isa" / "reservations.yaml"))
    ap.add_argument("--iset", type=int, help="ISA version for every FILE")
    ap.add_argument("--all", action="store_true", help="print every violation, not 20 a rule")
    ap.add_argument("-j", type=int, default=4, help="files decoded in parallel")
    ap.add_argument("--selftest", action="store_true",
                    help="check that composed instructions breaking each rule are caught")
    ap.add_argument("--binutils", type=Path, default=ROOT / "toolchains" / "binutils" / "bin",
                    help="MCST binutils for --selftest")
    args = ap.parse_args()
    if args.selftest:
        selftest(args.binutils, args.data)
    files = args.files or [f for f in DEFAULT_FILES if Path(f).exists()]
    if not files:
        sys.exit("no files: build toolchains/corpus with tools/isa-decode/build_corpus.sh")

    with concurrent.futures.ProcessPoolExecutor(args.j) as ex:
        results = list(ex.map(decode_file, files, [args.iset] * len(files),
                              [args.data] * len(files)))

    total = collections.Counter()
    by_iset = collections.defaultdict(lambda: dict(
        bundles=0, files=0, share=collections.Counter(), maxes=collections.Counter(),
        stats=collections.Counter()))
    shown = collections.Counter()
    for r in results:
        if "error" in r:
            print(f"skipped: {r['error']}")
            continue
        st = r["stats"]
        nviol = sum(r["viol"].values())
        name = r["path"].replace(str(ROOT) + "/", "")
        if len(results) <= 40 or nviol or not name.startswith("toolchains/corpus"):
            print(f"{name} (v{r['iset']}): {st['bundles']} instructions, "
                  f"{st['alu ops']} ALU operations, {r['apb']} APB program instructions, "
                  f"{r['invalid']} undecodable 16-byte blocks, {nviol} violations")
        agg = by_iset[r["iset"]]
        agg["bundles"] += st["bundles"]
        agg["files"] += 1
        agg["share"].update(r["share"])
        agg["stats"].update(st)
        for k, v in r["maxes"].items():
            agg["maxes"][k] = max(agg["maxes"][k], v)
        for rule, n in r["viol"].items():
            total[rule] += n
            for a, m, lines in r["examples"].get(rule, []):
                if not args.all and shown[rule] >= 20:
                    break
                shown[rule] += 1
                print(f"  {rule}: {name} {a:#x}: {m}")
                for line in lines:
                    print(f"      {line}")

    print()
    for iset in sorted(by_iset):
        agg = by_iset[iset]
        st = agg["stats"]
        print(f"v{iset}: {agg['files']} files, {agg['bundles']} instructions, "
              f"{st['alu ops']} ALU operations ({st['paired ops']} paired), "
              f"{st['transfers']} transfers")
        print("  most in one instruction: " + ", ".join(
            f"{k} {v}" for k, v in sorted(agg["maxes"].items())))
        notes = {k: v for k, v in st.items()
                 if k not in ("bundles", "alu ops", "paired ops", "transfers")}
        if notes:
            print("  notes: " + ", ".join(f"{k} {v}" for k, v in sorted(notes.items())))
        top = sorted(agg["share"].items(), key=lambda kv: -kv[1])
        print("  sharing: " + ", ".join(f"{a}{'+' if b else ''}{b} {n}"
                                        for (a, b), n in top[:30]))
    print()
    if total:
        print("violations: " + ", ".join(f"{k} {v}" for k, v in sorted(total.items())))
        sys.exit(1)
    print(f"violations: none ({sum(a['bundles'] for a in by_iset.values())} instructions "
          f"in {sum(a['files'] for a in by_iset.values())} files)")


if __name__ == "__main__":
    main()
