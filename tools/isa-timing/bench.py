#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Measure E2K operation latencies and pipeline hazards on hardware.

Generates assembly kernels and a C driver, builds them with lcc and runs
them under qemu-e2k or on an Elbrus machine over SSH. Three kinds of
measurement:

  lat    Latency of an operation: a chain of K dependent copies, one per
         wide instruction, in a counted loop. The pipeline stalls until an
         operand is ready, so the chain runs at one result per latency.
         latency = (T(K2) - T(K1)) / (iterations * (K2 - K1)), which
         cancels the loop overhead. Cycles come from %clkr.
  ctl    Control-transfer preparation: per-block cycles with the prepare
         (disp, movtd, return) placed D cycles before its ct, D = 1..10.
         The time is flat while D is below the latency.
  hz     Hazards: does an operation D cycles (0 = same wide instruction)
         after a producer see the new value or the old one? Old values
         mean the dependency is not interlocked.

Usage:
  bench.py [--target T] [--remote [USER@]HOST] [--ssh-key FILE]
           [--out DIR] [--yaml FILE] [--memory] [--numa CPU:MEM]

--target picks lcc as in tools/abi/compat/run.py (default v3). Results
are printed as tables; --yaml writes them as data (see
spec/isa/timing.md for the format).
"""

import argparse
import datetime
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LCC_VERSION = "1.29.16"
SSH_KEY = ROOT / "secrets" / "id_ed25519"

K1, K2 = 32, 64
ITER = 20000
REPS = 5

# Register conventions inside kernels (setwd wsz = 0x10, dbl = 1):
#   %dr0 iteration count, %dr1 pointer argument, %dr2 the chain value,
#   %dr3 = 1, %dr4 = 1.0, %dr5 = 0 (also +0.0), %r6 = 1.0f
INIT = [
    "addd,0 0x0, 0x1, %dr3",
    "addd,1 0x0, 0x3ff0000000000000, %dr4",
    "addd,3 0x0, 0x0, %dr5",
    "addd,4 0x0, 0x3f800000, %dr6",
]

# name: (initial %dr2 source, list of operations forming one chain step,
#        description). The chain value must stay valid for every operation.
LAT = {
    "adds": ("%dr3", ["adds,0 %r2, %r5, %r2"], "32-bit integer add"),
    "addd": ("%dr3", ["addd,0 %dr2, %dr5, %dr2"], "64-bit integer add"),
    "andd": ("%dr3", ["andd,0 %dr2, %dr2, %dr2"], "bitwise and"),
    "shld": ("%dr3", ["shld,0 %dr2, 0x0, %dr2"], "shift left"),
    "sxt": ("%dr3", ["sxt,0 0x3, %dr2, %dr2"], "sign extension"),
    "add_addd": ("%dr3", ["add_addd,1 %dr2, %dr5, %dr5, %dr2"],
                 "combined integer op"),
    "shl_addd": ("%dr3", ["shl_addd,1 %dr2, 0x0, %dr5, %dr2"],
                 "combined integer op"),
    "muls": ("%dr3", ["muls,0 %r2, %r3, %r2"], "32-bit integer multiply"),
    "muld": ("%dr3", ["muld,0 %dr2, %dr3, %dr2"], "64-bit integer multiply"),
    "sdivs": ("%dr3", ["sdivs,5 %r2, %r3, %r2"], "32-bit integer divide"),
    "sdivd": ("%dr3", ["sdivd,5 %dr2, %dr3, %dr2"], "64-bit integer divide"),
    "fadds": ("%dr6", ["fadds,0 %r2, %r5, %r2"], "single add"),
    "faddd": ("%dr4", ["faddd,0 %dr2, %dr5, %dr2"], "double add"),
    "fmuls": ("%dr6", ["fmuls,0 %r2, %r6, %r2"], "single multiply"),
    "fmuld": ("%dr4", ["fmuld,0 %dr2, %dr4, %dr2"], "double multiply"),
    "fmul_addd": ("%dr4", ["fmul_addd,0 %dr2, %dr4, %dr5, %dr2"],
                  "combined FP op"),
    "fdivs": ("%dr6", ["fdivs,5 %r2, %r6, %r2"], "single divide"),
    "fdivd": ("%dr4", ["fdivd,5 %dr2, %dr4, %dr2"], "double divide"),
    "fsqrts": ("%dr6", ["fsqrts,5 %r2, %r2"], "single square root"),
    "ldd_l1_ch0": ("%dr1", ["ldd,0 %dr2, 0x0, %dr2"], "load, L1 hit, ALC0"),
    "ldd_l1_ch2": ("%dr1", ["ldd,2 %dr2, 0x0, %dr2"], "load, L1 hit, ALC2"),
    "ldd_l1_ch3": ("%dr1", ["ldd,3 %dr2, 0x0, %dr2"], "load, L1 hit, ALC3"),
    "ldd_l1_ch5": ("%dr1", ["ldd,5 %dr2, 0x0, %dr2"], "load, L1 hit, ALC5"),
    "ldw_l1": ("%dr1", ["ldd,0 %dr2, 0x0, %dr2"], None),
    # Pairs: latency of the pair; the cross-domain penalty is
    # pair - sum of the two single latencies.
    "pair_int_int": ("%dr3", ["addd,0 %dr2, %dr5, %dr2",
                              "addd,0 %dr2, %dr5, %dr2"], "addd -> addd"),
    "pair_fp_fp": ("%dr4", ["faddd,0 %dr2, %dr5, %dr2",
                            "faddd,0 %dr2, %dr5, %dr2"], "faddd -> faddd"),
    "pair_int_fp": ("%dr4", ["addd,0 %dr2, %dr5, %dr2",
                             "faddd,0 %dr2, %dr5, %dr2"],
                    "addd -> faddd -> addd (both crossings)"),
    "pair_mul_add": ("%dr3", ["muld,0 %dr2, %dr3, %dr2",
                              "addd,0 %dr2, %dr5, %dr2"],
                     "muld -> addd -> muld"),
    "pair_ld_add": ("%dr1", ["ldd,0 %dr2, 0x0, %dr2",
                             "addd,0 %dr2, %dr5, %dr2"],
                    "ldd -> addd -> ldd (address use)"),
}
del LAT["ldw_l1"]

# Scheduled latency: the chain step is a list of (op, gap) where gap is
# the distance in cycles to the next operation of the chain. The scanned
# operation's gap goes from 1 to its maximum; the smallest gap at which
# the step takes exactly sum(gaps) cycles is the latency at which the
# consumer issues without a stall. Other ops get a gap large enough not
# to stall.
SCHED = {
    "addd": ("%dr3", [("addd,0 %dr2, %dr5, %dr2", None)], 4),
    "add_addd": ("%dr3", [("add_addd,1 %dr2, %dr5, %dr5, %dr2", None)], 8),
    "muld": ("%dr3", [("muld,0 %dr2, %dr3, %dr2", None)], 8),
    "faddd": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None)], 8),
    "fmuld": ("%dr4", [("fmuld,0 %dr2, %dr4, %dr2", None)], 8),
    "fmul_addd": ("%dr4", [("fmul_addd,0 %dr2, %dr4, %dr5, %dr2", None)], 12),
    "fdivs": ("%dr6", [("fdivs,5 %r2, %r6, %r2", None)], 16),
    "fdivd": ("%dr4", [("fdivd,5 %dr2, %dr4, %dr2", None)], 20),
    "fsqrts": ("%dr6", [("fsqrts,5 %r2, %r2", None)], 16),
    "sdivd": ("%dr3", [("sdivd,5 %dr2, %dr3, %dr2", None)], 16),
    "ldd": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None)], 8),
    "faddd_to_addd": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                               ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
    "addd_to_faddd": ("%dr4", [("addd,0 %dr2, %dr5, %dr2", None),
                               ("faddd,0 %dr2, %dr5, %dr2", 12)], 6),
    "muld_to_addd": ("%dr3", [("muld,0 %dr2, %dr3, %dr2", None),
                              ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
    "ldd_to_addd": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None),
                             ("addd,0 %dr2, %dr5, %dr2", 12)], 8),
    "addd_to_ldd": ("%dr1", [("addd,0 %dr2, %dr5, %dr2", None),
                             ("ldd,0 %dr2, 0x0, %dr2", 12)], 6),
    "faddd_to_fmuld": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                                ("fmuld,0 %dr2, %dr4, %dr2", 12)], 8),
    # Integer <-> FP crossings again, with other consumers and with the
    # consumer in the other cluster (ALC0-2 vs ALC3-5): the guide gives
    # int -> fp +1, which the ALC0 -> ALC0 case above does not show.
    "addd_to_fmuld": ("%dr4", [("addd,0 %dr2, %dr5, %dr2", None),
                               ("fmuld,0 %dr2, %dr4, %dr2", 12)], 6),
    "adds_to_fadds": ("%dr6", [("adds,0 %r2, %r5, %r2", None),
                               ("fadds,0 %r2, %r5, %r2", 12)], 6),
    "addd_to_faddd_x": ("%dr4", [("addd,0 %dr2, %dr5, %dr2", None),
                                 ("faddd,3 %dr2, %dr5, %dr2", 12)], 6),
    "faddd_to_addd_x": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                                 ("addd,3 %dr2, %dr5, %dr2", 12)], 10),
    "addd_to_addd_x": ("%dr3", [("addd,0 %dr2, %dr5, %dr2", None),
                                ("addd,3 %dr2, %dr5, %dr2", 12)], 6),
    "faddd_to_faddd_x": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                                  ("faddd,3 %dr2, %dr5, %dr2", 12)], 8),
    "addd_to_addd_c1": ("%dr3", [("addd,0 %dr2, %dr5, %dr2", None),
                                 ("addd,1 %dr2, %dr5, %dr2", 12)], 6),
    "addd_to_addd_c2": ("%dr3", [("addd,0 %dr2, %dr5, %dr2", None),
                                 ("addd,2 %dr2, %dr5, %dr2", 12)], 6),
    "addd_to_addd_c4": ("%dr3", [("addd,0 %dr2, %dr5, %dr2", None),
                                 ("addd,4 %dr2, %dr5, %dr2", 12)], 6),
    "addd3_to_addd0": ("%dr3", [("addd,3 %dr2, %dr5, %dr2", None),
                                ("addd,0 %dr2, %dr5, %dr2", 12)], 6),
    "faddd_to_faddd_c1": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                                   ("faddd,1 %dr2, %dr5, %dr2", 12)], 8),
    "faddd_to_faddd_c4": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                                   ("faddd,4 %dr2, %dr5, %dr2", 12)], 8),
    "ldd_to_addd_x": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None),
                               ("addd,3 %dr2, %dr5, %dr2", 12)], 8),
    "ldd_to_ldd_x": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None),
                              ("ldd,3 %dr2, 0x0, %dr2", 12)], 8),
    "addd_to_ldd_x": ("%dr1", [("addd,0 %dr2, %dr5, %dr2", None),
                               ("ldd,3 %dr2, 0x0, %dr2", 12)], 6),
    "muld_to_muld_x": ("%dr3", [("muld,0 %dr2, %dr3, %dr2", None),
                                ("muld,3 %dr2, %dr3, %dr2", 12)], 8),
    # Packed (64-bit SIMD) integer operations; the guide puts them in the
    # FP class with its +2 to integer consumers.
    "paddw": ("%dr3", [("paddw,0 %dr2, %dr5, %dr2", None)], 8),
    "pandd": ("%dr3", [("pandd,0 %dr2, %dr2, %dr2", None)], 8),
    "pcmpeqw": ("%dr3", [("pcmpeqw,0 %dr2, %dr5, %dr2", None)], 8),
    "pmaxsh": ("%dr3", [("pmaxsh,0 %dr2, %dr5, %dr2", None)], 8),
    "psllw": ("%dr3", [("psllw,1 %dr2, %dr5, %dr2", None)], 8),
    "pmullh": ("%dr3", [("pmullh,1 %dr2, %dr3, %dr2", None)], 8),
    "psadbw": ("%dr3", [("psadbw,1 %dr2, %dr5, %dr2", None)], 8),
    "pshufb": ("%dr3", [("pshufb,0 %dr2, %dr2, %dr5, %dr2", None)], 8),
    "paddw_to_addd": ("%dr3", [("paddw,0 %dr2, %dr5, %dr2", None),
                               ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
    "addd_to_paddw": ("%dr3", [("addd,0 %dr2, %dr5, %dr2", None),
                               ("paddw,0 %dr2, %dr5, %dr2", 12)], 6),
    "paddw_to_faddd": ("%dr4", [("paddw,0 %dr2, %dr5, %dr2", None),
                                ("faddd,0 %dr2, %dr5, %dr2", 12)], 8),
    "pmullh_to_addd": ("%dr3", [("pmullh,1 %dr2, %dr3, %dr2", None),
                                ("addd,1 %dr2, %dr5, %dr2", 12)], 10),
    # Other integer operations in restricted channels.
    "lzcnts": ("%dr3", [("lzcnts,1 %r2, %r2", None)], 6),
    "scls": ("%dr3", [("scls,0 %r2, 0x1, %r2", None)], 6),
    "getfs": ("%dr3", [("getfs,0 %r2, %r5, %r2", None)], 6),
    # Conversions and FP compares into a register.
    "fdtoid": ("%dr4", [("fdtoid,0 %dr2, %dr2", None)], 8),
    "idtofd": ("%dr3", [("idtofd,0 %dr2, %dr2", None)], 8),
    "fstofd": ("%dr6", [("fstofd,0 %r2, %dr2", None)], 8),
    "fdtofs": ("%dr4", [("fdtofs,0 %dr2, %r2", None)], 8),
    "fdtoid_to_addd": ("%dr4", [("fdtoid,0 %dr2, %dr2", None),
                                ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
    "addd_to_idtofd": ("%dr3", [("addd,0 %dr2, %dr5, %dr2", None),
                                ("idtofd,0 %dr2, %dr2", 12)], 6),
    "fcmpeqd": ("%dr4", [("fcmpeqd,0 %dr2, %dr5, %dr2", None)], 8),
    "fcmpeqd_to_addd": ("%dr4", [("fcmpeqd,0 %dr2, %dr5, %dr2", None),
                                 ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
}

# Scheduled load latency beyond L1: a pointer chase through a buffer of
# each size with every load followed by a gap; the knee is the latency.
# (bytes, huge): huge = 2 MiB transparent huge pages (no TLB misses to
# speak of); the last entry repeats a size with 4 KiB pages.
CHASE_SCHED_SIZES = [(256 << 10, 1), (1 << 20, 1), (8 << 20, 1),
                     (64 << 20, 1), (64 << 20, 0)]
CHASE_SCHED_GAPS = list(range(1, 21)) + [24, 28, 32, 40, 60, 80, 100,
                                         120, 140, 160, 180, 200, 240]
SCHED_STEPS = 32

# Pointer-chase sizes for the memory hierarchy (bytes of buffer).
CHASE_SIZES = [16 << 10, 256 << 10, 2 << 20, 16 << 20, 128 << 20]

CTL_DISTANCES = range(1, 11)


def lat_kernel(name, k):
    init, step, _ = LAT[name]
    fn = f"lat_{name}_{k}"
    body = []
    for _ in range(k // len(step)):
        for op in step:
            body.append(f"\t{{\n\t  {op}\n\t}}")
    return f"""
	.global	{fn}
	.type	{fn}, #function
	.align	8
{fn}:
	{{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  {INIT[0]}
	  {INIT[1]}
	}}
	{{
	  disp	%ctpr1, .L{fn}
	}}
	{{
	  {INIT[2]}
	  {INIT[3]}
	}}
	{{
	  nop 5
	  addd,0 0x0, {init}, %dr2
	}}
