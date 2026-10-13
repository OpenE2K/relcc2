#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write pocchk6.s and pocchk6-cases.h for pocchk6.c: the array prefetch
sequences of the proof-of-concept back end's P89
(docs/spec-poc-findings.md; spec/isa/hwbugs.md, the 2C3's stand-alone
APB SIGILL).

  python3 tools/isa-probes/gen_pocchk6.py

Every case is  long p6_NAME(const long *a, long n, long *out)  and sums
array elements read through one array prefetch area (fmt 4, mrng 8, the
area based at a, one movad,0 with am = 1 a step), in one of these
shapes (the loops are copies of apb-loop.s's):

  sa     a stand-alone program: bap, 21 cycles, three movads in three
         wide instructions, no loop (n is ignored; the sum is a[0..2])
  saL    sa with %lsr written (lcnt 64) before bap
  sa0    sa with %lsr written as 0 before bap
  A      a loop_mode loop counted by %lsr (written before bap),
         alc + ct ? %NOT_LOOP_END, one movad an iteration (apb-loop's loopA)
  D      A without the loop_mode bit (apb-loop's loopD): the shape of
         a pipelined loop that counts with %lsr but leaves loop_mode out
  Dz     D ending as lcc's loops and the proof of concept's do: the
         program stopped by disp %ctpr2, disp=0x0 and a zero written to
         %aabf0
  B      an ordinary branch loop counted in a register, no %lsr
         (apb-loop's loopB)
  Ae, De A and D with lcnt = n - 2 and ecnt = 2: the last two of the n
         iterations are epilogue iterations
  Aem, Dem  Ae and De with ldmc = 1

The loops store their iteration count in out[0].  Not cases (pocchk6.c's
@NAME steps between programs): p6_ct1 and p6_ct2, leaves that make a
taken ct through %ctpr1 or %ctpr2; p6_lsr*, leaves that write %lsr
(LSR_LEAVES below); p6_loop1 and p6_loop8, leaves that run a
loop of 1 or 8 iterations counted by %lsr with no array prefetch (the
one-iteration loop never takes its ct); p6_rdlsr and p6_rdilcr, which
read %lsr and %ilcr.
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent
LSR = 0xff2000000000   # vlc = 1, ldovl = 255; lcnt goes in the low half
PROG = ("fapb\tct=1, dcd=0, fmt=4, mrng=8, d=0, incr=0, ind=0, asz=5, "
        "abs=0, disp=0")
MOVA = "movad,0\tarea=0, ind=0, am=1, be=0, %dr8"
# Leaves that write %lsr (loops.md, "Registers": lcnt bits 0-31, ecnt
# 32-36, vlc 37, over 38, ldmc 39, ldovl 40-47).
LSR_LEAVES = [("lsr0", 0),                  # vlc 0
              ("lsrn", LSR | 64),           # lcnt 64
              ("lsrz", LSR),                # vlc, lcnt 0
              ("lsrfin", LSR | 1 << 38),    # what a finished loop leaves
              ("lsr1", LSR | 1),            # lcnt 1
              ("lsr2", LSR | 2),            # lcnt 2
              ("lsre", LSR | 2 << 32),      # lcnt 0, ecnt 2
              ("lsrm", LSR | 1 << 39)]      # lcnt 0, ldmc


def wi(*ops):
    return "\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n"


def func(name, body, prog=True):
    fn = f"p6_{name}"
    text = (f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n\t.align\t8\n"
            f"{fn}:\n" + body)
    if prog:
        text += f"\t.align\t8\n.L{name}_prog:\n" + wi(PROG)
    return text + f"\t.size\t{fn}, .-{fn}\n"


def stand_alone(name="sa", lsr=None):
    """bap, 21 cycles, three movads; the sum of the three values.  With
    LSR, %lsr is written with that value first (5 cycles before bap)."""
    b = wi("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += wi(f"ldisp\t%ctpr2, .L{name}_prog", "aaurwd,2\t%dr0, %aad0",
            "addd,0\t0x0, 0x0, %dr9")
    if lsr is not None:
        b += wi(f"addd,0\t0x0, _f64,_lts0 {lsr:#x}, %dr3")
        b += wi("rwd,0\t%dr3, %lsr")
        b += wi("nop 4")
    b += wi("nop 7", "bap")
    b += wi("nop 7") + wi("nop 4")
    b += wi(MOVA)
    b += wi(MOVA.replace("%dr8", "%dr10"))
    b += wi(MOVA.replace("%dr8", "%dr11"))
    b += wi("nop 4")
    b += wi("addd,0\t%dr8, %dr10, %dr9")
    b += wi("addd,0\t%dr9, %dr11, %dr0")
    b += wi("disp\t%ctpr2, disp=0x0")
    b += wi("ct\t%ctpr3")
    return func(name, b)


def counted(name, loop_mode, aabf, ecnt=0, ldmc=False):
    """apb-loop.s's loopA (loop_mode) and loopD (no loop_mode); with
    ECNT, lcnt = n - ECNT and ecnt = ECNT (n iterations in all, the last
    ECNT of them epilogue iterations), with LDMC also ldmc = 1."""
    lsr = LSR | ecnt << 32 | (1 << 39 if ldmc else 0)
    b = wi("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += wi(f"ldisp\t%ctpr2, .L{name}_prog", "aaurwd,2\t%dr0, %aad0",
            f"addd,0\t0x0, _f64,_lts0 {lsr:#x}, %dr3",
            "addd,1\t0x0, 0x0, %dr8", "addd,3\t0x0, 0x0, %dr9",
            "addd,4\t0x0, 0x0, %dr10", f"subd,5\t%dr1, {ecnt:#x}, %dr5")
    b += wi("insfd,0\t%dr3, _f32s,_lts0 0x8800, %dr5, %dr3")
    b += wi(f"disp\t%ctpr1, .L{name}_loop", "rwd,0\t%dr3, %lsr")
    b += wi("nop 7", "bap") + wi("nop 7") + wi("nop 7")
    b += f".L{name}_loop:\n"
    b += wi(*(["loop_mode"] if loop_mode else []),
            "alc\talcf=1, alct=1", "ct\t%ctpr1 ? %NOT_LOOP_END", MOVA,
            "addd,0\t%dr9, %dr8, %dr9", "addd,1\t%dr10, 0x1, %dr10")
    if aabf:
        b += wi("nop 7", "disp\t%ctpr2, disp=0x0", "addd,0\t0x0, 0x0, %dr4")
        b += wi("aaurw,2\t%r4, %aabf0")
    else:
        b += wi("nop 7", "disp\t%ctpr2, disp=0x0")
    b += wi("addd,0\t%dr9, %dr8, %dr0", "std,2\t%dr2, 0x0, %dr10")
    b += wi("ct\t%ctpr3")
    return func(name, b)


def branch_loop():
    """apb-loop.s's loopB: a branch loop counted in %dr10."""
    b = wi("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += wi("ldisp\t%ctpr2, .LB_prog", "aaurwd,2\t%dr0, %aad0",
            "addd,1\t0x0, 0x0, %dr8", "addd,3\t0x0, 0x0, %dr9",
            "addd,4\t0x0, 0x0, %dr10")
    b += wi("disp\t%ctpr1, .LB_loop")
    b += wi("nop 7", "bap") + wi("nop 7") + wi("nop 7")
    b += ".LB_loop:\n"
    b += wi(MOVA, "addd,1\t%dr10, 0x1, %dr10")
    b += wi("addd,0\t%dr9, %dr8, %dr9", "cmpldb,1\t%dr10, %dr1, %pred0")
    b += wi("nop 2")
    b += wi("ct\t%ctpr1 ? %pred0")
    b += wi("nop 7", "disp\t%ctpr2, disp=0x0")
    b += wi("addd,0\t%dr9, 0x0, %dr0", "std,2\t%dr2, 0x0, %dr10")
    b += wi("ct\t%ctpr3")
    return func("B", b)


def ct_leaf(n):
    """A leaf that makes a taken ct through %ctprN (isaleft.s's il_ctN)."""
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += wi(f"disp\t%ctpr{n}, .Lct{n}_t")
    b += wi("nop 4")
    b += wi(f"ct\t%ctpr{n}")
    b += wi("addd,0\t0x0, 0x1, %dr0")
    b += f".Lct{n}_t:\n" + wi("nop 5") + wi("ct\t%ctpr3")
    return func(f"ct{n}", b, prog=False)


def lsr_leaf(name, value):
    """A leaf that writes %lsr and waits until the write has taken effect
    (loops.md: 4 cycles before a loop or rrd)."""
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += wi(f"addd,0\t0x0, _f64,_lts0 {value:#x}, %dr3")
    b += wi("rwd,0\t%dr3, %lsr")
    b += wi("nop 5")
    b += wi("ct\t%ctpr3")
    return func(name, b, prog=False)


def loop_leaf(name, n):
    """A loop of N iterations counted by %lsr (alc, ct ? %NOT_LOOP_END,
    no loop_mode, no array prefetch); N = 1 never takes the ct."""
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += wi(f"addd,0\t0x0, _f64,_lts0 {LSR | n:#x}, %dr3",
            "addd,1\t0x0, 0x0, %dr4")
    b += wi(f"disp\t%ctpr1, .L{name}_loop", "rwd,0\t%dr3, %lsr")
    b += wi("nop 5")
    b += f".L{name}_loop:\n"
    b += wi("alc\talcf=1, alct=1", "ct\t%ctpr1 ? %NOT_LOOP_END",
            "addd,0\t%dr4, 0x1, %dr4")
    b += wi("addd,0\t%dr4, 0x0, %dr0")
    b += wi("ct\t%ctpr3")
    return func(name, b, prog=False)


def reader(name, reg):
    """long f(void): rrd of %lsr or %ilcr (the loop counters' state)."""
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += wi(f"rrd,0\t{reg}, %dr0")
    b += wi("nop 5")
    b += wi("ct\t%ctpr3")
    return func(name, b, prog=False)


def main():
    cases = [("sa", "S"), ("saL", "S"), ("sa0", "S"), ("A", "L"),
             ("D", "L"), ("Dz", "L"), ("B", "L"), ("Ae", "L"), ("Aem", "L"),
             ("De", "L"), ("Dem", "L")]
    text = ("! SPDX-License-Identifier: GPL-3.0-or-later\n"
            "! Generated by tools/isa-probes/gen_pocchk6.py; see pocchk6.c.\n"
            "\t.ignore\tld_st_style\n\t.text\n")
    text += stand_alone()
    text += stand_alone("saL", LSR | 64)
    text += stand_alone("sa0", 0)
    text += counted("A", True, False)
    text += counted("D", False, False)
    text += counted("Dz", False, True)
    text += counted("Ae", True, False, ecnt=2)
    text += counted("Aem", True, False, ecnt=2, ldmc=True)
    text += counted("De", False, False, ecnt=2)
    text += counted("Dem", False, False, ecnt=2, ldmc=True)
    text += branch_loop()
    text += ct_leaf(1) + ct_leaf(2)
    for name, value in LSR_LEAVES:
        text += lsr_leaf(name, value)
    text += loop_leaf("loop1", 1) + loop_leaf("loop8", 8)
    text += reader("rdlsr", "%lsr") + reader("rdilcr", "%ilcr")
    (HERE / "pocchk6.s").write_text(text)
    hdr = ("/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
           "/* Generated by tools/isa-probes/gen_pocchk6.py; see pocchk6.c."
           "  */\n")
    hdr += "".join(f"CASE ({n}, \"{k}\")\n" for n, k in cases)
    (HERE / "pocchk6-cases.h").write_text(hdr)
    print(f"pocchk6.s: {len(cases)} cases")


if __name__ == "__main__":
    main()
