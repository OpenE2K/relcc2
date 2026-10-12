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
  tp     Throughput: K wide instructions, each a group of independent
         operations (constant inputs, rotating destinations), back to
         back. cycles per instruction = (T(K2) - T(K1)) / (iterations *
         (K2 - K1)); 1.0 means every channel used takes a new
         operation each cycle.
  hz     Hazards: does an operation D cycles (0 = same wide instruction)
         after a producer see the new value or the old one? Old values
         mean the dependency is not interlocked.
  v3op   The v3 sweep, printed as sched and tp lines: a scheduled
         latency for every v3 computational operation without one
         (v3op_*, generated from binutils-ops.yaml), divider sharing of
         the remaining ALC5 operations (v3div_*), state-register costs
         (v3st_*) and wait costs (v3wait_*). `--only v3op` runs it
         alone; a full run leaves it out (about 9,000 kernels). The
         timing YAML files include it: run the built bench binary on the
         host once without an argument and once with `v3op`, join the
         two outputs and pass them to --results with --yaml. To
         re-measure some of its chains, run `--only v3op --chains REGEX
         --save FILE` and put FILE's lines in place of theirs in the
         joined output.

Usage:
  bench.py [--target T] [--remote [USER@]HOST] [--ssh-key FILE] [--ssh-port N]
           [--remote-dir DIR] [--out DIR] [--yaml FILE] [--memory]
           [--numa CPU:MEM] [--wrap CMD] [--only KIND,...] [--chains REGEX]
           [--save FILE] [--results FILE]

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
sys.path.insert(0, str(ROOT / "tools" / "setup"))
import sshkey  # noqa: E402  (never connect without relcc2's key)

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
    "fxsqrt_chain": ("fdtofx,0 %dr4, %dr2",
                     ["fxsqrtixx,5 %dr2, %dr7",
                      "fxsqrtuxx,5 %dr2, %dr7, %dr7",
                      "fxsqrttxx,5 %dr2, %dr7, %dr2"],
                     "lcc's 80-bit sqrt: fxsqrti, fxsqrtu, fxsqrtt"),
    "fsqrt_chain": ("%dr4", ["fsqrtid,5 %dr2, %dr7",
                             "fsqrttd,5 %dr2, %dr7, %dr2"],
                    "double sqrt: fsqrtid, fsqrttd"),
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
    # Narrow loads -> integer use: the loaded value (a zero at cell + 8)
    # is added to the pointer, so the chain runs load -> add -> address.
    "ldb_to_addd": ("%dr1", [("ldb,0 %dr2, 0x8, %dr7", None),
                             ("addd,0 %dr2, %dr7, %dr2", 12)], 8),
    "ldh_to_addd": ("%dr1", [("ldh,0 %dr2, 0x8, %dr7", None),
                             ("addd,0 %dr2, %dr7, %dr2", 12)], 8),
    "ldw_to_addd": ("%dr1", [("ldw,0 %dr2, 0x8, %dr7", None),
                             ("addd,0 %dr2, %dr7, %dr2", 12)], 8),
    "ldd8_to_addd": ("%dr1", [("ldd,0 %dr2, 0x8, %dr7", None),
                              ("addd,0 %dr2, %dr7, %dr2", 12)], 8),
    # 32-bit multiply and bit-field extract feeding 32-bit adds.
    "muls_to_adds": ("%dr3", [("muls,0 %r2, %r3, %r2", None),
                              ("adds,0 %r2, %r5, %r2", 12)], 10),
    "getfs_to_adds": ("%dr3", [("getfs,0 %r2, 0x200, %r2", None),
                               ("adds,0 %r2, %r5, %r2", 12)], 6),
    "ldb_to_getfs": ("%dr1", [("ldb,0 %dr2, 0x8, %dr7", None),
                              ("getfs,0 %r7, 0x200, %r7", 12),
                              ("addd,0 %dr2, %dr7, %dr2", 12)], 8),
    # Write-back conflicts: an unrelated op 2 cycles after an FP-side
    # producer whose result goes to an integer op.  The reported latency
    # is from that op to the consumer, so the producer -> consumer
    # distance is 2 more.  An integer op or load in the producer's
    # channel (_p0, _pl) delays the result a cycle; one in another
    # channel (_p1) or an FP op (_pf) does not.
    "muld_to_addd_p0": ("%dr3", [("muld,0 %dr2, %dr3, %dr2", 2),
                                 ("addd,0 %dr5, %dr5, %dr7", None),
                                 ("addd,1 %dr2, %dr5, %dr2", 12)], 8),
    "muld_to_addd_p1": ("%dr3", [("muld,0 %dr2, %dr3, %dr2", 2),
                                 ("addd,1 %dr5, %dr5, %dr7", None),
                                 ("addd,1 %dr2, %dr5, %dr2", 12)], 8),
    "muld_to_addd_pl": ("%dr3", [("muld,0 %dr2, %dr3, %dr2", 2),
                                 ("ldd,0 %dr1, 0x8, %dr7", None),
                                 ("addd,1 %dr2, %dr5, %dr2", 12)], 8),
    "muld_to_addd_pf": ("%dr3", [("muld,0 %dr2, %dr3, %dr2", 2),
                                 ("faddd,0 %dr5, %dr5, %dr7", None),
                                 ("addd,1 %dr2, %dr5, %dr2", 12)], 8),
    "faddd_to_addd_p0": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", 2),
                                  ("addd,0 %dr5, %dr5, %dr7", None),
                                  ("addd,1 %dr2, %dr5, %dr2", 12)], 8),
    # The same conflict with an FP consumer (PoC finding I19): fmuld,0
    # feeding faddd in the other cluster (ALC3) or the same (ALC1), with
    # an addd,0 2 cycles after the fmuld, or an addd,2 (no conflict).
    "fmuld_to_faddd_c1": ("%dr4", [("fmuld,0 %dr2, %dr4, %dr2", 2),
                                   ("addd,2 %dr5, %dr5, %dr7", None),
                                   ("faddd,1 %dr2, %dr5, %dr2", 12)], 8),
    "fmuld_to_faddd_c1_p0": ("%dr4", [("fmuld,0 %dr2, %dr4, %dr2", 2),
                                      ("addd,0 %dr5, %dr5, %dr7", None),
                                      ("faddd,1 %dr2, %dr5, %dr2", 12)], 8),
    "fmuld_to_faddd_x": ("%dr4", [("fmuld,0 %dr2, %dr4, %dr2", 2),
                                  ("addd,2 %dr5, %dr5, %dr7", None),
                                  ("faddd,3 %dr2, %dr5, %dr2", 12)], 8),
    "fmuld_to_faddd_x_p0": ("%dr4", [("fmuld,0 %dr2, %dr4, %dr2", 2),
                                     ("addd,0 %dr5, %dr5, %dr7", None),
                                     ("faddd,3 %dr2, %dr5, %dr2", 12)], 8),
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
    "paddw_to_paddw_x": ("%dr3", [("paddw,0 %dr2, %dr5, %dr2", None),
                                  ("paddw,3 %dr2, %dr5, %dr2", 12)], 6),
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
    # FP min/max (x86-style; semantics.md).
    "fmaxd": ("%dr4", [("fmaxd,0 %dr2, %dr5, %dr2", None)], 8),
    "fmind": ("%dr4", [("fmind,0 %dr2, %dr4, %dr2", None)], 8),
    "pfmaxd": ("%dr4", [("pfmaxd,0 %dr2, %dr5, %dr2", None)], 8),
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
    # Other operations a C compiler emits.
    "sxt": ("%dr3", [("sxt,0 0x2, %dr2, %dr2", None)], 4),
    "merged": ("%dr3", [("merged,0 %dr2, %dr5, %dr2, %pred0", None)], 4),
    "popcntd": ("%dr3", [("popcntd,1 %dr2, %dr2", None)], 6),
    "lzcntd": ("%dr3", [("lzcntd,1 %dr2, %dr2", None)], 6),
    "bitrevd": ("%dr3", [("bitrevd,0 %dr2, %dr2", None)], 6),
    "umulhd": ("%dr3", [("umulhd,0 %dr2, %dr3, %dr2", None)], 8),
    # 80-bit operations; the chain value is made a real 80-bit 1.0 first
    # (an init with a space is an operation writing %dr2).
    "fxaddxx": ("fdtofx,0 %dr4, %dr2", [("fxaddxx,0 %dr2, %dr5, %dr2", None)], 10),
    "fxmulxx": ("fdtofx,0 %dr4, %dr2", [("fxmulxx,0 %dr2, %dr2, %dr2", None)], 10),
    "fxdivxx": ("fdtofx,0 %dr4, %dr2", [("fxdivxx,5 %dr2, %dr2, %dr2", None)], 24),
    "movfi_movif": ("fdtofx,0 %dr4, %dr2", [("movfi,1 %dr2, %r6", None),
                                            ("movif,1 %dr2, %r6, %dr2", 12)], 8),
    "fxaddxx_to_addd": ("fdtofx,0 %dr4, %dr2", [("fxaddxx,0 %dr2, %dr5, %dr2", None),
                                                ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
    "fdtofx_fxtofd": ("%dr4", [("fdtofx,0 %dr2, %dr2", None),
                               ("fxtofd,0 %dr2, %dr2", 12)], 8),
    # The rest of the v3 user-mode operations (semantics.md, "Other
    # scalar operations", "80-bit operations"); the scanned operation is
    # the one with gap None, the others bring the value back.
    "smulx": ("%dr3", [("smulx,0 %r2, %r3, %dr2", None)], 8),
    "umulx": ("%dr3", [("umulx,0 %r2, %r3, %dr2", None)], 8),
    "smulx_to_addd": ("%dr3", [("smulx,0 %r2, %r3, %dr2", None),
                               ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
    "insfs": ("%dr3", [("insfs,0 %r2, %r5, %r5, %r2", None)], 6),
    "insfd": ("%dr3", [("insfd,0 %dr2, %dr5, %dr5, %dr2", None)], 6),
    "insfd_to_addd_x": ("%dr3", [("insfd,0 %dr2, %dr5, %dr5, %dr2", None),
                                 ("addd,3 %dr2, %dr5, %dr2", 12)], 6),
    "sdivx": ("%dr3", [("sdivx,5 %dr2, %r3, %r2", None)], 24),
    "udivx": ("%dr3", [("udivx,5 %dr2, %r3, %r2", None)], 24),
    "smodx": ("%dr3", [("smodx,5 %dr2, %r3, %r2", None)], 24),
    "umodx": ("%dr3", [("umodx,5 %dr2, %r3, %r2", None)], 24),
    "mpsadbh": ("%dr3", [("mpsadbh,1 %dr2, %dr5, %dr2", None)], 8),
    "mpsadbh_to_addd": ("%dr3", [("mpsadbh,1 %dr2, %dr5, %dr2", None),
                                 ("addd,1 %dr2, %dr5, %dr2", 12)], 10),
    "fsubd": ("%dr4", [("fsubd,0 %dr2, %dr5, %dr2", None)], 8),
    "fsubs": ("%dr6", [("fsubs,0 %r2, %r5, %r2", None)], 8),
    "fmaxs": ("%dr6", [("fmaxs,0 %r2, %r5, %r2", None)], 8),
    "fdtoifd": ("%dr4", [("fdtoifd,0 %dr5, %dr2, %dr2", None)], 8),
    "fstoifs": ("%dr6", [("fstoifs,0 %r5, %r2, %r2", None)], 8),
    "fstoid": ("%dr6", [("fstoid,0 %r2, %dr2", None),
                        ("idtofd,0 %dr2, %dr2", 12),
                        ("fdtofs,0 %dr2, %r2", 12)], 8),
    "fstoidtr": ("%dr6", [("fstoidtr,0 %r2, %dr2", None),
                          ("idtofd,0 %dr2, %dr2", 12),
                          ("fdtofs,0 %dr2, %r2", 12)], 8),
    "fdtoidtr": ("%dr4", [("fdtoidtr,0 %dr2, %dr2", None),
                          ("idtofd,0 %dr2, %dr2", 12)], 8),
    "fdtoistr": ("%dr4", [("fdtoistr,0 %dr2, %r2", None),
                          ("istofd,0 %r2, %dr2", 12)], 8),
    "istofd": ("%dr3", [("istofd,0 %r2, %dr2", None),
                        ("fdtoid,0 %dr2, %dr2", 12)], 8),
    "istofs": ("%dr3", [("istofs,0 %r2, %r2", None),
                        ("fstofd,0 %r2, %dr2", 12),
                        ("fdtoid,0 %dr2, %dr2", 12)], 8),
    "idtofs": ("%dr3", [("idtofs,0 %dr2, %r2", None),
                        ("fstofd,0 %r2, %dr2", 12),
                        ("fdtoid,0 %dr2, %dr2", 12)], 8),
    # The conversions above in round trips with valid values (the
    # fdtoid / idtofd chains feed an integer back as a double).
    "idtofd_to_fdtoid": ("%dr3", [("idtofd,0 %dr2, %dr2", None),
                                  ("fdtoid,0 %dr2, %dr2", 12)], 8),
    "fdtoid_to_idtofd": ("%dr4", [("fdtoid,0 %dr2, %dr2", None),
                                  ("idtofd,0 %dr2, %dr2", 12)], 8),
    "fstofd_to_fdtofs": ("%dr6", [("fstofd,0 %r2, %dr2", None),
                                  ("fdtofs,0 %dr2, %r2", 12)], 8),
    "fdtofs_to_fstofd": ("%dr4", [("fdtofs,0 %dr2, %r2", None),
                                  ("fstofd,0 %r2, %dr2", 12)], 8),
    # A result read in another format than it was produced in (a
    # double as a single, an integer as a double, ...).
    "faddd_to_fadds": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                                ("fadds,0 %r2, %r5, %r2", 12)], 8),
    "fadds_to_faddd": ("%dr6", [("fadds,0 %r2, %r5, %r2", None),
                                ("faddd,0 %dr2, %dr5, %dr2", 12)], 8),
    "fadds_to_fadds": ("%dr6", [("fadds,0 %r2, %r5, %r2", None),
                                ("fadds,0 %r2, %r5, %r2", 12)], 8),
    "faddd_to_fdtoid": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                                 ("fdtoid,0 %dr2, %dr2", 12)], 8),
    "fadds_to_fdtoid": ("%dr6", [("fadds,0 %r2, %r5, %r2", None),
                                 ("fdtoid,0 %dr2, %dr2", 12)], 8),
    "fdtoid_to_faddd": ("%dr4", [("fdtoid,0 %dr2, %dr2", None),
                                 ("faddd,0 %dr2, %dr5, %dr2", 12)], 8),
    "idtofd_to_faddd": ("%dr3", [("idtofd,0 %dr2, %dr2", None),
                                 ("faddd,0 %dr2, %dr5, %dr2", 12)], 8),
    "fstofd_to_fadds": ("%dr6", [("fstofd,0 %r2, %dr2", None),
                                 ("fadds,0 %r2, %r5, %r2", 12)], 8),
    "fdtofs_to_faddd": ("%dr4", [("fdtofs,0 %dr2, %r2", None),
                                 ("faddd,0 %dr2, %dr5, %dr2", 12)], 8),
    "fdtofs_to_fadds": ("%dr4", [("fdtofs,0 %dr2, %r2", None),
                                 ("fadds,0 %r2, %r5, %r2", 12)], 8),
    "fmuld_to_pfmuls": ("%dr4", [("fmuld,0 %dr2, %dr4, %dr2", None),
                                 ("pfmuls,0 %dr2, %dr4, %dr2", 12)], 8),
    "fmuld_to_fmuls": ("%dr4", [("fmuld,0 %dr2, %dr4, %dr2", None),
                                ("fmuls,0 %r2, %r6, %r2", 12)], 8),
    # x86 flag forms (their result is a flags word) and cctop, which
    # tests one into a predicate; cmpesb is the reference predicate.
    "adds_fw": ("%dr3", [("adds_fw,0 %r2, %r5, %r2", None)], 6),
    "addd_fd": ("%dr3", [("addd_fd,0 %dr2, %dr5, %r2", None)], 6),
    "adds_fw_to_adds": ("%dr3", [("adds_fw,0 %r2, %r5, %r2", None),
                                 ("adds,0 %r2, %r5, %r2", 12)], 6),
    "shls_fw": ("%dr3", [("shls_fw,0 %r2, %r5, %r5, %r2", None)], 6),
    "incs_fw": ("%dr3", [("incs_fw,0 %r2, %r5, %r5, %r2", None)], 6),
    "smulx_fw": ("%dr3", [("smulx_fw,0 %r2, %r3, %r2", None)], 8),
    "subs_fw": ("%dr3", [("subs_fw,0 %r2, %r5, %r2", None)], 6),
    "ands_fw": ("%dr3", [("ands_fw,0 %r2, %r3, %r2", None)], 6),
    "umulx_fw": ("%dr3", [("umulx_fw,0 %r2, %r3, %r2", None)], 8),
    "subd_fd": ("%dr3", [("subd_fd,0 %dr2, %dr5, %r2", None)], 6),
    "shld_fd": ("%dr3", [("shld_fd,0 %dr2, %dr5, %r5, %r2", None)], 6),
    "sard_fd": ("%dr3", [("sard_fd,0 %dr2, %dr5, %r5, %r2", None)], 6),
    "incd_fd": ("%dr3", [("incd_fd,0 %dr2, %dr5, %r5, %r2", None)], 6),
    "adds_fw_3": ("%dr3", [("adds_fw,3 %r2, %r5, %r2", None)], 6),
    # The flags of an add tested by cctopo, the predicate selecting the
    # next step's input: the scanned gap is adds_fw -> cctopo.
    "adds_fw_to_cctopo": ("%dr3", [("adds_fw,0 %r2, %r5, %r7", None),
                                   ("cctopo,0 %r7, %pred1", 12),
                                   ("merged,0 %dr5, %dr2, %dr2, %pred1", 12)],
                          8),
    "cctopb_to_merged": ("%dr3", [("cctopb,0 %r2, %pred1", None),
                                  ("merged,0 %dr5, %dr2, %dr2, %pred1", 12)],
                         8),
    "cctope_to_merged": ("%dr3", [("cctope,3 %r2, %pred1", None),
                                  ("merged,3 %dr5, %dr2, %dr2, %pred1", 12)],
                         8),
    "cctopl_to_merged": ("%dr3", [("cctopl,0 %r2, %pred1", None),
                                  ("merged,0 %dr5, %dr2, %dr2, %pred1", 12)],
                         8),
    "cctopo_to_merged": ("%dr3", [("cctopo,0 %r2, %pred1", None),
                                  ("merged,0 %dr5, %dr2, %dr2, %pred1", 12)],
                         8),
    "cmpesb_to_merged": ("%dr3", [("cmpesb,0 %r2, %r5, %pred1", None),
                                  ("merged,0 %dr5, %dr2, %dr2, %pred1", 12)],
                         8),
    # 80-bit operations with a narrower operand or result, conversions
    # and lcc's square root chain (fxsqrti, fxsqrtu, fxsqrtt).
    "fxadddd": ("fdtofx,0 %dr4, %dr2", [("fxadddd,0 %dr2, %dr5, %dr2", None),
                                        ("fdtofx,0 %dr2, %dr2", 12)], 10),
    "fxmuldd": ("fdtofx,0 %dr4, %dr2", [("fxmuldd,0 %dr2, %dr4, %dr2", None),
                                        ("fdtofx,0 %dr2, %dr2", 12)], 10),
    "fxmulxd": ("fdtofx,0 %dr4, %dr2", [("fxmulxd,0 %dr2, %dr2, %dr2", None),
                                        ("fdtofx,0 %dr2, %dr2", 12)], 10),
    "fxdivdd": ("fdtofx,0 %dr4, %dr2", [("fxdivdd,5 %dr2, %dr4, %dr2", None),
                                        ("fdtofx,0 %dr2, %dr2", 12)], 24),
    "fxtoid": ("fdtofx,0 %dr4, %dr2", [("fxtoid,0 %dr2, %dr2", None),
                                       ("idtofx,0 %dr2, %dr2", 12)], 10),
    "idtofx": ("%dr3", [("idtofx,0 %dr2, %dr2", None),
                        ("fxtoid,0 %dr2, %dr2", 12)], 10),
    "fstofx": ("%dr6", [("fstofx,0 %r2, %dr2", None),
                        ("fxtofs,0 %dr2, %r2", 12)], 10),
    "fxsqrtixx": ("fdtofx,0 %dr4, %dr2", [("fxsqrtixx,5 %dr2, %dr7", None),
                                          ("fxsqrtuxx,5 %dr2, %dr7, %dr7", 24),
                                          ("fxsqrttxx,5 %dr2, %dr7, %dr2", 32)],
                  24),
    "fxsqrtuxx": ("fdtofx,0 %dr4, %dr2", [("fxsqrtixx,5 %dr2, %dr7", 24),
                                          ("fxsqrtuxx,5 %dr2, %dr7, %dr7", None),
                                          ("fxsqrttxx,5 %dr2, %dr7, %dr2", 32)],
                  24),
    "fxsqrttxx": ("fdtofx,0 %dr4, %dr2", [("fxsqrtixx,5 %dr2, %dr7", 24),
                                          ("fxsqrtuxx,5 %dr2, %dr7, %dr7", 24),
                                          ("fxsqrttxx,5 %dr2, %dr7, %dr2", None)],
                  32),
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
CTL_KINDS = ["disp", "movtd", "cmp_ct", "cmp_alu", "cmp_andp", "andp_alu",
             "andp_ct", "andp_andp", "fcmp_alu", "fcmp_ct"]


