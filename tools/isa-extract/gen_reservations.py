#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write spec/isa/reservations.yaml: what each E2K operation reserves in a
wide instruction, per ISA version (v2-v7), as data for a scheduler's
pipeline description.

  python3 tools/isa-extract/gen_reservations.py          # write the file
  python3 tools/isa-extract/gen_reservations.py --check  # fail if stale

Inputs:

  spec/isa/binutils-ops.yaml   ALU operations: ISA versions, channels,
                               formats, operand widths, LOAD/STORE/MAS
                               flags; control operations (manual_ops)
  spec/isa/timing-e2s.yaml,    the ALC5 divider's issue distances
  spec/isa/timing-e8c2.yaml,   (bench.py's throughput kernels on the
  spec/isa/timing-e2c3.yaml    4C, 8C2 and 2C3)

and the syllable rules written below, each with its source: bundle.md
(what each syllable holds), predicates.md (PLS and CDS limits),
control-transfer.md and MCST gas (gas/config/tc-e2k.c: what it refuses
to put in one wide instruction). The data only: the generator that turns
it into define_cpu_unit / define_insn_reservation belongs in the port
(doc/gcc-port-plan.md, section 11). tools/isa-extract/check_reservations.py
checks the data against compiled code.
"""

import argparse
import collections
import hashlib
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
ISA = ROOT / "spec" / "isa"
OUT = ISA / "reservations.yaml"
ISETS = (2, 3, 4, 5, 6, 7)
TIMING = {3: ("timing-e2s.yaml", "Elbrus 4C"), 5: ("timing-e8c2.yaml", "Elbrus-8C2"),
          6: ("timing-e2c3.yaml", "Elbrus-2C3")}

# Operand roles in table order, per format (bundle.md, "Operand roles per
# format"). "pdst" is a predicate destination, which has no width in the
# table.
ROLES = {
    "ALOPF1": ("src1", "src2", "dst"),
    "MERGE": ("src1", "src2", "dst"),
    "ALOPF11": ("src1", "src2", "dst"),
    "ALOPF11_MERGE": ("src1", "src2", "dst"),
    "ALOPF11_LIT8": ("src1", "src2", "dst"),
    "ALOPF21": ("src1", "src2", "src3", "dst"),
    "ALOPF21_MERGE": ("src1", "src2", "src3", "dst"),
    "ALOPF21_LT_PACK_CODE": ("src1", "src2", "src3", "dst"),
    "ALOPF2": ("src2", "dst"),
    "ALOPF12": ("src2", "dst"),
    "ALOPF22": ("src2", "dst"),
    "ALOPF12_PSHUFH": ("src2", "dst"),
    "ALOPF12_IBRANCHD": ("src2", "dst"),
    "ALOPF12_ICALLD": ("src2", "dst"),
    "ALOPF7": ("src1", "src2"),
    "ALOPF17": ("src1", "src2"),
    "ALOPF8": ("src2",),
    "ALOPF3": ("src1", "src2", "src3"),
    "ALOPF13": ("src1", "src2", "src3"),
    "ALOPF15": ("src2",),
    "ALOPF16": ("dst",),
    "ALOPF10": ("src3",),
    "AAURW": ("src3",),
    "ALOPF19": ("dst",),
    "AAURR": ("dst",),
    "MMURR": ("dst",),
    "MMURW": ("src3",),
}
# src1 holds a 5-bit literal in these formats (bundle.md, "Operand
# encodings"); in the others it holds opce, a register number or wbs.
SRC1_LITERAL = {"ALOPF1", "MERGE", "ALOPF11", "ALOPF11_MERGE", "ALOPF11_LIT8",
                "ALOPF21", "ALOPF21_MERGE", "ALOPF21_LT_PACK_CODE", "ALOPF7",
                "ALOPF17", "ALOPF3", "ALOPF13"}
# src2 is the ALS byte that takes every literal form. ALOPF16 keeps a fixed
# value there, AAU and MMU accesses a register selector.
SRC2_LITERAL = {f for f, r in ROLES.items() if "src2" in r} - {"ALOPF16"}
SHORT = {"ALOPF1", "MERGE", "ALOPF2", "ALOPF3", "ALOPF7", "ALOPF8"}  # no ALES
OPC2 = {"EXT": 0x1, "EXT1": 0x2, "EXT2": 0x3, "FLB": 0x4, "FLH": 0x5, "FLW": 0x6,
        "FLD": 0x7, "ICMB0": 0x8, "ICMB1": 0x9, "ICMB2": 0xA, "ICMB3": 0xB,
        "FCMB0": 0xC, "FCMB1": 0xD, "PFCMB0": 0xE, "PFCMB1": 0xF, "LCMBD0": 0x10,
        "LCMBD1": 0x11, "LCMBQ0": 0x12, "LCMBQ1": 0x13, "QPFCMB0": 0x16,
        "QPFCMB1": 0x17}
ALES_DEFAULT = 0x01c0
MERGES = {"MERGE", "ALOPF11_MERGE", "ALOPF21_MERGE"}
COMPARES = {"ALOPF7", "ALOPF8", "ALOPF17"}
DIVIDER_NAME = re.compile(r"(div|sqrt|rcp|rsqrt|mod)")


def blob_sha(path):
    """git's blob id of PATH, so the header names the exact inputs."""
    data = path.read_bytes()
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def opc2_value(v):
    return OPC2[v] if isinstance(v, str) else v


