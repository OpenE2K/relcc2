#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write tagx.s, tagx-cases.h, tagx-v5.s and tagx-v5-cases.h for tagx.c:
the tag operations (puttag*, gettag*, movt*) and deliberately tagged
values, the tag operations' latencies, the ALU operations no other probe
model-checks (muls, muld, merges, movts, movtd, movtq, fstoistr), and
movaqp in an array prefetch program (spec/isa/semantics.md,
speculation.md, timing.md, apb.md).

  python3 tools/isa-probes/gen_tagx.py

Tag cases:  long tagx_NAME(unsigned long *out, long a, long b, long c)
with a..c in %dr1..%dr3 (a a value, b a tag operand, c a buffer or an
address to load from).  Before each wide-instruction group it stores the
group's number in out[6], so a signal names the group that trapped (the
readout is group 31).  out[0], out[1]: %dr8 with its tag cleared and its
tag (gettagd); out[3]: gettags of %r8; out[4], out[5]: the same for
%dr9; out[2]: 1 if an rbranch (%dt_al) was taken.  Quad cases (v5)
first store %qpr8 with stqp,sm and reload both halves with the fill MAS
into %dr8 and %dr9 (out[7], out[8] keep %dr8 as it was).  The return
value is the %clkr difference across the groups.

Latency cases reuse gen_spec.py's case() (%clkr around the groups):
tagx_lat_KIND_gG_nN runs N chain steps whose scanned operation is
followed by G - 1 empty cycles.

Further tag cases: pd_* use a value tagged by puttagd, pds_* one tagged
by puttagd,sm (a diagnostic value where the plain form traps), pa_* and
pas_* the buffer address tagged the same ways, ps_* puttags, pq_* (v5)
puttagqp; tagop_* give puttagd a tag operand from a failed load, a
diagnostic word asking for tag 0, and the same word untagged; ld_andsm,
ld_xorsm, ld_shrsm, ldw_andssm run speculative operations on a failed
load's diagnostic value whose computed result has bit 30 clear;
pd_ptrue, ps_pfalse, pq_pfalse predicate the puttags; ps_upper,
gts_upper, gtd_upper write into a register whose upper half is
0x55555555.  Latency kinds ending in _dbl1 run under setwd dbl = 1 (the
rest dbl = 0); adds_addd and the kinds after it read 32-bit integer
results as 64 bits.  APB cases movad7, nomova7 and nomova5 find which
operation an fmt 7 area makes trap.

Model cases:  void tagx_m_I(const u64 in[4], u64 out[3]): %dr2..%dr5 =
in[0..3], %dr6 = %dr7 = 0x5555555555555555, %pred1 = (%r5 == 1), the
%pfpfr flags cleared; then one operation; out[0], out[1] = %dr6, %dr7,
out[2] = %pfpfr.

APB cases:  long tagx_apb_NAME(unsigned long *out, void *a, void *b)
in gen_apb.py's shape (fapb() from it): area 0 over a, bap, 21 cycles,
two movas (movaqp_use then reads the first with a plain addd into
%dr9), results as value and tag pairs.
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_apb    # noqa: E402  fapb(): APB program syllables
import gen_spec   # noqa: E402  case(): %clkr-timed groups

NOP = ["nop 7"]


def wi(out, *ops):
    out.append("\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n")


def func(name, body):
    return (f"\n\t.global\ttagx_{name}\n\t.type\ttagx_{name}, #function\n"
            f"\t.align\t8\ntagx_{name}:\n" + "".join(body)
            + f"\t.size\ttagx_{name}, .-tagx_{name}\n")