# Throughput: name: (operations of one wide instruction, description).
# "{d}" is replaced by a destination register, a different one for
# every operation of 9 or more consecutive instructions, so no result is
# overwritten while in flight. Inputs are constants (INIT, %dr1 = cell),
# so there are no dependences.
def _ch(fmt, chans):
    return [fmt.format(c=c, o=8 * c, o2=16 * c, d="{d}", s="{s}")
            for c in chans]


TP = {
    "addd_1": (_ch("addd,{c} %dr3, %dr5, {d}", [0]), "1 addd"),
    "addd_6": (_ch("addd,{c} %dr3, %dr5, {d}", range(6)), "6 addd"),
    "muld_4": (_ch("muld,{c} %dr3, %dr3, {d}", [0, 1, 3, 4]), "4 muld"),
    "faddd_1": (_ch("faddd,{c} %dr4, %dr5, {d}", [0]), "1 faddd"),
    "faddd_4": (_ch("faddd,{c} %dr4, %dr5, {d}", [0, 1, 3, 4]), "4 faddd"),
    "fmuld_4": (_ch("fmuld,{c} %dr4, %dr4, {d}", [0, 1, 3, 4]), "4 fmuld"),
    "fmul_addd_4": (_ch("fmul_addd,{c} %dr4, %dr4, %dr5, {d}", [0, 1, 3, 4]),
                    "4 fmul_addd"),
    "fadds_4": (_ch("fadds,{c} %r6, %r5, {d}", [0, 1, 3, 4]), "4 fadds"),
    "faddd4_addd2": (_ch("faddd,{c} %dr4, %dr5, {d}", [0, 1, 3, 4])
                     + _ch("addd,{c} %dr3, %dr5, {d}", [2, 5]),
                     "4 faddd + 2 addd"),
    "fdtoid_4": (_ch("fdtoid,{c} %dr4, {d}", [0, 1, 3, 4]), "4 fdtoid"),
    "paddw_2": (_ch("paddw,{c} %dr3, %dr5, {d}", [0, 3]), "2 paddw"),
    "popcntd_2": (_ch("popcntd,{c} %dr3, {d}", [1, 4]), "2 popcntd"),
    # Combined integer operations: two simple operations in one channel
    # (ALC1/4 only), alone and with four ordinary ones: 8 simple
    # operations in one instruction if they all issue every cycle.
    "add_addd_2": (_ch("add_addd,{c} %dr3, %dr5, %dr3, {d}", [1, 4]),
                   "2 add_addd"),
    "add_addd2_addd4": (_ch("add_addd,{c} %dr3, %dr5, %dr3, {d}", [1, 4])
                        + _ch("addd,{c} %dr3, %dr5, {d}", [0, 2, 3, 5]),
                        "2 add_addd + 4 addd"),
    "shl_addd2_addd4": (_ch("shl_addd,{c} %dr3, %dr5, %dr3, {d}", [1, 4])
                        + _ch("addd,{c} %dr3, %dr5, {d}", [0, 2, 3, 5]),
                        "2 shl_addd + 4 addd"),
    "ldd_1": (_ch("ldd,{c} %dr1, {o:#x}, {d}", [0]), "1 ldd"),
    "ldd_2": (_ch("ldd,{c} %dr1, {o:#x}, {d}", [0, 3]), "2 ldd (ALC0, 3)"),
    "ldd_4": (_ch("ldd,{c} %dr1, {o:#x}, {d}", [0, 2, 3, 5]), "4 ldd"),
    "ldd4_addd2": (_ch("ldd,{c} %dr1, {o:#x}, {d}", [0, 2, 3, 5])
                   + _ch("addd,{c} %dr3, %dr5, {d}", [1, 4]),
                   "4 ldd + 2 addd"),
    "ldw_4": (_ch("ldw,{c} %dr1, {o:#x}, {d}", [0, 2, 3, 5]), "4 ldw"),
    "std_1": (_ch("std,{c} %dr1, {o:#x}, %dr3", [2]), "1 std"),
    "std_2": (_ch("std,{c} %dr1, {o:#x}, %dr3", [2, 5]), "2 std"),
    "std2_addd4": (_ch("std,{c} %dr1, {o:#x}, %dr3", [2, 5])
                   + _ch("addd,{c} %dr3, %dr5, {d}", [0, 1, 3, 4]),
                   "2 std + 4 addd"),
    # Two stores: same line (above), other lines, the same address.
    "std_2_8": (["std,2 %dr1, 0x100, %dr3", "std,5 %dr1, 0x108, %dr3"],
                "2 std, 8 bytes apart"),
    "std_2_40": (["std,2 %dr1, 0x100, %dr3", "std,5 %dr1, 0x140, %dr3"],
                 "2 std, 64 bytes apart"),
    "std_2_100": (["std,2 %dr1, 0x100, %dr3", "std,5 %dr1, 0x200, %dr3"],
                  "2 std, 256 bytes apart"),
    "std_2_0": (["std,2 %dr1, 0x100, %dr3", "std,5 %dr1, 0x100, %dr3"],
                "2 std, same address"),
    "stw_2": (["stw,2 %dr1, 0x100, %r3", "stw,5 %dr1, 0x140, %r3"],
              "2 stw, 64 bytes apart"),
    "std_1_5": (["std,5 %dr1, 0x100, %dr3"], "1 std in ALC5"),
    "std1_ldd2": (_ch("std,{c} %dr1, {o:#x}, %dr3", [2])
                  + _ch("ldd,{c} %dr1, {o:#x}, {d}", [0, 5]),
                  "1 std + 2 ldd"),
    "fdivd": (["fdivd,5 %dr4, %dr4, {d}"], "fdivd"),
    "fdivs": (["fdivs,5 %r6, %r6, {d}"], "fdivs"),
    "fsqrts": (["fsqrts,5 %r6, {d}"], "fsqrts"),
    "sdivd": (["sdivd,5 %dr3, %dr3, {d}"], "sdivd"),
    "sdivs": (["sdivs,5 %r3, %r3, {d}"], "sdivs"),
}
# Pairs of instructions alternating, to see whether two kinds of
# operation share a unit: name: ([ops of instruction A], [ops of B]).
TP2 = {
    "fdivd_sdivd": (["fdivd,5 %dr4, %dr4, {d}"], ["sdivd,5 %dr3, %dr3, {d}"],
                    "fdivd, sdivd alternating"),
    "fdivd_fsqrts": (["fdivd,5 %dr4, %dr4, {d}"], ["fsqrts,5 %r6, {d}"],
                     "fdivd, fsqrts alternating"),
    "alt_st_ld": (["std,2 %dr1, 0x100, %dr3"], ["ldd,0 %dr1, 0x108, {d}"],
                  "std, then ldd in the same 32 bytes"),
    "alt_st_ldfar": (["std,2 %dr1, 0x100, %dr3"], ["ldd,0 %dr1, 0x200, {d}"],
                     "std, then ldd elsewhere"),
    "alt_stld_nop": (["std,2 %dr1, 0x110, %dr3", "ldd,0 %dr1, 0x100, {d}"], [],
                     "std + ldd in the same 32 bytes every other cycle"),
    "alt_st_st": (["std,2 %dr1, 0x100, %dr3"], ["std,5 %dr1, 0x108, %dr3"],
                  "std, then std in the same bank"),
    "fdivd_nop": (["fdivd,5 %dr4, %dr4, {d}"], [],
                  "fdivd every other cycle"),
    "sdivd_nop": (["sdivd,5 %dr3, %dr3, {d}"], [],
                  "sdivd every other cycle"),
}
# Dividers and stores at a distance: one operation every G cycles.
for _n, _op in (("fdivd", "fdivd,5 %dr4, %dr4, {d}"),
                ("fdivs", "fdivs,5 %r6, %r6, {d}"),
                ("fsqrts", "fsqrts,5 %r6, {d}"),
                ("sdivd", "sdivd,5 %dr3, %dr3, {d}"),
                ("sdivs", "sdivs,5 %r3, %r3, {d}")):
    for _g in range(2, 9):
        TP[f"{_n}_g{_g}"] = ([_op, f"nop {_g - 1}"], f"{_n} every {_g} cycles")
