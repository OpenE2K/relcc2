#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check isa-score's stall rule against spec/isa/timing-e2s.yaml.

For a chain of one operation with latency L, a consumer placed G cycles
after its producer costs G when G >= L, and otherwise G plus the stall
model.stall(L - G) (4-cycle steps).  Every single-operation chain the
4C measured must agree to within the loop overhead (under 0.5 cycles).
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import model  # noqa: E402

CHAINS = ["addd", "add_addd", "muld", "faddd", "fmuld", "fmul_addd",
          "fdivs", "fdivd", "fsqrts", "sdivd", "ldd"]


def main():
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
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