def tcase(name, groups, quad=False):
    """GROUPS: wide instructions (lists of operations); '%%LOCKED' names
    the label an rbranch goes to.  QUAD: read %qpr8 back through memory."""
    o = []
    assert len(groups) < 31
    wi(o, "setwd\twsz = 0x20, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    wi(o, "addd,0\t0x0, 0x0, %dr8", "addd,1\t0x0, 0x0, %dr9",
       "addd,3\t0x0, 0x0, %dr10", "addd,4\t0x0, 0x0, %dr12")
    # The readout writes only the low halves of %dr6, %dr7, %dr11 and
    # %dr14 (dbl = 0) and then stores them whole: clear any stale tag.
    wi(o, "addd,0\t0x0, 0x0, %dr6", "addd,1\t0x0, 0x0, %dr7",
       "addd,3\t0x0, 0x0, %dr11", "addd,4\t0x0, 0x0, %dr14")
    wi(o, "addd,0\t0x0, 0x0, %dr13", "addd,1\t0x0, 0x0, %dr15",
       "addd,3\t0x0, 0x0, %dr22", "addd,4\t0x0, 0x0, %dr23")
    if quad:
        wi(o, "qppackdl,0\t0x0, 0x0, %qpr8")
    wi(o, "nop 2")
    wi(o, "rrd,0\t%clkr, %dr4")
    for k, g in enumerate(groups, 1):
        wi(o, f"addd,1\t{k:#x}, 0x0, %dr20")
        wi(o, "std,5\t%dr0, 0x30, %dr20")
        wi(o, *[x.replace("%%LOCKED", f".L{name}_locked") for x in g])
    wi(o, "rrd,0\t%clkr, %dr5")
    wi(o, f"ibranch\t.L{name}_out")
    o.append(f".L{name}_locked:\n")
    wi(o, "addd,1\t0x0, 0x1, %dr10", "rrd,0\t%clkr, %dr5")
    o.append(f".L{name}_out:\n")
    wi(o, "addd,1\t0x1f, 0x0, %dr20")
    wi(o, "std,5\t%dr0, 0x30, %dr20", "nop 7")
    if quad:
        # %dr22, %dr23: registers no case uses (a case's own register
        # whose upper half holds a diagnostic tag would make the std trap).
        wi(o, "gettagd,2\t%dr8, %r22", "puttagd,5\t%dr8, 0x0, %dr23")
        wi(o, "nop 2")
        wi(o, "std,2\t%dr0, 0x38, %dr23", "std,5\t%dr0, 0x40, %dr22")
        wi(o, "stqp,2,sm\t%dr0, 0x50, %qpr8")
        wi(o, "nop 3")
        wi(o, "ldd,0\t%dr0, 0x50, %dr8, mas=0x4",
           "ldd,3\t%dr0, 0x58, %dr9, mas=0x4")
    wi(o, "nop 7")
    wi(o, "gettagd,2\t%dr8, %r6", "gettagd,5\t%dr9, %r7")
    wi(o, "gettags,2\t%r8, %r11", "puttagd,5\t%dr8, 0x0, %dr12")
    wi(o, "puttagd,2\t%dr9, 0x0, %dr13")
    wi(o, "nop 2")
    wi(o, "std,2\t%dr0, 0x0, %dr12", "std,5\t%dr0, 0x8, %dr6")
    wi(o, "std,2\t%dr0, 0x10, %dr10", "std,5\t%dr0, 0x18, %dr11")
    wi(o, "std,2\t%dr0, 0x20, %dr13", "std,5\t%dr0, 0x28, %dr7")
    wi(o, "subd,0\t%dr5, %dr4, %dr0", "nop 5")
    wi(o, "ct\t%ctpr3")
    return func(name, o)


