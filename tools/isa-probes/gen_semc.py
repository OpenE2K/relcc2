#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write semc.s and semc-ops.h: the operations of v3 to v5 that no other
probe runs and that work on memory, the AAU, tags or protected-mode
descriptors, for semc.c, which runs each in a child process from user
mode (spec/isa/semantics.md, "Memory, tag and descriptor operations").

  python3 tools/isa-probes/gen_semc.py

Each function is  void sc_N(u64 *buf, u64 *out): %dr0 points at a
64-byte-aligned buffer of 256 bytes (pattern 0x11 * byte index), %dr1 at
out.  Loads take their address from %dr0 (+ 0x10 where there is an
offset operand) and put the result in %dr4 or %qr4/%qpr4, stores write
%dr2 = 0x0123456789abcdef (a pair: then %dr3 = 0x10) there with mask
%r8 = 0xff, register and tag operations read %dr2 / %qr2.  out[0..1] get
%dr4 and %dr5 (for a %qpr4 result only its low half).
"""

from pathlib import Path

import yaml

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

NAMES = (
    "stq stmqp ldaab ldaah ldaaw ldaad ldaaq ldaaqp aaurr aaurrq aaurwq "
    "gettags gettc gettst invtc movtcd movtcs movtcq movtrd movtrs movtrq "
    "movtrcd movtrcs movtrcq puttagqp aptoap aptoapb cudtoap gdtoap getsap "
    "ldcsb ldcsh ldcsw ldcsd ldcsq ldcsqp lddsb lddsh lddsw lddsd lddsq "
    "lddsqp ldesb ldesh ldesw ldesd ldesq ldesqp ldfsb ldfsh ldfsw ldfsd "
    "ldfsq ldfsqp ldgsb ldgsh ldgsw ldgsd ldgsq ldgsqp ldssb ldssh ldssw "
    "ldssd ldssq ldssqp ldgdb ldgdh ldgdw ldgdd ldgdq ldgdqp ldcudb ldcudh "
    "ldcudw ldcudd ldcudq ldcudqp ldapb ldaph ldapw ldapd ldapq ldapqp "
    "ldrd ldrqp stcsb stcsh stcsw stcsd stcsq stcsqp stcsmqp stdsb stdsh "
    "stdsw stdsd stdsq stdsqp stdsmqp stesb stesh stesw stesd stesq stesqp "
    "stesmqp stfsb stfsh stfsw stfsd stfsq stfsqp stfsmqp stgsb stgsh stgsw "
    "stgsd stgsq stgsqp stgsmqp stssb stssh stssw stssd stssq stssqp "
    "stssmqp stgdb stgdh stgdw stgdd stgdq stgdqp stgdmqp stapb staph "
    "stapw stapd stapq stapqp stapmqp strd strqp "
    "getpl getva vfsi puttc puttst ldq").split()
REG = {"d": "%dr", "s": "%r", "q": "%qr", "p": "%qpr"}


def ops():
    data = yaml.safe_load((ROOT / "spec/isa/binutils-ops.yaml").read_text())
    out = {}
    for e in data["alu_ops"]:
        if e["name"] in NAMES and set(e.get("isets") or []) & {3, 4, 5}:
            out.setdefault(e["name"], e)
    return [out[n] for n in NAMES if n in out]


def text(e):
    n, a, ch = e["name"], e["args"], e["channels"][0]
    alopf = e.get("alopf")
    dst = {"d": "%dr4", "s": "%r4", "q": "%qr4", "p": "%qpr4"}
    if n.startswith("ldaa"):
        return f"{n},{ch}\t%aad0[ %aasti1 ], {dst[a[-1]]}"
    if n == "aaurr":
        return f"aaurr,{ch}\t%aaind1, %r4"
    if n == "aaurrq":
        return f"aaurrq,{ch}\t%aad1, %qr4"
    if n == "aaurwq":
        # Write %aad1, then read it back into %qr4.
        return (f"aaurwq,{ch}\t%qr2, %aad1\n\t}}\n\t{{\n\t  nop 7\n\t}}\n"
                f"\t{{\n\t  aaurrq,{ch}\t%aad1, %qr4")
    if alopf in ("ALOPF13", "ALOPF3"):             # stores
        base = "%r0" if a[0] == "s" else ("%qr6" if a[0] == "q" else "%dr0")
        if "mqp" in n:
            return f"{n},{ch}\t{base}, %r8, %qpr2"
        off = "%r3" if a[1] == "s" else "%dr3"
        data = {"s": "%r2", "d": "%dr2", "q": "%qr2", "p": "%qpr2"}[a[2]]
        return f"{n},{ch}\t{base}, {off}, {data}"
    if n.startswith("ld"):                           # loads
        base = "%r0" if a[0] == "s" else ("%qr6" if a[0] == "q" else "%dr0")
        off = "%r3" if a[1] == "s" else "%dr3"
        return f"{n},{ch}\t{base}, {off}, {dst[a[2]]}"
    if alopf in ("ALOPF2", "ALOPF12", "ALOPF22"):    # one source
        return f"{n},{ch}\t{REG[a[0]]}2, {dst[a[-1]]}"
    srcs = [f"{REG[x]}{2 + k}" for k, x in enumerate(a[:-1])]
    return f"{n},{ch}\t{', '.join(srcs)}, {dst[a[-1]]}"


def fn(i, e):
    w = lambda *o: "\t{\n" + "".join(f"\t  {x}\n" for x in o) + "\t}\n"
    b = w("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += w("addd,0\t0x0, _f64,_lts0 0x0123456789abcdef, %dr2")
    b += w("addd,0\t0x0, _f64,_lts0 0x5555555555555555, %dr4")
    b += w("addd,0\t0x0, _f64,_lts0 0x5555555555555555, %dr5",
           "addd,2\t0x0, 0x10, %dr3", "addd,3\t0x0, 0xff, %dr8",
           "addd,4\t%dr0, 0x0, %dr6", "addd,5\t0x0, 0x0, %dr7")
    if e["name"].startswith("ldaa"):
        b += w("aaurwd,2\t%dr0, %aad0") + w("aaurw,2\t%r3, %aasti1")
        b += w("nop 7")
    b += w("nop 2")
    b += w(text(e))
    b += w("nop 7") + w("nop 7")
    b += w("std,2\t%dr1, 0x0, %dr4", "std,5\t%dr1, 0x8, %dr5")
    b += w("nop 5") + w("ct\t%ctpr3")
    return (f"\n\t.global\tsc_{i}\n\t.type\tsc_{i}, #function\n"
            f"\t.align\t8\nsc_{i}:\n{b}\t.size\tsc_{i}, .-sc_{i}\n")


def main():
    ls = ops()
    for path, keep in (("semc", lambda e: 3 in e["isets"]),
                       ("semc-v5", lambda e: 3 not in e["isets"])):
        cs = [(i, e) for i, e in enumerate(ls) if keep(e)]
        (HERE / f"{path}.s").write_text(
            "! SPDX-License-Identifier: GPL-3.0-or-later\n"
            "! Generated by tools/isa-probes/gen_semc.py; see semc.c.\n"
            "\t.ignore\tld_st_style\n\t.text\n"
            + "".join(fn(i, e) for i, e in cs))
        (HERE / f"{path}-ops.h").write_text(
            "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
            "/* Generated by tools/isa-probes/gen_semc.py.  */\n"
            + "".join(f'OP ({i}, "{e["name"]}")\n' for i, e in cs))
    print(len(ls), "operations")


if __name__ == "__main__":
    main()
