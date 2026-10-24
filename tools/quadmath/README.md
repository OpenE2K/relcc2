# libquadmath built by the PoC, call by call

`check.c` calls every function libquadmath exports (97 symbols, the
same set as lcc's `libquadmath.so.0`), and the `__float128` arithmetic,
comparisons and conversions that libgcc's soft-fp provides, on fixed
inputs in all four rounding modes. It prints one line per call: the
result's bits and the exceptions the call raised. The inputs come from
integer bit patterns and a fixed xorshift sequence, so every host
builds the same ones and two runs compare line by line.

`run.py` runs it three ways:

- **x86**: libquadmath built from the same GCC source by the host's x86
  GCC. This is the reference: x86 GCC is taken as correct, and E2K's
  `long double` is x87's format, so the library takes the same paths.
- **ours**: the PoC's libquadmath, on the machine of its CPU.
- **system**: the machine's own `libquadmath.so.0`, which lcc built
  from GCC 11's source; the same program, with only the library
  changed.

```
R=tools/quadmath
X=toolchains/poc-cross/cross-VERSION-v3/gcc-e2k-VERSION-v3
$R/run.py x86 toolchains/quadmath/v3                       # 2 s
$R/run.py e2k toolchains/quadmath/v3 $X claude@ringo.lan -j 8 > report.txt
#   --lib DIR: a libquadmath build directory instead of the cross's
#   own; the 8C2: the 8c2 cross and claude@geofront.lan
$R/run.py compare DIR-A DIR-B                              # any two runs
```

`run.py e2k` ships the program and the library to
`~/relcc2/quadmath/NAME` on the machine; remove that directory when
done. It prints each test that exited non-zero (132 is SIGILL) and
compares ours with x86, the system's with x86, and ours with the
system's.

## Reading the comparison

| Kind | Meaning |
|---|---|
| value | a result differs: a bug in one of the two, unless explained |
| flags only | the same result, different exceptions |
| NaN bits | both NaN, with another sign or payload |
| convention | `signbitq`'s nonzero value (C says only nonzero), and `ilogbq`'s `FP_ILOGB0` and `FP_ILOGBNAN`, which are each libc's: x86's `INT_MIN`, glibc's generic `-INT_MAX` and `INT_MAX` on E2K |

At PoC `0.5+41` (2026-10-24), the crosses' own libraries, v3 on the 4C
and 8C2-tuned on the 8C2, gave on each machine:

| Against x86 | Same | Value | Flags only | NaN bits | Convention |
|---|---|---|---|---|---|
| ours | 444,468 | 0 | 8 | 0 | 1,072 |
| the system's (lcc), 37 tests dead of SIGILL | 238,860 | 156,267 | 49,313 | 36 | 1,072 |

Known differences between ours and x86 that are not bugs:

- `cacosq` of a value whose real part is an infinity or NaN and whose
  imaginary part is a signalling NaN: x86 raises invalid, ours doesn't.
  `cacosq` classifies both parts with `__unordtf2`, which raises invalid
  for a signalling NaN. x86 GCC classifies both before the test; the
  PoC tests the real part first and skips the second classification.
  GCC treats soft-fp's comparison calls as const, so it may drop their
  exceptions (8 lines).

## Pitfalls

- **Argument order.** C leaves the order of a call's arguments
  unspecified, and x86 GCC and the PoC differ (P108 in
  `docs/spec-poc-findings.md`): `mk (rnd () & 1, e, rnd (), rnd ())`
  built different inputs on the two hosts. One `rnd ()` per statement.
- **Libcalls move.** GCC takes soft-fp's functions (`__trunctfdf2`,
  `__addtf3`, ...) for const, so it may call them after the
  `fetestexcept` that follows them in the source; x86 GCC 16 did, and
  every conversion looked exact. Results go through a volatile, or into
  a call, before the flags are read.
- **Speculative FP.** A PoC-built libquadmath without `-mno-spec-fp`
  dies with SIGILL in the trigonometric and Bessel functions once a
  program has cleared the FP flags. lcc's own library dies so in 37 of
  the tests (relcc2 AGENTS.md, Gotchas, "Sticky FP flags").