# Two memory accesses in one instruction, D bytes apart: two stores, a
# store and a load, two loads.
for _d in (0x8, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x80, 0xc0, 0x100,
           0x140, 0x180, 0x1c0, 0x200):
    TP[f"stst_{_d:x}"] = ([f"std,2 %dr1, 0x100, %dr3",
                           f"std,5 %dr1, {0x100 + _d:#x}, %dr3"],
                          f"std + std {_d} bytes apart")
    TP[f"stld_{_d:x}"] = ([f"std,2 %dr1, 0x100, %dr3",
                           f"ldd,0 %dr1, {0x100 + _d:#x}, {{d}}"],
                          f"std + ldd {_d} bytes apart")
    TP[f"ldld_{_d:x}"] = ([f"ldd,0 %dr1, 0x100, {{d}}",
                           f"ldd,3 %dr1, {0x100 + _d:#x}, {{d}}"],
                          f"ldd + ldd {_d} bytes apart")
# Which channels a store and loads in one instruction may use (far
# apart, so no bank conflict).
for _st, _lds in ((2, [0]), (2, [5]), (2, [0, 5]), (5, [2]), (5, [3]),
                  (5, [2, 3])):
    _ops = [f"std,{_st} %dr1, 0x100, %dr3"] + [
        f"ldd,{c} %dr1, {0x180 + 0x40 * i:#x}, {{d}}" for i, c in enumerate(_lds)]
    TP[f"st{_st}_ld{''.join(map(str, _lds))}"] = (
        _ops, f"std in ALC{_st} + ldd in ALC{', '.join(map(str, _lds))}")
# A store and a load in one instruction, each at one of the doublewords
# of a 32-byte block and the next (two blocks of the page).
for _b in (0x0, 0x100):
    for _s in range(0, 0x28, 8):
        for _l in range(0, 0x28, 8):
            TP[f"g_{_b + _s:x}_{_b + _l:x}"] = (
                [f"std,2 %dr1, {_b + _s:#x}, %dr3",
                 f"ldd,0 %dr1, {_b + _l:#x}, {{d}}"], "grid")
TP["stld_0"] = (["std,2 %dr1, 0x100, %dr3", "ldd,0 %dr1, 0x100, {d}"],
                "std + ldd same address")
# The pipelined 64-bit packed FP operations (two singles or one double
# in a 64-bit register), four per instruction in ALC0, 1, 3 and 4 (the
# only channels binutils gives them before v4).  %dr4 = 1.0 reads as
# the singles (0.0, 1.875), so no input is denormal; the rounding
# operand of pfdtoifd and pfstoifs is %dr5 = 0 (to nearest).
_PF4 = [0, 1, 3, 4]
TP_PF = {
    f"pf_{op}_4": (_ch(f"{op},{{c}} {args}, {{d}}", _PF4), f"4 {op}")
    for op, args in (
        ("pfaddd", "%dr4, %dr5"), ("pfadds", "%dr4, %dr4"),
        ("pfsubd", "%dr4, %dr5"), ("pfmuld", "%dr4, %dr4"),
        ("pfmuls", "%dr4, %dr4"), ("pfmaxd", "%dr4, %dr5"),
        ("pfmins", "%dr4, %dr4"), ("pfcmpeqd", "%dr4, %dr5"),
        ("pfcmplts", "%dr4, %dr4"), ("pfcmpuodd", "%dr4, %dr5"),
        ("pfhadds", "%dr4, %dr4"), ("pfaddsubs", "%dr4, %dr4"),
        ("pfmul_addd", "%dr4, %dr4, %dr5"), ("pfmul_adds", "%dr4, %dr4, %dr4"),
        ("pfadd_hadds", "%dr4, %dr4, %dr4"),
        ("pfdtoifd", "%dr5, %dr4"), ("pfstoifs", "%dr5, %dr4"),
        ("pfstois", "%dr4"), ("pfstofd", "%r6"), ("pistofs", "%dr3"))}
TP_PF.update({
    # Results of a single format go to %rN ({s}).
    "pf_pfdtois_4": (_ch("pfdtois,{c} %dr4, {s}", _PF4), "4 pfdtois"),
    "pf_pfdtofs_4": (_ch("pfdtofs,{c} %dr4, {s}", _PF4), "4 pfdtofs"),
    "pf_pfaddd_1": (_ch("pfaddd,{c} %dr4, %dr5, {d}", [0]), "1 pfaddd"),
    "pf_pfaddd_2": (_ch("pfaddd,{c} %dr4, %dr5, {d}", [0, 3]),
                    "2 pfaddd (ALC0, 3)"),
    "pf_pfaddd4_addd2": (_ch("pfaddd,{c} %dr4, %dr5, {d}", _PF4)
                         + _ch("addd,{c} %dr3, %dr5, {d}", [2, 5]),
                         "4 pfaddd + 2 addd"),
    "pf_pfmuls2_fmuld2": (_ch("pfmuls,{c} %dr4, %dr4, {d}", [0, 3])
                          + _ch("fmuld,{c} %dr4, %dr4, {d}", [1, 4]),
                          "2 pfmuls (ALC0, 3) + 2 fmuld (ALC1, 4)"),
    "pf_pfmul_addd4_ldd2": (_ch("pfmul_addd,{c} %dr4, %dr4, %dr5, {d}", _PF4)
                            + _ch("ldd,{c} %dr1, {o:#x}, {d}", [2, 5]),
                            "4 pfmul_addd + 2 ldd (ALC2, 5)"),
})
TP.update(TP_PF)
TP_DEST = range(8, 64)

# v5 and later (added when --target is v5+): FP operations in ALC2 and
# ALC5 (v4+ encoding), 128-bit packed operations on whole registers
# (%qprN), quad loads and stores.  %qpr7 holds (1.0, 1.0) for the
# throughput kernels; %qpr7 / %qpr8 hold (0.0, 0.0) / (1.0, 1.0) in the
# chains.
V5_TP_INIT = "qppackdl,0 %dr4, %dr4, %qpr7"
QP_INIT = ("qppackdl,0 %dr4, %dr4, %qpr2; qppackdl,1 %dr5, %dr5, %qpr7; "
           "qppackdl,3 %dr4, %dr4, %qpr8")
SCHED_V5 = {
    "faddd_c5": ("%dr4", [("faddd,5 %dr2, %dr5, %dr2", None)], 8),
    "faddd_c2": ("%dr4", [("faddd,2 %dr2, %dr5, %dr2", None)], 8),
    "fmuld_c5": ("%dr4", [("fmuld,5 %dr2, %dr4, %dr2", None)], 8),
    "fmul_addd_c5": ("%dr4", [("fmul_addd,5 %dr2, %dr4, %dr5, %dr2", None)], 12),
    "faddd_c5_to_addd": ("%dr4", [("faddd,5 %dr2, %dr5, %dr2", None),
                                  ("addd,5 %dr2, %dr5, %dr2", 12)], 10),
    "faddd_c5_to_faddd_c0": ("%dr4", [("faddd,5 %dr2, %dr5, %dr2", None),
                                      ("faddd,0 %dr2, %dr5, %dr2", 12)], 8),
    "faddd_c0_to_faddd_c5": ("%dr4", [("faddd,0 %dr2, %dr5, %dr2", None),
                                      ("faddd,5 %dr2, %dr5, %dr2", 12)], 8),
    "qpfaddd": (QP_INIT, [("qpfaddd,0 %qpr2, %qpr7, %qpr2", None)], 8),
    "qpfaddd_c5": (QP_INIT, [("qpfaddd,5 %qpr2, %qpr7, %qpr2", None)], 8),
    "qpfmuld": (QP_INIT, [("qpfmuld,0 %qpr2, %qpr8, %qpr2", None)], 8),
    "qpfmul_addd": (QP_INIT, [("qpfmul_addd,0 %qpr2, %qpr8, %qpr7, %qpr2",
                               None)], 12),
    "qpaddd": (QP_INIT, [("qpaddd,0 %qpr2, %qpr7, %qpr2", None)], 8),
    # Crossings: 128-bit results to the other cluster and to 64-bit
    # integer and FP consumers (which read the low half).
    "qpfaddd_to_qpfaddd_x": (QP_INIT, [("qpfaddd,0 %qpr2, %qpr7, %qpr2", None),
                                       ("qpfaddd,3 %qpr2, %qpr7, %qpr2", 12)], 8),
    "qpaddd_to_qpaddd_x": (QP_INIT, [("qpaddd,0 %qpr2, %qpr7, %qpr2", None),
                                     ("qpaddd,3 %qpr2, %qpr7, %qpr2", 12)], 6),
    "qpfaddd_to_addd": (QP_INIT, [("qpfaddd,0 %qpr2, %qpr7, %qpr2", None),
                                  ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
    "qpaddd_to_addd": (QP_INIT, [("qpaddd,0 %qpr2, %qpr7, %qpr2", None),
                                 ("addd,0 %dr2, %dr5, %dr2", 12)], 8),
    "qpfaddd_to_faddd": (QP_INIT, [("qpfaddd,0 %qpr2, %qpr7, %qpr2", None),
                                   ("faddd,0 %dr2, %dr5, %dr2", 12)], 8),
    "addd_to_qpaddd": (QP_INIT, [("addd,0 %dr2, %dr5, %dr2", None),
                                 ("qpaddd,0 %qpr2, %qpr7, %qpr2", 12)], 6),
    "fmuld_to_qpfmuld": (QP_INIT, [("fmuld,0 %dr2, %dr4, %dr2", None),
                                   ("qpfmuld,0 %qpr2, %qpr8, %qpr2", 12)], 8),
    "faddd_to_qpaddd": (QP_INIT, [("faddd,0 %dr2, %dr5, %dr2", None),
                                  ("qpaddd,0 %qpr2, %qpr7, %qpr2", 12)], 8),
    "paddw_to_qpaddd": (QP_INIT, [("paddw,0 %dr2, %dr5, %dr2", None),
                                  ("qpaddd,0 %qpr2, %qpr7, %qpr2", 12)], 6),
    "qpfaddd_to_qpfaddd_c1": (QP_INIT, [("qpfaddd,0 %qpr2, %qpr7, %qpr2", None),
                                        ("qpfaddd,1 %qpr2, %qpr7, %qpr2", 12)], 8),
    "faddd_to_qpfaddd": (QP_INIT, [("faddd,0 %dr2, %dr5, %dr2", None),
                                   ("qpfaddd,0 %qpr2, %qpr7, %qpr2", 12)], 8),
    "qpand": (QP_INIT, [("qpand,0 %qpr2, %qpr2, %qpr2", None)], 8),
    "ldqp": ("%dr1", [("ldqp,0 %dr2, 0x0, %qpr2", None)], 16),
    "ldqp_to_addd": ("%dr1", [("ldqp,0 %dr2, 0x0, %qpr2", None),
                              ("addd,0 %dr2, %dr5, %dr2", 16)], 16),
    # ldq loads a register pair (%qrN = %drN, %drN+1) through L1.
    "ldq": ("%dr1", [("ldq,0 %dr2, 0x0, %qr2", None)], 16),
    "ldqp_to_qpand": ("%dr1", [("ldqp,0 %dr2, 0x0, %qpr2", None),
                               ("qpand,0 %qpr2, %qpr2, %qpr2", 16)], 16),
    "ldd_to_addd_v5": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None),
                                ("addd,0 %dr2, %dr5, %dr2", 16)], 8),
    # Packed conversions with a consumer that reads their format (the
    # v5op_ chains read an integer result as doubles and back).
    "qpfdtoid_to_qpidtofd": (QP_INIT, [("qpfdtoid,0 %qpr2, %qpr2", None),
                                       ("qpidtofd,0 %qpr2, %qpr2", 12)], 8),
    "qpidtofd_to_qpfdtoid": (QP_INIT, [("qpidtofd,0 %qpr2, %qpr2", None),
                                       ("qpfdtoid,0 %qpr2, %qpr2", 12)], 8),
    "qpfdtoid_to_qpaddd": (QP_INIT, [("qpfdtoid,0 %qpr2, %qpr2", None),
                                     ("qpaddd,0 %qpr2, %qpr7, %qpr2", 12)], 10),
    # v4+ ldexp (semantics.md, "Other scalar operations").
    "fscaled": ("%dr4", [("fscaled,1 %dr2, %r5, %dr2", None)], 8),
    "fscales": ("%dr6", [("fscales,1 %r2, %r5, %r2", None)], 8),
}
TP_V5 = {
    "v5_faddd_6": (_ch("faddd,{c} %dr4, %dr5, {d}", range(6)), "6 faddd"),
    "v5_fmuld_6": (_ch("fmuld,{c} %dr4, %dr4, {d}", range(6)), "6 fmuld"),
    "v5_fmul_addd_6": (_ch("fmul_addd,{c} %dr4, %dr4, %dr5, {d}", range(6)),
                       "6 fmul_addd"),
    "v5_qpfaddd_6": (_ch("qpfaddd,{c} %qpr7, %qpr7, {d}", range(6)),
                     "6 qpfaddd"),
    "v5_qpfmuld_6": (_ch("qpfmuld,{c} %qpr7, %qpr7, {d}", range(6)),
                     "6 qpfmuld"),
    "v5_ldqp_4": (_ch("ldqp,{c} %dr1, {o2:#x}, {d}", [0, 2, 3, 5]), "4 ldqp"),
    "v5_ldqp_1": (_ch("ldqp,{c} %dr1, {o2:#x}, {d}", [0]), "1 ldqp"),
    "v5_ldqp_2": (_ch("ldqp,{c} %dr1, {o2:#x}, {d}", [0, 3]), "2 ldqp (ALC0, 3)"),
    "v5_ldqp_2b": (_ch("ldqp,{c} %dr1, {o2:#x}, {d}", [0, 2]), "2 ldqp (ALC0, 2)"),
    "v5_ldqp_3": (_ch("ldqp,{c} %dr1, {o2:#x}, {d}", [0, 2, 3]), "3 ldqp"),
    "v5_ldqp_4far": (["ldqp,0 %dr1, 0x0, {d}", "ldqp,2 %dr1, 0x100, {d}",
                      "ldqp,3 %dr1, 0x200, {d}", "ldqp,5 %dr1, 0x300, {d}"],
                     "4 ldqp, 256 bytes apart"),
    "v5_ldq_1": (["ldq,0 %dr1, 0x0, {d}"], "1 ldq"),
    "v5_ldq_2": (["ldq,0 %dr1, 0x0, {d}", "ldq,3 %dr1, 0x40, {d}"],
                 "2 ldq (ALC0, 3)"),
    "v5_ldd_4": (_ch("ldd,{c} %dr1, {o:#x}, {d}", [0, 2, 3, 5]), "4 ldd"),
    "v5tp_qpaddd_2": (['qpaddd,0 %qpr7, %qpr7, {d}', 'qpaddd,3 %qpr7, %qpr7, {d}'], "2 qpaddd (ALC0,3)"),
    "v5tp_qpand_4": (['qpand,0 %qpr7, %qpr7, {d}', 'qpand,1 %qpr7, %qpr7, {d}', 'qpand,3 %qpr7, %qpr7, {d}', 'qpand,4 %qpr7, %qpr7, {d}'], "4 qpand (ALC0,1,3,4)"),
    "v5tp_qpshufb_4": (['qpshufb,0 %qpr7, %qpr7, %qpr7, {d}', 'qpshufb,1 %qpr7, %qpr7, %qpr7, {d}', 'qpshufb,3 %qpr7, %qpr7, %qpr7, {d}', 'qpshufb,4 %qpr7, %qpr7, %qpr7, {d}'], "4 qpshufb (ALC0,1,3,4)"),
    "v5tp_qpmullh_2": (['qpmullh,1 %qpr7, %qpr7, {d}', 'qpmullh,4 %qpr7, %qpr7, {d}'], "2 qpmullh (ALC1,4)"),
    "v5tp_qpfcmpeqd_4": (['qpfcmpeqd,0 %qpr7, %qpr7, {d}', 'qpfcmpeqd,1 %qpr7, %qpr7, {d}', 'qpfcmpeqd,3 %qpr7, %qpr7, {d}', 'qpfcmpeqd,4 %qpr7, %qpr7, {d}'], "4 qpfcmpeqd (ALC0,1,3,4)"),
    "v5tp_qpfmaxd_4": (['qpfmaxd,0 %qpr7, %qpr7, {d}', 'qpfmaxd,1 %qpr7, %qpr7, {d}', 'qpfmaxd,3 %qpr7, %qpr7, {d}', 'qpfmaxd,4 %qpr7, %qpr7, {d}'], "4 qpfmaxd (ALC0,1,3,4)"),
    "v5tp_qpfdtoid_4": (['qpfdtoid,0 %qpr7, {d}', 'qpfdtoid,1 %qpr7, {d}', 'qpfdtoid,3 %qpr7, {d}', 'qpfdtoid,4 %qpr7, {d}'], "4 qpfdtoid (ALC0,1,3,4)"),
    "v5tp_qpfmul_addd_6": (['qpfmul_addd,0 %qpr7, %qpr7, %qpr7, {d}', 'qpfmul_addd,1 %qpr7, %qpr7, %qpr7, {d}', 'qpfmul_addd,2 %qpr7, %qpr7, %qpr7, {d}', 'qpfmul_addd,3 %qpr7, %qpr7, %qpr7, {d}', 'qpfmul_addd,4 %qpr7, %qpr7, %qpr7, {d}', 'qpfmul_addd,5 %qpr7, %qpr7, %qpr7, {d}'], "6 qpfmul_addd (ALC0,1,2,3,4,5)"),
    "v5tp_qpsllw_4": (['qpsllw,0 %qpr7, %dr5, {d}', 'qpsllw,1 %qpr7, %dr5, {d}', 'qpsllw,3 %qpr7, %dr5, {d}', 'qpsllw,4 %qpr7, %dr5, {d}'], "4 qpsllw (ALC0,1,3,4)"),
    "v5tp_qppermb_4": (['qppermb,0 %qpr7, %qpr7, %qpr7, {d}', 'qppermb,1 %qpr7, %qpr7, %qpr7, {d}', 'qppermb,3 %qpr7, %qpr7, %qpr7, {d}', 'qppermb,4 %qpr7, %qpr7, %qpr7, {d}'], "4 qppermb (ALC0,1,3,4)"),
    "v5tp_qpmerge_4": (['qpmerge,0 %qpr7, %qpr7, %qpr7, {d}', 'qpmerge,1 %qpr7, %qpr7, %qpr7, {d}', 'qpmerge,3 %qpr7, %qpr7, %qpr7, {d}', 'qpmerge,4 %qpr7, %qpr7, %qpr7, {d}'], "4 qpmerge (ALC0,1,3,4)"),
    "v5tp_addcd_2": (['addcd,1 %dr5, %dr5, %r5, {d}', 'addcd,4 %dr5, %dr5, %r5, {d}'], "2 addcd (ALC1,4)"),
    "v5tp_plog_0x96_4": (['plog_0x96,0 %dr5, %dr5, %dr5, {d}', 'plog_0x96,1 %dr5, %dr5, %dr5, {d}', 'plog_0x96,3 %dr5, %dr5, %dr5, {d}', 'plog_0x96,4 %dr5, %dr5, %dr5, {d}'], "4 plog_0x96 (ALC0,1,3,4)"),
    "v5tp_qplog_0x96_4": (['qplog_0x96,0 %qpr7, %qpr7, %qpr7, {d}', 'qplog_0x96,1 %qpr7, %qpr7, %qpr7, {d}', 'qplog_0x96,3 %qpr7, %qpr7, %qpr7, {d}', 'qplog_0x96,4 %qpr7, %qpr7, %qpr7, {d}'], "4 qplog_0x96 (ALC0,1,3,4)"),
    "v5tp_qpsadbw_2": (['qpsadbw,1 %qpr7, %qpr7, {d}', 'qpsadbw,4 %qpr7, %qpr7, {d}'], "2 qpsadbw (ALC1,4)"),
    "v5_stqp_2": (["stqp,2 %dr1, 0x100, %qpr7", "stqp,5 %dr1, 0x140, %qpr7"],
                  "2 stqp, 64 bytes apart"),
}
# v4+ also puts the packed FP adds, single multiplies and their combined
# forms in ALC2 and ALC5 (binutils; pfmuld, pfmax*, pfmin*, pfcmp* and
# the conversions stay in ALC0, 1, 3, 4): six per instruction.
TP_V5.update({
    f"v5_{op}_6": (_ch(f"{op},{{c}} {args}, {{d}}", range(6)), f"6 {op}")
    for op, args in (
        ("pfaddd", "%dr4, %dr5"), ("pfadds", "%dr4, %dr4"),
        ("pfsubd", "%dr4, %dr5"), ("pfmuls", "%dr4, %dr4"),
        ("pfhadds", "%dr4, %dr4"), ("pfaddsubs", "%dr4, %dr4"),
        ("pfmul_addd", "%dr4, %dr4, %dr5"), ("pfmul_adds", "%dr4, %dr4, %dr4"),
        ("pfadd_hadds", "%dr4, %dr4, %dr4"))})