def ales_kind(op, chan):
    """What ALES the operation needs in CHAN: none, default (0x01c0) or
    value (anything else). bundle.md, "Operand roles per format"."""
    f = op["alopf"]
    if f in SHORT:
        return "none"
    if f in ("ALOPF22", "ALOPF15", "ALOPF16", "AAURR", "AAURW",
             "ALOPF12_IBRANCHD", "ALOPF12_ICALLD"):
        return "default"
    if f in ("ALOPF10", "ALOPF19"):
        return "value" if op["name"] in ("staaqp", "ldaaqp") else "default"
    if f in ("ALOPF17", "ALOPF11_LIT8", "ALOPF12_PSHUFH") or f.startswith("ALOPF21"):
        return "value"  # cmpopce, an 8-bit literal or src3 in bits 0-7
    opc2 = op["ales_opc2"]
    if isinstance(opc2, dict):
        opc2 = opc2[chan]
    v = opc2_value(opc2) << 8 | op.get("ales_opce", 0)
    return "default" if v == ALES_DEFAULT else "value"


def quad_pairs(op, chans):
    """Channel pairs of a two-channel operation, or None (bundle.md,
    "Operations that take two channels"; gas tc-e2k.c, the
    real_alses/implicit_paired_alc_delta code)."""
    f, a = op["alopf"], op["args"]
    if ((f in ("ALOPF11", "ALOPF11_MERGE") and a[2] == "q")
            or (f in ("ALOPF2", "ALOPF12", "ALOPF22") and a[1] == "q")):
        if 1 in chans:
            return [[0, 1], [3, 4]]
        return [[0, 2], [3, 5]]
    if (f == "ALOPF13" and a[2] == "q") or (f in ("ALOPF10", "AAURW", "ALOPF19", "AAURR")
                                           and a[0] == "q"):
        return [[2, 5]]
    return None


def by_channel(values):
    """{chan: value} -> value if all equal, else {value: [chans]}."""
    vals = set(values.values())
    if len(vals) == 1:
        return vals.pop()
    out = collections.defaultdict(list)
    for c in sorted(values):
        out[values[c]].append(c)
    return dict(sorted(out.items()))


def alu_form(entries, iset):
    """Reservation of one operation name on one ISA version, from its
    binutils table entries valid there."""
    chans = sorted({c for e in entries for c in e["channels"]})
    fmt, ales, opnds = {}, {}, set()
    for e in entries:
        for c in e["channels"]:
            fmt[c] = e["alopf"]
            ales[c] = ales_kind(e, c)
        roles = ROLES[e["alopf"]]
        opnds.add(tuple(zip(roles, e["args"][:len(roles)])))
    if len(opnds) != 1:
        raise SystemExit(f"{entries[0]['name']} v{iset}: operand formats differ: {opnds}")
    e = entries[0]
    form = {"channels": chans, "format": by_channel(fmt), "ales": by_channel(ales)}
    form["operands"] = dict(opnds.pop())
    if any(f in COMPARES for f in fmt.values()):
        form["operands"]["pdst"] = "pred"
    lit = {}
    if all(f in SRC1_LITERAL for f in fmt.values()) and "src1" in form["operands"]:
        lit["src1"] = [5]
    if all(f in SRC2_LITERAL for f in fmt.values()) and "src2" in form["operands"]:
        # gas never gives a 32-bit operand a 64-bit literal: it truncates
        # (bundle.md, "What gas picks for a bare constant").
        lit["src2"] = [4, 16, 32] if form["operands"]["src2"] == "s" else [4, 16, 32, 64]
    if e["alopf"] == "ALOPF21_LT_PACK_CODE":
        lit["src3"] = [32]
    if e["alopf"] in ("ALOPF10", "ALOPF19"):
        lit["aau"] = [32]
    if e["alopf"] in ("ALOPF11_LIT8", "ALOPF12_PSHUFH"):
        lit["lit8"] = [8]
    if lit:
        form["literals"] = lit
    pairs = None
    for x in entries:
        p = quad_pairs(x, chans)
        if p:
            pairs = p
    if pairs:
        # gas also maps ALC3/4 for getsap and getsod, which only ALC0/1 take.
        pairs = [p for p in pairs if set(p) & set(chans)]
        if not pairs:
            raise SystemExit(f"{e['name']} v{iset}: no pair fits {chans}")
        form["pairs"] = pairs
        form["pair_encoding"] = "lower" if iset >= 7 else "both"
    flags = {f for x in entries for f in x["flags"]}
    if "LOAD" in flags:
        form["ldst"] = "load"
    if "STORE" in flags:
        form["ldst"] = "store"
    if "MAS" in flags:
        form["mas"] = "optional"
    if e["alopf"] in ("AAURR", "AAURW", "MMURR", "MMURW"):
        form["mas"] = "always"
    if any(f in COMPARES for f in fmt.values()):
        form["pred_dst"] = True
        if any(x.get("int_comparison") for x in entries):
            form["int_compare"] = True
    if any(f in MERGES for f in fmt.values()):
        form["mrgc"] = True
    if e["name"] in ("movtd", "getpl"):
        form["ctpr_dst_channels"] = [0]
    if e["name"] == "getpl":
        form["implicit_pair"] = {"delta": 1, "when": "destination is not a CTPR"}
    if e["alopf"] in ("ALOPF12_IBRANCHD", "ALOPF12_ICALLD"):
        form["uses"] = ["ss.ctop", "ss.ctcond"]
    if chans == [5] and DIVIDER_NAME.search(e["name"]):
        form["divider"] = True
    elif chans == [5]:
        raise SystemExit(f"{e['name']}: ALC5 only but not named as a divider op")
    if iset == 4 and any(x.get("explicit_ales25_v4") for x in entries) \
            and set(chans) & {2, 5}:
        form["explicit_ales25_v4"] = True
    return form


