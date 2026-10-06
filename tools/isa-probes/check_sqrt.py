#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check sqrt.c's output against exactly rounded results (spec/isa/
semantics.md, "Square root and reciprocals").

  python3 tools/isa-probes/check_sqrt.py out.txt

For fsqrt2 (fsqrtid + fsqrttd), fsqrts, fdivd, fdivs: the number of
results that differ from the exactly rounded one in each rounding mode.
For frcps and frsqrts: how far from the exact value (in units in the
last place) and how often they equal the rounded result.  For fsqrtid:
how close it is to sqrt(x).
"""

import math
import struct
import sys
from collections import defaultdict
from fractions import Fraction


def d2f(b):
    return struct.unpack("<d", struct.pack("<Q", b))[0]


def s2f(b):
    return struct.unpack("<f", struct.pack("<I", b & 0xffffffff))[0]


def isqrt_frac(fr, bits):
    """floor(sqrt(fr) * 2**bits) for a positive Fraction."""
    n, d = fr.numerator, fr.denominator
    return math.isqrt(n * 4 ** bits // d) if d == 1 else \
        math.isqrt((n << (2 * bits)) // d)


def round_frac(fr, mant, emin, mode):
    """Round a positive Fraction to a binary float with MANT significand
    bits and minimum normal exponent EMIN; return the Fraction value
    (or math.inf)."""
    if fr == 0:
        return Fraction(0)
    e = fr.numerator.bit_length() - fr.denominator.bit_length()
    if Fraction(2) ** e > fr:
        e -= 1
    e = max(e, emin)
    q = Fraction(2) ** (e - mant + 1)
    lo = (fr / q).__floor__()
    rem = fr / q - lo
    if mode == 0:
        up = rem > Fraction(1, 2) or (rem == Fraction(1, 2) and lo % 2)
    elif mode in (1, 3):
        up = False
    else:
        up = rem > 0
    return (lo + up) * q


def exact_sqrt_rounded(x, mant, emin, mode):
    # Exact enough: sqrt of a Fraction to 3 * mant extra bits.
    # Enough bits for any double, subnormal ones included.
    bits = 1300
    s = Fraction(isqrt_frac(x, bits), 2 ** bits)
    exact_sq = s * s == x
    r = round_frac(s, mant, emin, mode)
    if not exact_sq and mode == 2 and r == s:
        r += Fraction(2) ** (r.numerator.bit_length()
                             - r.denominator.bit_length() - mant)
    return r


def frac(v):
    return Fraction(v)


def main():
    stats = defaultdict(lambda: [0, 0])
    approx = defaultdict(list)
    other = []
    for line in open(sys.argv[1]):
        p = line.split()
        if p[0] == "lat":
            print(line.strip())
            continue
        op, mode = p[0], int(p[1])
        x, y, r, flags = (int(v, 16) for v in p[2:6])
        if op in ("fsqrt2", "fdivd"):
            xv = d2f(x) if op == "fsqrt2" else 1.0 / 1.0
            arg = d2f(x) if op == "fsqrt2" else d2f(y)
            rv = d2f(r)
            if not (math.isfinite(arg) and arg > 0) or (op == "fdivd" and
                                                        arg < 1e-300):
                other.append(line.strip())
                continue
            if op == "fsqrt2":
                want = exact_sqrt_rounded(frac(arg), 53, -1022, mode)
            else:
                want = round_frac(1 / frac(arg), 53, -1022, mode)
            st = stats[(op, mode)]
            st[0] += 1
            if not math.isfinite(rv) or frac(rv) != want:
                st[1] += 1
                if st[1] <= 3:
                    print("MISMATCH", line.strip(), float(want))
        elif op in ("fsqrts", "fdivs", "frcps", "frsqrts"):
            arg = s2f(y if op == "fdivs" else x)
            rv = s2f(r)
            if not (math.isfinite(arg) and arg > 0):
                other.append(line.strip())
                continue
            if op == "fsqrts":
                want = exact_sqrt_rounded(frac(arg), 24, -126, mode)
                exact = None
            elif op in ("fdivs", "frcps"):
                exact = 1 / frac(arg)
                want = round_frac(exact, 24, -126, mode)
            else:
                exact = 1 / Fraction(math.sqrt(arg)) if False else None
                xs = frac(arg)
                bits = 400
                s = Fraction(isqrt_frac(xs, bits), 2 ** bits)
                exact = 1 / s
                want = round_frac(exact, 24, -126, mode)
            if not math.isfinite(rv) or want == 0 or want > Fraction(2) ** 128:
                other.append(line.strip())
                continue
            if op in ("frcps", "frsqrts"):
                e = want.numerator.bit_length() - want.denominator.bit_length()
                ulp = Fraction(2) ** (e - 23)
                approx[(op, mode)].append((float(abs(frac(rv) - exact) / ulp),
                                           frac(rv) == want))
            else:
                st = stats[(op, mode)]
                st[0] += 1
                if frac(rv) != want:
                    st[1] += 1
                    if st[1] <= 3:
                        print("MISMATCH", line.strip(), float(want))
        elif op == "fsqrtid":
            arg = d2f(x)
            if math.isfinite(arg) and arg > 0:
                approx[("fsqrtid", mode)].append(
                    (d2f(r) / math.sqrt(arg) if math.isfinite(d2f(r)) else
                     float("nan"), x, r))
            other.append(line.strip())
        else:
            other.append(line.strip())
    print("\nexactly rounded? (op, mode: results, mismatches)")
    for k in sorted(stats):
        print(f"  {k[0]:8} mode {k[1]}: {stats[k][0]:5} results, "
              f"{stats[k][1]} differ")
    print("\napproximations: max error in ulp, share equal to the rounded "
          "value")
    for k in sorted(approx):
        if k[0] == "fsqrtid":
            rs = [v[0] for v in approx[k]]
            print(f"  fsqrtid mode {k[1]}: result/sqrt(x) from {min(rs):.6g}"
                  f" to {max(rs):.6g}")
            continue
        errs = [e for e, _ in approx[k]]
        eq = sum(1 for _, q in approx[k] if q)
        print(f"  {k[0]:8} mode {k[1]}: max {max(errs):.3f} ulp, "
              f"{eq}/{len(errs)} rounded exactly")
    print("\nspecial values and other cases:")
    for line in other:
        if not line.startswith("fsqrtid") or "0000000000000000 " in line[:30]:
            print("  " + line)


if __name__ == "__main__":
    main()
