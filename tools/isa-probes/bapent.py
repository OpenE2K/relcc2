#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build bapent.c's binaries: lcc's -O3 array-prefetch loops with N more
cycles between their bap and the loop (spec/isa/apb.md, "v6").

  python3 tools/isa-probes/bapent.py [--target v6.2c3] [--out DIR] [--max 8]

Compiles tools/isa-score/validate/kernels.c with the lcc in
/opt/mcst/lcc-1.29.16.e2k-TARGET.linux-6.1 at -O3, writes DIR/k3xN.s
with an empty wide instruction of N cycles after each instruction that
holds a bap (N = 0 .. --max, at most 8), links DIR/bapentN with
bapent.c, and prints each kernel's distance from bap to its loop as
assembled (gas may pad before a jump target, hwbugs.md), using
toolchains/binutils's objdump when it is there.  Copy the binaries to
the machine and run them as bapent.c says.
"""

import argparse
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LCC = "/opt/mcst/lcc-1.29.16.e2k-{}.linux-6.1/bin/lcc"
OBJDUMP = ROOT / "toolchains" / "binutils" / "bin" / "e2k-linux-objdump"
KERNELS = ("k_sum_O3", "k_saxpy_O3", "k_copy_O3")


def pad(asm, n):
    """ASM with an empty instruction of N cycles after each bap's."""
    out, after_bap = [], False
    for line in asm.split("\n"):
        out.append(line)
        if re.match(r"\s*bap\s*$", line):
            after_bap = True
        elif after_bap and line.strip() == "}":
            if n:
                out += ["\t{", f"\t  nop {n - 1}" if n > 1 else "\t  nop", "\t}"]
            after_bap = False
    return "\n".join(out)


def distances(binary):
    """Kernel -> cycles from its bap to the instruction after the
    unconditional ct that enters the loop."""
    txt = subprocess.run([str(OBJDUMP), "-d", str(binary)], check=True,
                         capture_output=True, text=True).stdout
    out = {}
    for f in re.split(r"\n(?=[0-9a-f]+ <)", txt):
        m = re.match(r"[0-9a-f]+ <(\w+)>:", f)
        if not m or m.group(1) not in KERNELS:
            continue
        parts = re.split(r"\n\s+[0-9a-f]+:\s*\n", "\n" + f.split("\n", 1)[1])
        t = None
        for ins in parts[1:]:
            if t is None and re.search(r"\bbap\b", ins):
                t = 0
            if t is None:
                continue
            nop = re.search(r"\bnop (\d+)", ins)
            t += 1 + (int(nop.group(1)) if nop else 0)
            if re.search(r"^\s*ct %ctpr\d\s*$", ins, re.M) and "?" not in ins:
                out[m.group(1)] = t
                break
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--target", default="v6.2c3")
    ap.add_argument("--out", type=Path, default=ROOT / "toolchains" / "bapent")
    ap.add_argument("--max", type=int, default=8, choices=range(9))
    a = ap.parse_args()
    lcc = LCC.format(a.target)
    a.out.mkdir(parents=True, exist_ok=True)
    base = a.out / "k3.s"
    subprocess.run([lcc, "-O3", "-DSUF=O3", "-S", "-o", str(base),
                    str(ROOT / "tools" / "isa-score" / "validate" / "kernels.c")],
                   check=True)
    asm = base.read_text()
    for n in range(a.max + 1):
        s = a.out / f"k3x{n}.s"
        s.write_text(pad(asm, n))
        exe = a.out / f"bapent{n}"
        subprocess.run([lcc, "-O1", str(Path(__file__).with_name("bapent.c")),
                        str(ROOT / "tools" / "isa-score" / "validate" / "clk.s"),
                        str(s), "-o", str(exe)], check=True)
        if OBJDUMP.exists():
            d = distances(exe)
            print(f"bapent{n}: bap to loop " + ", ".join(
                f"{k[2:-3]} {d.get(k, '?')}" for k in KERNELS))
        else:
            print(f"bapent{n}: built")


if __name__ == "__main__":
    main()
