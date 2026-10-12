#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check isa-score's stall rule against spec/isa/timing-e2s.yaml, and
its latency rules against the edges timing.md and apb.md give.

For a chain of one operation with latency L, a consumer placed G cycles
after its producer costs G when G >= L, and otherwise G plus the stall
model.stall(L - G) (4-cycle steps).  Every single-operation chain the
4C measured must agree to within the loop overhead (under 0.5 cycles).
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analysis  # noqa: E402
import model  # noqa: E402

CHAINS = ["addd", "add_addd", "muld", "faddd", "fmuld", "fmul_addd",
          "fdivs", "fdivd", "fsqrts", "sdivd", "ldd"]

# (ISA version, setwd dbl, producer, consumer, register, cycles): edges
# from timing.md ("Formats": 32-bit results read as 64 bits,
# tools/isa-probes/tagx.c; "Elbrus-8C2": the narrow operand of an 80-bit
# operation, sregs.c latg and lp) and apb.md ("AAU loads", memleft.c).
EDGES = [
    (3, 0, "adds,0 %r8, 0x0, %r8", "addd,0 %dr8, 0x0, %dr8", "r8", 4),
    (3, 0, "ands,0 %r8, %r8, %r8", "andd,0 %dr8, %dr8, %dr8", "r8", 4),
    (3, 0, "shls,0 %r8, 0x0, %r8", "shld,0 %dr8, 0x0, %dr8", "r8", 4),
    (3, 0, "adds,0 %r8, 0x0, %r8", "addd,1 %dr8, 0x0, %dr8", "r8", 4),
    (3, 0, "adds,0 %r8, 0x0, %r8", "addd,3 %dr8, 0x0, %dr8", "r8", 4),
    (3, 0, "adds,3 %r8, 0x0, %r8", "addd,4 %dr8, 0x0, %dr8", "r8", 4),
    (3, 0, "adds,0 %r8, 0x0, %r8", "movtd,0 %dr8, %dr8", "r8", 4),
    (3, 0, "adds,0 %r8, 0x0, %r8", "gettagd,2 %dr8, %r8", "r8", 4),
    (3, 0, "gettagd,2 %dr8, %r8", "gettagd,2 %dr8, %r8", "r8", 4),
    (3, 0, "adds,0 %r8, 0x0, %r8", "adds,0 %r8, 0x0, %r8", "r8", 1),
    (3, 0, "adds,0 %r8, 0x0, %r8", "sxt,0 0x2, %r8, %dr8", "r8", 1),
    (3, 0, "muls,0 %r8, 0x1, %r8", "addd,0 %dr8, 0x0, %dr8", "r8", 6),
    (3, 0, "addd,0 %dr8, 0x0, %dr8", "adds,0 %r8, 0x0, %r8", "r8", 1),
    (3, 0, "ldw,0 %dr0, 0x0, %r8", "addd,0 %dr8, 0x0, %dr8", "r8", 3),
    (3, 1, "adds,0 %r8, 0x0, %r8", "addd,0 %dr8, 0x0, %dr8", "r8", 1),
    (3, 1, "adds,0 %r8, 0x0, %r8", "addd,3 %dr8, 0x0, %dr8", "r8", 2),
    (5, 0, "adds,0 %r8, 0x0, %r8", "addd,0 %dr8, 0x0, %dr8", "r8", 4),
    (6, 0, "adds,0 %r8, 0x0, %r8", "addd,0 %dr8, 0x0, %dr8", "r8", 4),
    (3, 1, "faddd,0 %dr9, %dr5, %dr10", "fxadddd,0 %dr7, %dr10, %dr9",
     "r10", 4),
    (5, 1, "faddd,0 %dr9, %dr5, %dr10", "fxadddd,0 %dr7, %dr10, %dr9",
     "r10", 6),
    (6, 1, "faddd,0 %dr9, %dr5, %dr10", "fxadddd,0 %dr7, %dr10, %dr9",
     "r10", 4),
    (3, 1, "fxadddd,0 %dr7, %dr9, %dr10", "fxadddd,0 %dr7, %dr10, %dr9",
     "r10", 5),
    (5, 1, "fxadddd,0 %dr7, %dr9, %dr10", "fxadddd,0 %dr7, %dr10, %dr9",
     "r10", 7),
    (5, 1, "fxtofd,0 %dr10, %dr9", "fxadddx,0 %dr7, %dr9, %dr10", "r9", 7),
    (5, 1, "fxdivtss,5 %dr8, %r7, %r8", "fxdivtss,5 %dr8, %r7, %r8", "r8",
     18),
    (5, 1, "fxdivtdd,5 %dr9, %r7, %dr9", "fxdivtdd,5 %dr9, %r7, %dr9",
     "r9", 18),
    (5, 1, "fxdivdd,5 %dr7, %dr9, %dr9", "fxdivdd,5 %dr7, %dr9, %dr9",
     "r9", 18),
    (5, 1, "fxdivxx,5 %dr7, %dr6, %dr6", "fxdivxx,5 %dr7, %dr6, %dr6",
     "r6", 16),
    (5, 1, "fxdivtsx,5 %dr4, %r6, %dr6", "fxdivtsx,5 %dr4, %r6, %dr6",
     "r6", 16),
    (5, 1, "fxadddd,0 %dr7, %dr9, %dr10", "faddd,0 %dr10, %dr5, %dr9",
     "r10", 5),
    (3, 1, "ldd,0 %dr0, 0x0, %dr10", "fxadddd,0 %dr7, %dr10, %dr9", "r10",
     3),
    (5, 1, "ldd,0 %dr0, 0x0, %dr10", "fxadddd,0 %dr7, %dr10, %dr9", "r10",
     4),
    (5, 1, "ldd,3 %dr0, 0x0, %dr10", "fxadddd,0 %dr7, %dr10, %dr9", "r10",
     4),
    (5, 1, "ldd,0 %dr0, 0x0, %dr10", "faddd,0 %dr10, %dr5, %dr9", "r10", 3),
    (5, 1, "ldd,0 %dr0, 0x0, %dr5", "fxdivtdx,5 %dr5, %r6, %dr6", "r5", 4),
    (5, 1, "addd,0 %dr0, 0x0, %dr10", "fxadddd,0 %dr7, %dr10, %dr9", "r10",
     1),
    (3, 1, "ldaad,2 %aad0 [ %aasti1 ], %dr4", "addd,0 %dr4, 0x0, %dr5",
     "r4", 9),
    (3, 1, "ldaad,5 %aad0 [ %aasti1 ], %dr4", "addd,0 %dr4, 0x0, %dr5",
     "r4", 9),
    (5, 1, "ldaad,2 %aad0 [ %aasti1 ], %dr4", "addd,0 %dr4, 0x0, %dr5",
     "r4", 11),
    (6, 1, "ldaad,2 %aad0 [ %aasti1 ], %dr4", "addd,0 %dr4, 0x0, %dr5",
     "r4", 13),
]