.L{fn}:
{chr(10).join(body)}
	{{
	  subd,0 %dr0, 0x1, %dr0
	}}
	{{
	  nop 3
	  cmpedb,0 %dr0, 0x0, %pred0
	}}
	{{
	  ct	%ctpr1 ? ~%pred0
	}}
	{{
	  nop 7
	  addd,0 0x0, %dr2, %dr0
	}}
	{{
	  ct	%ctpr3
	}}
	.size	{fn}, .-{fn}
"""


def nop_field(d):
    """Text closing a wide instruction so that the next one issues D cycles
    after it: a header `nop` (at most 7) and, beyond that, empty wide
    instructions. Returns the lines to put inside the producer's braces
    followed by the closing brace and any extra empty instructions."""
    gap = d - 1
    head = min(gap, 7)
    text = f"\t  nop {head}\n" if head else ""
    text += "\t}\n"
    gap -= head
    while gap > 0:
        n = min(gap - 1, 7)
        text += f"\t{{\n\t  nop {n}\n\t}}\n" if n else "\t{\n\t}\n"
        gap -= n + 1
    return text


CHASE_SCHED = {"chase": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None)], None)}


def sched_kernel(name, gap, table=SCHED):
    init, step, _ = table[name]
    fn = f"sched_{name}_{gap}"
    body = []
    for _ in range(SCHED_STEPS):
        for op, g in step:
            g = gap if g is None else g
            body.append(f"\t{{\n\t  {op}\n{nop_field(g)}")
    return f"""
	.global	{fn}
	.type	{fn}, #function
	.align	8
{fn}:
	{{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  {INIT[0]}
	  {INIT[1]}
	}}
	{{
	  disp	%ctpr1, .L{fn}
	}}
	{{
	  {INIT[2]}
	  {INIT[3]}
	}}
	{{
	  nop 5
	  addd,0 0x0, {init}, %dr2
	}}