def check_ales25(name, iset, form):
    """bundle.md, "ALES2 and ALES5": before v4 channels 2 and 5 can only
    have the default ALES. The binutils channel lists must agree."""
    if iset >= 4:
        return
    a = form["ales"]
    bad = [c for c in (2, 5) if c in form["channels"]
           and (a == "value" if isinstance(a, str) else c in a.get("value", []))]
    if bad:
        raise SystemExit(f"{name} v{iset}: non-default ALES in ALC{bad}")


def operations(table):
    by_name = collections.defaultdict(list)
    for e in table["alu_ops"]:
        by_name[e["name"]].append(e)
    for m in table["manual_ops"]:
        if m["name"] in ("mmurr", "mmurw"):
            by_name[m["name"]].append(dict(m, ales_opc2="EXT", ales_opce=0xc0))
    ops, aliases = {}, {}
    for name in sorted(by_name):
        forms = []
        for iset in ISETS:
            entries = [e for e in by_name[name] if iset in e["isets"]]
            if not entries:
                continue
            form = alu_form(entries, iset)
            check_ales25(name, iset, form)
            if forms and forms[-1][1] == form and forms[-1][0][-1] == iset - 1:
                forms[-1][0].append(iset)
            else:
                forms.append(([iset], form))
        if forms:
            ops[name] = [Flow({"isets": i}, **f) for i, f in forms]
            for e in by_name[name]:
                for a in e.get("aliases", []):
                    aliases[a] = name
    return ops, dict(sorted(aliases.items()))


# Control operations: which syllable or field each one takes. Field names
# are those of WIDE_INSTRUCTION below.
CONTROL = {
    "disp": (["cs0"], "prepare a transfer to a label in %ctpr1-3"),
    "ldisp": (["cs0"], "prepare an array-prefetch program in %ctpr2"),
    "sdisp": (["cs0"], "prepare a system call"),
    "return": (["cs0"], "prepare the return in %ctpr3"),
    "gettsd": (["cs0"], "prepare from a type descriptor"),
    "pref": (["cs0"], "instruction prefetch to %iprN"),
    "ct": (["ss.ctop", "ss.ctcond"], "transfer through %ctprN"),
    "call": (["ss.ctop", "ss.ctcond", "cs1.call"], "call through %ctprN; wbs in CS1"),
    "icall": (["ss.ctop", "ss.ctcond", "cs0", "cs1.call"], "call to a displacement in CS0"),
    "hcall": (["ss.ctop", "ss.ctcond", "cs0", "cs1.call"], "hypervisor call"),
    "ibranch": (["ss.ctop", "ss.ctcond", "cs0"], "branch to the displacement in CS0"),
    "rbranch": (["ss.ctop", "ss.ctcond", "cs0"], "ibranch for DAM recovery (%MLOCK)"),
    "done": (["ss.ctop", "ss.ctcond", "cs0"], "return from a trap (kernel)"),
    "iret": (["ss.ctop", "ss.ctcond", "cs0"], "kernel"),
    "hret": (["ss.ctop", "ss.ctcond", "cs0"], "hypervisor"),
    "glaunch": (["ss.ctop", "ss.ctcond", "cs0"], "hypervisor"),
    "setwd": (["cs1.setcmd", "lts0"], "wsz, nfx, dbl in LTS0; may share CS1 with setbn, setbp"),
    "vfrpsz": (["cs1.setcmd", "lts0"], "rpsz in LTS0 bits 12-16; CS1 opcode 1"),
    "settr": (["cs1.setcmd", "lts0"], "type in LTS0 bits 17-31"),
    "setbn": (["cs1.setcmd"], "rotating register base; shares CS1 with setwd, setbp"),
    "setbp": (["cs1.setcmd"], "rotating predicate base; shares CS1 with setwd, setbn"),
    "setei": (["cs1.setei"], "CS1 opcode 2"),
    "setsft": (["cs1.setsft"], "CS1 opcode 2 with bit 27; a software trap"),
    "wait": (["cs1.wait"], "CS1 opcode 3"),
    "flushr": (["cs1.flush"], "CS1 opcode 7; combines with flushc, fillr, fillc"),
    "flushc": (["cs1.flush"], "CS1 opcode 7"),
    "fillr": (["cs1.flush"], "CS1 opcode 7, v6+"),
    "fillc": (["cs1.flush"], "CS1 opcode 7, v6+"),
    "nop": (["hs.nop"], "empty cycles after the instruction, 0-7"),
    "loop_mode": (["hs.lm"], "loop-mode instruction"),
    "set_mark": (["hs.sw"], ""),
    "ipd": (["ss.ipd"], "instruction prefetch depth; without an SS a default applies"),
    "alc": (["ss.alc"], "advance loop counters"),
    "abn": (["ss.abn"], "advance the rotating register base"),
    "abp": (["ss.abp"], "advance the rotating predicate base"),
    "abg": (["ss.abg"], "advance %bgr (v2-v5)"),
    "abgc": (["ss.type1", "ss.abg", "ss.aa", "ss.ctcond"],
             "v6+: NUM in the aa1-aa3 bits of an SS of type 1, the condition's "
             "predicate number in ctcond.psrc, its polarity in ss.abg"),
    "rbgc": (["ss.type1", "ss.abg", "ss.aa", "ss.ctcond"], "v6+, as abgc"),
    "srp": (["ss.rp"], "recovery point; a condition takes MRGC entry 0 (cds)"),
    "crp": (["ss.rp"], "as srp"),
    "slrp": (["ss.rp"], "as srp; crp and srp bits together"),
    "bap": (["ss.type0", "ss.bap"], "begin array prefetch"),
    "eap": (["ss.type0", "ss.eap"], "end array prefetch"),
    "vfdi": (["ss.type0", "ss.vfdi"], ""),
    "ctp_call": (["ss.type1", "ss.aa"], "v7"),
    "dtbp": (["ss.dtbp"], "v7; needs an ibranch or rbranch in the instruction"),
    "flushts": (["ss"], "gas writes the whole SS (kernel only)"),
    "pass": (["pls"], "an ELP (into @p0-@p3) or a CLP's result into %predN"),
    "andp": (["pls"], "a CLP"),
    "landp": (["pls"], "a CLP"),
    "movep": (["pls"], "an MLP (a CLP slot)"),
    "clpandp": (["pls"], "shorthand that fills PLS0; gas refuses it inside braces"),
    "clplandp": (["pls"], "as clpandp"),
    "incr": (["als"], "AAU index increment: bits of the staa in the same channel (2 or 5); "
             "a condition of its own takes an RLP entry's AM bit (cds)"),
    "movab": (["aas", "ss.aa", "ss.type0"], "array prefetch buffer read"),
    "movah": (["aas", "ss.aa", "ss.type0"], ""),
    "movaw": (["aas", "ss.aa", "ss.type0"], ""),
    "movad": (["aas", "ss.aa", "ss.type0"], ""),
    "movaq": (["aas", "ss.aa", "ss.type0"], "two mova channels, the first even"),
    "movaqp": (["aas", "ss.aa", "ss.type0"], "one mova channel"),
    "fapb": ([], "an APB program instruction, not part of a wide instruction (apb.md)"),
}