TP_V5["v5_pfmuld4_pfaddd2"] = (
    _ch("pfmuld,{c} %dr4, %dr4, {d}", [0, 1, 3, 4])
    + _ch("pfaddd,{c} %dr4, %dr5, {d}", [2, 5]),
    "4 pfmuld + 2 pfaddd (ALC2, 5)")
TP2_V5 = {
    # fdivd, then faddd in ALC5 G cycles later, then 8 quiet cycles.
    **{f"v5_fdivd_faddd5_g{g}": ([f"fdivd,5 %dr4, %dr4, {{d}}"]
                                 + ([f"nop {g - 1}"] if g > 1 else []),
                                 ["faddd,5 %dr4, %dr5, {d}", "nop 7"],
                                 f"fdivd, faddd,5 {g} later, 8 quiet")
       for g in range(1, 9)},
    # fdivd, K cycles, faddd in ALC5, M cycles, repeated (no conflict:
    # K + M cycles per pair).
    **{f"v5_div{k}_add{m}": (["fdivd,5 %dr4, %dr4, {d}"]
                             + ([f"nop {k - 1}"] if k > 1 else []),
                             ["faddd,5 %dr4, %dr5, {d}"]
                             + ([f"nop {m - 1}"] if m > 1 else []),
                             f"fdivd, {k} cycles, faddd,5, {m} cycles")
       for k in range(1, 6) for m in range(1, 6)},
    # faddd in ALC5, then fdivd G cycles later.
    **{f"v5_faddd5_fdivd_g{g}": ([f"faddd,5 %dr4, %dr5, {{d}}"]
                                 + ([f"nop {g - 1}"] if g > 1 else []),
                                 ["fdivd,5 %dr4, %dr4, {d}", "nop 7"],
                                 f"faddd,5, fdivd {g} later, 8 quiet")
       for g in range(1, 9)},
    "v5_fdivd_faddd5_alt": (["fdivd,5 %dr4, %dr4, {d}"],
                            ["faddd,5 %dr4, %dr5, {d}"],
                            "fdivd and faddd alternating in ALC5"),
}


# Patterns of four instructions: fdivd in ALC5, then OP in ALC5 G
# cycles later (G = 9..16), then quiet.  Cycles per instruction times 4
# is the pattern's cost; without a conflict it is G + 16.
TPN = {}
TPN_V5 = {
    f"v5_fdivd_{op}5_g{g}": ([["fdivd,5 %dr4, %dr4, {d}", "nop 7"],
                              [f"nop {g - 9}"] if g > 9 else [],
                              [f"{op},5 %dr4, %dr5, {{d}}", "nop 7"],
                              ["nop 7"]],
                             f"fdivd, {op},5 {g} later (no conflict: {g + 16} / 4)")
    for op in ("faddd", "fmuld") for g in range(9, 17)}


def v5_op_chains():
    """A dependent chain for every ALU operation new in v5 (from
    binutils-ops.yaml): the result is the next copy's first source, the
    other sources are constants (%qpr7 = 0.0 pairs, %dr5 / %r5 = 0)."""
    import yaml
    data = yaml.safe_load((ROOT / "spec/isa/binutils-ops.yaml").read_text())
    first = {}
    for e in data["alu_ops"]:
        if e.get("isets"):
            first.setdefault(e["name"], []).append(e)
    reg = {"p": ("%qpr2", "%qpr7"), "d": ("%dr2", "%dr5"), "s": ("%r2", "%r5")}
    out = {}
    for name, es in sorted(first.items()):
        if min(min(e["isets"]) for e in es) != 5:
            continue
        if name.startswith(("ld", "st", "apto", "getva", "puttag")):
            continue
        if name.startswith(("plog_", "qplog_")) and not name.endswith(
                ("_0x96", "_0xe8")):
            continue
        e = next(e for e in es if 5 in e["isets"])
        args = "pppp" if name.startswith("qplog") else e["args"]
        srcs, dst = args[:-1], args[-1]
        # The chain runs through the first source of the destination's
        # kind (else the first source): it gets %qpr2 / %dr2 / %r2.
        k = srcs.index(dst) if dst in srcs else 0
        ops = [reg[a][0] if i == k else reg[a][1] for i, a in enumerate(srcs)]
        ops.append(reg[dst][0])
        out[f"v5op_{name}"] = (QP_INIT, [(f"{name},{e['channels'][0]} "
                                          + ", ".join(ops), None)], 8)
    return out


def add_v5():
    """Add the v5+ kernels to the tables (for --target v5 and later)."""
    SCHED.update(SCHED_V5)
    SCHED.update(v5_op_chains())
    TP.update(TP_V5)
    TP2.update(TP2_V5)
    TPN.update(TPN_V5)


# v6 and later (added when --target is v6+): the operations v6 adds,
# loads into other consumers and channels (an L1 load takes 5 cycles on
# v6, 3 before), and the packed compares into predicates.
SCHED_V6 = {
    "ldd_to_faddd": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None),
                              ("faddd,0 %dr2, %dr5, %dr2", 12)], 8),
    "ldd_to_faddd_x": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None),
                                ("faddd,3 %dr2, %dr5, %dr2", 12)], 8),
    "ldd_c2": ("%dr1", [("ldd,2 %dr2, 0x0, %dr2", None)], 8),
    "ldd_c3": ("%dr1", [("ldd,3 %dr2, 0x0, %dr2", None)], 8),
    "ldd_c5": ("%dr1", [("ldd,5 %dr2, 0x0, %dr2", None)], 8),
    "ldd_c0_to_ldd_c5": ("%dr1", [("ldd,0 %dr2, 0x0, %dr2", None),
                                  ("ldd,5 %dr2, 0x0, %dr2", 12)], 8),
    "addd_to_ldd": ("%dr1", [("addd,0 %dr2, %dr5, %dr2", None),
                             ("ldd,0 %dr2, 0x0, %dr2", 12)], 6),
    "ldw_to_adds": ("%dr1", [("ldw,0 %dr2, 0x0, %r2", None),
                             ("adds,0 %r2, %r5, %r2", 12)], 8),
    "ldqp_to_qpaddd_x": ("%dr1", [("ldqp,0 %dr2, 0x0, %qpr2", None),
                                  ("qpaddd,3 %qpr2, %qpr7, %qpr2", 12)], 8),
    "ldqp_c5": ("%dr1", [("ldqp,5 %dr2, 0x0, %qpr2", None)], 8),
    "fmad_c": ("%dr4", [("fmad,0 %dr4, %dr5, %dr2, %dr2", None)], 10),
    "fmad_c5": ("%dr4", [("fmad,5 %dr2, %dr4, %dr5, %dr2", None)], 10),
    "fmad_to_addd": ("%dr4", [("fmad,0 %dr2, %dr4, %dr5, %dr2", None),
                              ("addd,0 %dr2, %dr5, %dr2", 12)], 10),
    "fmad_to_fmad_x": ("%dr4", [("fmad,0 %dr2, %dr4, %dr5, %dr2", None),
                                ("fmad,3 %dr2, %dr4, %dr5, %dr2", 12)], 10),
    "qpfmad_c": (QP_INIT, [("qpfmad,0 %qpr8, %qpr7, %qpr2, %qpr2", None)], 10),
    "pcmpeqbop_to_merged": ("%dr3", [("pcmpeqbop,0 %dr2, %dr5, %pred1", None),
                                     ("merged,0 %dr2, %dr5, %dr2, %pred1", 12)], 6),
    "qpcmpeqbop_to_merged": (QP_INIT, [("qpcmpeqbop,0 %qpr2, %qpr7, %pred1", None),
                                       ("merged,0 %dr2, %dr5, %dr2, %pred1", 12)], 6),
    "merged_to_pcmpeqbop": ("%dr3", [("merged,0 %dr2, %dr5, %dr2, %pred1", None),
                                     ("pcmpeqbop,0 %dr2, %dr5, %pred1", 12)], 6),
    "pmrgp": ("%dr3", [("pmrgp,0 %dr2, %dr5, %dr2, %pred0", None)], 6),
    "qpmrgp": (QP_INIT, [("qpmrgp,0 %qpr2, %qpr7, %qpr2, %pred0", None)], 6),
}
TP_V6 = {
    "v6_fmad_6": (_ch("fmad,{c} %dr4, %dr4, %dr5, {d}", range(6)), "6 fmad"),
    "v6_fmas_6": (_ch("fmas,{c} %r6, %r6, %r5, {d}", range(6)), "6 fmas"),
    "v6_qpfmad_6": (_ch("qpfmad,{c} %qpr7, %qpr7, %qpr7, {d}", range(6)),
                    "6 qpfmad"),
    "v6_clmull_4": (_ch("clmull,{c} %dr3, %dr3, {d}", [0, 1, 3, 4]),
                    "4 clmull"),
    "v6_qpsrad_4": (_ch("qpsrad,{c} %qpr7, %dr5, {d}", [0, 1, 3, 4]),
                    "4 qpsrad"),
    # Equal operands: qpcheckd does nothing (it traps when they differ).
    "v6_qpcheckd_2": (_ch("qpcheckd,{c} %dr5, %dr5, {d}", [0, 3]),
                      "2 qpcheckd"),
    "v6_qpcheckd_1": (_ch("qpcheckd,{c} %dr5, %dr5, {d}", [0]),
                      "1 qpcheckd"),
}