# Sweep cases: a = value, b = tag operand, c = a 64-byte buffer.
PD = "puttagd,2\t%dr1, %r2, %dr12"
PA = "puttagd,2\t%dr3, %r2, %dr12"
PDS = "puttagd,2,sm\t%dr1, %r2, %dr12"
PAS = "puttagd,2,sm\t%dr3, %r2, %dr12"
SWEEP_D = [
    ("pd", [["puttagd,2\t%dr1, %r2, %dr8"], NOP]),
    ("pdsm", [["puttagd,2,sm\t%dr1, %r2, %dr8"], NOP]),
    ("pd5", [["puttagd,5\t%dr1, %r2, %dr8"], NOP]),
    ("pd_clr", [[PD], NOP, ["puttagd,2\t%dr12, 0x0, %dr8"], NOP]),
    ("pd_add", [[PD], NOP, ["addd,0\t%dr12, 0x1, %dr8"], NOP]),
    ("pd_addsm", [[PD], NOP, ["addd,0,sm\t%dr12, 0x1, %dr8"], NOP]),
    ("pd_adds", [[PD], NOP, ["adds,0\t%r12, 0x1, %r8"], NOP]),
    ("pd_addssm", [[PD], NOP, ["adds,0,sm\t%r12, 0x1, %r8"], NOP]),
    ("pd_dtal", [[PD], NOP, ["rbranch\t%%LOCKED ? %dt_al0",
                             "addd,0,sm\t%dr12, 0x1, %dr8"], NOP]),
    ("pd_cmp", [[PD], NOP, ["cmpedb,0\t%dr12, 0x0, %pred0"], NOP,
                ["addd,0\t0x0, 0x7, %dr8 ? %pred0"], NOP]),
    ("pd_cmpsm", [[PD], NOP, ["cmpedb,0,sm\t%dr12, 0x0, %pred0"], NOP,
                  ["addd,0,sm\t0x0, 0x7, %dr8 ? %pred0"], NOP]),
    ("pd_movtd", [[PD], NOP, ["movtd,0\t%dr12, %dr8"], NOP]),
    ("pd_movts", [[PD], NOP, ["movts,0\t%r12, %r8"], NOP]),
    ("pd_movtq", [[PD, "addd,1\t0x0, %dr1, %dr13"], NOP,
                  ["movtq,0\t%qr12, %qr8"], NOP]),
    ("pd_mergeF", [[PD, "cmpesb,0\t0x0, 0x1, %pred0"], NOP,
                   ["merged,0\t%dr12, %dr1, %dr8, %pred0"], NOP]),
    ("pd_mergeT", [[PD, "cmpesb,0\t0x0, 0x0, %pred0"], NOP,
                   ["merged,0\t%dr12, %dr1, %dr8, %pred0"], NOP]),
    ("pd_gettagd", [[PD], NOP, ["gettagd,2\t%dr12, %r8"], NOP]),
    ("pd_gettags", [[PD], NOP, ["gettags,2\t%r12, %r8"], NOP]),
    ("pd_std", [[PD], NOP, ["std,2\t%dr3, 0x0, %dr12"], NOP,
                ["ldd,0\t%dr3, 0x0, %dr8, mas=0x4"], NOP]),
    ("pd_stdsm", [[PD], NOP, ["std,2,sm\t%dr3, 0x0, %dr12"], NOP,
                  ["ldd,0\t%dr3, 0x0, %dr8, mas=0x4", "ldd,3\t%dr3, 0x0, %dr9"],
                  NOP]),
    ("pd_pfalse", [["cmpesb,0\t0x0, 0x1, %pred0"], NOP,
                   ["puttagd,2\t%dr1, %r2, %dr8 ? %pred0"], NOP]),
    ("pd_fadd", [[PD], NOP, ["faddd,0\t%dr12, %dr1, %dr8"], NOP]),
    ("pd_faddsm", [[PD], NOP, ["faddd,0,sm\t%dr12, %dr1, %dr8"], NOP]),
    # The buffer address c tagged, then used as a load or store address
    # (buf[0] holds 0x1111222233334444).
    ("pa_ld", [[PA], NOP, ["ldd,0\t%dr12, 0x0, %dr8"], NOP, NOP]),
    ("pa_ldsm", [[PA], NOP, ["ldd,0,sm\t%dr12, 0x0, %dr8"], NOP, NOP]),
    ("pa_st", [[PA], NOP, ["std,2\t%dr12, 0x8, %dr1"], NOP,
               ["ldd,0\t%dr3, 0x8, %dr8"], NOP, NOP]),
    # The same uses of a value tagged by puttagd,sm (a diagnostic value
    # where the plain puttagd traps).
    ("pds_add", [[PDS], NOP, ["addd,0\t%dr12, 0x1, %dr8"], NOP]),
    ("pds_addsm", [[PDS], NOP, ["addd,0,sm\t%dr12, 0x1, %dr8"], NOP]),
    ("pds_dtal", [[PDS], NOP, ["rbranch\t%%LOCKED ? %dt_al0",
                               "addd,0,sm\t%dr12, 0x1, %dr8"], NOP]),
    ("pds_clr", [[PDS], NOP, ["puttagd,2\t%dr12, 0x0, %dr8"], NOP]),
    ("pds_movtd", [[PDS], NOP, ["movtd,0\t%dr12, %dr8"], NOP]),
    ("pds_gettagd", [[PDS], NOP, ["gettagd,2\t%dr12, %r8"], NOP]),
    ("pds_std", [[PDS], NOP, ["std,2\t%dr3, 0x0, %dr12"], NOP,
                 ["ldd,0\t%dr3, 0x0, %dr8, mas=0x4"], NOP]),
    ("pds_stdsm", [[PDS], NOP, ["std,2,sm\t%dr3, 0x0, %dr12"], NOP,
                   ["ldd,0\t%dr3, 0x0, %dr8, mas=0x4",
                    "ldd,3\t%dr3, 0x0, %dr9"], NOP]),
    ("pas_ld", [[PAS], NOP, ["ldd,0\t%dr12, 0x0, %dr8"], NOP, NOP]),
    ("pas_ldsm", [[PAS], NOP, ["ldd,0,sm\t%dr12, 0x0, %dr8"], NOP, NOP]),
    # Control for pd_pfalse: the same predicated puttagd with the
    # predicate true behaves as the plain one.
    ("pd_ptrue", [["cmpesb,0\t0x0, 0x0, %pred0"], NOP,
                  ["puttagd,2\t%dr1, %r2, %dr8 ? %pred0"], NOP]),
]
PS = "puttags,2\t%r1, %r2, %r12"
SWEEP_S = [
    ("ps", [["puttags,2\t%r1, %r2, %r8"], NOP]),
    ("pssm", [["puttags,2,sm\t%r1, %r2, %r8"], NOP]),
    ("ps_adds", [[PS], NOP, ["adds,0\t%r12, 0x1, %r8"], NOP]),
    ("ps_addssm", [[PS], NOP, ["adds,0,sm\t%r12, 0x1, %r8"], NOP]),
    ("ps_addd", [[PS], NOP, ["addd,0\t%dr12, 0x1, %dr8"], NOP]),
    ("ps_dtal", [[PS], NOP, ["rbranch\t%%LOCKED ? %dt_al0",
                             "adds,0,sm\t%r12, 0x1, %r8"], NOP]),
    ("ps_movts", [[PS], NOP, ["movts,0\t%r12, %r8"], NOP]),
    ("ps_gettags", [[PS], NOP, ["gettags,2\t%r12, %r8"], NOP]),
    ("ps_stw", [[PS], NOP, ["stw,2\t%dr3, 0x0, %r12"], NOP,
                ["ldw,0\t%dr3, 0x0, %r8, mas=0x4"], NOP]),
    ("ps_stwsm", [[PS], NOP, ["stw,2,sm\t%dr3, 0x0, %r12"], NOP,
                  ["ldw,0\t%dr3, 0x0, %r8, mas=0x4", "ldw,3\t%dr3, 0x0, %r9"],
                  NOP]),
    ("ps_pfalse", [["cmpesb,0\t0x0, 0x1, %pred0"], NOP,
                   ["puttags,2\t%r1, %r2, %r8 ? %pred0"], NOP]),
    # The destination's upper half preset to 0x55555555 (dbl = 0).
    ("ps_upper", [["addd,0\t0x0, _f64,_lts0 0x5555555555555555, %dr8"], NOP,
                  ["puttags,2\t%r1, %r2, %r8"], NOP]),
]
# v5: %qpr12 = (a + 1, a); puttagqp into %qpr8 or %qpr14.
QB = [["addd,1\t%dr1, 0x1, %dr13"], NOP,
      ["qppackdl,0\t%dr13, %dr1, %qpr12"], NOP]