WIDE_INSTRUCTION = {
    "max_words": 16,
    "size": "A wide instruction is 2-16 32-bit words (8-64 bytes, a multiple of 8): HS "
            "+ SS + one ALS per busy channel + CS0 + the ALES2/ALES5 word + CS1, then "
            "the 16-bit halves (ALES0, 1, 3, 4, then AAS) rounded up to whole words, "
            "then LTS, PLS and CDS words, the total rounded up to an even number "
            "(bundle.md, \"Overall layout\"; gas: \"wide instruction requires N "
            "syllables which exceeds the maximal supported size of 16 ones\").",
    "syllables": {
        "hs": {"words": 1, "fields": {"nop": "empty cycles after it, 0-7",
                                      "lm": "loop_mode", "sw": "set_mark"},
               "note": "always present"},
        "ss": {"words": 1, "fields": {
            "ctop": "the transfer: ct or call through %ctprN (N), or 0 for ibranch, rbranch, "
                    "done, icall, hcall, ibranchd, icalld; one per instruction",
            "ctcond": "the one transfer condition (bundle.md, \"ctcond\")",
            "aa": "which AAS mova syllables are present (type 0); abgc NUM or ctp_call (type 1)",
            "alc": "alct/alcf", "abp": "abpt/abpf", "abn": "abnt/abnf",
            "abg": "abgi/abgd (v2-v5); abgc/rbgc condition polarity (v6+)",
            "rp": "crp, srp (both: slrp)", "vfdi": "", "bap": "", "eap": "",
            "ipd": "instruction prefetch depth", "dtbp": "v7",
            "type": "0: the fields above; 1 (v6+): abgc, rbgc, ctp_call"},
            "note": "present when any field is used; a field may be set by two operations "
                    "only to the same value (gas: \"explicitly overriding a unique "
                    "non-default value\")"},
        "als": {"count": 6, "words": "1 each", "note": "ALS0-ALS5, one operation per channel"},
        "cs0": {"words": 1, "holds_one_of": [
            "disp", "ldisp", "sdisp", "return", "gettsd", "pref", "puttsd",
            "ibranch/rbranch displacement", "icall/hcall displacement",
            "done/iret/hret/glaunch"],
            "note": "one prepare per instruction; ibranch, rbranch, icall and hcall keep "
                    "their displacement here, so they exclude every prepare (gas: \"CS0 in "
                    "the current wide instruction is already busy\"); puttsd: gas does not "
                    "accept it"},
        "ales25": {"words": 1, "isets": [4, 5, 6, 7],
                   "note": "ALES5 in bits 0-15, ALES2 in 16-31; emitted only when either is "
                           "not the default 0x01c0 (on v4 also for an operation with "
                           "explicit_ales25_v4); before v4 it does not exist, so ALC2 and "
                           "ALC5 take only operations with no ALES or the default one"},
        "cs1": {"words": 1, "holds_one_group": {
            "setcmd": {"opcodes": [0, 1, 4], "ops": ["setwd", "vfrpsz", "settr", "setbn",
                                                      "setbp"],
                       "note": "one CS1 for all of them: setwd/vfrpsz/settr pick opcode 0 or "
                               "1 and take LTS0, setbn and setbp are bits 26/27 of opcodes "
                               "0, 1 and 4 (lcc's first instruction: setwd + setbn)"},
            "setei": {"opcodes": [2], "ops": ["setei"]},
            "setsft": {"opcodes": [2], "ops": ["setsft"], "note": "bit 27"},
            "wait": {"opcodes": [3], "ops": ["wait"]},
            "call": {"opcodes": [5], "ops": ["call", "icall", "hcall"],
                     "note": "wbs; a call cannot share with set*, wait or a MAS"},
            "mas": {"opcodes": [6], "ops": "every load or store with a non-zero MAS, and "
                                            "aaurr/aaurw/mmurr/mmurw (always)",
                    "note": "one 7-bit field per channel 0, 2, 3, 5, so any number of MAS "
                            "accesses share it (bundle.md, \"Bundling restrictions\"); a "
                            "zero MAS needs no CS1 (gas omits it)"},
            "flush": {"opcodes": [7], "ops": ["flushr", "flushc", "fillr", "fillc"]},
            "vfbg": {"opcodes": [8], "ops": ["vfbg"], "note": "gas does not accept it"}}},
        "ales_half": {"count": 4, "words": "1/2 each",
                      "note": "ALES0, ALES1, ALES3, ALES4: present when the channel's "
                              "operation has an ALES, default or not"},
        "aas": {"words": "1/2 each", "mova_channels": 4,
                "note": "AAS0 (destinations of mova 0, 1) and AAS1 (mova 2, 3), then one "
                        "half per mova (AAS2-AAS5); needs an SS of type 0"},
        "lts": {"count": 4, "words": "1 each",
                "note": "literals, LTS0 first, placed from the end backwards in whatever "
                        "room is left; LTS0 is also setwd/vfrpsz/settr's (literals, below)"},
        "pls": {"count": 3, "words": "1 each",
                "note": "PLS0 (ELP @p0, @p1; CLP0 -> @p4), PLS1 (ELP @p2, @p3; CLP1 -> @p5), "
                        "PLS2 (CLP2 -> @p6); using PLS N costs PLS0..N (predicates.md, "
                        "\"Slots and limits\")"},
        "cds": {"count": 3, "words": "1 each",
                "note": "two 16-bit RLP/MRGC entries each, 6 in all"},
    },
}