def v6_op_chains():
    """A dependent chain for every ALU operation new in v6, as
    v5_op_chains; a compare into a predicate is followed by a merge that
    reads it (the pair is one step), the merges by a predicate read
    %pred0."""
    import yaml
    data = yaml.safe_load((ROOT / "spec/isa/binutils-ops.yaml").read_text())
    first = {}
    for e in data["alu_ops"]:
        if e.get("isets"):
            first.setdefault(e["name"], []).append(e)
    reg = {"p": ("%qpr2", "%qpr7"), "d": ("%dr2", "%dr5"), "s": ("%r2", "%r5")}
    out = {}
    for name, es in sorted(first.items()):
        if min(min(e["isets"]) for e in es) != 6:
            continue
        # (vfbgv's b is %r5 = 0, so it never traps; modbgv's mask is 0.)
        if name.startswith(("ibranchd", "icalld", "qpcheck")):
            continue
        e = next(e for e in es if 6 in e["isets"])
        ch = e["channels"][0]
        if e["alopf"] == "ALOPF17":
            a = reg[e["args"][0]]
            step = [(f"{name},{ch} {a[0]}, {a[1]}, %pred1", None),
                    ("merged,0 %dr2, %dr5, %dr2, %pred1", 4)]
            out[f"v6op_{name}"] = (QP_INIT, step, 6)
            continue
        srcs, dst = e["args"][:-1], e["args"][-1]
        k = srcs.index(dst) if dst in srcs else 0
        ops = [reg[a][0] if i == k else reg[a][1] for i, a in enumerate(srcs)]
        ops.append(reg[dst][0])
        tail = ", %pred0" if e["alopf"].endswith("_MERGE") else ""
        out[f"v6op_{name}"] = (QP_INIT, [(f"{name},{ch} " + ", ".join(ops)
                                          + tail, None)], 10)
    return out


def add_v6():
    """Add the v6+ kernels (for --target v6 and later)."""
    SCHED.update(SCHED_V6)
    SCHED.update(v6_op_chains())
    TP.update(TP_V6)
    CTL_KINDS.extend(["pcmp_alu", "pcmp_ct"])


# The v3 sweep, kind "v3op" (only with --only v3op, not in a full run;
# v3 operations being valid on all later targets, it runs on each):
#   v3op_*   a scheduled-latency chain for every v3 computational
#            operation without a latency of its own (generated from
#            binutils-ops.yaml, as v5op_*), to a consumer of its own
#            class and, for FP-side results, to an integer operation;
#            the combined integer operations through operand c (a
#            representative set) and fxdivt* through each operand;
#   v3div_*  divider sharing: the ALC5 operations not yet timed, alone,
#            every g cycles, alternating with fdivd, and two a cycle
#            apart;
#   v3st_*   state registers: rrs/rws %pfpfr (and %fpcr, %fpsr for
#            80-bit operations), rrd %ip, rrs/rws %upsr, alone and to the
#            reads and operations after them;
#   v3wait_* wait all_c, ld_c|st_c, all_e alone in a loop and after a
#            store, a load or an FP operation.
# Kernel names are the keys of SCHED / TP / TP2 as for the other kinds;
# V3OP says which of them belong to v3op.
V3OP = set()
V3OP_SKIPPED = {}       # operation -> why it has no v3op chain
TP_INIT = {}            # TP kernel name -> extra setup ("; " between ops)

# Operand types: i / w a 64 / 32-bit integer, I packed integers in a
# 64-bit register, F a 32-bit x86 flags word, g / f a double / single,
# P two packed singles, x an 80-bit value, p a predicate, n an 8-bit
# immediate.  Chain registers: %dr2 / %r2 the chained value, %dr8 / %r8
# a result that needs converting back; constants (INIT and the chain's
# setup): %dr3 1, %dr4 1.0, %dr5 0, %r6 1.0f, %dr7 (1.0f, 1.0f) or
# 80-bit 1.0.  An 80-bit operand is never 0 (%dr5 has no valid
# exponent part); it is 1.0.
_V3_INTLIKE = set("iwIF")
_V3_SIZE = {"i": "d", "I": "d", "g": "d", "P": "d", "x": "d",
            "w": "s", "F": "s", "f": "s"}
_V3_ZERO = {"i": "%dr5", "I": "%dr5", "g": "%dr5", "P": "%dr5",
            "x": "%dr7", "w": "%r5", "F": "%r5", "f": "%r5"}
_V3_ONE = {"i": "%dr3", "I": "%dr3", "g": "%dr4", "P": "%dr7",
           "x": "%dr7", "w": "%r3", "F": "%r3", "f": "%r6"}
# The 64-bit register holding a valid value of a type (after `ord` with 0).
_V3_ONEBITS = {"i": "%dr3", "I": "%dr3", "w": "%dr3", "F": "%dr3",
               "g": "%dr4", "f": "%dr6", "P": "%dr7"}
_V3_INIT = {"i": "addd,0 0x0, 0x1, %dr2", "w": "addd,0 0x0, 0x1, %dr2",
            "I": "addd,0 0x0, 0x1, %dr2", "F": "addd,0 0x0, 0x1, %dr2",
            "g": "addd,0 0x0, %dr4, %dr2", "f": "addd,0 0x0, %dr6, %dr2",
            "P": "addd,0 0x0, 0x3f8000003f800000, %dr2",
            "x": "fdtofx,0 %dr4, %dr2"}
_V3_INIT7 = {"P": "addd,1 0x0, 0x3f8000003f800000, %dr7",
             "x": "fdtofx,1 %dr4, %dr7"}
# Converters bringing a result of type R back to the chained source's
# type T, each reading the result in the format it was produced in.
_V3_CONV = {("f", "g"): "fstofd", ("g", "f"): "fdtofs",
            ("w", "g"): "istofd", ("w", "f"): "istofs",
            ("i", "g"): "idtofd", ("i", "f"): "idtofs",
            ("g", "i"): "fdtoid", ("f", "i"): "fstoid",
            ("g", "w"): "fdtoistr", ("f", "w"): "fstoistr",
            ("P", "I"): "pfstois", ("I", "P"): "pistofs",
            ("f", "x"): "fstofx", ("g", "x"): "fdtofx",
            ("w", "x"): "istofx", ("i", "x"): "idtofx",
            ("x", "f"): "fxtofs", ("x", "g"): "fxtofd",
            ("x", "w"): "fxtois", ("x", "i"): "fxtoid"}
_V3_FX = {"s": "f", "d": "g", "x": "x"}
_V3_CVT = {"fs": "f", "fd": "g", "is": "w", "id": "i"}
# Integer-side results (an integer consumer is the chain itself).
_V3_INTOPS = re.compile(
    r"^((add|sub|and|andn|or|orn|xor|xorn|shl|shr|sar|scl|scr|getf|insf|"
    r"merge|popcnt|lzcnt|bitrev)[sd]|sxt|(?![su]mulx)[a-z]+_f[bhwd])$")
_V3_COMBINED = re.compile(r"^(and|andn|or|orn|xor|xorn|add|sub|scl|scr|shl|"
                          r"shr|sar|getf|merge)_")
# Timed elsewhere (timing.md rows from pocchk.c, pmu.c, or a bench.py
# pair whose name is not the operation's).  sqrt.c's numbers for
# fsqrtid, fsqrttd, frcps and frsqrts are back-to-back costs, so those
# get chains here.
_V3_MEASURED = {"sdivs", "udivs", "udivd", "fdtofx", "fxtofd"}
_V3_NOTSWEPT = [
    (re.compile(r"^(ld|st)"), "memory access (timing.md, Memory)"),
    (re.compile(r"^aaur"), "AAU register access (apb.md)"),
    (re.compile(r"^(apto|cudtoap|gdtoap|getsap|getva|vfsi|getpl|gettag|"
                r"puttag|gettc|gettst|invtc|puttc|puttst|movt)"),
     "tags, protected mode, CTPR moves (movtd -> ct is a ctl kernel)"),
    (re.compile(r"^(rr|rw)[sd]$"), "state register: v3st_* kernels"),
    (re.compile(r"^getsp$"), "allocates stack; timed by pmu.c"),
    (re.compile(r"^mov(if|fi)$"), "timed (movfi_movif, pmu.c)"),
    (re.compile(r"^fxsqrt[iut]xx$"), "timed (fxsqrt*xx)"),
]


def _v3_types(n, alopf, args):
    """(source types, result type, index of the chained source) of a v3
    operation, or a string saying why it is not swept on its own."""
    w = {"s": "w", "d": "i"}
    if alopf in ("ALOPF7", "ALOPF8"):          # into a predicate
        srcs = list(args)
    else:
        srcs = list(args[:-1])
    ns = len(srcs)
    if n in ("movx", "movxa", "movxc"):
        return ["x"], "x", 0
    m = re.match(r"(f[sd]|i[sd])tofx$", n)
    if m:
        return [_V3_CVT[m[1]]], "x", 0
    m = re.match(r"fxto(fs|fd|id|is)(tr)?$", n)
    if m:
        return ["x"], _V3_CVT[m[1]], 0
    if re.match(r"fxsqrt[iut][sdx]x$", n):
        return "square root step: v3op_fxsqrt*[sd]x chains of all three"
    if re.match(r"fxdivt[sd][sdx]$", n):
        return "narrow src1, 80-bit src2: v3op_fxdivt* chains through each"
    m = re.match(r"fxcmp[a-z]+?([sdx])([bf])$", n)
    if m:
        return ["x", _V3_FX[m[1]]], "p" if m[2] == "b" else "F", 0
    m = re.match(r"fx[a-z]+?([sdx])([sdx])$", n)
    if m:
        return ["x", _V3_FX[m[1]]], _V3_FX[m[2]], 0
    m = re.match(r"(f[sd]|i[sd])to(f[sd]|i[sd])(tr)?$", n)
    if m:
        return [_V3_CVT[m[1]]], _V3_CVT[m[2]], 0
    if n in ("fdtoifd", "fstoifs"):
        t = "g" if n == "fdtoifd" else "f"
        return [w[args[0]], t], t, 1
    if n.startswith(("popcnt", "lzcnt", "bitrev")):
        return [w[a] for a in srcs], w[args[-1]], 0
    if n in ("pfstois", "pfstoistr"):
        return ["P"], "I", 0
    if n == "pistofs":
        return ["I"], "P", 0
    if n == "pfdtofs":
        return ["g"], "f", 0
    if n in ("pfdtois", "pfdtoistr"):
        return ["g"], "w", 0
    if n == "pfstofd":
        return ["f"], "g", 0
    if n == "pfdtoifd":
        return ["i", "g"], "g", 1
    if n == "pfstoifs":
        return ["i", "P"], "P", 1
    if n in ("pfdivs", "pfsqrts"):              # scalar singles (semantics.md)
        return ["f"] * ns, "f", 0
    if n.startswith("pf"):
        t = "P" if n.endswith("s") else "g"
        return [t] * ns, t, 0
    if n == "pextrh":
        return ["I", "I", "n"], "w", 0
    if n in ("pinsh", "pshufw", "psllqh", "psllql", "psrlqh", "psrlql"):
        return ["I", "I", "n"], "I", 0
    if n == "pshufh":
        return ["I", "n"], "I", 0
    if n.startswith("p"):
        return ["I"] * ns, "I", 0
    m = re.match(r"fcmp(eq|lt|le|uod|neq|nlt|nle|od|ud)([sd])([bf]?)$", n)
    if m:
        t = "f" if m[2] == "s" else "g"
        return [t, t], {"b": "p", "f": "F", "": t}[m[3]], 0
    if n in ("fsqrts", "frcps", "frsqrts"):
        return ["f"], "f", 0
    if n == "fsqrtid":
        return ["g"], "g", 0
    if n == "fsqrttd":
        return ["g", "g"], "g", 0
    m = re.match(r"f(add|sub|mul|max|min|div)([sd])$", n) or re.match(
        r"f(add|sub|mul)_(add|sub|rsub)([sd])$", n)
    if m:
        t = "f" if n.endswith("s") else "g"
        return [t] * ns, t, 0
    if n.startswith("cctop"):
        return ["F"], "p", 0
    if re.match(r"cmp", n):
        return [w[a] for a in srcs], "p", 0
    if re.search(r"_f[bhwd]$", n):
        return [w[a] for a in srcs], "F", 0
    if n in ("smulx", "umulx"):
        return ["w", "w"], "i", 0
    if re.match(r"[su](div|mod)x$", n):
        return ["i", "w"], "w", 0
    if n in ("sxt", "insfs", "insfd") or alopf in ("ALOPF1", "ALOPF11",
                                                   "ALOPF2", "MERGE"):
        return [w[a] for a in srcs], w[args[-1]], 0
    return f"no rule for {alopf} {args}"


def _v3_alu_ops(isa):
    """{name: (alopf, args, channels)} of the v3 ALU operations valid on
    ISA (channels on v3, the smallest set; later ISAs add channels)."""
    import yaml
    data = yaml.load((ROOT / "spec/isa/binutils-ops.yaml").read_text(),
                     Loader=getattr(yaml, "CSafeLoader", yaml.SafeLoader))
    isets, out = {}, {}
    for e in data["alu_ops"]:
        if e.get("isets"):
            isets.setdefault(e["name"], set()).update(e["isets"])
    for e in data["alu_ops"]:
        n = e["name"]
        if not e.get("isets") or min(isets[n]) > 3 or 3 not in isets[n]:
            continue
        if isa not in isets[n]:
            V3OP_SKIPPED[n] = f"not in elbrus-v{isa}"
            continue
        if 3 in e["isets"]:
            alopf, args, chans = out.get(n, (e["alopf"], e["args"], set()))
            out[n] = (alopf, args, chans | set(e["channels"]))
    return out


def _v3_reg(t, k):
    return f"%r{k}" if _V3_SIZE[t] == "s" else f"%dr{k}"


def _v3_fpch(ch):
    """An ALC0/1/3/4 channel (FP conversions) in CH's cluster."""
    return ch if ch in (0, 1, 3, 4) else (1 if ch == 2 else 3)


def _v3_back_pred(t, ch):
    """From %pred1 back to the chained value of type T (merge keeps it or
    gives 0; an 80-bit value is rebuilt as 1.0)."""
    if t == "x":
        return [(f"merged,{ch} %dr4, %dr4, %dr8, %pred1", 12),
                (f"fdtofx,{_v3_fpch(ch)} %dr8, %dr2", 12)]
    return [(f"merged,{ch} %dr5, %dr2, %dr2, %pred1", 12)]