PQ = "puttagqp,0\t%qpr12, %r2, %qpr14"
SWEEP_Q = [
    ("pq", QB + [["puttagqp,0\t%qpr12, %r2, %qpr8"], NOP], True),
    ("pqsm", QB + [["puttagqp,0,sm\t%qpr12, %r2, %qpr8"], NOP], True),
    ("pq_addd", QB + [[PQ], NOP, ["addd,0\t%dr14, 0x1, %dr8"], NOP], False),
    ("pq_qpand", QB + [[PQ], NOP, ["qpand,0\t%qpr14, %qpr14, %qpr8"], NOP],
     True),
    ("pq_qpandsm", QB + [[PQ], NOP,
                         ["qpand,0,sm\t%qpr14, %qpr14, %qpr8"], NOP], True),
    ("pq_stqp", QB + [[PQ], NOP, ["stqp,2\t%dr3, 0x0, %qpr14"], NOP,
                      ["ldd,0\t%dr3, 0x0, %dr8, mas=0x4",
                       "ldd,3\t%dr3, 0x8, %dr9, mas=0x4"], NOP], False),
    # qpand,sm's result read back one step a group (groups 7..): which
    # step of the quad readout traps.
    ("pq_qpsm_dbg", QB + [[PQ], NOP,
                          ["qpand,0,sm\t%qpr14, %qpr14, %qpr16"], NOP,
                          ["gettagd,2\t%dr16, %r18"], NOP,
                          ["puttagd,5\t%dr16, 0x0, %dr19"], NOP,
                          ["stqp,2,sm\t%dr3, 0x0, %qpr16"], NOP, NOP,
                          ["ldd,0\t%dr3, 0x0, %dr8, mas=0x4",
                           "ldd,3\t%dr3, 0x8, %dr9, mas=0x4"], NOP], False),
    ("pq_pfalse", QB + [["cmpesb,1\t0x0, 0x1, %pred0"], NOP,
                        ["puttagqp,0\t%qpr12, %r2, %qpr8 ? %pred0"], NOP],
     True),
]
# Address cases: a = 0x0123456789abcdef, c = an address to load from.
LD = ["ldd,0,sm\t%dr3, 0x0, %dr12"]
LDW = ["ldw,0,sm\t%dr3, 0x0, %r12"]
DG = "addd,0\t0x0, _f64,_lts0 0x40000000, %dr13"
ADDR = [
    ("ld", [["ldd,0,sm\t%dr3, 0x0, %dr8"], NOP, NOP]),
    ("ldw", [["ldw,0,sm\t%dr3, 0x0, %r8"], NOP, NOP]),
    ("ld_put", [LD, NOP, NOP, ["puttagd,2\t%dr12, 0x0, %dr8"], NOP]),
    ("ld_putsm", [LD, NOP, NOP, ["puttagd,2,sm\t%dr12, 0x0, %dr8"], NOP]),
    ("ld_put_use", [LD, NOP, NOP, ["puttagd,2\t%dr12, 0x0, %dr12"], NOP,
                    ["pcmpeqb,0\t%dr12, %dr1, %dr8"], NOP]),
    ("ld_use", [LD, NOP, NOP, ["pcmpeqb,0\t%dr12, %dr1, %dr8"], NOP]),
    ("ldw_puts", [LDW, NOP, NOP, ["puttags,2\t%r12, 0x0, %r8"], NOP]),
    ("tagop", [LDW, NOP, NOP, ["puttagd,2\t%dr1, %r12, %dr8"], NOP]),
    ("tagop_sm", [LDW, NOP, NOP, ["puttagd,2,sm\t%dr1, %r12, %dr8"], NOP]),
    # A diagnostic tag operand whose low bits ask for tag 0 (0x40000000
    # with tag 1, made by puttags,sm), against the same value untagged.
    ("tagop_dg", [[DG], NOP, ["puttags,2,sm\t%r13, 0x1, %r12"], NOP,
                  ["puttagd,2\t%dr1, %r12, %dr8"], NOP]),
    ("tagop_dgsm", [[DG], NOP, ["puttags,2,sm\t%r13, 0x1, %r12"], NOP,
                    ["puttagd,2,sm\t%dr1, %r12, %dr8"], NOP]),
    ("tagop_num", [[DG], NOP, ["puttags,2\t%r13, 0x0, %r12"], NOP,
                   ["puttagd,2\t%dr1, %r12, %dr8"], NOP]),
    ("ld_gettagd", [LD, NOP, NOP, ["gettagd,2\t%dr12, %r8"], NOP]),
    ("ld_gettagdsm", [LD, NOP, NOP, ["gettagd,2,sm\t%dr12, %r8"], NOP]),
    ("ldw_gettags", [LDW, NOP, NOP, ["gettags,2\t%r12, %r8"], NOP]),
    ("ld_movtd", [LD, NOP, NOP, ["movtd,0\t%dr12, %dr8"], NOP]),
    ("ld_movtdsm", [LD, NOP, NOP, ["movtd,0,sm\t%dr12, %dr8"], NOP]),
    ("ldw_movts", [LDW, NOP, NOP, ["movts,0\t%r12, %r8"], NOP]),
    ("ld_movtq", [LD + ["addd,1\t0x0, %dr1, %dr13"], NOP, NOP,
                  ["movtq,0\t%qr12, %qr8"], NOP]),
    ("ld_mergeF", [LD + ["cmpesb,1\t0x0, 0x1, %pred0"], NOP, NOP,
                   ["merged,0\t%dr12, %dr1, %dr8, %pred0"], NOP]),
    ("ld_mergeT", [LD + ["cmpesb,1\t0x0, 0x0, %pred0"], NOP, NOP,
                   ["merged,0\t%dr12, %dr1, %dr8, %pred0"], NOP]),
    ("ld_mergeFsm", [LD + ["cmpesb,1\t0x0, 0x1, %pred0"], NOP, NOP,
                     ["merged,0,sm\t%dr12, %dr1, %dr8, %pred0"], NOP]),
    ("ldw_mergesF", [LDW + ["cmpesb,1\t0x0, 0x1, %pred0"], NOP, NOP,
                     ["merges,0\t%r12, %r1, %r8, %pred0"], NOP]),
    ("ldw_mergesT", [LDW + ["cmpesb,1\t0x0, 0x0, %pred0"], NOP, NOP,
                     ["merges,0\t%r12, %r1, %r8, %pred0"], NOP]),
    # Speculative operations on a diagnostic operand whose computed
    # result has bit 30 clear: and with 0, xor with itself, shift by 63.
    ("ld_andsm", [LD, NOP, NOP, ["andd,0,sm\t%dr12, 0x0, %dr8"], NOP]),
    ("ld_andsm_use", [LD, NOP, NOP, ["andd,0,sm\t%dr12, 0x0, %dr13"], NOP,
                      ["addd,0\t%dr13, 0x0, %dr8"], NOP]),
    ("ld_andsm_dtal", [LD, NOP, NOP, ["rbranch\t%%LOCKED ? %dt_al0",
                                      "andd,0,sm\t%dr12, 0x0, %dr8"], NOP]),
    ("ld_xorsm", [LD, NOP, NOP, ["xord,0,sm\t%dr12, %dr12, %dr8"], NOP]),
    ("ld_xorsm_use", [LD, NOP, NOP, ["xord,0,sm\t%dr12, %dr12, %dr13"], NOP,
                      ["addd,0\t%dr13, 0x0, %dr8"], NOP]),
    ("ld_shrsm", [LD, NOP, NOP, ["shrd,0,sm\t%dr12, 0x3f, %dr8"], NOP]),
    ("ldw_andssm", [LDW, NOP, NOP, ["ands,0,sm\t%r12, 0x0, %r8"], NOP]),
    # gettags / gettagd into a register whose upper half is 0x55555555
    # (dbl = 0).
    ("gts_upper", [["addd,0\t0x0, _f64,_lts0 0x5555555555555555, %dr8"],
                   NOP, ["gettags,2\t%r1, %r8"], NOP]),
    ("gtd_upper", [["addd,0\t0x0, _f64,_lts0 0x5555555555555555, %dr8"],
                   NOP, ["gettagd,2\t%dr1, %r8"], NOP]),
]
ADDR5 = [
    ("q_ld_put", [["ldqp,0,sm\t%dr3, 0x0, %qpr12"], NOP, NOP,
                  ["puttagqp,0\t%qpr12, 0x0, %qpr8"], NOP], True),
    ("q_ld", [["ldqp,0,sm\t%dr3, 0x0, %qpr8"], NOP, NOP], True),
]