LITERALS = {
    "src1": {5: "5-bit, zero-extended, in the ALS: free"},
    "src2": {4: "4-bit, zero-extended, in the ALS: free",
             16: "half of LTS0 or LTS1 (lo or hi), sign-extended: 1/2 word",
             32: "one of LTS0-LTS3, sign-extended: 1 word",
             64: "LTSn and LTSn+1, n = 0-2: 2 words"},
    "src3": {32: "v7 _lt3 forms: one LTS"},
    "aau": {32: "ldaa*/staa*: one LTS (the ALS lt field), zero-extended"},
    "lit8": {8: "ALOPF11_LIT8 and pshufh: in the ALES, free"},
    "sharing": "operations of one instruction that need the same value share its LTS "
               "half or word (gas tc-e2k.c place_literal); four LTS at most, fewer when "
               "the other syllables leave less room; setwd/vfrpsz/settr take all of LTS0",
    "source": "bundle.md, \"Literal values\" (gas and objdump: check_literals.py)",
}

PLS = {
    "elp": {"slots": 4, "dst": ["@p0", "@p1", "@p2", "@p3"],
            "sources": "%predN, %lcntex, %spredN, %rndpredN, %bgrpred (no negation)"},
    "clp": {"slots": 3, "ops": ["andp", "landp", "movep"],
            "inputs": {"clp0": ["@p0", "@p1"], "clp1": ["@p0", "@p1", "@p2", "@p3", "@p4"],
                       "clp2": ["@p0", "@p1", "@p2", "@p3", "@p4", "@p5"]},
            "dst": {"clp0": "@p4", "clp1": "@p5", "clp2": "@p6"},
            "max_depth": 2,
            "note": "at most two levels (ELP -> CLP -> CLP: \"maximal supported cascading "
                    "of 2 exceeded\"); a CLP may also pass its result to one %predN"},
    "source": "predicates.md, \"Slots and limits\"; gas verify_pls_insns",
}

CDS = {
    "entries": 6,
    "entry": "kind (RLP or MRGC) x cluster (ALC0-2 or ALC3-5) x predicate (%predN or "
             "%pcntN), with a channel mask and a negation bit per channel",
    "sharing": "operations of one cluster with the same predicate and kind share one "
               "entry, each with its own negation; an operation may have one %pred and "
               "one %pcnt RLP (two entries) and a merge also an MRGC",
    "pairs": "a two-channel operation's entry covers both channels on v2-v6 (a (2, 5) "
             "pair: one entry per cluster) and only the lower channel on v7",
    "extra_conditions": "the staa AM increment (mask bit 3 of an RLP entry) and the "
                        "srp/crp/slrp condition (mask bit 3 of MRGC entry 0)",
    "source": "predicates.md, \"RLP\", \"MRGC\", \"How much fits in one wide instruction\"; "
              "gas: \"out of free cds half-syllables\"",
}

AAS = {
    "mova_channels": [0, 1, 2, 3],
    "movaq": "takes mova channels N and N+1, N even (gas: \"movaq should be encoded in "
             "an even channel\"); v2-v6 encode both, v7 only N",
    "needs": "an SS of type 0 (ss.aa marks each mova)",
    "source": "bundle.md, \"AAS\"; gas parse_mova_args",
}

CLUSTERS = {"clusters": [[0, 1, 2], [3, 4, 5]],
            "mas_channels": [0, 2, 3, 5],
            "note": "a cluster selects the RLP/MRGC entry kind; crossing costs are timing "
                    "(timing.md, \"Clusters\"), not reservations"}