.L{fn}:
{"".join(body)}	{{
	  subd,0 %dr0, 0x1, %dr0
	}}
	{{
	  nop 3
	  cmpedb,0 %dr0, 0x0, %pred0
	}}
	{{
	  ct	%ctpr1 ? ~%pred0
	}}
	{{
	  nop 7
	  addd,0 0x0, %dr2, %dr0
	}}
	{{
	  ct	%ctpr3
	}}
	.size	{fn}, .-{fn}
"""


def ctl_kernel(kind, d):
    """One loop iteration = 16 blocks, each a prepare D cycles before ct."""
    fn = f"ctl_{kind}_{d}"
    blocks = []
    for i in range(16):
        nxt = f".L{fn}_b{i}"
        if kind == "disp":
            prep = f"\t  disp\t%ctpr2, {nxt}\n"
        elif kind == "cmp_ct":
            # The compare is false, so ct falls through; it still waits
            # for the predicate. %ctpr2 is prepared once, far away.
            prep = "\t  cmpedb,0 %dr3, 0x0, %pred0\n"
            blocks.append(f"\t{{\n{prep}{nop_field(d)}"
                          f"\t{{\n\t  ct\t%ctpr2 ? %pred0\n\t}}\n{nxt}:")
            continue
        elif kind == "cmp_alu":
            prep = "\t  cmpedb,0 %dr3, 0x0, %pred0\n"
            blocks.append(f"\t{{\n{prep}{nop_field(d)}"
                          f"\t{{\n\t  addd,0 %dr5, 0x1, %dr5 ? %pred0\n\t}}\n{nxt}:")
            continue
        elif kind in ("cmp_andp", "andp_alu", "andp_ct", "andp_andp",
                      "fcmp_alu", "fcmp_ct"):
            # Predicate logic. %dr3 = 1, so every predicate is false and
            # predicated consumers do nothing; ct falls through.
            cmp = "\t  cmpedb,0 %dr3, 0x0, %pred0\n"
            andp = ("\t  pass %pred0, @p0\n\t  pass %pred0, @p1\n"
                    "\t  andp @p0, @p1, @p4\n\t  pass @p4, %pred2\n")
            alu = "\t{\n\t  addd,0 %dr5, 0x1, %dr5 ? %pred2\n\t}\n"
            if kind == "cmp_andp":
                text = f"\t{{\n{cmp}{nop_field(d)}\t{{\n{andp}{nop_field(4)}{alu}"
            elif kind in ("fcmp_alu", "fcmp_ct"):
                fc = "\t  fcmpltdb,0 %dr3, %dr3, %pred2\n"
                use = (alu if kind == "fcmp_alu"
                       else "\t{\n\t  ct\t%ctpr2 ? %pred2\n\t}\n")
                text = f"\t{{\n{fc}{nop_field(d)}{use}"
            else:
                text = f"\t{{\n{cmp}{nop_field(4)}\t{{\n{andp}{nop_field(d)}"
                if kind == "andp_alu":
                    text += alu
                elif kind == "andp_ct":
                    text += "\t{\n\t  ct\t%ctpr2 ? %pred2\n\t}\n"
                else:
                    text += ("\t{\n\t  pass %pred2, @p0\n\t  pass %pred2, @p1\n"
                             "\t  andp @p0, @p1, @p4\n\t  pass @p4, %pred3\n"
                             f"{nop_field(4)}"
                             "\t{\n\t  addd,0 %dr5, 0x1, %dr5 ? %pred3\n\t}\n")
            blocks.append(text + f"{nxt}:")
            continue
        elif kind == "movtd":
            # The label address is set up before the prepare, far enough
            # away (address computation is not what is being measured).
            blocks.append(f"\t{{\n\t  nop 7\n\t  addd,0 0x0, {nxt}, %dr7\n\t}}")
            prep = "\t  movtd,0 %dr7, %ctpr2\n"
        blocks.append(f"\t{{\n{prep}{nop_field(d)}"
                      f"\t{{\n\t  ct\t%ctpr2\n\t}}\n{nxt}:")
    return f"""
	.global	{fn}
	.type	{fn}, #function
	.align	8
{fn}:
	{{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	}}
	{{
	  disp	%ctpr1, .L{fn}
	  addd,0 0x0, 0x1, %dr3
	}}
	{{
	  disp	%ctpr2, {fn}
	}}
	{{
	  nop 7
	}}
