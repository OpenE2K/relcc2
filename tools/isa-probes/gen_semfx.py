#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write semfx.s and semfx-ops.h: every 80-bit (x87 extended) operation of
v3 and the conversions and moves around it, for semfx.c
(spec/isa/semantics.md, "80-bit operations").

  python3 tools/isa-probes/gen_semfx.py

The mnemonics name their operand types: fx<op><b><r> takes an 80-bit
first operand, a second of type b (s single, d double, x 80-bit) and
gives an r; fxcmp<c><b>b/f compares into a predicate or a register;
fxsqrti<b>x / fxsqrtu<b>x / fxsqrtt<b>x are the steps of a square root
(i: start from b; u and t: b and the previous step's x).

Each function is  void fx_N(const u64 in[4], u64 out[4], unsigned fpcr):
an 80-bit value is in[0] (significand) and in[1] (sign and exponent, built
with movif), the second operand in[2] (and in[3] when it is 80-bit), all
in %fpcr = FPCR (flags in %fpsr cleared first).  out[0] is the result's
64 bits (an 80-bit result's significand), out[1] its sign and exponent
(movfi) or, for a predicate, 1/0, out[2] %fpsr.
"""

import re
from pathlib import Path

import yaml

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def ops(v4=False):
    """The v3 operations, or (V4) those new in v4 or v5 (semfx-v4.s, run
    on the 8C2)."""
    data = yaml.safe_load((ROOT / "spec/isa/binutils-ops.yaml").read_text())
    names = {}
    for e in data["alu_ops"]:
        names.setdefault(e["name"], set()).update(e.get("isets") or [])
    out = {}
    for e in data["alu_ops"]:
        n = e["name"]
        if not (n.startswith(("fx", "movx")) or n in ("idtofx", "istofx",
                                                       "fstofx", "fdtofx")):
            continue
        first = min(names[n])
        if (first <= 3) if v4 else (3 not in (e.get("isets") or [])):
            continue
        if v4 and first > 5:
            continue
        out.setdefault(n, (e["channels"][0], e["alopf"]))
    return sorted(out.items())


def types(n):
    """(first operand, second operand or None, result) types."""
    if n in ("movx", "movxa", "movxc"):
        return "x", None, "x"
    if n in ("idtofx",):
        return "di", None, "x"
    if n in ("istofx",):
        return "si", None, "x"
    if n in ("fstofx",):
        return "s", None, "x"
    if n in ("fdtofx",):
        return "d", None, "x"
    m = re.match(r"fxto(fs|fd|id|idtr|is|istr|fx)$", n)
    if m:
        return "x", None, {"fs": "s", "fd": "d", "id": "di", "idtr": "di",
                           "is": "si", "istr": "si"}[m.group(1)]
    m = re.match(r"fxsqrti([sdx])x$", n)
    if m:
        return m.group(1), None, "x"
    m = re.match(r"fxsqrt([ut])([sdx])x$", n)
    if m:
        return m.group(2), "x", "x"
    m = re.match(r"fxcmp[a-z]+?([sdx])([bf])$", n)
    if m:
        return "x", m.group(1), "p" if m.group(2) == "b" else "si"
    m = re.match(r"fxscale([sdx])x$", n)
    if m:
        return "x", "si", "x"
    m = re.match(r"fx[a-z]+?([sdx])([sdx])$", n)
    if m:
        return "x", m.group(1), m.group(2)
    return None


def load(t, k, reg):
    """Instructions putting operand K (in[2k], in[2k+1]) of type T in REG."""
    off = 16 * k
    if t == "x":
        return [f"ldd,0\t%dr0, {off:#x}, %dr8", f"ldd,2\t%dr0, {off + 8:#x}, %dr9"], \
               f"movif,0\t%dr8, %r9, {reg}"
    if t in ("d", "di"):
        return [f"ldd,0\t%dr0, {off:#x}, %d{reg[1:]}"], None
    return [f"ldw,0\t%dr0, {off:#x}, {reg}"], None


def fn(i, n, ch, alopf):
    t1, t2, tr = types(n)
    w = lambda *o: "\t{\n" + "".join(f"\t  {x}\n" for x in o) + "\t}\n"
    b = w("setwd\twsz = 0x8, nfx = 0x0, dbl = 0x0", "return\t%ctpr3",
          "adds,1\t%r2, 0x0, %r4")
    b += w("rrs,0\t%fpcr, %r12")
    b += w("rrs,0\t%fpsr, %r13")
    lds, mv = load(t1, 0, "%r2")
    b += w(*lds) + w("nop 2")
    if mv:
        b += w(mv)
    if t2:
        lds, mv = load(t2, 1, "%r3")
        b += w(*lds) + w("nop 2")
        if mv:
            b += w(mv)
    b += w("addd,1\t0x0, _f64,_lts0 0x5555555555555555, %dr5",
           "addd,2\t0x0, 0x0, %dr6")
    b += w("rws,0\t%r4, %fpcr")
    b += w("rws,0\t0x0, %fpsr")
    b += w("nop 7")
    srcs = ["%r2"] + (["%r3"] if t2 else [])
    if tr == "p":
        b += w(f"{n},{ch}\t{', '.join(srcs)}, %pred1")
        b += w("nop 7")
        b += w("addd,0\t0x1, 0x0, %dr6 ? %pred1")
    else:
        b += w(f"{n},{ch}\t{', '.join(srcs)}, %r5")
        b += w("nop 7") + w("nop 7")
        if tr == "x":
            b += w("movfi,0\t%r5, %r6")
    b += w("nop 7")
    b += w("rrs,0\t%fpsr, %r7")
    b += w("nop 2")
    b += w("std,2\t%dr1, 0x0, %dr5", "std,5\t%dr1, 0x8, %dr6")
    b += w("stw,2\t%dr1, 0x10, %r7")
    b += w("rws,0\t%r12, %fpcr") + w("rws,0\t%r13, %fpsr")
    b += w("nop 5") + w("ct\t%ctpr3")
    return (f"\n\t.global\tfx_{i}\n\t.type\tfx_{i}, #function\n"
            f"\t.align\t8\nfx_{i}:\n{b}\t.size\tfx_{i}, .-fx_{i}\n")


def chain(i, t):
    """fx_I: the square root as lcc computes it: fxsqrti<t>x a, r0;
    fxsqrtu<t>x a, r0, r0; fxsqrtt<t>x a, r0, r5 (ALC5)."""
    w = lambda *o: "\t{\n" + "".join(f"\t  {x}\n" for x in o) + "\t}\n"
    b = w("setwd\twsz = 0x8, nfx = 0x0, dbl = 0x0", "return\t%ctpr3",
          "adds,1\t%r2, 0x0, %r4")
    b += w("rrs,0\t%fpcr, %r12") + w("rrs,0\t%fpsr, %r13")
    lds, mv = load(t, 0, "%r2")
    b += w(*lds) + w("nop 2") + (w(mv) if mv else "")
    b += w("addd,2\t0x0, 0x0, %dr6")
    b += w("rws,0\t%r4, %fpcr") + w("rws,0\t0x0, %fpsr") + w("nop 7")
    b += w(f"fxsqrti{t}x,5\t%r2, %r10") + w("nop 7") + w("nop 7")
    b += w(f"fxsqrtu{t}x,5\t%r2, %r10, %r10") + w("nop 7") + w("nop 7")
    b += w(f"fxsqrtt{t}x,5\t%r2, %r10, %r5") + w("nop 7") + w("nop 7")
    b += w("movfi,0\t%r5, %r6") + w("nop 7")
    b += w("rrs,0\t%fpsr, %r7") + w("nop 2")
    b += w("std,2\t%dr1, 0x0, %dr5", "std,5\t%dr1, 0x8, %dr6")
    b += w("stw,2\t%dr1, 0x10, %r7")
    b += w("rws,0\t%r12, %fpcr") + w("rws,0\t%r13, %fpsr")
    b += w("nop 5") + w("ct\t%ctpr3")
    return (f"\n\t.global\tfx_{i}\n\t.type\tfx_{i}, #function\n"
            f"\t.align\t8\nfx_{i}:\n{b}\t.size\tfx_{i}, .-fx_{i}\n")


def main():
    ls = [(n, ch, a) for n, (ch, a) in ops() if types(n)]
    skipped = [n for n, _ in ops() if not types(n)]
    k = len(ls)
    chains = "".join(chain(k + j, t) for j, t in enumerate("sdx"))
    (HERE / "semfx.s").write_text(
        "! SPDX-License-Identifier: GPL-3.0-or-later\n"
        "! Generated by tools/isa-probes/gen_semfx.py; see semfx.c.\n"
        "\t.ignore\tld_st_style\n\t.text\n"
        + "".join(fn(i, *o) for i, o in enumerate(ls)) + chains)
    (HERE / "semfx-ops.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_semfx.py: index, name, the\n"
        "   first and second operand types and the result type.  */\n"
        + "".join(f'OP ({i}, "{n}", "{types(n)[0]}", "{types(n)[1] or ""}", '
                  f'"{types(n)[2]}")\n' for i, (n, ch, a) in enumerate(ls))
        + "".join(f'OP ({k + j}, "fxsqrtchain{t}x", "{t}", "", "x")\n'
                  for j, t in enumerate("sdx")))
    l4 = [(n, ch, a) for n, (ch, a) in ops(v4=True) if types(n)]
    (HERE / "semfx-v4.s").write_text(
        "! SPDX-License-Identifier: GPL-3.0-or-later\n"
        "! Generated by tools/isa-probes/gen_semfx.py; see semfx.c (v4+).\n"
        "\t.ignore\tld_st_style\n\t.text\n"
        + "".join(fn(1000 + i, *o) for i, o in enumerate(l4)))
    (HERE / "semfx-v4-ops.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_semfx.py (v4+).  */\n"
        + "".join(f'OP ({1000 + i}, "{n}", "{types(n)[0]}", '
                  f'"{types(n)[1] or ""}", "{types(n)[2]}")\n'
                  for i, (n, ch, a) in enumerate(l4)))
    print(len(ls), "cases,", len(l4), "v4+ cases; not typed:", skipped)


if __name__ == "__main__":
    main()