# Latency chains: (operation, gap) steps; gap None is the scanned one.
LAT = {
    "adds": [("adds,0\t%r8, 0x0, %r8", None)],
    "puttagd": [("puttagd,2\t%dr8, 0x0, %dr8", None)],
    "puttagd_src2": [("puttagd,2\t%dr9, %r8, %dr8", None)],
    "puttagd_addd": [("puttagd,2\t%dr8, 0x0, %dr8", None),
                     ("addd,0\t%dr8, 0x0, %dr8", 4)],
    "addd_puttagd": [("addd,0\t%dr8, 0x0, %dr8", None),
                     ("puttagd,2\t%dr8, 0x0, %dr8", 8)],
    "puttagd5_addd": [("puttagd,5\t%dr8, 0x0, %dr8", None),
                      ("addd,0\t%dr8, 0x0, %dr8", 4)],
    "addd_puttagd5": [("addd,0\t%dr8, 0x0, %dr8", None),
                      ("puttagd,5\t%dr8, 0x0, %dr8", 8)],
    "gettagd": [("gettagd,2\t%dr8, %r8", None)],
    "gettagd_adds": [("gettagd,2\t%dr8, %r8", None),
                     ("adds,0\t%r8, 0x0, %r8", 4)],
    "adds_gettagd": [("adds,0\t%r8, 0x0, %r8", None),
                     ("gettagd,2\t%dr8, %r8", 8)],
    "puttags": [("puttags,2\t%r8, 0x0, %r8", None)],
    "puttags_adds": [("puttags,2\t%r8, 0x0, %r8", None),
                     ("adds,0\t%r8, 0x0, %r8", 4)],
    "gettags": [("gettags,2\t%r8, %r8", None)],
    "gettags_adds": [("gettags,2\t%r8, %r8", None),
                     ("adds,0\t%r8, 0x0, %r8", 4)],
    "movtd": [("movtd,0\t%dr8, %dr8", None)],
    "movtd_addd": [("movtd,0\t%dr8, %dr8", None),
                   ("addd,0\t%dr8, 0x0, %dr8", 4)],
    "movts": [("movts,0\t%r8, %r8", None)],
    "movts_adds": [("movts,0\t%r8, %r8", None),
                   ("adds,0\t%r8, 0x0, %r8", 4)],
    "movtq": [("movtq,0\t%qr8, %qr8", None)],
    "movtq_addd": [("movtq,0\t%qr8, %qr8", None),
                   ("addd,0\t%dr8, 0x0, %dr8", 4)],
    # A 32-bit result read as 64 bits (gettagd reads %dr8 after a 32-bit
    # write of %r8): adds -> addd and gettagd -> gettagd under dbl = 0
    # and, with the suffix _dbl1, under dbl = 1.
    "adds_addd": [("adds,0\t%r8, 0x0, %r8", None),
                  ("addd,0\t%dr8, 0x0, %dr8", 8)],
    "adds_addd_dbl1": [("adds,0\t%r8, 0x0, %r8", None),
                       ("addd,0\t%dr8, 0x0, %dr8", 8)],
    "gettagd_dbl1": [("gettagd,2\t%dr8, %r8", None)],
    "adds_gettagd_dbl1": [("adds,0\t%r8, 0x0, %r8", None),
                          ("gettagd,2\t%dr8, %r8", 8)],
    # More 32-bit producers and 64-bit consumers: other operations, other
    # channels, the other cluster; sxt (reads a 32-bit operand) and muls
    # (6 to an integer op) for comparison; addd -> adds as a control.
    "ands_andd": [("ands,0\t%r8, %r8, %r8", None),
                  ("andd,0\t%dr8, %dr8, %dr8", 8)],
    "shls_shld": [("shls,0\t%r8, 0x0, %r8", None),
                  ("shld,0\t%dr8, 0x0, %dr8", 8)],
    "adds_addd1": [("adds,0\t%r8, 0x0, %r8", None),
                   ("addd,1\t%dr8, 0x0, %dr8", 8)],
    "adds_addd3": [("adds,0\t%r8, 0x0, %r8", None),
                   ("addd,3\t%dr8, 0x0, %dr8", 8)],
    "adds3_addd4": [("adds,3\t%r8, 0x0, %r8", None),
                    ("addd,4\t%dr8, 0x0, %dr8", 8)],
    "adds_sxt": [("adds,0\t%r8, 0x0, %r8", None),
                 ("sxt,0\t0x2, %r8, %dr8", 8)],
    "adds_movtd": [("adds,0\t%r8, 0x0, %r8", None),
                   ("movtd,0\t%dr8, %dr8", 8)],
    "muls_adds": [("muls,0\t%r8, 0x1, %r8", None),
                  ("adds,0\t%r8, 0x0, %r8", 8)],
    "muls_addd": [("muls,0\t%r8, 0x1, %r8", None),
                  ("addd,0\t%dr8, 0x0, %dr8", 8)],
    "addd_adds": [("addd,0\t%dr8, 0x0, %dr8", None),
                  ("adds,0\t%r8, 0x0, %r8", 8)],
    "ands_andd_dbl1": [("ands,0\t%r8, %r8, %r8", None),
                       ("andd,0\t%dr8, %dr8, %dr8", 8)],
    "adds_addd3_dbl1": [("adds,0\t%r8, 0x0, %r8", None),
                        ("addd,3\t%dr8, 0x0, %dr8", 8)],
}
LAT5 = {
    "puttagqp": [("puttagqp,0\t%qpr8, 0x0, %qpr8", None)],
    "puttagqp_addd": [("puttagqp,0\t%qpr8, 0x0, %qpr8", None),
                      ("addd,0\t%dr8, 0x0, %dr8", 4)],
    "addd_puttagqp": [("addd,0\t%dr8, 0x0, %dr8", None),
                      ("puttagqp,0\t%qpr8, 0x0, %qpr8", 8)],
    "puttagqp_qpand": [("puttagqp,0\t%qpr8, 0x0, %qpr8", None),
                       ("qpand,0\t%qpr8, %qpr8, %qpr8", 4)],
}
GAPS = range(1, 7)
NS = (16, 48)