def _v3_reset(t, ch, out, r):
    """An integer consumer of OUT (type R) that throws its value away, then
    the chained source rebuilt from a constant of type T: for results no
    converter brings back (80-bit, flags, NaN-prone)."""
    size = _V3_SIZE[r]
    tmp = "%r8" if size == "s" else "%dr8"
    lines = [(f"and{size},{ch} {out}, 0x0, {tmp}", 12)]
    if t == "x":
        lines += [(f"ord,{ch} %dr8, %dr4, %dr8", 12),
                  (f"fdtofx,{_v3_fpch(ch)} %dr8, %dr2", 12)]
    else:
        lines += [(f"ord,{ch} %dr8, {_V3_ONEBITS[t]}, %dr2", 12)]
    return f"and{size}", lines


def v3_op_chains(isa=3):
    """Scheduled-latency chains for the v3 computational operations
    without a latency of their own: {name: (init, step, maxgap)}.

    v3op_NAME           the result feeds the same operation (or, for a
                        result in another type, a converter reading it in
                        its own format: v3op_NAME_to_CONV; a predicate a
                        merge, v3op_NAME_to_merged; a flags word cctope);
    v3op_NAME_to_addd   (or adds, andd, ands) an integer consumer of an
                        FP-side result, in the producer's channel.
    The chained source gets %dr2 / %r2; other sources are 0, or 1 of
    their type for divisors (a 0 divisor is SIGFPE), multipliers and
    80-bit second operands."""
    out = {}
    measured = set(SCHED) | _V3_MEASURED     # (LAT is back-to-back only)
    for n, (alopf, args, chans) in sorted(_v3_alu_ops(isa).items()):
        why = next((r for p, r in _V3_NOTSWEPT if p.match(n)), None)
        if why is None and _V3_COMBINED.match(n):
            why = "combined integer: v3op_* representative set"
        typ = why or _v3_types(n, alopf, args)
        if isinstance(typ, str):
            V3OP_SKIPPED[n] = typ
            continue
        srcs, r, k = typ
        t = srcs[k]
        ch = min(chans)
        self_done = n in measured or any(s.startswith(n + "_to_")
                                         for s in SCHED)
        int_done = any(s.startswith((n + "_to_addd", n + "_to_adds"))
                       for s in SCHED)
        alc5 = chans == {5}
        types = set(srcs) | {r, t}
        init = "; ".join([_V3_INIT[t]] + [_V3_INIT7[c] for c in "Px"
                                          if c in types])
        ops = []
        for i, a in enumerate(srcs):
            if a == "n":
                ops.append("0x1")
            elif i == k:
                ops.append(_v3_reg(a, 2))
            elif i == 1 and (re.search(r"div|mod|mul|sqrtt", n)
                             or n.startswith("fx")):
                ops.append(_V3_ONE[a])
            else:
                ops.append(_V3_ZERO[a])
        direct = r == t or (r in _V3_INTLIKE and t in _V3_INTLIKE)
        res = "%pred1" if r == "p" else _v3_reg(r, 2 if direct else 8)
        tail = ", %pred0" if alopf.endswith("MERGE") else ""
        op = f"{n},{ch} " + ", ".join(ops + [res]) + tail
        big = 24 if alc5 else 10
        if not self_done:
            if direct:
                key, step = f"v3op_{n}", []
                gap = 6 if _V3_INTOPS.match(n) else (8 if r in "IF" else big)
            elif r == "p":
                key, step = f"v3op_{n}_to_merged", _v3_back_pred(t, ch)
                gap = 6 if t in _V3_INTLIKE else big
            elif r == "F":
                cc = 0 if ch < 3 else 3
                key = f"v3op_{n}_to_cctope"
                step = [(f"cctope,{cc} %r8, %pred1", 12)] + _v3_back_pred(t, cc)
                gap = big
            else:
                cv = _V3_CONV[(r, t)]
                key = f"v3op_{n}_to_{cv}"
                step = [(f"{cv},{_v3_fpch(ch)} {res}, {_v3_reg(t, 2)}", 12)]
                gap = big
            out[key] = (init, [(op, None)] + step, gap)
        if r == "p" or _V3_INTOPS.match(n) or int_done:
            continue
        if direct and r != "x":
            c = "adds" if _V3_SIZE[r] == "s" else "addd"
            step = [(f"{c},{ch} {res}, {_V3_ZERO[r]}, {res}", 12)]
        elif (r, t) in _V3_CONV and r != "x":
            c = "adds" if _V3_SIZE[r] == "s" else "addd"
            step = [(f"{c},{ch} {res}, {_V3_ZERO[r]}, {res}", 12),
                    (f"{_V3_CONV[(r, t)]},{_v3_fpch(ch)} {res}, "
                     f"{_v3_reg(t, 2)}", 12)]
        else:
            c, step = _v3_reset(t, ch, res, r)
        out[f"v3op_{n}_to_{c}"] = (init, [(op, None)] + step, big + 2)
    out.update(_v3_special_chains())
    return out


def _v3_special_chains():
    """The combined integer operations through each operand (a
    representative set), merge_* (with its predicate), and the 80-bit
    square root steps from a single or double."""
    out = {}
    one = "addd,0 0x0, 0x1, %dr2"
    firsts = ("and", "andn", "or", "orn", "xor", "xorn", "add", "sub", "scl",
              "scr", "shl", "shr", "sar", "getf")
    seconds = ("and", "andn", "or", "orn", "xor", "xorn", "sub", "rsub")
    # X_Y a, b, c computes Y(c, X(a, b)) (semantics.md); _opc chains
    # through c, _opb through b, no suffix through a.
    for x in firsts:
        op = f"{x}_addd"
        out[f"v3op_{op}_opc"] = (one, [(f"{op},1 %dr5, %dr5, %dr2, %dr2",
                                        None)], 6)
        if op not in SCHED:
            out[f"v3op_{op}"] = (one, [(f"{op},1 %dr2, %dr5, %dr5, %dr2",
                                        None)], 6)
    for y in seconds:
        op = f"add_{y}d"
        out[f"v3op_{op}_opc"] = (one, [(f"{op},1 %dr5, %dr5, %dr2, %dr2",
                                        None)], 6)
    for x in ("add", "and", "xor", "shl"):
        op = f"{x}_adds"
        out[f"v3op_{op}_opc"] = (one, [(f"{op},1 %r5, %r5, %r2, %r2",
                                        None)], 6)
    for x in ("add", "shl"):
        op = f"{x}_addd"
        out[f"v3op_{op}_opb"] = (one, [(f"{op},1 %dr5, %dr2, %dr5, %dr2",
                                        None)], 6)
    # merge_Y a, b, c, pred: Y(c, pred ? b : a).  %pred0 is false in the
    # loop; %pred2 is set true for the _t form.
    for y in ("and", "andn", "or", "orn", "xor", "xorn", "add", "sub",
              "rsub"):
        for wd, r in (("d", "%dr"), ("s", "%r")):
            op = f"merge_{y}{wd}"
            out[f"v3op_{op}_opc"] = (
                one, [(f"{op},1 {r}5, {r}5, {r}2, {r}2, %pred0", None)], 6)
    for wd, r in (("d", "%dr"), ("s", "%r")):
        op = f"merge_add{wd}"
        out[f"v3op_{op}"] = (
            one, [(f"{op},1 {r}2, {r}5, {r}5, {r}2, %pred0", None)], 6)
    out["v3op_merge_addd_opb"] = (
        one, [("merge_addd,1 %dr5, %dr2, %dr5, %dr2, %pred0", None)], 6)
    out["v3op_merge_addd_opb_t"] = (
        one + "; cmpedb,3 %dr3, 0x1, %pred2",
        [("merge_addd,1 %dr5, %dr2, %dr5, %dr2, %pred2", None)], 6)
    out["v3op_cmpesb_to_merge_addd"] = (
        one, [("cmpesb,1 %r2, %r5, %pred1", None),
              ("merge_addd,1 %dr5, %dr2, %dr5, %dr2, %pred1", 12)], 6)
    # A double square root, fsqrtid a, t then fsqrttd a, t, r (fsqrttd
    # needs fsqrtid's t: semantics.md): each step to the other and the
    # pair to an FP operation.
    sq = "addd,0 0x0, %dr4, %dr2"
    out["v3op_fsqrtid_to_fsqrttd"] = (sq, [
        ("fsqrtid,5 %dr2, %dr7", None), ("fsqrttd,5 %dr2, %dr7, %dr8", 24),
        ("faddd,3 %dr8, %dr5, %dr2", 24)], 18)
    out["v3op_fsqrttd_to_fsqrtid"] = (sq, [
        ("fsqrtid,5 %dr2, %dr7", 24), ("fsqrttd,5 %dr2, %dr7, %dr2", None)],
        24)
    out["v3op_fsqrttd_to_faddd"] = (sq, [
        ("fsqrtid,5 %dr2, %dr7", 24), ("fsqrttd,5 %dr2, %dr7, %dr8", None),
        ("faddd,3 %dr8, %dr5, %dr2", 12)], 24)
    # An integer divide's result to an FP-side consumer and to an integer
    # operation in the other cluster (v3op_sdivd_to_addd: in ALC5).
    out["v3op_sdivd_to_idtofd"] = (one, [
        ("sdivd,5 %dr2, %dr3, %dr8", None), ("idtofd,3 %dr8, %dr9", 12),
        ("fdtoid,3 %dr9, %dr2", 12)], 18)
    out["v3op_sdivd_to_addd0"] = (one, [
        ("sdivd,5 %dr2, %dr3, %dr8", None), ("addd,0 %dr8, %dr5, %dr2", 12)],
        18)
    # fxsqrti<t>x a, s; fxsqrtu<t>x a, s, s; fxsqrtt<t>x a, s, r (lcc's
    # sqrtl, semantics.md) from a single or double a, the root converted
    # back to a's type: each step scanned in turn.
    for t, init, back in (("s", "addd,0 0x0, %dr6, %dr2", "fxtofs,3 %dr9, %r2"),
                          ("d", "addd,0 0x0, %dr4, %dr2", "fxtofd,3 %dr9, %dr2")):
        a = "%r2" if t == "s" else "%dr2"
        i = f"fxsqrti{t}x,5 {a}, %dr8"
        u = f"fxsqrtu{t}x,5 {a}, %dr8, %dr8"
        s = f"fxsqrtt{t}x,5 {a}, %dr8, %dr9"
        out[f"v3op_fxsqrti{t}x"] = (init, [(i, None), (u, 24), (s, 24),
                                           (back, 12)], 18)
        out[f"v3op_fxsqrtu{t}x"] = (init, [(i, 24), (u, None), (s, 24),
                                           (back, 12)], 24)
        out[f"v3op_fxsqrtt{t}x_to_{back.split(',')[0]}"] = (
            init, [(i, 24), (u, 24), (s, None), (back, 12)], 24)
    # fxdivt<a><r> src1, src2 is src1 / src2 with src1 a single or double
    # (a) and src2 80-bit (semantics.md, "80-bit operations"); binutils
    # gives src1 the format d and src2 that of a, so objdump prints a
    # single src1 as %drN and the 80-bit src2 of an s form as %rN (gas
    # takes either spelling), and these chains spell them so.  Each chain
    # divides 1.0 by 1.0: through src1 (no suffix; an 80-bit result
    # converted back to a), through src2 (_opb; a narrow result converted
    # to 80-bit), and to an integer operation (through src1).
    for a, r in (("s", "s"), ("s", "x"), ("d", "d"), ("d", "x")):
        n = f"fxdivt{a}{r}"
        s2 = "%r" if a == "s" else "%dr"        # src2's register syntax
        x2, c2 = s2 + "7", s2 + "2"             # 80-bit 1.0; the chain
        one1 = "%dr6" if a == "s" else "%dr4"   # 1.0 of type a as src1
        res = {"s": "%r8", "d": "%dr8", "x": "%dr8"}[r]
        init = f"addd,0 0x0, {one1}, %dr2; fdtofx,1 %dr4, %dr7"
        if r == "x":
            back = "fxtofs,3 %dr8, %r2" if a == "s" else "fxtofd,3 %dr8, %dr2"
            out[f"v3op_{n}_to_{back.split(',')[0]}"] = (
                init, [(f"{n},5 %dr2, {x2}, %dr8", None), (back, 12)], 24)
            out[f"v3op_{n}_opb"] = (
                "fdtofx,0 %dr4, %dr2",
                [(f"{n},5 {one1}, {c2}, %dr2", None)], 24)
            out[f"v3op_{n}_to_andd"] = (init, [
                (f"{n},5 %dr2, {x2}, %dr8", None),
                ("andd,5 %dr8, 0x0, %dr8", 12),
                (f"ord,5 %dr8, {one1}, %dr2", 12)], 26)
        else:
            to = "fstofx,3 %r8, %dr2" if a == "s" else "fdtofx,3 %dr8, %dr2"
            own = res.replace("8", "2")
            out[f"v3op_{n}"] = (init, [(f"{n},5 %dr2, {x2}, {own}", None)], 24)
            out[f"v3op_{n}_opb_to_{to.split(',')[0]}"] = (
                "fdtofx,0 %dr4, %dr2",
                [(f"{n},5 {one1}, {c2}, {res}", None),
                 (to, 12)], 24)
            c = "adds" if a == "s" else "addd"
            z = "%r5" if a == "s" else "%dr5"
            out[f"v3op_{n}_to_{c}"] = (init, [
                (f"{n},5 %dr2, {x2}, {own}", None),
                (f"{c},5 {own}, {z}, {own}", 12)], 26)
    return out


# Divider sharing (v3div_*): the ALC5 operations whose unit was not
# measured, with constant operands (1, 1.0, 1.0f, 80-bit 1.0 in %dr7).
# "{s}" is a 32-bit destination.
V3_DIV = [
    ("fsqrtid", "fsqrtid,5 %dr4, {d}"),
    ("fsqrttd", "fsqrttd,5 %dr4, %dr4, {d}"),
    ("udivs", "udivs,5 %r3, %r3, {s}"),
    ("udivd", "udivd,5 %dr3, %dr3, {d}"),
    ("sdivx", "sdivx,5 %dr3, %r3, {s}"),
    ("udivx", "udivx,5 %dr3, %r3, {s}"),
    ("smodx", "smodx,5 %dr3, %r3, {s}"),
    ("umodx", "umodx,5 %dr3, %r3, {s}"),
    ("fxdivxx", "fxdivxx,5 %dr7, %dr7, {d}"),
    ("fxdivdd", "fxdivdd,5 %dr7, %dr4, {d}"),
    ("fxdivss", "fxdivss,5 %dr7, %r6, {s}"),
    ("fxsqrtixx", "fxsqrtixx,5 %dr7, {d}"),
    ("fxsqrtuxx", "fxsqrtuxx,5 %dr7, %dr7, {d}"),
    ("fxsqrttxx", "fxsqrttxx,5 %dr7, %dr7, {d}"),
    ("pfdivs", "pfdivs,5 %r6, %r6, {s}"),
    ("pfdivd", "pfdivd,5 %dr4, %dr4, {d}"),
    ("pfsqrts", "pfsqrts,5 %r6, {s}"),
    ("pfsqrttd", "pfsqrttd,5 %dr4, %dr4, {d}"),
    ("frcps", "frcps,5 %r6, {s}"),
    ("frsqrts", "frsqrts,5 %r6, {s}"),
]
_FDIVD = "fdivd,5 %dr4, %dr4, {d}"


