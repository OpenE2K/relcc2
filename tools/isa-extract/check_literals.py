#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Show how MCST gas encodes literal operands (spec/isa/bundle.md,
"Literal values"): assemble one wide instruction per case with gas, then
print what objdump makes of it, or gas's error.

  python3 tools/isa-extract/check_literals.py [--binutils DIR] [--work DIR]
"""

import argparse
import os
import re
import subprocess

# (wide instruction, what it shows)
CASES = [
    # What gas picks for a bare constant.
] + [("ldd,0 %%dr0, %s, %%dr1" % v, "ldd, bare %s" % v) for v in (
    "0", "15", "16", "31", "32", "-1", "-16", "0x7fff", "0x8000", "-0x8000",
    "-0x8001", "0xffff", "0x7fffffff", "0x80000000", "-0x80000000",
    "-0x80000001", "0xffffffff", "0x100000000", "0xffffffffffffffff")] + [
    ("addd,0 %%dr0, %s, %%dr1" % v, "addd, bare %s" % v) for v in (
        "-1", "0xffffffff", "0x100000000")] + [
    ("adds,0 %%r0, %s, %%r1" % v, "adds, bare %s" % v) for v in (
        "-1", "0x80000000", "0xffffffff", "0x100000000")] + [
    ("getsp,0 %s, %%dr1" % v, "getsp, bare %s" % v) for v in (
        "-96", "-0x10000", "0x10")] + [
    # Explicit formats and slots.
    ("ldd,0 %dr0, _f16s 0x8000, %dr1", "_f16s 0x8000"),
    ("ldd,0 %dr0, _f16 0x8000, %dr1", "_f16 0x8000 (unsigned spelling)"),
    ("ldd,0 %dr0, _f16s 0x12345, %dr1", "_f16s too large"),
    ("ldd,0 %dr0, _f32s 0x123456789, %dr1", "_f32s too large"),
    ("ldd,0 %dr0, _f16s -1, %dr1", "_f16s -1 (minus after the format)"),
    ("ldd,0 %dr0, _f32s -1, %dr1", "_f32s -1"),
    ("ldd,0 %dr0, _f64 -1, %dr1", "_f64 -1"),
    ("ldd,0 %dr0, _f16s (-1), %dr1", "_f16s (-1)"),
    ("ldd,0 %dr0, _f16s,_lts0lo 0xffff, %dr1", "_f16s,_lts0lo 0xffff"),
    ("ldd,0 %dr0, _f16s,_lts1hi 0x10, %dr1", "_f16s,_lts1hi"),
    ("ldd,0 %dr0, _f32s,_lts2 0x10, %dr1", "_f32s,_lts2"),
    ("ldd,0 %dr0, _f64,_lts1 0x10, %dr1", "_f64,_lts1"),
    ("addd,0 %dr0, _f32 0xffffffff, %dr1", "_f32 0xffffffff on addd"),
    ("addd,0 %dr0, _f64 0xffffffff, %dr1", "_f64 0xffffffff on addd"),
    ("adds,0 %r0, _f64 0x10, %r1", "_f64 on a 32-bit op"),
    # src1 literals.
    ("ldd,0 0x10, %dr0, %dr1", "src1 0x10"),
    ("ldd,0 0x1f, %dr0, %dr1", "src1 0x1f"),
    ("ldd,0 0x20, %dr0, %dr1", "src1 0x20"),
    ("ldd,0 -8, %dr0, %dr1", "src1 -8"),
    ("ldd,0 0x5, 0x10, %dr1", "absolute: src1 and src2 literals"),
    ("adds,0 0x1f, %r0, %r1", "adds src1 0x1f"),
    # Store data.
    ("std,2 %dr0, -8, %dr1", "std offset -8"),
    ("std,2 %dr0, 0x8, 0x5", "std literal datum"),
    # Several literals in one wide instruction.
    ("ldd,0 %dr0, 0x12345, %dr1\n\tldd,2 %dr0, 0x12345, %dr2",
     "two loads, one value"),
    ("ldd,0 %dr0, 0x100, %dr1\n\tldd,2 %dr0, 0x200, %dr2\n\t"
     "ldd,3 %dr0, 0x300, %dr3\n\tldd,5 %dr0, 0x400, %dr4",
     "four 16-bit values"),
    ("ldd,0 %dr0, 0x100, %dr1\n\tldd,2 %dr0, 0x200, %dr2\n\t"
     "ldd,3 %dr0, 0x300, %dr3\n\tldd,5 %dr0, 0x400, %dr4\n\t"
     "addd,1 %dr0, 0x500, %dr5", "five 16-bit values"),
    ("ldd,0 %dr0, 0x10000, %dr1\n\tldd,2 %dr0, 0x20000, %dr2\n\t"
     "ldd,3 %dr0, 0x30000, %dr3\n\tldd,5 %dr0, 0x40000, %dr4",
     "four 32-bit values"),
    ("ldd,0 %dr0, 0x10000, %dr1\n\tldd,2 %dr0, 0x20000, %dr2\n\t"
     "ldd,3 %dr0, 0x30000, %dr3\n\tldd,5 %dr0, 0x40000, %dr4\n\t"
     "addd,1 %dr0, 0x50000, %dr5", "five 32-bit values"),
    ("ldd,0 %dr0, 0x100000000, %dr1\n\tldd,2 %dr0, 0x200000000, %dr2",
     "two 64-bit values"),
    ("ldd,0 %dr0, 0x100000000, %dr1\n\tldd,2 %dr0, 0x200000000, %dr2\n\t"
     "ldd,3 %dr0, 0x300000000, %dr3", "three 64-bit values"),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--binutils", default="toolchains/binutils/bin")
    ap.add_argument("--work", default="toolchains/literals")
    args = ap.parse_args()
    os.makedirs(args.work, exist_ok=True)
    gas = os.path.join(args.binutils, "e2k-linux-as")
    objdump = os.path.join(args.binutils, "e2k-linux-objdump")
    s = os.path.join(args.work, "t.s")
    o = os.path.join(args.work, "t.o")
    for text, what in CASES:
        with open(s, "w") as f:
            f.write("\t.text\n\t.ignore ld_st_style\nf:\n\t{\n\t%s\n\t}\n"
                    % text)
        r = subprocess.run([gas, "-mcpu=elbrus-v3", "-mptr64", s, "-o", o],
                           capture_output=True, text=True)
        msgs = [l.split("Error: ", 1)[1] for l in r.stderr.splitlines()
                if "Error: " in l]
        if r.returncode != 0:
            print("%-36s ERROR %s" % (what, msgs[0] if msgs else r.stderr))
            continue
        d = subprocess.run([objdump, "-dr", o], capture_output=True,
                           text=True).stdout
        ops = [l.strip() for l in d.splitlines()
               if re.match(r"\s+(ldd|std|addd|adds|getsp)", l)
               or "R_E2K" in l]
        print("%-36s %s" % (what, " | ".join(ops)))


if __name__ == "__main__":
    main()