# check: how tools/isa-extract/check_reservations.py treats the rule in
# decoded code ("checked": it reports breaks; "encoding": a decoded
# instruction cannot break it; "gas": only an assembler rule).
RESTRICTIONS = [
    {"id": "channel",
     "rule": "every ALU operation in one of its form's channels (operations.channels)",
     "source": "binutils-ops.yaml channels (gas supported_in_alc)", "check": "checked"},
    {"id": "one-op-per-channel",
     "rule": "an ALU channel holds one operation; a paired operation holds both channels "
             "of its pair (pairs, pair_encoding), and getpl into a register also holds "
             "the next channel (implicit_pair)",
     "source": "bundle.md, \"Operations that take two channels\"; gas place_alses",
     "check": "checked"},
    {"id": "ales",
     "rule": "an operation has an ALES exactly when its form says so in that channel "
             "(operations.ales)",
     "source": "bundle.md, \"Operand roles per format\"", "check": "checked"},
    {"id": "store-alc2-load-alc3",
     "rule": "no store in ALC2 together with a load in ALC3",
     "forbid": [{"ldst": "store", "channel": 2}, {"ldst": "load", "channel": 3}],
     "source": "gas place_alses (\"the combination of STORE in ALC2 with LOAD in ALC3 is "
               "prohibited\"); bundle.md, \"Bundling restrictions\"",
     "note": "both channels of a paired load or store count; ldst comes from the table's "
             "LOAD/STORE flags, so AAU stores and aaurw* are stores, aaurr*/ldaa* and "
             "prefetches loads",
     "check": "checked"},
    {"id": "store-alc5-load-alc0",
     "rule": "no store in ALC5 together with a load in ALC0",
     "forbid": [{"ldst": "store", "channel": 5}, {"ldst": "load", "channel": 0}],
     "source": "as above", "check": "checked"},
    {"id": "cs0-one",
     "rule": "one CS0 operation: one prepare (disp, ldisp, sdisp, return, gettsd), pref, "
             "or the displacement of ibranch/rbranch/icall/hcall, or done",
     "source": "bundle.md, \"CS0\" and \"Bundling restrictions\"; gas parse_copf2_args",
     "check": "encoding (the checker reports reserved CS0 codes)"},
    {"id": "ctop-one",
     "rule": "one transfer: ct, call, ibranch, rbranch, done, icall, hcall, ibranchd or "
             "icalld (gas: \"SS.ctop in the current wide instruction is already busy\")",
     "source": "gas parse_ct_args, parse_ibranch_args, parse_alf2_args", "check": "checked"},
    {"id": "ctcond-one",
     "rule": "one transfer condition per instruction, and none without a transfer; a "
             "conditional abgc/rbgc (v6+) uses it too, so it cannot share with a ct",
     "source": "predicates.md, \"Control transfers\"", "check": "checked"},
    {"id": "cs1-one-group",
     "rule": "CS1 holds one group: setcmd (setwd, setbn, setbp, vfrpsz, settr together), "
             "setei, setsft, wait, call, MAS (any number of accesses), flush (flushr, "
             "flushc, fillr, fillc together) or vfbg",
     "source": "bundle.md, \"CS1\" and \"Bundling restrictions\" (\"cannot encode MAS: "
               "CS1 syllable is busy with set-instructions\") [checked, v3, v5, v6]",
     "check": "encoding (the checker reports aaurr/aaurw/mmur* without the MAS group)"},
    {"id": "mas",
     "rule": "a MAS field is set only for a channel holding a MAS access (mas)",
     "source": "bundle.md, \"CS1\"", "check": "checked"},
    {"id": "uses",
     "rule": "no two operations of an instruction claim the same CS0, ss.ctop, ss.ctcond "
             "or CS1 group in control_operations.uses (the operations of one CS1 group "
             "share it)",
     "source": "this file's control_operations, checked against compiled code",
     "check": "checked"},
    {"id": "same-ctpr", "isets": [2, 3, 4, 5],
     "rule": "a prepare (CS0) and a ct or call through the same %ctprN in one "
             "instruction (v6+ allows it, and the transfer then falls through)",
     "source": "gas end_wide_command (MCST bug 154538); control-transfer.md, "
               "\"Restrictions\"", "check": "checked"},
    {"id": "ctpr-dst-alc0",
     "rule": "movtd and getpl into %ctpr1-3 only in ALC0 (ctpr_dst_channels)",
     "source": "gas parse_alf2_args (\"may be encoded in the 0-th channel only\"); "
               "bundle.md, \"Bundling restrictions\"", "check": "checked"},
    {"id": "ctcond-names-ops",
     "rule": "a condition naming %cmpN (ctcond 9, 11) needs an integer compare "
             "(int_compare) in ALC N, %dt_alN (ctcond 8) a speculative operation in ALC N, "
             "%clpN (ctcond 9, 11) CLP N, in the same instruction",
     "source": "gas end_wide_command, check_for_cmp; bundle.md, \"ctcond\"",
     "check": "checked"},
    {"id": "pls-slots",
     "rule": "the CLP inputs and depth in pls; ELPs cannot be negated; only CLP results "
             "go back to %predN",
     "source": "predicates.md, \"Slots and limits\"", "check": "checked"},
    {"id": "cds-entries",
     "rule": "at most 6 RLP/MRGC entries (cds), each naming channels that hold an "
             "operation; a merge needs an MRGC entry; the AM bit needs a staa with incr, "
             "MRGC entry 0's bit 3 an srp/crp",
     "source": "predicates.md, \"How much fits in one wide instruction\", \"MRGC\"",
     "check": "checked"},
    {"id": "lts-room",
     "rule": "at most 4 LTS; a 16-bit literal only in a half of LTS0 or LTS1, a 64-bit "
             "one in LTS0-1, 1-2 or 2-3; setwd/vfrpsz/settr take LTS0",
     "source": "bundle.md, \"Literal values\"; gas place_literals", "check": "checked"},
    {"id": "literal-form",
     "rule": "an operand takes only the literal sizes of its form (operations.literals); "
             "no literal store data or src3 before v7",
     "source": "bundle.md, \"Literal values\"", "check": "checked"},
    {"id": "ales25-before-v4", "isets": [2, 3],
     "rule": "no ALES2/ALES5 word: an operation needing a non-default ALES can't go in "
             "ALC2 or ALC5 (the channel lists already leave those out)",
     "source": "bundle.md, \"ALES2 and ALES5\"", "check": "checked"},
    {"id": "ss-type",
     "rule": "mova*, bap, eap and vfdi need an SS of type 0; abgc, rbgc and ctp_call one of "
             "type 1, so they exclude each other",
     "source": "gas parse_mova_args, parse_bap_args, parse_abgc_args, ...",
     "check": "encoding"},
    {"id": "movaq-even",
     "rule": "movaq in mova channel 0 or 2, holding the next one too",
     "source": "gas parse_mova_args", "check": "checked"},
    {"id": "size",
     "rule": "everything fits in 16 words (wide_instruction.size)",
     "source": "bundle.md, \"Overall layout\"; predicates.md (\"HS + 6 ALS + 4 LTS + 2 PLS "
               "+ 3 CDS = 16 assembles\")", "check": "encoding"},
    {"id": "ct11-flags", "isets": [7],
     "rule": "ctcond type 11 is false when alc, abn or abp have different t and f bits in "
             "the instruction (gas refuses it)",
     "source": "gas end_wide_command (MCST bug 149213)", "check": "checked"},
    {"id": "dtbp-branch", "isets": [7],
     "rule": "dtbp only with an ibranch or rbranch",
     "source": "gas parse_dtbp_args", "check": "checked"},
    {"id": "clp-shorthand",
     "rule": "clpandp/clplandp only outside braces (a wide instruction of their own)",
     "source": "predicates.md, \"Slots and limits\"", "check": "gas"},
]