def _v3_tp():
    """The v3div_, v3st_ and v3wait_ throughput kernels: (TP, TP2)."""
    tp, tp2 = {}, {}
    for n, op in V3_DIV:
        tp[f"v3div_{n}"] = ([op], f"{n} every cycle")
        for g in range(2, 9):
            tp[f"v3div_{n}_g{g}"] = ([op, f"nop {g - 1}"],
                                     f"{n} every {g} cycles")
        # One unit with fdivd: alternating, 5 cycles an instruction as
        # fdivd alone every cycle; separate, 1.
        tp2[f"v3div_fdivd_{n}_alt"] = ([_FDIVD], [op],
                                       f"fdivd, {n} alternating")
        # Two a cycle apart, then 8 quiet: 9 cycles a pair without a
        # conflict (4.5 an instruction), 13 with fdivd's (6.5); alone
        # (_g8) 8 an instruction.
        tp2[f"v3div_{n}_{n}_1"] = ([op], [op, "nop 7"],
                                   f"{n}, {n} 1 later, 8 quiet")
        tp2[f"v3div_fdivd_{n}_1"] = ([_FDIVD], [op, "nop 7"],
                                     f"fdivd, {n} 1 later, 8 quiet")
        tp2[f"v3div_{n}_fdivd_1"] = ([op], [_FDIVD, "nop 7"],
                                     f"{n}, fdivd 1 later, 8 quiet")
    tp2["v3div_fdivd_fdivd_1"] = ([_FDIVD], [_FDIVD, "nop 7"],
                                  "fdivd, fdivd 1 later, 8 quiet")
    sid, std = dict(V3_DIV)["fsqrtid"], dict(V3_DIV)["fsqrttd"]
    tp2["v3div_fsqrtid_fsqrttd_alt"] = ([sid], [std],
                                        "fsqrtid, fsqrttd alternating")
    tp2["v3div_fsqrtid_fsqrttd_1"] = ([sid], [std, "nop 7"],
                                      "fsqrtid, fsqrttd 1 later, 8 quiet")
    for name in list(tp) + list(tp2):
        TP_INIT[name] = "fdtofx,0 %dr4, %dr7"
    # State registers, every instruction (a write writes back the value
    # read in the setup, %r7).
    st = {
        "v3st_rrs_pfpfr": (["rrs,0 %pfpfr, {s}"], "rrs %pfpfr", None),
        "v3st_rws_pfpfr": (["rws,0 %r7, %pfpfr"], "rws %pfpfr", "%pfpfr"),
        "v3st_rws_fpcr": (["rws,0 %r7, %fpcr"], "rws %fpcr", "%fpcr"),
        "v3st_rrs_upsr": (["rrs,0 %upsr, {s}"], "rrs %upsr", None),
        "v3st_rws_upsr": (["rws,0 %r7, %upsr"], "rws %upsr", "%upsr"),
        "v3st_rrd_ip": (["rrd,0 %ip, {d}"], "rrd %ip", None),
    }
    for name, (ops, what, reg) in st.items():
        tp[name] = (ops, what + " every cycle")
        if reg:
            TP_INIT[name] = f"rrs,0 {reg}, %r7"
    # Does a write of %pfpfr wait for operations in flight?
    for name, a, what in (
            ("v3st_rws_rrs_pfpfr", "rrs,0 %pfpfr, {s}", "rws %pfpfr, rrs %pfpfr"),
            ("v3st_faddd_rws_pfpfr", "faddd,0 %dr4, %dr5, {d}",
             "faddd, rws %pfpfr"),
            ("v3st_fdivd_rws_pfpfr", _FDIVD, "fdivd, rws %pfpfr")):
        first, second = ((["rws,0 %r7, %pfpfr"], [a]) if name.startswith(
            "v3st_rws") else ([a], ["rws,0 %r7, %pfpfr"]))
        tp2[name] = (first, second, what + " alternating")
        TP_INIT[name] = "rrs,0 %pfpfr, %r7"
    # wait alone, and after a store, a load or FP operations.
    for name, w in (("all_c", "all_c = 1"), ("ld_st_c", "ld_c = 1, st_c = 1"),
                    ("all_e", "all_e = 1"), ("st_c", "st_c = 1"),
                    ("ld_c", "ld_c = 1")):
        tp[f"v3wait_{name}"] = ([f"wait {w}"], f"wait {w} every instruction")
    for name, a, w in (
            ("std_ld_st_c", "std,2 %dr1, 0x100, %dr3", "ld_c = 1, st_c = 1"),
            ("std_st_c", "std,2 %dr1, 0x100, %dr3", "st_c = 1"),
            ("ldd_ld_st_c", "ldd,0 %dr1, 0x100, {d}", "ld_c = 1, st_c = 1"),
            ("std_all_c", "std,2 %dr1, 0x100, %dr3", "all_c = 1"),
            ("faddd_all_c", "faddd,0 %dr4, %dr5, {d}", "all_c = 1"),
            ("faddd_all_e", "faddd,0 %dr4, %dr5, {d}", "all_e = 1"),
            ("fdivd_all_c", _FDIVD, "all_c = 1")):
        tp2[f"v3wait_{name}"] = ([a], [f"wait {w}"],
                                 f"{a.split(',')[0]}, wait {w} alternating")
    return tp, tp2


# Costs around the state registers (v3st_*, scheduled latency): a read
# to its consumer, a write to a read or a use of the new state, an FP
# operation to a read of the flags it sets (inexact: 1 + 2^-63 as a
# double, 1 + 2^-70 as an 80-bit value; as hazard test fadd_pfpfr).
# The setup reads the register into %r8, which the write writes back.
SCHED_V3ST = {
    "v3st_rrs_pfpfr_to_ands": (
        "addd,1 0x0, 0x0, %dr2",
        [("rrs,0 %pfpfr, %r8", None), ("ands,0 %r8, %r5, %r9", 12)], 16),
    "v3st_rrs_fpsr_to_ands": (
        "addd,1 0x0, 0x0, %dr2",
        [("rrs,0 %fpsr, %r8", None), ("ands,0 %r8, %r5, %r9", 12)], 16),
    "v3st_rrs_fpcr_to_ands": (
        "addd,1 0x0, 0x0, %dr2",
        [("rrs,0 %fpcr, %r8", None), ("ands,0 %r8, %r5, %r9", 12)], 16),
    "v3st_rws_pfpfr_to_rrs": (
        "addd,1 0x0, 0x0, %dr2; rrs,0 %pfpfr, %r8",
        [("rws,0 %r8, %pfpfr", None), ("rrs,0 %pfpfr, %r9", 12)], 12),
    "v3st_rws_pfpfr_to_faddd": (
        "addd,1 0x0, 0x0, %dr2; rrs,0 %pfpfr, %r8",
        [("rws,0 %r8, %pfpfr", None), ("faddd,0 %dr4, %dr5, %dr9", 12)], 12),
    "v3st_faddd_to_rrs_pfpfr": (
        "addd,1 0x0, 0x0, %dr2; addd,3 0x0, 0x3c00000000000000, %dr10",
        [("faddd,0 %dr4, %dr10, %dr9", None), ("rrs,0 %pfpfr, %r8", 12)], 12),
    "v3st_rws_fpcr_to_fxaddxx": (
        "addd,1 0x0, 0x0, %dr2; rrs,0 %fpcr, %r8; fdtofx,3 %dr4, %dr7",
        [("rws,0 %r8, %fpcr", None), ("fxaddxx,0 %dr7, %dr7, %dr9", 12)], 12),
    "v3st_fxadddx_to_rrs_fpsr": (
        "addd,1 0x0, 0x0, %dr2; fdtofx,3 %dr4, %dr7; "
        "addd,4 0x0, 0x3b90000000000000, %dr10",
        [("fxadddx,0 %dr7, %dr10, %dr9", None), ("rrs,0 %fpsr, %r8", 12)], 12),
    "v3st_rrd_ip_to_addd": (
        "addd,1 0x0, 0x0, %dr2",
        [("rrd,0 %ip, %dr8", None), ("addd,0 %dr8, %dr5, %dr9", 12)], 8),
    "v3st_rrs_upsr_to_ands": (
        "addd,1 0x0, 0x0, %dr2",
        [("rrs,0 %upsr, %r8", None), ("ands,0 %r8, %r5, %r9", 12)], 8),
    "v3st_rws_upsr_to_rrs": (
        "addd,1 0x0, 0x0, %dr2; rrs,0 %upsr, %r8",
        [("rws,0 %r8, %upsr", None), ("rrs,0 %upsr, %r9", 12)], 12),
}


def add_v3op(isa=3):
    """Add the v3op kernels for elbrus-vISA (once; every target)."""
    if V3OP:
        return
    chains = v3_op_chains(isa)
    chains.update(SCHED_V3ST)
    tp, tp2 = _v3_tp()
    for table, new in ((SCHED, chains), (TP, tp), (TP2, tp2)):
        assert not set(new) & set(table), set(new) & set(table)
        table.update(new)
        V3OP.update(new)