def lat_case(kind, steps, g, n, quad=False):
    groups = []
    for _ in range(n):
        for op, gap in steps:
            gap = g if gap is None else gap
            groups.append([op] + ([f"nop {gap - 1}"] if gap > 1 else []))
    name = f"lat_{kind}_g{g}_n{n}"
    text = gen_spec.case(name, groups, quad=quad)
    if kind.endswith("_dbl1"):
        text = text.replace("dbl = 0x0", "dbl = 0x1")
    return name, text.replace(f"spec_{name}", f"tagx_{name}")


# Model-checked operations: (name, operation text).
MODEL = [
    ("muls", "muls,0\t%r2, %r3, %r6"),
    ("muld", "muld,0\t%dr2, %dr3, %dr6"),
    ("merges", "merges,0\t%r2, %r3, %r6, %pred1"),
    ("merged", "merged,0\t%dr2, %dr3, %dr6, %pred1"),
    ("movts", "movts,0\t%r2, %r6"),
    ("movtd", "movtd,0\t%dr2, %dr6"),
    ("movtq", "movtq,0\t%qr2, %qr6"),
    ("fstoistr", "fstoistr,0\t%r2, %r6"),
]


def model_fn(i, op):
    o = []
    wi(o, "setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    wi(o, "rrs,0\t%pfpfr, %r8")
    wi(o, "ldd,0\t%dr0, 0x0, %dr2", "ldd,2\t%dr0, 0x8, %dr3",
       "ldd,3\t%dr0, 0x10, %dr4", "ldd,5\t%dr0, 0x18, %dr5",
       "addd,1\t0x0, _f64,_lts0 0x5555555555555555, %dr6")
    wi(o, "ands,0\t%r8, 0xffffffc0, %r9", "addd,1\t0x0, %dr6, %dr7")
    wi(o, "rws,0\t%r9, %pfpfr", "cmpesb,1\t%r5, 0x1, %pred1")
    wi(o, "nop 7")
    wi(o, op)
    wi(o, "nop 7")
    wi(o, "nop 7")
    wi(o, "nop 2", "rrs,0\t%pfpfr, %r9")
    wi(o, "std,2\t%dr1, 0x0, %dr6", "std,5\t%dr1, 0x8, %dr7")
    wi(o, "std,2\t%dr1, 0x10, %dr9")
    wi(o, "rws,0\t%r8, %pfpfr")
    wi(o, "nop 5")
    wi(o, "ct\t%ctpr3")
    return func(f"m_{i}", o)


# APB: (name, fapb fields, mova operation, destination registers).
APB = [
    ("movad", {"fmt": 4, "mrng": 8}, "movad", ("%dr8", "%dr10")),
    ("movaq", {"fmt": 5, "mrng": 16}, "movaq", ("%qr8", "%qr10")),
    ("movaqp", {"fmt": 5, "mrng": 16}, "movaqp", ("%dr8", "%dr10")),
    ("movaqp7", {"fmt": 7, "mrng": 16}, "movaqp", ("%dr8", "%dr10")),
    # The first result used by a non-speculative addd (into %dr9).
    ("movaqp_use", {"fmt": 5, "mrng": 16}, "movaqp", ("%dr8", "%dr10"),
     ["nop 7"], ["addd,0\t%dr8, 0x0, %dr9"]),
    # Which operation an fmt 7 area makes trap: a movad, or bap without
    # any mova (fmt 5 as the control); movaqp on a doubleword area.
    ("movad7", {"fmt": 7, "mrng": 16}, "movad", ("%dr8", "%dr10")),
    ("nomova7", {"fmt": 7, "mrng": 16}, "movad", ()),
    ("nomova5", {"fmt": 5, "mrng": 16}, "movad", ()),
    ("movaqp4", {"fmt": 4, "mrng": 8}, "movaqp", ("%dr8", "%dr10")),
]


def apb_case(name, fields, op, dsts, *after):
    o = []
    wi(o, "setwd\twsz = 0x20, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    wi(o, *[f"addd,{c}\t0x0, 0x0, %dr{r}"
            for c, r in zip((0, 1, 3, 4), (8, 9, 10, 11))])
    wi(o, f"ldisp\t%ctpr2, .Lapb_{name}_prog", "aaurwd,2\t%dr1, %aad0")
    wi(o, "bap")
    wi(o, "nop 7")
    wi(o, "nop 7")
    wi(o, "nop 4")
    wi(o, "rrd,0\t%clkr, %dr4")
    for d in dsts:
        wi(o, f"{op},0\tarea=0, ind=0, am=1, be=0, {d}")
    for ops in after:
        wi(o, *ops)
    wi(o, "rrd,0\t%clkr, %dr5")
    wi(o, "nop 7")
    for k in range(8, 12):
        wi(o, f"gettagd,2\t%dr{k}, %r6", f"puttagd,5\t%dr{k}, 0x0, %dr7")
        wi(o, "nop 2")
        wi(o, f"std,2\t%dr0, {(k - 8) * 16:#x}, %dr7",
           f"std,5\t%dr0, {(k - 8) * 16 + 8:#x}, %dr6")
    wi(o, "disp\t%ctpr2, disp=0x0", "subd,0\t%dr5, %dr4, %dr0")
    wi(o, "ct\t%ctpr3")
    o.append(f"\t.align\t8\n.Lapb_{name}_prog:\n")
    wi(o, gen_apb.fapb("l", True, fields))
    return func(f"apb_{name}", o)