NOT_RESERVATIONS = [
    "Two writes of one register in one instruction are allowed; the higher channel's "
    "result is left (bundle.md, \"Two writes of one register\"; predicates.md for "
    "predicates).",
    "Memory accesses to one address in one instruction take effect ALC2 before ALC5, "
    "a load before a store of its cluster (bundle.md, \"Memory accesses in one wide "
    "instruction\").",
    "Store bank conflicts (two stores agreeing in address bits 6-7, a store and a load "
    "in one 32-byte block) are timing, not reservations (timing.md, \"Throughput\").",
    "Cluster crossings, write-back conflicts and an ALC5 FP operation next to a divide "
    "(v5) are timing (timing.md, \"Clusters\", \"Write-back conflicts\", \"Elbrus-8C2\").",
    "A taken transfer runs every operation of its own wide instruction "
    "(gcc-port-plan.md, 7.3).",
]


def divider(ops):
    """The ALC5 divider: its operations per version and the measured cost of
    issuing one every g cycles (bench.py's throughput kernels)."""
    names = sorted(n for n, forms in ops.items() if any(f.get("divider") for f in forms))
    machines = {}
    for iset, (fname, machine) in TIMING.items():
        tp = yaml.load((ISA / fname).read_text(), Loader=yaml.CSafeLoader)["throughput"]
        rows = {}
        for key, v in tp.items():
            m = re.fullmatch(r"(?:v3div_)?([a-z0-9]+?)(?:_g(\d+))?", key)
            if m and m.group(1) in names:
                rows.setdefault(m.group(1), {})[int(m.group(2) or 1)] = round(
                    v["cycles_per_instruction"], 2)
        ops_rows = {}
        for n in names:
            r = rows.get(n)
            if not r or sorted(r) != list(range(1, 9)):
                continue
            cyc = [r[g] for g in range(1, 9)]
            # A distance conflicts when issuing every g cycles costs more
            # than g a divide (allowing for the kernels' loop overhead).
            ops_rows[n] = Flow(cycles=cyc, conflicts=[g for g, c in zip(range(1, 9), cyc)
                                                      if c > g + 0.1])
        machines[f"v{iset}"] = {"machine": machine, "source": f"spec/isa/{fname}",
                                "ops": ops_rows}
    return {
        "channel": 5,
        "ops": names,
        "rule": "one pipelined unit in ALC5 shared by every divide, square root and "
                "reciprocal estimate (integer, FP, packed, 80-bit); an operation issued g "
                "cycles after another on the unit stalls when g is in that operation's "
                "conflicts; smodx/umodx hold it 9 cycles",
        "cycles_note": "cycles: cycles per operation when one issues every g = 1..8 "
                       "cycles, back to back with itself; mixed pairs measured at "
                       "distance 1 cost as two of the same (timing.md, \"Throughput\", "
                       "\"The divider\"); operations missing from a version's ops were "
                       "not measured; v2, v4 and v7 were not measured",
        "by_version": machines,
    }


def summary(ops):
    out = {}
    for iset in ISETS:
        sets = collections.Counter()
        n = 0
        for forms in ops.values():
            for f in forms:
                if iset in f["isets"]:
                    n += 1
                    key = " ".join(map(str, f["channels"]))
                    if "pairs" in f:
                        key += " paired"
                    sets[key] += 1
        out[f"v{iset}"] = {"operations": n, "channel_sets": dict(sorted(
            sets.items(), key=lambda kv: (-kv[1], kv[0])))}
    return out


