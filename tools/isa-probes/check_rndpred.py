#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check rndpred.c's output against the model of %rndpr and %rndpred1-15
in spec/isa/predicates.md ("Random predicates").

  python3 tools/isa-probes/check_rndpred.py DIR

DIR holds what ./rndpred printed (stdout.txt) and the files it wrote
(seqN.txt, trN.txt, mix1.txt, zero.txt, zeroa.txt, twoN.txt, tri.txt,
cond.txt).  Prints the number of reads (or lines) that disagree with
the model; 0 everywhere means the model is exact.  For %rndpred1-5 the
million-read runs are checked by their count and first 64 reads (the
probe records each read only for N >= 6).

The model: %rndpr holds two Fibonacci LFSRs, a = bits 0-16 (x^17 + x^14
+ 1) and b = bits 32-49 (x^18 + x^11 + 1); every wide instruction with
an ELP reading any %rndpredN shifts both left by one, the feedback
entering bit 0, after the reads.  %rndpredN is true when a[k] ==
b[16 - k] for k < M1(N) and a[i] == b[6 - i] for i < M2(N).
"""

import re
import sys
from pathlib import Path

A, B = (1 << 17) - 1, (1 << 18) - 1
M1 = {n: n + 1 for n in range(1, 8)} | {8: 10, 9: 12, 10: 14, 11: 16} \
    | {n: 17 for n in range(12, 16)}
M2 = {n: 0 for n in range(1, 12)} | {12: 1, 13: 3, 14: 6, 15: 7}


def step(a, b):
    return (((a << 1) | ((a >> 16 ^ a >> 13) & 1)) & A,
            ((b << 1) | ((b >> 17 ^ b >> 10) & 1)) & B)


def rnd(a, b, n):
    return (all(((a >> k) ^ (b >> (16 - k))) & 1 == 0 for k in range(M1[n]))
            and all(((a >> i) ^ (b >> (6 - i))) & 1 == 0
                    for i in range(M2[n])))


def split(s):
    return s & A, (s >> 32) & B


def main():
    d = Path(sys.argv[1])
    out = (d / "stdout.txt").read_text()
    # The million-read runs follow each other from the start state.
    a, b = split(int(re.search(r"at start:\n +([0-9a-f]+)", out)[1], 16))
    for n in range(1, 16):
        m = re.search(rf"%rndpred{n} +([0-9.]+) ([01]+)", out)
        ones = None
        if (d / f"seq{n}.txt").exists():
            t = (d / f"seq{n}.txt").read_text().split()
            assert split(int(t[0], 16)) == (a, b), f"seq{n} start"
            ones = set(map(int, t[1:]))
        cnt, first, bad = 0, "", 0
        for i in range(1000000):
            v = rnd(a, b, n)
            cnt += v
            if i < 64:
                first += "01"[v]
            if ones is not None:
                bad += v != (i in ones)
            a, b = step(a, b)
        bad += (f"{cnt / 1e6:.6f}" != m[1]) + (first != m[2])
        print(f"%rndpred{n:<2}: {cnt / 1e6:.6f} true, {bad} disagree")
    # The state lines: idle, unused ELPs, getpid.
    for what, steps in (("idle 1e6 iterations", 0), ("1 unused ELP", 1),
                        ("2 unused ELPs", 2), ("getpid", 0)):
        m = re.search(rf"{what}: ([0-9a-f]+) -> ([0-9a-f]+)", out)
        a, b = split(int(m[1], 16))
        for _ in range(steps):
            a, b = step(a, b)
        bad = int((a, b) != split(int(m[2], 16)))
        print(f"{what}: {bad} disagree")
    # The same state twice: 64 reads of %rndpred8.
    for what, s0 in (("from -1:", -1), ("from -1 again:", -1),
                     ("getpid at 32:", -1),
                     ("from 1234500006789:", 0x0001234500006789)):
        m = re.search(rf"{re.escape(what)} +([01]+)", out)
        a, b = split(s0)
        got = ""
        for _ in range(64):
            got += "01"[rnd(a, b, 8)]
            a, b = step(a, b)
        print(f"{what} {int(got != m[1])} disagree")
    # Single-bit flips of all ones and all zeros: the bits whose flip
    # makes one read false.
    bad = 0
    for m in re.finditer(r"%rndpred(\d+) +from (ones|zeros): (\d); flips:"
                         r"([ \d]*)", out):
        n, base = int(m[1]), (0x0003ffff0001ffff if m[2] == "ones" else 0)
        a, b = split(base)
        want = [k for k in list(range(17)) + list(range(32, 50))
                if not rnd(*split(base ^ (1 << k)), n)]
        bad += ((str(int(rnd(a, b, n))) != m[3])
                + (want != [int(x) for x in m[4].split()]))
    print(f"flips: {bad} disagree")
    files = sorted(d.glob("tr[0-9]*.txt")) + sorted(d.glob("two*.txt"))
    files += [d / f for f in ("tri.txt", "mix1.txt", "zero.txt",
                              "zeroa.txt")]
    for f in files:
        k = f.stem
        ns = ([3, 7, 11] if k == "tri" else
              [int(k[3:])] * 2 if k.startswith("two") else
              [1] if k in ("mix1", "zero", "zeroa") else [int(k[2:])])
        bad, prev = 0, None
        for line in f.read_text().splitlines():
            s, bits = line.split()
            a, b = split(int(s, 16))
            bad += prev is not None and step(*prev) != (a, b)
            bad += "".join("01"[rnd(a, b, n)] for n in ns) != bits
            prev = (a, b)
        print(f"{f.name}: {bad} disagree")
    if (d / "cond.txt").exists():
        bad = 0
        for line in (d / "cond.txt").read_text().splitlines():
            s, bits = line.split()
            a, b = split(int(s, 16))
            bad += "".join("01"[rnd(a, b, n)] for n in range(8, 16)) != bits
        print(f"cond.txt: {bad} disagree")


if __name__ == "__main__":
    main()