.L{fn}:
{chr(10).join(blocks)}
	{{
	  subd,0 %dr0, 0x1, %dr0
	}}
	{{
	  nop 3
	  cmpedb,0 %dr0, 0x0, %pred0
	}}
	{{
	  ct	%ctpr1 ? ~%pred0
	}}
	{{
	  ct	%ctpr3
	}}
	.size	{fn}, .-{fn}
"""


def ret_kernels(d):
    """A loop calling a leaf whose `return` is D cycles before its ct."""
    fn, leaf = f"ctl_return_{d}", f"leaf_return_{d}"
    calls = "\n".join(
        f"\t{{\n\t  disp\t%ctpr2, {leaf}\n\t}}\n\t{{\n\t  nop 5\n\t}}\n"
        f"\t{{\n\t  call\t%ctpr2, wbs = 0xc\n\t}}" for _ in range(8))
    return f"""
	.type	{leaf}, #function
	.align	8
{leaf}:
	{{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
{nop_field(d)}	{{
	  ct	%ctpr3
	}}
	.size	{leaf}, .-{leaf}

	.global	{fn}
	.type	{fn}, #function
	.align	8
{fn}:
	{{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x1
	  disp	%ctpr1, .L{fn}
	}}
.L{fn}:
{calls}
	{{
	  subd,0 %dr0, 0x1, %dr0
	  disp	%ctpr1, .L{fn}
	}}
	{{
	  nop 3
	  cmpedb,0 %dr0, 0x0, %pred0
	}}
	{{
	  ct	%ctpr1 ? ~%pred0
	}}
	{{
	  return	%ctpr3
	}}
	{{
	  nop 6
	}}
	{{
	  ct	%ctpr3
	}}
	.size	{fn}, .-{fn}
"""


# Hazard tests. Each function returns 1 if the consumer saw the new value
# (or the transfer went the new way), 0 if it saw the old one.
# (name, description, setup bundles, producer op, consumer bundle text,
#  epilogue bundles computing %dr0, cleanup)
def hz_kernel(name, d):
    fn = f"hz_{name}_{d}"
    pre = ["setwd\twsz = 0x10, nfx = 0x1, dbl = 0x1", "return\t%ctpr3"]
    settle = "\t{\n\t  nop 7\n\t}\n" * 2
    setup2 = []
    setup3 = []
    if name in ("cmp_pred_alu", "cmp_ct", "cmp_ibranch"):
        setup = ["addd,0 0x0, 0x0, %dr5"]
        setup2 = ["cmpedb,1 %dr5, 0x1, %pred0"]
        if name == "cmp_ct":
            setup2.append("disp\t%ctpr1, .L" + fn + "_taken")
        producer = "cmpedb,0 %dr5, 0x0, %pred0"
        if name == "cmp_pred_alu":
            consumer = "addd,0 %dr5, 0x1, %dr5 ? %pred0"
            tail = (f"{settle}\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, %dr5, %dr0\n\t}}")
        else:
            consumer = ("ct\t%ctpr1 ? %pred0" if name == "cmp_ct"
                        else f"ibranch\t.L{fn}_taken ? %pred0")
            tail = (f"\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, 0x0, %dr0\n\t}}\n"
                    f".L{fn}_taken:\n"
                    f"\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, 0x1, %dr0\n\t}}")
    elif name in ("fpcr_rrs", "lsr_rrd"):
        reg, w = ("%fpcr", "s") if name == "fpcr_rrs" else ("%lsr", "d")
        r = (lambda n: f"%r{n}") if w == "s" else (lambda n: f"%dr{n}")
        # Save the register, write it with bit 10 flipped, read it back D
        # cycles later, restore.
        setup = [f"rr{w},0 {reg}, {r(8)}"]
        setup2 = [f"xor{w},0 {r(8)}, 0x400, {r(9)}"]
        producer = f"rw{w},0 {r(9)}, {reg}"
        consumer = f"rr{w},0 {reg}, {r(10)}"
        tail = (f"{settle}\t{{\n\t  rw{w},0 {r(8)}, {reg}\n\t}}\n{settle}"
                f"\t{{\n\t  cmpe{w}b,0 {r(10)}, {r(9)}, %pred1\n\t}}\n"
                f"{settle}\t{{\n\t  addd,0 0x0, 0x0, %dr0\n\t}}\n"
                f"\t{{\n\t  addd,0 0x0, 0x1, %dr0 ? %pred1\n\t}}\n{settle}"
                f"\t{{\n\t  ct\t%ctpr3\n\t}}")
        pre_bundles = "\t{\n\t  " + "\n\t  ".join(pre + setup) + "\n\t}\n" + settle
        pre_bundles += "\t{\n\t  " + "\n\t  ".join(setup2) + "\n\t}\n" + settle
        return _hz_body(fn, pre_bundles, producer, consumer, d, tail)
    elif name == "fadd_pfpfr":
        # Clear the exception flags in %pfpfr (glibc: scalar FP state),
        # do an inexact faddd, read %pfpfr D cycles later: 1 if the
        # inexact flag (0x20) is visible.
        setup = ["addd,1 0x0, 0x3ff0000000000000, %dr4", "rrs,0 %pfpfr, %r8"]
        setup2 = ["ands,0 %r8, 0xffffffc0, %r9", "addd,1 0x0, 0x3c00000000000000, %dr5"]
        setup3 = ["rws,0 %r9, %pfpfr"]
        producer = "faddd,1 %dr4, %dr5, %dr6"
        consumer = "rrs,0 %pfpfr, %r10"
        tail = (f"{settle}\t{{\n\t  rws,0 %r8, %pfpfr\n\t  ands,1 %r10, 0x20, %r10\n\t}}\n{settle}"
                f"\t{{\n\t  cmpesb,0 %r10, 0x0, %pred1\n\t}}\n{settle}"
                f"\t{{\n\t  addd,0 0x0, 0x1, %dr0\n\t}}\n"
                f"\t{{\n\t  addd,0 0x0, 0x0, %dr0 ? %pred1\n\t}}\n{settle}"
                f"\t{{\n\t  ct\t%ctpr3\n\t}}")
    elif name == "pfpfr_round":
        # Set rounding toward +inf in %pfpfr (glibc FE_UPWARD, assuming the
        # default round-to-nearest), compute 2.0/3.0 D cycles later: 1 if
        # it was rounded up.
        setup = ["rrs,0 %pfpfr, %r8", "addd,1 0x0, 0x4000000000000000, %dr4"]
        setup2 = ["xors,0 %r8, 0x4000, %r9", "addd,3 0x0, 0x4008000000000000, %dr5"]
        producer = "rws,0 %r9, %pfpfr"
        consumer = "fdivd,5 %dr4, %dr5, %dr6"
        tail = (f"{settle}{settle}\t{{\n\t  rws,0 %r8, %pfpfr\n\t}}\n{settle}"
                f"\t{{\n\t  cmpedb,0 %dr6, 0x3fe5555555555556, %pred1\n\t}}\n{settle}"
                f"\t{{\n\t  addd,0 0x0, 0x0, %dr0\n\t}}\n"
                f"\t{{\n\t  addd,0 0x0, 0x1, %dr0 ? %pred1\n\t}}\n{settle}"
                f"\t{{\n\t  ct\t%ctpr3\n\t}}")
    elif name in ("int_fp_use", "fp_int_use", "int_fp_use_x", "fp_int_use_x"):
        # Cross-domain interlock check with a value that changes, so a stale
        # read is visible: %dr7 starts as +0.0; the producer makes it 1.0
        # (integer copy) or 2.0 (FP add); the consumer doubles it (FP) or
        # copies it (integer). New value: 0x4000000000000000, old: 0.
        setup = ["addd,3 0x0, 0x3ff0000000000000, %dr4", "addd,4 0x0, 0x0, %dr7",
                 "addd,1 0x0, 0x0, %dr8"]
        ch = "3" if name.endswith("_x") else "0"
        if name.startswith("int_fp"):
            producer = "addd,0 %dr4, 0x0, %dr7"
            consumer = f"faddd,{ch} %dr7, %dr7, %dr8"
        else:
            producer = "faddd,0 %dr4, %dr4, %dr7"
            consumer = f"addd,{ch} %dr7, 0x0, %dr8"
        tail = f"{settle}\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, %dr8, %dr0\n\t}}"
    elif name in ("cmp_andp_hz", "andp_pred_alu", "cmp_spred_andp", "clp_chain"):
        # Predicate logic correctness. %pred0 starts false and %pred1
        # false; the result 1 means the consumer saw the new value.
        setup = ["addd,0 0x0, 0x0, %dr5"]
        setup2 = ["cmpedb,0 %dr5, 0x1, %pred0", "cmpedb,1 %dr5, 0x1, %pred1"]
        use = (f"{settle}\t{{\n\t  addd,0 0x0, 0x0, %dr0\n\t}}\n"
               "\t{\n\t  addd,0 0x0, 0x1, %dr0 ? %pred1\n\t}\n"
               f"{settle}\t{{\n\t  ct\t%ctpr3\n\t}}")
        if name == "cmp_andp_hz":
            producer = "cmpedb,0 %dr5, 0x0, %pred0"
            consumer = ("pass %pred0, @p0\n\t  pass %pred0, @p1\n"
                        "\t  andp @p0, @p1, @p4\n\t  pass @p4, %pred1")
        elif name == "andp_pred_alu":
            setup2.append("cmpedb,3 %dr5, 0x0, %pred2")
            producer = ("pass %pred2, @p0\n\t  pass %pred2, @p1\n"
                        "\t  andp @p0, @p1, @p4\n\t  pass @p4, %pred1")
            consumer = "addd,0 0x0, 0x1, %dr6 ? %pred1"
            use = (f"{settle}\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, %dr6, %dr0\n\t}}")
            setup.append("addd,1 0x0, 0x0, %dr6")
        elif name == "cmp_spred_andp":
            # Same wide instruction only: the CLP reads the compare of this
            # instruction through %spred0 (ALC0's compare result).
            producer = "cmpedb,0 %dr5, 0x0, %pred0"
            consumer = ("pass %spred0, @p0\n\t  pass %spred0, @p1\n"
                        "\t  andp @p0, @p1, @p4\n\t  pass @p4, %pred1")
        else:  # clp_chain: a CLP reads @p4 written by the CLP before it
            setup2 = ["cmpedb,0 %dr5, 0x0, %pred0", "cmpedb,1 %dr5, 0x1, %pred1"]
            producer = ("pass %pred0, @p0\n\t  pass %pred0, @p1\n"
                        "\t  andp @p0, @p1, @p4\n"
                        "\t  andp @p4, @p1, @p5\n\t  pass @p5, %pred1")
            consumer = ""
        tail = use
    elif name in ("muld_use", "ldd_use", "fdivd_use", "sdivd_use"):
        # Interlock check: the consumer must see the produced value.
        setup = ["addd,0 0x0, 0x7, %dr5", "addd,1 0x0, %dr1, %dr6",
                 "addd,3 0x0, 0x3ff0000000000000, %dr4", "addd,4 0x0, 0x0, %dr7"]
        producer = {"muld_use": "muld,0 %dr5, %dr5, %dr7",          # 49
                    "ldd_use": "ldd,0 %dr6, 0x0, %dr7",             # *p
                    "fdivd_use": "fdivd,5 %dr4, %dr4, %dr7",        # 1.0
                    "sdivd_use": "sdivd,5 %dr5, %dr5, %dr7"}[name]  # 1
        consumer = "addd,0 %dr7, 0x0, %dr8"
        tail = f"{settle}\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, %dr8, %dr0\n\t}}"
    elif name in ("disp_ct", "movtd_ct"):
        # %ctpr1 first points at an "old" target returning 0; the producer
        # retargets it at one returning 1.
        setup = ["addd,0 0x0, 0x0, %dr0", f"addd,1 0x0, .L{fn}_taken, %dr7"]
        setup2 = [f"disp\t%ctpr1, .L{fn}_old"]
        producer = (f"disp\t%ctpr1, .L{fn}_taken" if name == "disp_ct"
                    else "movtd,0 %dr7, %ctpr1")
        consumer = "ct\t%ctpr1"
        tail = (f"\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, 0x2, %dr0\n\t}}\n"
                f".L{fn}_old:\n"
                f"\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, 0x0, %dr0\n\t}}\n"
                f".L{fn}_taken:\n"
                f"\t{{\n\t  ct\t%ctpr3\n\t  addd,0 0x0, 0x1, %dr0\n\t}}")
    else:
        raise ValueError(name)
    pre_bundles = "\t{\n\t  " + "\n\t  ".join(pre + setup) + "\n\t}\n" + settle
    for extra in (setup2, setup3):
        if extra:
            pre_bundles += "\t{\n\t  " + "\n\t  ".join(extra) + "\n\t}\n" + settle
    return _hz_body(fn, pre_bundles, producer, consumer, d, tail)


def _hz_body(fn, pre_bundles, producer, consumer, d, tail):
    if d == 0:
        consumer = consumer.replace("addd,0 ", "addd,1 ")
        mid = f"\t{{\n\t  {producer}\n\t  {consumer}\n\t}}\n"
    else:
        mid = (f"\t{{\n\t  {producer}\n{nop_field(d)}"
               f"\t{{\n\t  {consumer}\n\t}}\n")
    return f"""
	.global	{fn}
	.type	{fn}, #function
	.align	8
{fn}:
{pre_bundles}{mid}{tail}
	.size	{fn}, .-{fn}
"""


HZ = {
    "cmp_pred_alu": "compare -> ALU op predicated on its result",
    "cmp_ct": "compare -> ct %ctpr ? predicate",
    "cmp_ibranch": "compare -> ibranch ? predicate",
    "fpcr_rrs": "rws %fpcr -> rrs %fpcr",
    "pfpfr_round": "rws %pfpfr (rounding) -> fdivd uses it",
    "fadd_pfpfr": "faddd (inexact) -> rrs %pfpfr sees the flag",
    "lsr_rrd": "rwd %lsr -> rrd %lsr",
    "muld_use": "muld -> use (interlock check)",
    "ldd_use": "ldd -> use (interlock check)",
    "fdivd_use": "fdivd -> use (interlock check)",
    "sdivd_use": "sdivd -> use (interlock check)",
    "disp_ct": "disp -> ct",
    "movtd_ct": "movtd -> ct",
    "int_fp_use": "addd -> faddd, same cluster (new value 0x4000000000000000)",
    "int_fp_use_x": "addd,0 -> faddd,3, other cluster",
    "fp_int_use": "faddd -> addd, same cluster (new value 0x4000000000000000)",
    "fp_int_use_x": "faddd,0 -> addd,3, other cluster",
    "cmp_andp_hz": "compare -> andp reading its %pred",
    "andp_pred_alu": "andp -> ALU op predicated on its result",
    "cmp_spred_andp": "compare -> andp via %spred in the same instruction",
    "clp_chain": "andp -> andp via @p4 in the same instruction",
}
HZ_EXPECT = {"muld_use": 49, "ldd_use": None, "fdivd_use": 0x3ff0000000000000,
             "sdivd_use": 1}
HZ_DISTANCES = range(0, 9)


def generate(out, memory=False):
    out.mkdir(parents=True, exist_ok=True)
    asm = ["\t.ignore\tld_st_style", "\t.ignore\tstrict_delay", "\t.text",
           """
	.global	rdclk
	.type	rdclk, #function
	.align	8
rdclk:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  rrd,0	%clkr, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	rdclk, .-rdclk
"""]
    decl, lat_rows, ctl_rows, hz_rows = [], [], [], []
    for name in LAT:
        for k in (K1, K2):
            asm.append(lat_kernel(name, k))
            decl.append(f"long lat_{name}_{k}(long, void *);")
        lat_rows.append(f'\t{{"{name}", {len(LAT[name][1])}, '
                        f'lat_{name}_{K1}, lat_{name}_{K2}}},')
    sched_rows = []
    for name, (_, step, maxgap) in SCHED.items():
        fixed = sum(g for _, g in step if g is not None)
        for gap in range(1, maxgap + 1):
            asm.append(sched_kernel(name, gap))
            decl.append(f"long sched_{name}_{gap}(long, void *);")
            sched_rows.append(f'\t{{"{name}", {gap}, {fixed}, sched_{name}_{gap}}},')
    chase_rows = []
    for gap in CHASE_SCHED_GAPS:
        asm.append(sched_kernel("chase", gap, CHASE_SCHED))
        decl.append(f"long sched_chase_{gap}(long, void *);")
        chase_rows.append(f'\t{{{gap}, sched_chase_{gap}}},')
    for d in CTL_DISTANCES:
        for kind in ("disp", "movtd", "cmp_ct", "cmp_alu", "cmp_andp",
                     "andp_alu", "andp_ct", "andp_andp", "fcmp_alu", "fcmp_ct"):
            asm.append(ctl_kernel(kind, d))
            decl.append(f"long ctl_{kind}_{d}(long, void *);")
            ctl_rows.append(f'\t{{"{kind}", {d}, 16, ctl_{kind}_{d}}},')
        asm.append(ret_kernels(d))
        decl.append(f"long ctl_return_{d}(long, void *);")
        ctl_rows.append(f'\t{{"return", {d}, 8, ctl_return_{d}}},')
    for name in HZ:
        for d in HZ_DISTANCES:
            if name in ("cmp_spred_andp", "clp_chain") and d != 0:
                continue
            if name in ("muld_use", "ldd_use", "fdivd_use", "sdivd_use",
                        "andp_pred_alu",
                        "int_fp_use", "fp_int_use", "int_fp_use_x",
                        "fp_int_use_x", "disp_ct", "movtd_ct", "fpcr_rrs", "lsr_rrd",
                        "pfpfr_round", "fadd_pfpfr") and d == 0:
                continue
            asm.append(hz_kernel(name, d))
            decl.append(f"long hz_{name}_{d}(long, void *);")
            hz_rows.append(f'\t{{"{name}", {d}, hz_{name}_{d}}},')
    (out / "kernels.s").write_text("\n".join(asm))
    # Beyond-L1 load latencies are informational only (compilers schedule
    # loads for an L1 hit), so the pointer chases run only with --memory.
    chase = CHASE_SIZES if memory else [16 << 10]
    chase_sched = CHASE_SCHED_SIZES if memory else []
    nchs = len(chase_sched)
    chase_sched = chase_sched or [(0, 1)]
    sizes = ", ".join(str(s) for s in chase)
    (out / "main.c").write_text(f"""/* Generated by tools/isa-timing/bench.py. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
unsigned long rdclk(void);
{chr(10).join(decl)}
typedef long (*kern)(long, void *);
static struct {{ const char *name; int ops; kern f1, f2; }} lat[] = {{
{chr(10).join(lat_rows)}
}};
static struct {{ const char *name; int gap, fixed; kern f; }} sched[] = {{
{chr(10).join(sched_rows)}
}};
static struct {{ int gap; kern f; }} chase_sched[] = {{
{chr(10).join(chase_rows)}
}};
static struct {{ const char *kind; int d, blocks; kern f; }} ctl[] = {{
{chr(10).join(ctl_rows)}
}};
static struct {{ const char *name; int d; kern f; }} hz[] = {{
{chr(10).join(hz_rows)}
}};
static void *cell[16] __attribute__((aligned(64)));

static unsigned long best(kern f, long n, void *p)
{{
	unsigned long b = ~0UL;
	for (int r = 0; r < {REPS}; r++) {{
		unsigned long t0 = rdclk();
		f(n, p);
		unsigned long t = rdclk() - t0;
		if (t < b) b = t;
	}}
	return b;
}}

/* Like best(), but each run continues the chase where the previous one
   stopped (the kernels return the last pointer), so repeated runs don't
   revisit cached lines. */
static unsigned long bestc(kern f, long n, void **q)
{{
	unsigned long b = ~0UL;
	for (int r = 0; r < {REPS}; r++) {{
		unsigned long t0 = rdclk();
		*q = (void *)f(n, *q);
		unsigned long t = rdclk() - t0;
		if (t < b) b = t;
	}}
	return b;
}}

/* A random cyclic pointer chain through SIZE bytes, one pointer per
   64-byte line, in 2 MiB pages if HUGE, else in 4 KiB pages. Returns
   the start. */
static void **chain(size_t size, int huge)
{{
	size_t n = size / 64, i, al = 2 << 20;
	char *buf = aligned_alloc(al, (size + al - 1) / al * al);
	madvise(buf, size, huge ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
	size_t *perm = malloc(n * sizeof *perm);
	for (i = 0; i < n; i++) perm[i] = i;
	srand(1);
	for (i = n - 1; i > 0; i--) {{
		size_t j = (size_t)rand() % (i + 1), t = perm[i];
		perm[i] = perm[j]; perm[j] = t;
	}}
	for (i = 0; i < n; i++)
		*(void **)(buf + perm[i] * 64) = buf + perm[(i + 1) % n] * 64;
	free(perm);
	return (void **)buf;
}}

int main(void)
{{
	long n = {ITER};
	setvbuf(stdout, NULL, _IOLBF, 0);
	cell[0] = &cell[0];
	for (unsigned i = 0; i < sizeof lat / sizeof lat[0]; i++) {{
		void *p = cell;
		unsigned long t1 = best(lat[i].f1, n, p), t2 = best(lat[i].f2, n, p);
		printf("lat %s %.3f\\n", lat[i].name,
		       (double)(t2 - t1) * lat[i].ops / ((double)n * ({K2} - {K1})));
	}}
	size_t sizes[] = {{ {sizes} }};
	for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {{
		void **p = chain(sizes[i], 1), *q = p;
		long m = n / 8;
		bestc(lat_ldd_l1_ch0_{K2}, m, &q);
		unsigned long t1 = bestc(lat_ldd_l1_ch0_{K1}, m, &q),
			      t2 = bestc(lat_ldd_l1_ch0_{K2}, m, &q);
		printf("chase %zu %.3f\\n", sizes[i],
		       (double)(t2 - t1) / ((double)m * ({K2} - {K1})));
		free(p);
	}}
	for (unsigned i = 0; i < sizeof sched / sizeof sched[0]; i++) {{
		long m = n / 4;
		unsigned long t = best(sched[i].f, m, cell),
			      t0 = best(sched[i].f, m / 2, cell);
		printf("sched %s %d %d %.3f\\n", sched[i].name, sched[i].gap,
		       sched[i].fixed, (double)(t - t0) / ((double)(m - m / 2) * {SCHED_STEPS}));
	}}
	size_t csizes[] = {{ {", ".join(str(x) for x, _ in chase_sched)} }};
	int chuge[] = {{ {", ".join(str(h) for _, h in chase_sched)} }};
	for (unsigned i = 0; i < {nchs}; i++) {{
		void **p = chain(csizes[i], chuge[i]), *q = p;
		long m = csizes[i] > (4 << 20) ? 40 : 400;
		for (unsigned j = 0; j < sizeof chase_sched / sizeof chase_sched[0]; j++) {{
			unsigned long t = bestc(chase_sched[j].f, m, &q),
				      t0 = bestc(chase_sched[j].f, m / 2, &q);
			printf("chsched %zu%s %d %.3f\\n", csizes[i], chuge[i] ? "" : "-4k",
			       chase_sched[j].gap,
			       (double)(t - t0) / ((double)(m - m / 2) * {SCHED_STEPS}));
		}}
		free(p);
	}}
	for (unsigned i = 0; i < sizeof ctl / sizeof ctl[0]; i++) {{
		unsigned long t = best(ctl[i].f, n / 4, cell);
		printf("ctl %s %d %.3f\\n", ctl[i].kind, ctl[i].d,
		       (double)t / ((double)(n / 4) * ctl[i].blocks));
	}}
	for (unsigned i = 0; i < sizeof hz / sizeof hz[0]; i++)
		printf("hz %s %d %ld\\n", hz[i].name, hz[i].d, hz[i].f(0, cell));
	return 0;
}}
""")


def find_lcc(target):
    opt = Path(f"/opt/mcst/lcc-{LCC_VERSION}.e2k-{target}.linux-6.1")
    if not opt.is_dir():
        sys.exit(f"no lcc for target {target}: {opt} does not exist")
    return opt / "bin" / "lcc", opt / "fs"


def run(args, prog, sysroot):
    if args.remote:
        ssh = ["ssh", "-o", "BatchMode=yes"]
        if args.ssh_key:
            ssh += ["-i", str(args.ssh_key), "-o", "IdentitiesOnly=yes"]
        rdir = "relcc2/isa-timing"
        subprocess.run(ssh + [args.remote, f"mkdir -p {rdir}"], check=True)
        subprocess.run(["scp", "-q"] + ssh[1:] + [str(prog), f"{args.remote}:{rdir}/"],
                       check=True)
        wrap = ""
        if args.numa:
            cpu, mem = args.numa.split(":")
            wrap = f"numactl --cpunodebind={cpu} --preferred={mem} "
        cmd = ssh + [args.remote, f"{wrap}{rdir}/{prog.name}"]
    else:
        isa = re.match(r"v(\d+)", args.target).group(1)
        cmd = ["qemu-e2k", "-cpu", f"elbrus-v{isa}", "-L", str(sysroot), str(prog)]
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def knee(points):
    """Latency from per-block times at distances 1..N. For large D the time
    is D + c (no stall); the latency is the smallest D at which the time
    is already D + c, with c taken from the largest D."""
    dmax, tmax = points[-1]
    c = tmax - dmax
    for d, t in points:
        if t <= d + c + 0.3:
            return d
    return None


def parse(text):
    res = {"lat": {}, "sched": {}, "chase": {}, "chsched": {}, "ctl": {}, "hz": {}}
    for line in text.splitlines():
        f = line.split()
        if f[0] == "lat":
            res["lat"][f[1]] = float(f[2])
        elif f[0] == "sched":
            res["sched"].setdefault(f[1], []).append((int(f[2]), int(f[3]), float(f[4])))
        elif f[0] == "chsched":
            res["chsched"].setdefault(f[1], []).append((int(f[2]), float(f[3])))
        elif f[0] == "chase":
            res["chase"][int(f[1])] = float(f[2])
        elif f[0] == "ctl":
            res["ctl"].setdefault(f[1], []).append((int(f[2]), float(f[3])))
        elif f[0] == "hz":
            res["hz"].setdefault(f[1], []).append((int(f[2]), int(f[3])))
    return res


def sched_latency(pts):
    """Smallest gap at which a step costs exactly gap + fixed cycles."""
    # The loop's own overhead adds a few tenths of a cycle per step.
    for gap, fixed, t in pts:
        if t <= gap + fixed + 0.45:
            return gap
    return None


def chase_latency(pts):
    """Smallest gap at which a chased load no longer stalls; memory times
    vary, so allow 2% plus the loop overhead."""
    for gap, t in pts:
        if t <= gap * 1.02 + 0.6:
            return gap
    return None


def report(res):
    print("Back-to-back cost (cycles per chain step, stalling):")
    for name, v in res["lat"].items():
        print(f"  {name:14} {v:6.2f}  {LAT[name][2]}")
    print("Scheduled latency (smallest distance without a stall):")
    for name, pts in res["sched"].items():
        print(f"  {name:15} {sched_latency(pts)!s:>3}   " +
              " ".join(f"{g}:{t:.1f}" for g, _, t in pts))
    if res["chsched"]:
        print("Pointer chase with a gap after each load (scheduled latency):")
        for size, pts in res["chsched"].items():
            kib = f"{int(size.split('-')[0]) >> 10} KiB" + (" 4k" if "-4k" in size else "")
            print(f"  {kib:>13}  latency {chase_latency(pts)}   " +
                  " ".join(f"{g}:{t:.1f}" for g, t in pts))
    print("Pointer chase (cycles per load):")
    for size, v in res["chase"].items():
        print(f"  {size >> 10:8} KiB {v:7.2f}")
    print("Control transfer: cycles per block at prepare distance D")
    for kind, pts in res["ctl"].items():
        print(f"  {kind:7} " + " ".join(f"{d}:{t:.1f}" for d, t in pts)
              + f"   -> latency {knee(pts)}")
    print("Hazards: 1 = new value seen, 0 = old value (by distance D)")
    for name, pts in res["hz"].items():
        print(f"  {name:13} " + " ".join(f"{d}:{v:#x}" if v > 1 else f"{d}:{v}"
                                         for d, v in pts) + f"   {HZ[name]}")


def write_yaml(path, res, args):
    lines = ["# SPDX-License-Identifier: CC-BY-SA-4.0",
             "# Generated by tools/isa-timing/bench.py; see spec/isa/timing.md.",
             f"measured_on: {args.machine!r}",
             f"lcc_target: {args.target}",
             f"numa: {args.numa or 'not pinned'}",
             f"date: {datetime.date.today().isoformat()}",
             "back_to_back:"]
    for name, v in res["lat"].items():
        lines.append(f"  {name}: {{cycles_per_step: {v:.2f}, ops_per_step: "
                     f"{len(LAT[name][1])}, what: {LAT[name][2]!r}}}")
    lines.append("scheduled_latency:")
    for name, pts in res["sched"].items():
        lines.append(f"  {name}:")
        lines.append(f"    latency: {sched_latency(pts)}")
        lines.append("    cycles_per_step_by_gap: {" +
                     ", ".join(f"{g}: {t:.2f}" for g, _, t in pts) + "}")
    lines.append("pointer_chase_cycles:")
    for size, v in res["chase"].items():
        lines.append(f"  {size}: {v:.2f}")
    lines.append("pointer_chase_scheduled:")
    for size, pts in res["chsched"].items():
        lines.append(f"  {size!r}:")
        lines.append(f"    latency: {chase_latency(pts)}")
        lines.append("    cycles_per_load_by_gap: {" +
                     ", ".join(f"{g}: {t:.2f}" for g, t in pts) + "}")
    lines.append("control_prepare:")
    for kind, pts in res["ctl"].items():
        lines.append(f"  {kind}:")
        lines.append(f"    latency: {knee(pts)}")
        lines.append("    cycles_per_block: {" +
                     ", ".join(f"{d}: {t:.2f}" for d, t in pts) + "}")
    lines.append("hazards:")
    for name, pts in res["hz"].items():
        lines.append(f"  {name}:")
        lines.append(f"    what: {HZ[name]!r}")
        lines.append("    by_distance: {" +
                     ", ".join(f"{d}: {v:#x}" if v > 1 else f"{d}: {v}"
                               for d, v in pts) + "}")
    Path(path).write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--target", default="v3")
    ap.add_argument("--remote", metavar="[USER@]HOST")
    ap.add_argument("--ssh-key", type=Path,
                    default=SSH_KEY if SSH_KEY.exists() else None)
    ap.add_argument("--out", type=Path, default=ROOT / "toolchains" / "isa-timing")
    ap.add_argument("--yaml", type=Path)
    ap.add_argument("--memory", action="store_true",
                    help="also measure load latency beyond L1 (pointer chases "
                    "over 256 KiB..128 MiB; informational, slow)")
    ap.add_argument("--numa", metavar="CPU:MEM",
                    help="with --remote: run on NUMA node CPU, allocating "
                    "on node MEM (numactl --preferred)")
    ap.add_argument("--machine", default="unspecified",
                    help="description recorded in --yaml output")
    args = ap.parse_args()

    lcc, sysroot = find_lcc(args.target)
    generate(args.out, args.memory)
    prog = args.out / "bench"
    subprocess.run([str(lcc), "-O1", str(args.out / "main.c"),
                    str(args.out / "kernels.s"), "-o", str(prog)], check=True)
    res = parse(run(args, prog, sysroot))
    report(res)
    if args.yaml:
        write_yaml(args.yaml, res, args)


if __name__ == "__main__":
    main()