def control_ops(table):
    out = {}
    names = set()
    for m in table["manual_ops"]:
        if m["name"] in ("mmurr", "mmurw"):
            continue
        isets = [i for i in m["isets"] if i in ISETS]
        if not isets:
            continue
        if m["name"] not in CONTROL:
            raise SystemExit(f"control operation {m['name']} has no rule in CONTROL")
        names.add(m["name"])
        uses, note = CONTROL[m["name"]]
        e = {"isets": isets, "uses": uses}
        if "ctprs" in m:
            e["ctprs"] = m["ctprs"]
        if note:
            e["note"] = note
        out[m["name"]] = e
    extra = set(CONTROL) - names
    if extra:
        raise SystemExit(f"CONTROL rules for unknown operations: {sorted(extra)}")
    return dict(sorted(out.items()))


class Flow(dict):
    """A mapping written on one line (an operation's form, a divider row)."""


class Dumper(yaml.SafeDumper):
    pass


def _flow_list(dumper, data):
    flow = all(isinstance(x, (int, float)) or isinstance(x, str) and len(x) <= 32
               for x in data)
    flow = flow or all(isinstance(x, list) for x in data)
    return dumper.represent_sequence("tag:yaml.org,2002:seq", data, flow_style=flow)


def _flow_dict(dumper, data):
    return dumper.represent_mapping("tag:yaml.org,2002:map", data.items(), flow_style=True)


Dumper.add_representer(list, _flow_list)
Dumper.add_representer(Flow, _flow_dict)


def render(table):
    ops, aliases = operations(table)
    src = table["source"]
    inputs = ["binutils-ops.yaml"] + [f for f, _ in TIMING.values()]
    doc = {
        "source": {
            "binutils": {"project": src["project"], "commit": src["commit"],
                         "release": src["release"]},
            "inputs": {f"spec/isa/{f}": blob_sha(ISA / f) for f in inputs},
            "rules": "the syllable rules in tools/isa-extract/gen_reservations.py, from "
                     "spec/isa/bundle.md, predicates.md, control-transfer.md and MCST gas",
        },
        "isets": list(ISETS),
        "notes": [
            "operations: per ALU operation, a list of forms, each valid for the ISA "
            "versions in isets. channels: the ALCs it may issue in (a paired one may be "
            "written with either channel of its pair). format and ales: per channel when "
            "they differ ({kind: [channels]}). ales: none (no ALES), default (0x01c0: a "
            "half-syllable in ALC0/1/3/4, nothing in ALC2/5) or value (a half-syllable "
            "in ALC0/1/3/4, the shared ALES2/ALES5 word in ALC2/5). literals: the "
            "literal sizes (bits) each operand may take (literals). ldst: load or "
            "store for the channel restrictions. mas: optional (CS1's MAS field when "
            "the MAS is not 0) or always. pred_dst: writes a predicate; int_compare: "
            "counts for %cmpN. mrgc: needs an MRGC entry (cds). divider: uses the ALC5 "
            "divider. uses: control fields it also takes.",
            "control_operations: the operations outside the ALU channels and the "
            "syllables or fields they take (wide_instruction). Two operations that use "
            "the same syllable or field cannot share a wide instruction, except within "
            "one CS1 group, MAS fields of different channels, PLS slots and CDS entries "
            "as described, and SS fields set to the same value.",
            "Checked against decoded compiler output by "
            "tools/isa-extract/check_reservations.py.",
        ],
        "wide_instruction": WIDE_INSTRUCTION,
        "alu": CLUSTERS,
        "literals": LITERALS,
        "pls": PLS,
        "cds": CDS,
        "aas": AAS,
        "restrictions": RESTRICTIONS,
        "not_reservations": NOT_RESERVATIONS,
        "divider": divider(ops),
        "control_operations": control_ops(table),
        "summary": summary(ops),
    }
    tail = {"aliases": aliases, "operations": ops}
    head = ("# SPDX-License-Identifier: CC-BY-SA-4.0\n"
            "# Generated by tools/isa-extract/gen_reservations.py from\n"
            "# spec/isa/binutils-ops.yaml, the timing tables and the syllable rules in\n"
            "# the generator; see spec/NOTICE.md. Do not edit: regenerate.\n"
            "# What each E2K operation reserves in a wide instruction, per ISA\n"
            "# version: data for a scheduler's pipeline description.\n")
    body = yaml.dump(doc, Dumper=Dumper, sort_keys=False, width=100, allow_unicode=True)
    # One line per operation form, however long.
    body += yaml.dump(tail, Dumper=Dumper, sort_keys=False, width=1 << 16,
                      allow_unicode=True)
    return head + body


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if spec/isa/reservations.yaml is not up to date")
    ap.add_argument("-o", "--output", type=Path, default=OUT)
    args = ap.parse_args()
    out = args.output.resolve()
    shown = out.relative_to(ROOT) if out.is_relative_to(ROOT) else out
    table = yaml.load((ISA / "binutils-ops.yaml").read_text(), Loader=yaml.CSafeLoader)
    text = render(table)
    if args.check:
        if not out.exists() or out.read_text() != text:
            sys.exit(f"{shown} is out of date: run {Path(__file__).name}")
        print(f"{shown}: up to date")
        return
    out.write_text(text)
    n = text.count("\n")
    print(f"wrote {shown} ({n} lines)")


if __name__ == "__main__":
    main()