def tp_kernel(name, k):
    if name in TP:
        groups, what = [TP[name][0]], TP[name][1]
    elif name in TPN:
        groups, what = TPN[name]
    else:
        a, b, what = TP2[name]
        groups = [a, b]
    fn = f"tp_{name}_{k}"
    body, r = [], 0
    for i in range(k):
        ops = []
        for op in groups[i % len(groups)]:
            reg = TP_DEST[r % len(TP_DEST)]
            if op.startswith("nop"):
                ops.append(op)
                continue
            if "{s}" in op:
                ops.append(op.replace("{s}", f"%r{reg}"))
            elif op.startswith(("fadds", "fdivs", "fsqrts", "sdivs")):
                ops.append(op.replace("{d}", f"%r{reg}"))
            elif op.startswith("ldq,"):
                # A register pair: an even register, a fresh pair each time.
                ops.append(op.replace("{d}", f"%qr{8 + (2 * r) % 56}"))
            elif op.startswith(("qp", "ldqp")):
                ops.append(op.replace("{d}", f"%qpr{reg}"))
            else:
                ops.append(op.replace("{d}", f"%dr{reg}"))
            r += 1
        body.append("\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}")
    return f"""
	.global	{fn}
	.type	{fn}, #function
	.align	8
{fn}:
	{{
	  setwd	wsz = 0x20, nfx = 0x1, dbl = 0x1
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
	  nop 7
	  {V5_TP_INIT if name in TP_V5 else TP_INIT.get(name, "").replace("; ", chr(10) + chr(9) + "  ")}
	}}
	{{
	  nop 7
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
	}}
	{{
	  ct	%ctpr3
	}}
	.size	{fn}, .-{fn}
"""


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
	  {init.replace("; ", chr(10) + chr(9) + "  ") if " " in init else f"addd,0 0x0, {init}, %dr2"}
	}}
	{{
	  nop 7
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
	  {init.replace("; ", chr(10) + chr(9) + "  ") if " " in init else f"addd,0 0x0, {init}, %dr2"}
	}}
	{{
	  nop 7
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
        elif kind in ("pcmp_alu", "pcmp_ct"):
            # v6: a packed compare into a predicate (x > x in no lane:
            # false), then a predicated ALU op or ct.
            pc = "\t  pcmpgtbop,0 %dr3, %dr3, %pred2\n"
            use = ("\t{\n\t  addd,0 %dr5, 0x1, %dr5 ? %pred2\n\t}\n"
                   if kind == "pcmp_alu" else "\t{\n\t  ct\t%ctpr2 ? %pred2\n\t}\n")
            blocks.append(f"\t{{\n{pc}{nop_field(d)}{use}{nxt}:")
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


# Branch costs: cycles per block of a loop of 16 identical blocks, each
# ending at the label of the next. Compare with the plain blocks of the
# same shape (same number of wide instructions and nops) to get the cost
# of the transfer itself. %pred0 is false and %pred1 true throughout.
BR_BLOCKS = {
    "plain1": "\t{\n\t  nop\n\t}\n",
    "ibr_taken": "\t{\n\t  ibranch\t{nxt}\n\t}\n",
    "ibr_nottaken": "\t{\n\t  ibranch\t{nxt} ? %pred0\n\t}\n",
    "ibr_taken_pred": "\t{\n\t  ibranch\t{nxt} ? %pred1\n\t}\n",
    "plain3": "\t{\n\t  nop\n\t}\n\t{\n\t  nop 5\n\t}\n\t{\n\t  nop\n\t}\n",
    "ct_taken": ("\t{\n\t  disp\t%ctpr2, {nxt}\n\t}\n\t{\n\t  nop 5\n\t}\n"
                 "\t{\n\t  ct\t%ctpr2\n\t}\n"),
    "ct_taken_ipd0": ("\t{\n\t  disp\t%ctpr2, {nxt}; ipd 0\n\t}\n\t{\n\t  nop 5\n\t}\n"
                      "\t{\n\t  ct\t%ctpr2\n\t}\n"),
    "ct_taken_pred": ("\t{\n\t  disp\t%ctpr2, {nxt}\n\t}\n\t{\n\t  nop 5\n\t}\n"
                      "\t{\n\t  ct\t%ctpr2 ? %pred1\n\t}\n"),
    "ct_nottaken": ("\t{\n\t  disp\t%ctpr2, {nxt}\n\t}\n\t{\n\t  nop 5\n\t}\n"
                    "\t{\n\t  ct\t%ctpr2 ? %pred0\n\t}\n"),
    # A call to a leaf that prepares its return at entry and returns 6
    # cycles later: compare with plain3 plus the leaf's own 8 cycles.
    "call_return": ("\t{\n\t  disp\t%ctpr2, br_leaf\n\t}\n\t{\n\t  nop 5\n\t}\n"
                    "\t{\n\t  call\t%ctpr2, wbs = 0xc\n\t}\n"),
}


def br_kernel(name):
    fn = f"br_{name}"
    blocks = []
    for i in range(16):
        nxt = f".L{fn}_b{i}"
        blocks.append(BR_BLOCKS[name].replace("{nxt}", nxt) + f"{nxt}:")
    return f"""
	.global	{fn}
	.type	{fn}, #function
	.align	8
{fn}:
	{{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x1
	  disp	%ctpr1, .L{fn}
	  addd,0 0x0, 0x0, %dr5
	}}
	{{
	  cmpedb,0 %dr5, 0x1, %pred0
	  cmpedb,1 %dr5, 0x0, %pred1
	}}
	{{
	  nop 7
	}}
.L{fn}:
{chr(10).join(blocks)}
	{{
	  subd,0 %dr0, 0x1, %dr0
	  disp	%ctpr1, .L{fn}
	}}
	{{
	  nop 3
	  cmpedb,0 %dr0, 0x0, %pred2
	}}
	{{
	  ct	%ctpr1 ? ~%pred2
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


BR_LEAF = """
	.type	br_leaf, #function
	.align	8
br_leaf:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	br_leaf, .-br_leaf
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
    add_v3op()          # no-op when main() added them for its target
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
    decl, lat_rows, ctl_rows, hz_rows, tp_rows = [], [], [], [], []
    v3tp_rows, v3sched_rows = [], []    # the v3op kind (--only v3op)
    for name in list(TP) + list(TP2) + list(TPN):
        for k in (K1, K2):
            asm.append(tp_kernel(name, k))
            decl.append(f"long tp_{name}_{k}(long, void *);")
        (v3tp_rows if name in V3OP else tp_rows).append(
            f'\t{{"{name}", tp_{name}_{K1}, tp_{name}_{K2}}},')
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
            (v3sched_rows if name in V3OP else sched_rows).append(
                f'\t{{"{name}", {gap}, {fixed}, sched_{name}_{gap}}},')
    chase_rows = []
    for gap in CHASE_SCHED_GAPS:
        asm.append(sched_kernel("chase", gap, CHASE_SCHED))
        decl.append(f"long sched_chase_{gap}(long, void *);")
        chase_rows.append(f'\t{{{gap}, sched_chase_{gap}}},')
    br_rows = []
    asm.append(BR_LEAF)
    for name in BR_BLOCKS:
        asm.append(br_kernel(name))
        decl.append(f"long br_{name}(long, void *);")
        br_rows.append(f'\t{{"{name}", br_{name}}},')
    for d in CTL_DISTANCES:
        for kind in CTL_KINDS:
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
                        "fp_int_use_x", "disp_ct", "fpcr_rrs", "lsr_rrd",
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
#include <string.h>
#include <sys/mman.h>
unsigned long rdclk(void);
{chr(10).join(decl)}
typedef long (*kern)(long, void *);
static struct {{ const char *name; kern f1, f2; }} tp[] = {{
{chr(10).join(tp_rows)}
}};
static struct {{ const char *name; int ops; kern f1, f2; }} lat[] = {{
{chr(10).join(lat_rows)}
}};
static struct {{ const char *name; int gap, fixed; kern f; }} sched[] = {{
{chr(10).join(sched_rows)}
}};
/* The v3op kind: printed as tp and sched lines; a null name ends them. */
static struct {{ const char *name; kern f1, f2; }} v3tp[] = {{
{chr(10).join(v3tp_rows)}
	{{0}}
}};
static struct {{ const char *name; int gap, fixed; kern f; }} v3sched[] = {{
{chr(10).join(v3sched_rows)}
	{{0}}
}};
static struct {{ int gap; kern f; }} chase_sched[] = {{
{chr(10).join(chase_rows)}
}};
static struct {{ const char *name; kern f; }} br[] = {{
{chr(10).join(br_rows)}
}};
static struct {{ const char *kind; int d, blocks; kern f; }} ctl[] = {{
{chr(10).join(ctl_rows)}
}};
static struct {{ const char *name; int d; kern f; }} hz[] = {{
{chr(10).join(hz_rows)}
}};
static void *cell[512] __attribute__((aligned(4096)));
static const char *only;

/* With an argument, run only the kinds named in it ("tp,lat"). */
/* v3op (about 9,000 kernels) runs only when asked for by name.  */
static int want(const char *kind)
{{
	if (!only)
		return strcmp(kind, "v3op") != 0;
	return strstr(only, kind) != 0;
}}

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

int main(int argc, char **argv)
{{
	long n = {ITER};
	only = argc > 1 ? argv[1] : NULL;
	setvbuf(stdout, NULL, _IOLBF, 0);
	cell[0] = &cell[0];
	for (unsigned i = 0; want("tp") && i < sizeof tp / sizeof tp[0]; i++) {{
		void *p = cell;
		unsigned long t1 = best(tp[i].f1, n, p), t2 = best(tp[i].f2, n, p);
		printf("tp %s %.3f %.2f %.2f\\n", tp[i].name,
		       (double)(t2 - t1) / ((double)n * ({K2} - {K1})),
		       (double)t1 / n, (double)t2 / n);
	}}
	for (unsigned i = 0; want("v3op") && v3tp[i].name; i++) {{
		void *p = cell;
		unsigned long t1 = best(v3tp[i].f1, n, p), t2 = best(v3tp[i].f2, n, p);
		printf("tp %s %.3f %.2f %.2f\\n", v3tp[i].name,
		       (double)(t2 - t1) / ((double)n * ({K2} - {K1})),
		       (double)t1 / n, (double)t2 / n);
	}}
	memset(cell, 0, sizeof cell);	/* the throughput stores wrote it */
	cell[0] = &cell[0];
	for (unsigned i = 0; want("lat") && i < sizeof lat / sizeof lat[0]; i++) {{
		void *p = cell;
		unsigned long t1 = best(lat[i].f1, n, p), t2 = best(lat[i].f2, n, p);
		printf("lat %s %.3f\\n", lat[i].name,
		       (double)(t2 - t1) * lat[i].ops / ((double)n * ({K2} - {K1})));
	}}
	size_t sizes[] = {{ {sizes} }};
	for (unsigned i = 0; want("chase") && i < sizeof sizes / sizeof sizes[0]; i++) {{
		void **p = chain(sizes[i], 1), *q = p;
		long m = n / 8;
		bestc(lat_ldd_l1_ch0_{K2}, m, &q);
		unsigned long t1 = bestc(lat_ldd_l1_ch0_{K1}, m, &q),
			      t2 = bestc(lat_ldd_l1_ch0_{K2}, m, &q);
		printf("chase %zu %.3f\\n", sizes[i],
		       (double)(t2 - t1) / ((double)m * ({K2} - {K1})));
		free(p);
	}}
	for (unsigned i = 0; want("sched") && i < sizeof sched / sizeof sched[0]; i++) {{
		long m = n / 4;
		unsigned long t = best(sched[i].f, m, cell),
			      t0 = best(sched[i].f, m / 2, cell);
		printf("sched %s %d %d %.3f\\n", sched[i].name, sched[i].gap,
		       sched[i].fixed, (double)(t - t0) / ((double)(m - m / 2) * {SCHED_STEPS}));
	}}
	for (unsigned i = 0; want("v3op") && v3sched[i].name; i++) {{
		long m = n / 8;	/* half of sched's: there are thousands */
		unsigned long t = best(v3sched[i].f, m, cell),
			      t0 = best(v3sched[i].f, m / 2, cell);
		printf("sched %s %d %d %.3f\\n", v3sched[i].name, v3sched[i].gap,
		       v3sched[i].fixed, (double)(t - t0) / ((double)(m - m / 2) * {SCHED_STEPS}));
	}}
	size_t csizes[] = {{ {", ".join(str(x) for x, _ in chase_sched)} }};
	int chuge[] = {{ {", ".join(str(h) for _, h in chase_sched)} }};
	for (unsigned i = 0; want("chsched") && i < {nchs}; i++) {{
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
	for (unsigned i = 0; want("br") && i < sizeof br / sizeof br[0]; i++) {{
		unsigned long t = best(br[i].f, n / 4, cell);
		printf("br %s %.3f\\n", br[i].name, (double)t / ((double)(n / 4) * 16));
	}}
	for (unsigned i = 0; want("ctl") && i < sizeof ctl / sizeof ctl[0]; i++) {{
		unsigned long t = best(ctl[i].f, n / 4, cell);
		printf("ctl %s %d %.3f\\n", ctl[i].kind, ctl[i].d,
		       (double)t / ((double)(n / 4) * ctl[i].blocks));
	}}
	for (unsigned i = 0; want("hz") && i < sizeof hz / sizeof hz[0]; i++)
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
        ssh = ["ssh", *sshkey.options(sshkey.find_key(args.ssh_key),
                                      args.ssh_port),
               "-o", "ServerAliveInterval=30"]
        rdir = args.remote_dir
        subprocess.run(ssh + [args.remote, f"mkdir -p {rdir}"], check=True)
        subprocess.run(["scp", "-q"] + ssh[1:] + [str(prog), f"{args.remote}:{rdir}/"],
                       check=True)
        wrap = f"{args.wrap} " if args.wrap else ""
        if args.numa:
            cpu, mem = args.numa.split(":")
            wrap += f"numactl --cpunodebind={cpu} --preferred={mem} "
        cmd = ssh + [args.remote, f"{wrap}{rdir}/{prog.name}"
                     + (f" {args.only}" if args.only else "")]
    else:
        isa = re.match(r"v(\d+)", args.target).group(1)
        cmd = ["qemu-e2k", "-cpu", f"elbrus-v{isa}", "-L", str(sysroot), str(prog)]
        cmd += [args.only] if args.only else []
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
    res = {"tp": {}, "lat": {}, "sched": {}, "chase": {}, "chsched": {}, "br": {}, "ctl": {},
           "hz": {}}
    for line in text.splitlines():
        f = line.split()
        if f[0] == "tp":
            res["tp"][f[1]] = float(f[2])
        elif f[0] == "lat":
            res["lat"][f[1]] = float(f[2])
        elif f[0] == "sched":
            res["sched"].setdefault(f[1], []).append((int(f[2]), int(f[3]), float(f[4])))
        elif f[0] == "chsched":
            res["chsched"].setdefault(f[1], []).append((int(f[2]), float(f[3])))
        elif f[0] == "chase":
            res["chase"][int(f[1])] = float(f[2])
        elif f[0] == "br":
            res["br"][f[1]] = float(f[2])
        elif f[0] == "ctl":
            res["ctl"].setdefault(f[1], []).append((int(f[2]), float(f[3])))
        elif f[0] == "hz":
            res["hz"].setdefault(f[1], []).append((int(f[2]), int(f[3])))
    return res


def sched_latency(pts):
    """Smallest gap at which a step costs exactly gap + fixed cycles."""
    # The loop's own overhead adds a few tenths of a cycle per step: the
    # smallest excess over gap + fixed among the gaps (a stall adds at
    # least 3), up to 0.45 above it.  On the 4C it is sometimes 0.46-0.5
    # for a whole run; a stray reading below gap + fixed counts as 0.
    over = max(0.0, min(t - gap - fixed for gap, fixed, t in pts))
    if over > 1.5:
        return None              # every gap stalls
    for gap, fixed, t in pts:
        if t <= gap + fixed + over + 0.45:
            return gap
    return None


def chase_latency(pts):
    """Smallest gap at which a chased load no longer stalls; memory times
    vary, so allow 2% plus the loop overhead."""
    for gap, t in pts:
        if t <= gap * 1.02 + 0.6:
            return gap
    return None


def tp_what(name):
    if name in TP:
        return TP[name][1]
    return TPN[name][1] if name in TPN else TP2[name][2]


def report(res):
    if res["tp"]:
        print("Throughput (cycles per wide instruction of independent ops):")
        for name, v in res["tp"].items():
            print(f"  {name:14} {v:6.2f}  {tp_what(name)}")
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
    print("Branch costs: cycles per block (compare with plain1 / plain3)")
    for name, v in res["br"].items():
        print(f"  {name:15} {v:.2f}")
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
             f"numa: '{args.numa or 'not pinned'}'",
             f"date: {datetime.date.today().isoformat()}",
             "throughput:"]
    for name, v in res["tp"].items():
        lines.append(f"  {name}: {{cycles_per_instruction: {v:.2f}, "
                     f"what: {tp_what(name)!r}}}")
    lines.append("back_to_back:")
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
    lines.append("branch_cycles_per_block:")
    for name, v in res["br"].items():
        lines.append(f"  {name}: {v:.2f}")
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
                    help="default: $RELCC2_SSH_KEY or secrets/id_ed25519")
    ap.add_argument("--ssh-port", type=int, metavar="PORT")
    ap.add_argument("--out", type=Path, default=ROOT / "toolchains" / "isa-timing")
    ap.add_argument("--yaml", type=Path)
    ap.add_argument("--remote-dir", default="relcc2/isa-timing",
                    help="with --remote: working directory on the host, "
                    "relative to $HOME (one per task; shared hosts)")
    ap.add_argument("--memory", action="store_true",
                    help="also measure load latency beyond L1 (pointer chases "
                    "over 256 KiB..128 MiB; informational, slow)")
    ap.add_argument("--numa", metavar="CPU:MEM",
                    help="with --remote: run on NUMA node CPU, allocating "
                    "on node MEM (numactl --preferred)")
    ap.add_argument("--only", metavar="KIND,...",
                    help="run only these kinds (tp, lat, chase, sched, "
                    "chsched, br, ctl, hz, v3op); not with --yaml")
    ap.add_argument("--chains", metavar="REGEX",
                    help="with --only v3op: build only the v3op kernels "
                    "whose names match REGEX (re.search)")
    ap.add_argument("--save", type=Path, metavar="FILE",
                    help="also write the run's output to FILE, as "
                    "--results reads it")
    ap.add_argument("--wrap", metavar="CMD",
                    help="with --remote: a prefix for the command on the "
                    "host (e.g. flock -w 3600 LOCKFILE)")
    ap.add_argument("--results", type=Path, metavar="FILE",
                    help="don't run: read the output of an earlier run "
                    "(e.g. one started with nohup on the host)")
    ap.add_argument("--machine", default="unspecified",
                    help="description recorded in --yaml output")
    args = ap.parse_args()
    if args.only and args.yaml:
        ap.error("--yaml needs a full run")
    if args.chains and (args.only != "v3op" or args.results):
        ap.error("--chains needs --only v3op and a run")

    m = re.match(r"v(\d+)", args.target)
    if m and int(m.group(1)) >= 5:
        add_v5()
    if m and int(m.group(1)) >= 6:
        add_v6()
    add_v3op(int(m.group(1)) if m else 3)
    if args.chains:
        for table in (SCHED, TP, TP2):
            for name in [n for n in table if n in V3OP
                         and not re.search(args.chains, n)]:
                del table[name]
    if args.results:
        res = parse(args.results.read_text())
        report(res)
        if args.yaml:
            write_yaml(args.yaml, res, args)
        return
    lcc, sysroot = find_lcc(args.target)
    generate(args.out, args.memory)
    prog = args.out / "bench"
    subprocess.run([str(lcc), "-O1", str(args.out / "main.c"),
                    str(args.out / "kernels.s"), "-o", str(prog)], check=True)
    text = run(args, prog, sysroot)
    if args.save:
        args.save.write_text(text)
    res = parse(text)
    report(res)
    if args.yaml:
        write_yaml(args.yaml, res, args)


if __name__ == "__main__":
    main()