HEADER = ("! SPDX-License-Identifier: GPL-3.0-or-later\n"
          "! Generated by tools/isa-probes/gen_tagx.py; see tagx.c.\n"
          "\t.ignore\tld_st_style\n\t.text\n")


def main():
    v3, h3, v5, h5 = [], [], [], []
    for name, groups in SWEEP_D:
        v3.append(tcase(name, groups))
        h3.append(f"TCASE ({name}, SWD)\n")
    for name, groups in SWEEP_S:
        v3.append(tcase(name, groups))
        h3.append(f"TCASE ({name}, SWS)\n")
    for name, groups in ADDR:
        v3.append(tcase(name, groups))
        h3.append(f"TCASE ({name}, ADR)\n")
    for kind, steps in LAT.items():
        for g in GAPS:
            for n in NS:
                name, text = lat_case(kind, steps, g, n)
                v3.append(text)
                h3.append(f"LCASE ({kind}, {g}, {n})\n")
    for i, (name, op) in enumerate(MODEL):
        v3.append(model_fn(i, op))
        h3.append(f'MCASE ({i}, "{name}")\n')
    for name, fields, op, dsts, *after in APB:
        v3.append(apb_case(name, fields, op, dsts, *after))
        h3.append(f"ACASE ({name})\n")
    for name, groups, quad in SWEEP_Q:
        v5.append(tcase(name, groups, quad))
        h5.append(f"TCASE ({name}, SWQ)\n")
    for name, groups, quad in ADDR5:
        v5.append(tcase(name, groups, quad))
        h5.append(f"TCASE ({name}, ADR)\n")
    for kind, steps in LAT5.items():
        for g in GAPS:
            for n in NS:
                name, text = lat_case(kind, steps, g, n, quad=True)
                v5.append(text)
                h5.append(f"LCASE ({kind}, {g}, {n})\n")
    hh = ("/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
          "/* Generated by tools/isa-probes/gen_tagx.py.  */\n")
    (HERE / "tagx.s").write_text(HEADER + "".join(v3))
    (HERE / "tagx-cases.h").write_text(hh + "".join(h3))
    (HERE / "tagx-v5.s").write_text(HEADER + "".join(v5))
    (HERE / "tagx-v5-cases.h").write_text(hh + "".join(h5))
    print(len(h3), "v3 cases,", len(h5), "v5 cases")


if __name__ == "__main__":
    main()