class _Names:
    known_names = set()


def op(text, iset, dbl):
    o = model.parse_line(text, _Names)
    o.iset, o.dbl = iset, dbl
    return o


def stall_rule():
    data = model.load_yaml((model.ROOT / "spec" / "isa" / "timing-e2s.yaml")
                           .read_text())["scheduled_latency"]
    bad = 0
    for name in CHAINS:
        lat = data[name]["latency"]
        for gap, measured in data[name]["cycles_per_step_by_gap"].items():
            predicted = gap + model.stall(lat - gap)
            ok = abs(measured - predicted) < 0.5
            bad += not ok
            if not ok:
                print(f"{name} gap {gap}: measured {measured}, "
                      f"model {predicted}")
    print("stall rule:", "ok" if not bad else f"{bad} disagreements")
    return bad


def edges():
    bad = 0
    for iset, dbl, ptext, ctext, reg, want in EDGES:
        p, c = op(ptext, iset, dbl), op(ctext, iset, dbl)
        key = ("r", int(reg[1:]))
        got = analysis.latency(p, c, key, p.chan, c.chan)
        if got != want:
            bad += 1
            print(f"v{iset} dbl {dbl}: {ptext} -> {ctext}: "
                  f"model {got}, measured {want}")
    print("latency rules:", "ok" if not bad else f"{bad} disagreements")
    return bad


def main():
    return 1 if stall_rule() + edges() else 0


if __name__ == "__main__":
    sys.exit(main())
