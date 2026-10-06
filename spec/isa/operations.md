# Operations

Every operation of the E2K instruction set from v2 on
(`binutils-ops.yaml`), what it computes, how a compiler uses it, and
what is known about it. Operations only v1 (Elbrus-3M) had are left
out: no working machine exists. **Generated** by
`tools/isa-extract/gen_operations.py` from `operations.yaml` (the
rules, hand-written), `binutils-ops.yaml`, the timing tables,
`lcc-usage.yaml` and the probes' assembly; edit the sources, not this
page.

Columns:

- **ISA**: the `elbrus-vN` versions that have it (8 is binutils'
  placeholder for future ones). Rows marked 1–2 don't exist on v3 and
  later (`v2ops.c`).
- **ALC**: the ALU channels it may issue in (blank: a control or
  syllable operation, bundle.md).
- **Does**: the result, with a, b, c the first to third source operand
  in gas order (stores: address, offset, data under `.ignore
  ld_st_style`). Lanes are numbered from the least significant.
  Results narrower than the register follow `setwd dbl`
  (registers.md). Details and corner cases are in the section named.
- **Lat**: scheduled latency in cycles to a consumer of the same kind
  (timing.md; "(v5)": measured on the Elbrus-8C2 only); several values
  for a row with several operations. Rows without a kernel of their
  own give timing.md's value for them, where it has one.
- **lcc v3 / v5**: how many times the operations of the row appear in
  the shared libraries of lcc 1.29.16's v3 and v5 sysroots
  (`lcc-usage.yaml`): what lcc relies on.
- **Run on hardware**: the probes (`tools/isa-probes`,
  `tools/abi/probes`) whose assembly executes the operations, and the
  machines (4C: Elbrus 4C, v3; 8C2: Elbrus-8C2, v5); "*n* of *m*" when
  not all of the row's operations ran. Rows without a probe were taken
  from the sources named in the row.

An operation's behaviour is checked when its probe compares results
with a model (semantics.c, semp.c, sem5.c, semx.c, semfx.c, semc.c,
fcomb.c, icomb.c, sqrt.c, getf.c, divx.c, stmqp.c) or records what
user mode gets (v2ops.c, ctlops.c: completes, or which signal); the
others use it in a larger test (loops, speculation, prefetch). For v6 and v7 the
only sources are the community qemu (`OpenE2K/qemu-e2k-community`,
which matches the hardware on every v5 operation) and binutils; where
neither implements an operation, only its name and operand formats are
known.

## Integer arithmetic and logic

One cycle in any channel unless noted. `s` forms are 32-bit, `d` forms 64-bit. The second operand may be a literal (bundle.md).

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `adds`, `addd` | 1–8 | 0–5 | a + b, wrapping. **Use:** `add<mode>3`. (semantics.md, "Integer arithmetic and logic") | 1 | 634k / 612k | abg, abi/ctpr.s, abi/dbl.s, abi/elfops/ext.s… (4C, 8C2) |
| `subs`, `subd` | 1–8 | 0–5 | a − b (`rsub` exists only as a combined stage). **Use:** `sub<mode>3`; `neg` is `subd 0, x`. | 1 | 66k / 59k | abi/getsp.s, abi/reenter.s, apb-stream, apb… (4C, 8C2) |
| `ands`, `andd`, `ors`, `ord`, `xors`, `xord` | 1–8 | 0–5 | Bitwise and, or, xor. **Use:** `and`, `ior`, `xor<mode>3`; `xor x, -1` for `not`. | 1 | 26k / 24k | abi/dbl.s, abi/elfops/islocal.s, icomb, p28… (4C, 8C2) |
| `andns`, `andnd`, `orns`, `ornd`, `xorns`, `xornd` | 1–8 | 0–5 | a & ~b, a \| ~b, a ^ ~b (the **second** operand inverted). **Use:** Combine patterns for `(and (not b) a)` and friends. | 1 | 1.4k / 1.4k | abi/dbl.s, icomb, semantics (4C, 8C2) |
| `addcd`, `subcd`, `addcd_c`, `subcd_c` | 5–8 | 1,4 | 64-bit add or subtract with carry or borrow in from c bit 0: `addcd` gives the sum, `addcd_c` the carry out (0 or 1); `subcd` and `subcd_c` likewise (community qemu `gen_addcd`, `tcg_gen_add2`). **Use:** Multi-word arithmetic (`__int128`, bignums); v5+ only. (semantics.md, "v5 operations") | 1 (v5) | 0 / 193 | sem5 (8C2) |
| `muls`, `muld` | 1–8 | 0,1,3,4 | Low 32 or 64 bits of the product (signed and unsigned alike). ALC0, 1, 3, 4; 4 cycles, 6 to an integer consumer. **Use:** `mul<mode>3`. | 4 | 21k / 18k | abi/dbl.s, pmu (4C) |
| `smulx`, `umulx` | 1–8 | 0,1,3,4 | 64-bit product of the two 32-bit operands, signed or unsigned. **Use:** `mulsidi3`, `umulsidi3` (lcc: `(long) a * b`). (semantics.md, "Other scalar operations") | 4 | 607 / 597 | semx (4C, 8C2) |
| `smulhd`, `umulhd` | 3–8 | 0,1,3,4 | High 64 bits of the 128-bit product, signed or unsigned. **Use:** `smuldi3_highpart`, `umuldi3_highpart`, division by constants. | 4 | 1.0k / 1.0k | semantics (4C) |
| `sdivs`, `sdivd`, `udivs`, `udivd` | 1–8 | 5 | Quotient truncated toward zero; division by zero is SIGFPE (`FPE_INTDIV`); `sdivd MIN, −1` = MIN. ALC5 only, shares the divider (timing.md, "Divider"). **Use:** `div`, `udiv<mode>3`; `%` is divide, multiply, subtract (as lcc). Speculative (`,sm`) forms defer the fault. (semantics.md, "Division") | 11 | 686 / 685 | abi/dbl.s, semantics, spec (4C, 8C2) |
| `sdivx`, `udivx`, `smodx`, `umodx` | 1–8 | 5 | 64-bit a ÷ 32-bit b into 32 bits (quotient or remainder); SIGFPE for a zero divisor, a quotient that doesn't fit, and for the remainders when a shifted left by the divisor's leading zeros overflows. ALC5. **Use:** Only for proven ranges. (semantics.md, "Other scalar operations") | 10, 11 | 3 / 3 | divx, semx (4C, 8C2) |
| `sxt` | 1–8 | 0–5 | Extend b by mode a: 0, 1, 2 sign-extend 8, 16, 32 bits; 4, 5, 6 zero-extend them (3 = 2, 7 = 6). **Use:** `extend`, `zero_extend`; `sxt 6` is the free `x & 0xffffffff`. (semantics.md, "Integer arithmetic and logic") | 1 | 25k / 25k | abi/dbl.s, mas_be, pmu, semantics (4C, 8C2) |
| `merges`, `merged` | 1–8 | 0–5 | pred ? b : a: **a when the predicate is false**. **Use:** `mov<mode>cc`, conditional select. (semantics.md, "Integer arithmetic and logic") | 1 | 31k / 27k | abi/dbl.s, loops, p28, preds… (4C, 8C2) |
| `popcnts`, `popcntd`, `lzcnts`, `lzcntd`, `bitrevs`, `bitrevd` | 2–8 | 0,1,3,4 | Number of set bits; leading zeros (width for 0); bit reversal. v2+. `popcnt`, `lzcnt` ALC1/4. **Use:** `popcount`, `clz` (`CLZ_DEFINED_VALUE_AT_ZERO` = width), `bitreverse`; `ctz` = `lzcnt (bitrev x)`. | 1 | 607 / 829 | abi/dbl.s, semantics, semp (4C, 8C2) |
| `shls`, `shld`, `shrs`, `shrd`, `sars`, `sard`, `scls`, `scld`, `scrs`, `scrd` | 1–8 | 0–5 | Shift left, logical right, arithmetic right, rotate left, rotate right; the count is b modulo the width. **Use:** `ashl`, `lshr`, `ashr`, `rotl`, `rotr<mode>3`; `SHIFT_COUNT_TRUNCATED` holds. (semantics.md, "Shifts and rotates") | 1 | 62k / 59k | abi/dbl.s, abi/elfops/islocal.s, icomb, loops… (4C, 8C2) |
| `getfs`, `getfd`, `getfzs`, `getfzd` | 1–8 | 0–5 | Field extract: rotate a right by b's offset, keep *len* bits, zero- or sign-extend (v1–v4 take the sign bit from b's BYTE field). `getfz` (v5+) then clears up to 7 low bits. **Use:** `extzv`, `extv`, shift-and-mask, sign-extending shift pairs, wide low masks. (semantics.md, "Field extract") | 1, 1 (v5) | 15k / 14k | abi/dbl.s, getf, icomb, sem5 (4C, 8C2) |
| `insfs`, `insfd` | 1–7 | 0,1,3,4 | Rotate a right by b's offset, then replace its low *len* bits by c's. ALC0, 1, 3, 4. **Use:** `insv` with a rotate back (`scl`), as lcc stores bit-fields. (semantics.md, "Other scalar operations") | 1 | 13k / 12k | apb-loop, semx (4C, 8C2) |
| `mpsadbh` | 3–8 | 1,4 | Four 16-bit sums of absolute byte differences, lane i = Σ \|a.byte[i+j] − b.byte[j]\| (x86 `mpsadbw` on 64 bits). v3+. (semantics.md, "Other scalar operations") | 2 |  | semx (4C, 8C2) |
| `clmull`, `clmulh` | 6–8 | 0,1,3,4 | Carry-less 64 × 64 product, low or high 64 bits (community qemu). v6+. **Use:** CRC and GCM (`clmul`). |  |  | v6+: no hardware here |

## Integer compares

Into a predicate (`%predN`), from a − b (or a & b) tested like x86 flags. Compare → predicated operation 2 cycles, → `ct` 3.

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `cmposb`, `cmpodb`, `cmpbsb`, `cmpbdb`, `cmpesb`, `cmpedb`, … (16 operations) | 1–8 | 0,1,3,4 | o: signed overflow of a − b; b: a < b unsigned; e: a = b; be: a ≤ b unsigned; s: a − b negative; p: even parity of its low byte; l: a < b signed; le: a ≤ b signed. No ne/gt/ge forms: swap operands or negate at the use. **Use:** `cbranch`, `cstore`, `mov<mode>cc` conditions; ALC0, 1, 3, 4. (semantics.md, "Integer compares") | 2; 3 to `ct` | 194k / 183k | abi/ctpr.s, abi/dbl.s, abi/reenter.s, apb-loop… (4C, 8C2) |
| `cmpandesb`, `cmpandedb`, `cmpandssb`, `cmpandsdb`, `cmpandpsb`, `cmpandpdb`, `cmpandlesb`, `cmpandledb` | 1–8 | 0,1,3,4 | The same tests on a & b: e zero, s sign bit, p even parity of the low byte, le ≤ 0 signed. **Use:** Bit tests (`if (x & m)`); lcc tests FP compare flags with them. (semantics.md, "Integer compares") | 2; 3 to `ct` | 14k / 13k | loops, p28, semantics, semx (4C, 8C2) |
| `cctopo`, `cctopb`, `cctope`, `cctopbe`, `cctops`, `cctopp`, `cctopl`, `cctople` | 1–8 | 0,3 | Predicate: x86 condition c on b read as EFLAGS (`o` OF, `b` CF, `e` ZF, `be` CF\|ZF, `s` SF, `p` PF, `l` SF≠OF, `le` ZF\|(SF≠OF)). ALC0/3; 2 cycles to a predicated op. **Use:** With the flag forms below, e.g. overflow checks. (semantics.md, "x86 flag operations") | 2 |  | semx (4C, 8C2) |

## x86 flag operations

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `adds_fb`, `adds_fh`, `adds_fw`, `subs_fb`, `subs_fh`, `subs_fw`, … (66 operations) | 1–8 | 0,3 | Return the x86 EFLAGS the operation would set (CF 0x01, PF 0x04, AF 0x10, ZF 0x40, SF 0x80, OF 0x800) as a 32-bit result, for operand width b/h/w/d: add, sub, the logic operations, inc/dec (a ± b, CF from c), shifts and rotates (old flags in c), 32 × 32 multiplies. For x86 binary translation; 2 cycles (`smulx_f*` 4); ALC0/3. **Use:** `adds_fw` + `cctopo` give `__builtin_add_overflow`'s flag (lcc doesn't use them). (semantics.md, "x86 flag operations") | 2, 4 |  | semx (4C, 8C2) |

## Combined integer operations

`X_Y a, b, c` = Y(c, X(a, b)): two ordinary operations chained in ALC1 or ALC4 with the latency of both (2). They save an instruction slot, not time (semantics.md, "FP min, max and combined operations").

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `and_ands`, `and_andd`, `andn_ands`, `andn_andd`, `or_ands`, `or_andd`, … (252 operations) | 1–7 | 1,4 | Y(c, X(a, b)), `rsub` being t − c. Every one equals the two ordinary operations (icomb.c, 252 of v3's). **Use:** Peephole fusion of dependent pairs where channels are short; lcc rarely uses them. (semantics.md, "FP min, max and combined operations") | 2 | 9.6k / 8.6k | icomb (4C, 8C2) |
| `merge_ands`, `merge_andd`, `merge_andns`, `merge_andnd`, `merge_ors`, `merge_ord`, … (18 operations) | 1–7 | 1,4 | Y(c, merge(a, b, pred)): select, then the second stage. (semantics.md, "Other scalar operations") |  | 115 / 102 | semx (4C, 8C2) |

## Floating point

IEEE single and double in the general registers, `%pfpfr` holding the rounding mode and flags (x86 MXCSR layout); 4 cycles in ALC0, 1, 3, 4 (v5 also 2, 5). Operations set the denormal flag for a denormal operand. `s` forms keep the upper half of their destination.

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `fadds`, `faddd`, `fsubs`, `fsubd`, `fmuls`, `fmuld` | 1–8 | 0–5 | a + b, a − b, a × b, correctly rounded. **Use:** `add`, `sub`, `mul<mode>3`. | 4 | 10k / 10k | abi/dbl.s, semx, spec (4C, 8C2) |
| `fdivs`, `fdivd` | 1–8 | 5 | a ÷ b, correctly rounded, IEEE flags. ALC5 (divider), 11 / 14 cycles. **Use:** `div<mode>3`. (semantics.md, "Square root and reciprocals") | 11, 14 | 791 / 789 | spec, sqrt (4C, 8C2) |
| `fsqrts`, `fsqrtid`, `fsqrttd` | 1–8 | 5 | `fsqrts` √a; a double square root is `fsqrtid a, t` then `fsqrttd a, t, r`, correctly rounded as a pair (`fsqrtid` alone is an intermediate in its own format). ALC5. **Use:** `sqrt<mode>2` (keep the pair together). (semantics.md, "Square root and reciprocals") | 12 | 155 / 155 | sqrt (4C, 8C2) |
| `frcps`, `frsqrts` | 1–8 | 5 | 12-bit estimates of 1/a and 1/√a, no flags. **Use:** `-ffast-math` reciprocal estimates with Newton steps (lcc never). (semantics.md, "Square root and reciprocals") | 8 |  | sqrt (4C, 8C2) |
| `fmaxs`, `fmaxd`, `fmins`, `fmind` | 1–8 | 0,1,3,4 | x86 `maxsd`/`minsd`, **b** if either is a NaN or both are zeros. 2 cycles. **Use:** `smax`, `smin` only under `-ffinite-math-only` or for `a > b ? a : b`. (semantics.md, "FP min, max and combined operations") | 2 | 39 / 39 | semantics, semx (4C, 8C2) |
| `fscales`, `fscaled` | 4–8 | 1,4 | a × 2^b, b a signed 32-bit integer, rounded once (`ldexp`). v4+; ALC1/4. **Use:** `ldexp<mode>3` with `-fno-math-errno`. (semantics.md, "Other scalar operations") | 4 (v5) | 0 / 103 | semx (8C2) |
| `fcmpeqsb`, `fcmpeqdb`, `fcmpltsb`, `fcmpltdb`, `fcmplesb`, `fcmpledb`, … (16 operations) | 1–8 | 0,1,3,4 | Predicate: a c b, quiet for eq, neq, uod, od, signalling for lt, le, nlt, nle; neq, nlt, nle are true when unordered. 4 cycles to a predicated op, 5 to `ct`. **Use:** `cbranch`, `cstore` for FP; swap operands for gt/ge. (semantics.md, "Floating-point compares") | 4; 5 to `ct` | 3.1k / 2.8k | semantics, semx (4C, 8C2) |
| `fcmpeqs`, `fcmpeqd`, `fcmplts`, `fcmpltd`, `fcmples`, `fcmpled`, … (16 operations) | 1–8 | 0,1,3,4 | All ones (64 bits, or the low 32 for `s`) if a c b, else 0: x86 `cmpsd`/`cmpss`. **Use:** Branch-free FP selects with `and`/`andn`. (semantics.md, "Floating-point compares") | 2 |  | semx (4C, 8C2) |
| `fcmpudsf`, `fcmpuddf`, `fcmpodsf`, `fcmpoddf` | 1–8 | 0,1,3,4 | x86 `ucomisd`/`comisd` flags into a register: less 0x01, equal 0x40, greater 0, unordered 0x45; `ud` quiet, `od` signalling. **Use:** C's `isless` family: `fcmpuddf` then a `cmpand*`. (semantics.md, "Floating-point compares") |  | 160 / 161 | semantics (4C) |
| `fstois`, `fstoistr`, `fstoid`, `fstoidtr`, `fdtois`, `fdtoistr`, `fdtoid`, `fdtoidtr` | 1–8 | 0,1,3,4 | FP → integer: single or double to 32 or 64 bits, in the current rounding mode or truncated (`tr`); out of range or NaN gives the integer indefinite (0x80…0) and invalid. 4 cycles to a consumer of their format, 6 to an integer op. **Use:** `fix_trunc`, `lrint<mode><mode>2`. (semantics.md, "Other scalar operations") | 4, 5 | 238 / 260 | abi/dbl.s, semx (4C, 8C2) |
| `istofs`, `istofd`, `idtofs`, `idtofd` | 1–8 | 0,1,3,4 | 32- or 64-bit signed integer → single or double, current rounding mode. **Use:** `float<mode><mode>2`; unsigned via a fix-up sequence. (semantics.md, "Other scalar operations") | 4, 5 | 816 / 832 | semx (4C, 8C2) |
| `fstofd`, `fdtofs` | 1–8 | 0,1,3,4 | Single → double (exact), double → single (rounded). **Use:** `extendsfdf2`, `truncdfsf2`. (timing.md, "Formats") | 5, 6 | 425 / 425 | semx (4C, 8C2) |
| `fdtoifd`, `fstoifs` | 3–8 | 0,1,3,4 | Round b to an integral value with a as x86 `roundsd`'s immediate (bits 0–1 mode, bit 2 use `%pfpfr`, bit 3 no inexact). v3+. **Use:** `floor`, `ceil`, `trunc`, `round`(even), `nearbyint`, `rint<mode>2`. (semantics.md, "Other scalar operations") | 4 | 146 / 152 | semx (4C, 8C2) |
| `fmas`, `fmad`, `fmss`, `fmsd`, `fnmas`, `fnmad`, `fnmss`, `fnmsd` | 6–8 | 0–5 | Fused a × b + c, a × b − c, −(a × b) + c, −(a × b) − c, one rounding (community qemu). v6+. **Use:** `fma`, `fms`, `fnma`, `fnms<mode>4`; `fma()` (combined operations round twice). |  |  | v6+: no hardware here |

## Combined FP operations

`X_Y a, b, c` = Y(c, X(a, b)) with a rounding after each stage, 8 cycles; the packed forms work lane by lane with `hadd`, `hsub` and `addsub` stages (semantics.md, "FP min, max and combined operations").

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `fmul_adds`, `fmul_addd`, `fmul_subs`, `fmul_subd`, `fadd_adds`, `fadd_addd`, … (18 operations) | 1–7 | 0–5 | (a X b) then Y with c as the first operand: `fmul_addd` = a × b + c, `fmul_subd` = c − a × b, `fmul_rsubd` = a × b − c; **two roundings**. **Use:** Multiply-add only under `-ffp-contract=fast`, never for `fma()`; lcc uses `fmul_add*` in loops. | 8 | 540 / 667 | fcomb, semantics (4C, 8C2) |
| `pfmul_adds`, `pfmul_addd`, `pfmul_subs`, `pfmul_subd`, `pfadd_adds`, `pfadd_addd`, … (45 operations) | 1–7 | 0–5 | The same on 64-bit packed singles (and `d` scalar doubles), with hadd(x, y) = (x0 + x1, y0 + y1), hsub likewise, addsub(x, y) = (x0 − y0, x1 + y1). |  |  | fcomb (4C, 8C2) |
| `qpfadd_adds`, `qpfadd_addd`, `qpfsub_adds`, `qpfsub_addd`, `qpfhadd_adds`, `qpfhsub_adds`, … (52 operations) | 5–7 | 0–5 | The same on 128-bit registers (4 singles or 2 doubles). v5+. (semantics.md, "v5 operations") | 8 (v5) |  | sem5 (8C2) |

## 80-bit (x87 extended) operations

`long double`: a 64-bit significand in the register and sign and exponent in its 16-bit extended part, under `%fpcr`/`%fpsr` (x87 control and status words). Every operation equals x87 code exactly, including rounding first to the precision control and then to a narrower result (semantics.md, "80-bit operations").

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `fxaddss`, `fxadddd`, `fxaddsx`, `fxadddx`, `fxaddxx`, `fxaddxd`, … (25 operations) | 1–8 | 0,1,3,4 | a (80-bit) op b (type of the first suffix letter: single, double, 80-bit) → result of the second letter's type; `rsub` is b − a. 4–5 cycles. **Use:** `long double` arithmetic, also with a narrower operand or result. | 4, 5 | 5.0k / 5.0k | semfx (4C, 8C2) |
| `fxdivss`, `fxdivdd`, `fxdivsx`, `fxdivdx`, `fxdivxx`, `fxdivxd`, `fxdivxs` | 1–8 | 5 | a ÷ b as above, ALC5, 16–18 cycles. | 16, 18 | 368 / 367 | semfx (4C, 8C2) |
| `fxdivtss`, `fxdivtsx`, `fxdivtdd`, `fxdivtdx` | 1–8 | 5 | A division step that needs a prepared operand (invalid for ordinary ones); not decoded, lcc doesn't emit it. (semantics.md, "80-bit operations") |  | 20 / 20 | semfx (4C, 8C2) |
| `fxsqrtusx`, `fxsqrtudx`, `fxsqrtuxx`, `fxsqrttsx`, `fxsqrttdx`, `fxsqrttxx`, `fxsqrtisx`, `fxsqrtidx`, `fxsqrtixx` | 1–8 | 5 | Square root steps: lcc's `fxsqrtixx a, t; fxsqrtuxx a, t, t; fxsqrttxx a, t, r` equals x87 `fsqrt`; the steps alone are not decoded. ALC5, 53 cycles for the chain. **Use:** `sqrtxf2` as the three-step chain. | 11, 16, 17 | 171 / 171 | semfx (4C, 8C2) |
| `fxscalesx` | 4–8 | 1,4 | a × 2^b (80-bit), b a 32-bit integer (community qemu). v4+. |  | 0 / 32 | semfx (8C2) |
| `fxcmpeqsb`, `fxcmpltsb`, `fxcmplesb`, `fxcmpuodsb`, `fxcmpneqsb`, `fxcmpnltsb`, … (24 operations) | 1–8 | 0,1,3,4 | Predicate compare of a (80-bit) with b; quiet and signalling as the scalar compares. |  | 1.3k / 1.3k | semfx (4C, 8C2) |
| `fxcmpodsf`, `fxcmpudsf`, `fxcmpoddf`, `fxcmpuddf`, `fxcmpodxf`, `fxcmpudxf` | 1–8 | 0,1,3,4 | x86 flag layout as `fcmp*f`; `ud` quiet, `od` signalling. |  | 225 / 225 | semfx (4C, 8C2) |
| `fxtofs`, `fxtofd`, `fxtois`, `fxtoid`, `fxtoistr`, `fxtoidtr`, `fdtofx`, `fstofx`, `idtofx`, `istofx` | 1–8 | 0,1,3,4 | Conversions to and from 80-bit as x87 loads and stores (`fld`, `fstp`, `fild`, `fistp`, `fisttp` for `tr`). | 5 | 1.3k / 1.3k | semfx (4C, 8C2) |
| `movx`, `movxa`, `movxc` | 2–8 | 0,1,3,4 | 80-bit copy, copy with the sign cleared (`fabs`), copy with the sign flipped (`fchs`); no flags. **Use:** `movxf`, `absxf2`, `negxf2`. |  | 1.5k / 1.5k | semfx (4C, 8C2) |
| `movif`, `movfi` | 1–8 | 0,1,3,4 | `movif a, b, dst` packs significand a and sign/exponent b into an 80-bit register; `movfi` reads the 16-bit extended part. 4 cycles. **Use:** `long double` arguments, results and spills (spec/abi/registers.md). | 4 | 6.0k / 5.9k | abi/ldouble.s, pmu, semfx (4C, 8C2) |

## Packed integer (64-bit registers)

Lanes of 8 (b), 16 (h), 32 (w) or 64 (d) bits, little-endian. Mostly 1 cycle (multiplies and SAD 2) to packed or FP consumers, 4 to integer ones; add/sub/compare/min/max/average forms in ALC0/3, multiplies, `pmadd*`, `psadbw`, unpacks and packs in ALC1/4 (v3). Semantics follow the community qemu's `helper_vec.c` except the points in semantics.md, "Packed operations of v3".

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `paddb`, `paddh`, `paddw`, `paddd`, `psubb`, `psubh`, `psubw`, `psubd` | 1–8 | 0,3 | Lane-wise add or subtract, wrapping. **Use:** `add`/`sub` on `V8QI`, `V4HI`, `V2SI`. | 1 | 374 / 493 | semp (4C, 8C2) |
| `paddsb`, `paddsh`, `paddusb`, `paddush`, `psubsb`, `psubsh`, `psubusb`, `psubush` | 1–8 | 0,3 | Lane-wise saturating, signed or unsigned (x86 `paddsb` …). **Use:** `ssadd`, `usadd`, `sssub`, `ussub`. |  |  | semp (4C, 8C2) |
| `pmaxsb`, `pmaxsh`, `pmaxsw`, `pmaxub`, `pmaxuh`, `pmaxuw`, `pminsb`, `pminsh`, `pminsw`, `pminub`, `pminuh`, `pminuw` | 1–8 | 0,3 | Lane-wise maximum, minimum, signed or unsigned. **Use:** `smax`, `umax`, `smin`, `umin`. | 1 | 153 / 51 | semp (4C, 8C2) |
| `pavgusb`, `pavgush` | 1–8 | 0,3 | Lane-wise (a + b + 1) >> 1 unsigned (x86 `pavgb`, `pavgw`). **Use:** `uavg<mode>3_ceil`. |  |  | semp (4C, 8C2) |
| `pcmpeqb`, `pcmpeqh`, `pcmpeqw`, `pcmpeqd`, `pcmpgtb`, `pcmpgth`, `pcmpgtw`, `pcmpgtd` | 1–8 | 0,3 | All-ones lanes where a = b, or a > b signed. **Use:** `vec_cmp`, masks for `pmerge`. | 1 | 453 / 80 | semp (4C, 8C2) |
| `pcmpeqbop`, `pcmpeqbap`, `pcmpeqhop`, `pcmpeqhap`, `pcmpeqwop`, `pcmpeqwap`, … (16 operations) | 6–8 | 0,3 | Into a predicate: true if the compare holds in any lane (`op`) or in all lanes (`ap`) (community qemu). v6+. **Use:** Vector loop exits (`any`/`all`). |  |  | v6+: no hardware here |
| `pmullh`, `pmulhh`, `pmulhuh`, `pmulhrsh`, `pmullw` | 1–8 | 1,4 | 16-bit lanes: low half of the product, high half signed or unsigned, (a × b + 0x4000) >> 15 (x86 `pmulhrsw`); `pmullw` 32-bit lanes, low half (v5). **Use:** `mul`, `smulhs`, `umulhs`, `smulhrs`. | 2, 2 (v5) |  | sem5, semp (4C, 8C2) |
| `pmaddh`, `pmaddubsh` | 1–8 | 1,4 | `pmaddh`: x86 `pmaddwd`; `pmaddubsh`: signed bytes of **a** × unsigned bytes of **b**, pairs summed with signed saturation (x86 `pmaddubsw` with the roles swapped). **Use:** Dot products (`sdot_prod`). |  | 222 / 231 | semp (4C, 8C2) |
| `pmulubhh` | 2–8 | 1,4 | 16-bit lanes: (unsigned byte i of a × signed halfword i of b + 0x80) >> 8, bytes 0–3 of a. |  |  | semp (4C, 8C2) |
| `psadbw` | 1–8 | 1,4 | Sum of the 8 absolute byte differences in the low bits (x86 `psadbw`). **Use:** `usad`, `sad` reductions. | 2 |  | semp (4C, 8C2) |
| `psignb`, `psignh`, `psignw` | 3–8 | 0,3 | x86 `psign`: lane of a negated, kept or zeroed by the sign of b's lane. |  |  | semp (4C, 8C2) |
| `psllh`, `psllw`, `pslld`, `psrlh`, `psrlw`, `psrld`, `psrah`, `psraw`, `psrcw`, `psrcd` | 1–8 | 0,1,3,4 | Every lane shifted by the 64-bit count b: left, logical right, arithmetic right, rotate right (`psrc`, v5); counts at or beyond the lane width give 0 (arithmetic: the sign). **Use:** `ashl`, `lshr`, `ashr` with a scalar count; `vrotr` (v5). | 1, 1 (v5) | 162 / 164 | sem5, semp (4C, 8C2) |
| `psllqh`, `psllql`, `psrlqh`, `psrlql` | 1–8 | 1,4 | Shift a:b (a high) as one 128-bit value by 8 × imm bits, keep the low (`l`) or high (`h`) half; 16 and more give 0. **Use:** Byte-granular vector shifts and alignment. (semantics.md, "Packed operations of v3") |  |  | semp (4C, 8C2) |
| `packsshb`, `packushb`, `packsswh`, `packuswh` | 1–8 | 1,4 | Narrow with signed or unsigned saturation: halfwords → bytes, words → halfwords; **a's lanes go to the high half**, b's to the low half. **Use:** `vec_pack_ssat`, `vec_pack_usat` (swap the operands). |  |  | semp (4C, 8C2) |
| `punpcklbh`, `punpcklhw`, `punpcklwd`, `punpckhbh`, `punpckhhw`, `punpckhwd` | 1–8 | 1,4 | Interleave the low (`l`) or high (`h`) halves: **b's lanes at even positions**, a's at odd. **Use:** `vec_interleave_lo/hi`, widening by unpacking with zero or a sign mask. |  |  | semp (4C, 8C2) |
| `pshufb` | 2–7 | 0,1,3,4 | Byte i from mask c's byte: bit 7 set gives a constant (bits 5–6: 0, 0x7f, 0x80, 0xff), else bits 0–2 pick a byte of a (bit 3 set) or b; v5 adds invert, bit-reverse and sign-replicate in bits 4–6, **v3 ignores them**. **Use:** `vec_perm` on 64 bits, byte swaps (`bswap` when MAS bit 3 can't be used). (semantics.md, "Packed operations of v3") | 1 | 1.0k / 666 | mas_be, semp (4C, 8C2) |
| `pshufh`, `pshufw` | 1–8 | 1,4 | `pshufh a, imm`: halfword i = a's halfword (imm >> 2i) & 3 (x86 `pshufw`); `pshufw a, b, imm`: word i from a (selector > 1) or b, word selector bit 0. **Use:** `vec_perm` with constant selectors. |  |  | semp (4C, 8C2) |
| `pmerge` | 2–7 | 0,1,3,4 | Byte i = c's byte i negative ? b's : a's. **Use:** `vcond`, `vec_merge` with a byte mask (after `pcmp*`). |  |  | semp (4C, 8C2) |
| `pextrh`, `pinsh` | 1–8 | 1,4 | `pextrh a, b, imm`: halfword imm of a:b (0–3 from b), imm > 7 differs by machine; `pinsh a, b, imm`: a with halfword imm & 3 replaced by b's low halfword. **Use:** `vec_extract`, `vec_set`. (semantics.md, "Packed operations of v3") |  |  | semp (4C, 8C2) |
| `pmovmskb`, `pmovmskps`, `pmovmskpd` | 1–8 | 1,4 | Sign bits gathered: `pmovmskb` b's 8 bytes in bits 0–7, a's in 8–15; `ps` 2 + 2 words; `pd` 1 + 1 doubles. **Use:** `vec_pack_sfix`-free mask tests, `memchr`-style loops. |  | 31 / 0 | semp (4C, 8C2) |
| `phminposuh` | 3–8 | 1,4 | Minimum unsigned halfword of the 8 lanes of a (lanes 0–3) and b (4–7) and its index in bits 16 up (x86 `phminposuw`). |  |  | semp (4C, 8C2) |
| `phaddh`, `phaddw`, `phaddsh`, `phsubh`, `phsubw`, `phsubsh` | 3–8 | 0,3 | Horizontal add or subtract of adjacent pairs (saturating `s`), a's pairs into the low half, b's into the high half (x86 `phaddw` …). **Use:** Reductions. |  |  | semp (4C, 8C2) |
| `pandd`, `pandnd`, `pord`, `pxord` | 1–8 | 0,1,3,4 | 64-bit bitwise and, a & ~b, or, xor, on the packed side (latency 1 to packed and FP consumers). **Use:** Logic between packed values without the FP → integer crossing. | 1 | 58 / 129 | semp (4C, 8C2) |
| `plog_0x00`, `plog_0x01`, `plog_0x02`, `plog_0x03`, `plog_0x04`, `plog_0x05`, … (256 operations) | 5–7 | 0,1,3,4 | Any function of three operands bit by bit: result bit = bit ((a << 2) \| (b << 1) \| c) of the 8-bit table in the name (`plog_0x80` = a & b & c, `plog_0x96` = a ^ b ^ c). 1 cycle. v5+. **Use:** Any three-input logic expression (`ternlog`) in one operation. (semantics.md, "v5 operations") | 1 (v5) | 0 / 8 | sem5 (8C2) |
| `pistofs`, `pfstois`, `pfstoistr` | 1–8 | 0,1,3,4 | Two 32-bit lanes: integer → single, single → integer (rounding mode or truncated). **Use:** `float`, `fix_trunc` on `V2SI`/`V2SF`. |  |  | semp (4C, 8C2) |
| `pmrgp` | 6–8 | 0,1,3,4 | Packed merge by a predicate (as `merged`) (community qemu). v6+. |  |  | v6+: no hardware here |

## Packed FP (64-bit registers)

`pf*s` treat the register as two singles, `pf*d` as one double (identical to the scalar operation, kept for encoding symmetry). 4 cycles; flags as the scalar operations (semantics.md, "Packed operations of v3").

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `pfadds`, `pfaddd`, `pfsubs`, `pfsubd`, `pfmuls`, `pfmuld`, `pfmaxs`, `pfmaxd`, `pfmins`, `pfmind` | 1–8 | 0–5 | Lane-wise add, subtract, multiply, x86-style max and min. **Use:** `V2SF` arithmetic. | 2 |  | semp (4C, 8C2) |
| `pfdivs`, `pfdivd`, `pfsqrts`, `pfsqrttd` | 1–8 | 5 | `pfdivs`, `pfdivd`: IEEE values but **no flags**; `pfsqrts` = `fsqrts` of the low single (not packed), `pfsqrttd` = `fsqrttd`. (semantics.md, "Packed operations of v3") |  |  | semp, sqrt (4C, 8C2) |
| `pfcmpeqs`, `pfcmpeqd`, `pfcmplts`, `pfcmpltd`, `pfcmples`, `pfcmpled`, … (16 operations) | 1–8 | 0,1,3,4 | All-ones lanes where the compare holds; eq, neq, od, uod quiet (also on qemu's error list), lt/le signal. |  |  | semp (4C, 8C2) |
| `pfhadds`, `pfhsubs`, `pfaddsubs` | 3–8 | 0–5 | Horizontal add/subtract (a's pair low, b's high); `pfaddsubs` (a0 − b0, a1 + b1). |  |  | semp (4C, 8C2) |
| `pfdtofs`, `pfdtois`, `pfdtoistr`, `pfstofd`, `pfdtoifd`, `pfstoifs` | 1–8 | 0,1,3,4 | Scalar conversions under packed names (`pfdtofs` = `fdtofs`, …); `pfstoifs` rounds both singles to integral with mode a. |  |  | semp (4C, 8C2) |

## 128-bit packed operations (v5+)

The `qp` operations work on whole 128-bit registers (`%qprN`), with the latency of their 64-bit counterparts and full pipelining (timing.md, "Elbrus-8C2"). All 703 v5 ALU operations give the same 128 bits on the 8C2 as the community qemu (sem5.c), so its helpers describe them.

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `qpaddb`, `qpaddh`, `qpaddw`, `qpaddd`, `qpsubb`, `qpsubh`, … (48 operations) | 5–8 | 0,1,3,4 | As the 64-bit operation of the same name without `q`, over twice the lanes. **Use:** `V16QI`, `V8HI`, `V4SI`, `V2DI` patterns. | 1 (v5), 2 (v5) | 0 / 747 | sem5 (8C2) |
| `qpand`, `qpandn`, `qpor`, `qpxor` | 5–8 | 0,1,3,4 | 128-bit and, a & ~b, or, xor. | 1 (v5) | 0 / 470 | abg, sem5 (8C2) |
| `qplog_0x00`, `qplog_0x01`, `qplog_0x02`, `qplog_0x03`, `qplog_0x04`, `qplog_0x05`, … (256 operations) | 5–7 | 0–5 | `plog` on 128 bits: result bit = bit ((a << 2) \| (b << 1) \| c) of the table. | 1 (v5) | 0 / 64 | sem5 (8C2) |
| `qppermb_lt3`, `qpinss_lt3`, `qpinsd_lt3`, `qplog_0x00_lt3`, `qplog_0x01_lt3`, `qplog_0x02_lt3`, … (259 operations) | 7 | 0–5 | v7 encodings with the table in a literal (binutils); not implemented in qemu. |  |  | v6+: no hardware here |
| `qpsllh`, `qpsllw`, `qpslld`, `qpsrlh`, `qpsrlw`, `qpsrld`, `qpsrah`, `qpsraw`, `qpsrcw`, `qpsrcd`, `qpsrad` | 5–8 | 0,1,3,4 | Every lane shifted by the 64-bit count b; counts ≥ the lane width give 0 (the sign for `qpsra*`). MCST's qemu takes `qpslld`/`qpsrld` counts modulo 64, the hardware doesn't. `qpsrad` (64-bit lanes) v6+. (semantics.md, "v5 operations") | 1 (v5) | 0 / 48 | sem5 (8C2); 10 of 11 |
| `qphaddh`, `qphaddw`, `qphaddsh`, `qphsubh`, `qphsubw`, `qphsubsh` | 5–8 | 0,3 | Horizontal pairs over 128 bits, a's into the low half, b's into the high half. | 1 (v5) |  | sem5 (8C2) |
| `qpsadbw`, `qpmpsadbh`, `qpmulubhh` | 5–8 | 1,4 | `psadbw` per 64-bit half; `mpsadbh` per half with the same 32-bit b; `qpmulubhh`: `pmulubhh` per half, using bytes 0–3 of the 64-bit a for the low half and 4–7 for the high half. | 2 (v5) |  | sem5 (8C2) |
| `qpacksshb`, `qpackushb`, `qpacksswh`, `qpackuswh`, `qpackhbss`, `qpackhbus`, `qpackwhss`, `qpackwhus` | 5–8 | 0,1,3,4 | Narrow with saturation: a's lanes into the high 64 bits, b's into the low 64 (`qpackhbss` etc., v7: the same per qemu). | 1 (v5) |  | sem5 (8C2); 4 of 8 |
| `qpshufb` | 5–7 | 0,1,3,4 | Low 64 result bits = `pshufb` (b.high, b.low, c.low), high = `pshufb` (a.high, a.low, c.high), with the v5 byte rules. | 1 (v5) |  | sem5 (8C2) |
| `qppermb` | 5–7 | 0,1,3,4 | Byte i: c's byte selects byte (bits 0–3) of a (bit 4 set) or b, then optionally inverts (bit 5), bit-reverses (bit 6) or sign-replicates (bit 7). **Use:** `vec_perm` on 128 bits (two-source byte permute). | 1 (v5) | 0 / 228 | sem5 (8C2) |
| `qpmerge` | 5–7 | 0,1,3,4 | Byte i = c's byte negative ? b's : a's. | 1 (v5) | 0 / 38 | sem5 (8C2) |
| `qppackdl`, `qpswitchd`, `qpswitchw` | 5–8 | 0,1,3,4 | `qppackdl a, b`: 128 bits with a high, b low; `qpswitchd` swaps the halves, `qpswitchw` the 32-bit words within each half. **Use:** Building and splitting 128-bit values (lcc loads 128-bit data as two `ldd` + `qppackdl`). | 1 (v5) | 0 / 12k | loops, sem5, spec (8C2) |
| `qpsgn2mskb`, `qpmsk2sgnb` | 5–8 | 0,1,3,4 | `qpsgn2mskb b`: the 16 byte sign bits as a 16-bit mask; `qpmsk2sgnb a, b`: a with each byte's sign bit set from mask bit i. | 1 (v5), 4 (v5) | 0 / 327 | sem5 (8C2) |
| `qphminposuh` | 5–8 | 1,4 | Minimum unsigned halfword of 16 lanes (a's lanes first) and its index (bits 16 up), into a 64-bit register. | 4 (v5) |  | sem5 (8C2) |
| `qpcmpeqbop`, `qpcmpeqbap`, `qpcmpeqhop`, `qpcmpeqhap`, `qpcmpeqwop`, `qpcmpeqwap`, … (16 operations) | 6–8 | 0,3 | Into a predicate, any (`op`) or all (`ap`) lanes (community qemu). v6+. |  |  | v6+: no hardware here |
| `qpcext_0x00`, `qpcext_0x7f`, `qpcext_0x80`, `qpcext_0xff` | 6–8 | 0,1,3,4 | 128 bits with b in the low half and the high half filled with the byte in the name (community qemu). v6+. |  |  | v6+: no hardware here |
| `qpmrgp` | 6–8 | 0,1,3,4 | 128-bit merge by a predicate (community qemu). v6+. |  |  | v6+: no hardware here |
| `qpidtofs`, `qpidtofd`, `qpistofs`, `qpistofd`, `qpfdtoid`, `qpfdtois`, … (14 operations) | 5–8 | 0,1,3,4 | Packed conversions between 32/64-bit integers, singles and doubles (widening ones read 64 bits, narrowing ones write 64). 4 cycles to a consumer of their format. (timing.md, "Formats") | 5 (v5), 6 (v5) |  | sem5 (8C2) |
| `qpfdtoifd`, `qpfstoifs` | 5–8 | 0,1,3,4 | Round every lane to integral, a giving the mode (bit 3 suppresses inexact). (semantics.md, "v5 operations") | 4 (v5) |  | sem5 (8C2) |
| `qpfadds`, `qpfaddd`, `qpfsubs`, `qpfsubd`, `qpfmuls`, `qpfmuld`, … (14 operations) | 5–8 | 0–5 | Lane-wise FP arithmetic on 4 singles or 2 doubles; `addsub` subtracts in even lanes and adds in odd (x86 `addsubps`). **Use:** `V4SF`, `V2DF` patterns; FP max/min 2 cycles. | 2 (v5), 4 (v5) |  | sem5 (8C2) |
| `qpfcmpeqs`, `qpfcmpeqd`, `qpfcmplts`, `qpfcmpltd`, `qpfcmples`, `qpfcmpled`, … (16 operations) | 5–8 | 0,1,3,4 | All-ones lanes; eq, neq, od, uod quiet. | 2 (v5) |  | sem5 (8C2) |
| `qpfmas`, `qpfmad`, `qpfmss`, `qpfmsd`, `qpfnmas`, `qpfnmad`, `qpfnmss`, `qpfnmsd`, `qpfmass`, `qpfmasd`, `qpfmsas`, `qpfmsad` | 6–8 | 0–5 | Fused multiply-add per lane (`mas`: add in even lanes, subtract in odd; `msa` the reverse) (community qemu). v6+. |  |  | v6+: no hardware here |
| `qpchecks`, `qpcheckd`, `qpcheckq`, `qpcm2b`, `qpcm2h`, `qpcm2w`, … (41 operations) | 6–8 | 0–5 | v6/v7 operations no open source implements: dot products (`qpidot*`, `qpbfdots`), bfloat16 conversion (`qpfstobf`), AES (`qpaes*`) and Kuznyechik/streebog (`qpkuz*`, `qpsbgltr*`, `qpmgme`) rounds, table lookups (`qptbl_*`), masks (`qpcm2*`, `qps2*`), checks. Only names and operand formats (binutils) are known. |  |  | v6+; no open source |

## Memory

Address a + b (64-bit), MAS in the operation's `mas=` field (bit 3 byte swap, bits 5–6 cache hints, mod in bits 0–2: speculation and atomics). Loads take 3 cycles from L1 and zero-extend; stores take their data without the FP-side or cluster penalty. Up to four loads (ALC0, 2, 3, 5) and two stores (ALC2, 5) a cycle.

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `ldb`, `ldh`, `ldw`, `ldd` | 1–8 | 0,2,3,5 | Load 1, 2, 4, 8 bytes, zero-extended. `,sm` is speculative (diagnostic value instead of a fault). **Use:** `mov<mode>` loads; `zero_extend` from memory is free. (speculation.md; semantics.md, "Byte-swapped memory accesses") | 3 | 331k / 326k | abg, abi/ctpr.s, abi/dbl.s, abi/elfops/gotplt.s… (4C, 8C2) |
| `stb`, `sth`, `stw`, `std` | 1–8 | 2,5 | Store the low 1, 2, 4, 8 bytes of the data. ALC2/5. **Use:** `mov<mode>` stores. |  | 139k / 124k | abg, abi/dbl.s, abi/getsp.s, abi/litmus.s… (4C, 8C2) |
| `ldq`, `stq` | 5–8 | 0,2,3,5 | Load or store a register pair (16 bytes at a + b, the low register at the lower address); `ldq` goes through L1 (3 cycles). v5+. (semantics.md, "Memory, tag and descriptor operations") | 3 (v5) |  | semc (8C2) |
| `ldqp`, `stqp` | 5–8 | 0,2,3,5 | 128-bit load/store of a `%qpr` register. `ldqp` bypasses L1 (11 cycles from L2), so lcc loads 128-bit data as two `ldd`. v5+. **Use:** 16-byte atomics (with the lock MAS), `V16QI` spills. (timing.md, "Elbrus-8C2"; atomics.md) | 11 (v5) | 0 / 8.0k | abg, apb, loops, mas_be… (8C2) |
| `stmqp` | 5–8 | 2,5 | Masked 128-bit store: byte i written when bit i of b's low 16 bits is set; address a alone; SIGBUS unless 16-byte aligned. v5+. **Use:** Vector tails at aligned addresses (lcc doesn't). (semantics.md, "Memory, tag and descriptor operations") |  | 0 / 199 | semc, stmqp (8C2) |
| `ldaab`, `ldaah`, `ldaaw`, `ldaad`, `ldaaq`, `ldaaqp`, `staab`, `staah`, `staaw`, `staad`, `staaq`, `staaqp` | 1–8 | 2,5 | Array access through an AAU descriptor `%aadN` and index `%aastiN` (+ literal); `incr` advances the index by `%aaincrN` × the element size. Stores obey loop-mode store suppression. **Use:** Pipelined loops (lcc's APB loops store with `staa`). (apb.md, loops.md) |  | 1.0k / 1.3k | apb, loops, semc (4C, 8C2) |
| `ldcsb`, `ldcsh`, `ldcsw`, `ldcsd`, `ldcsq`, `ldcsqp`, … (78 operations) | 1–8 | 0,2,3,5 | Loads and stores through x86-style segment registers set up by a binary translator; **fault in user mode** (SIGILL, SIGSEGV or SIGBUS). (semantics.md, "Memory, tag and descriptor operations") |  |  | semc (4C, 8C2) |
| `ldgdb`, `ldgdh`, `ldgdw`, `ldgdd`, `ldgdq`, `ldgdqp`, … (19 operations) | 1–8 | 0,2,3,5 | Loads/stores relative to the global (GD) or compilation-unit (CUD) descriptor of protected mode; SIGSEGV `SEGV_BNDERR` in ordinary user mode. |  |  | semc (4C, 8C2) |
| `ldapb`, `ldaph`, `ldapw`, `ldapd`, `ldapq`, `ldapqp`, … (13 operations) | 1–8 | 0,2,3,5 | Loads/stores through a protected-mode array pointer; SIGILL `ILL_ILLOPN` in ordinary user mode. |  |  | semc (4C, 8C2) |
| `ldrd`, `strd`, `ldrqp`, `strqp` | 1–8 | 0,2,3,5 | Privileged register-space access; SIGILL `ILL_PRVOPC` in user mode. |  |  | semc (4C, 8C2) |
| `ldodpb`, `ldodph`, `ldodpw`, `ldodpd`, `ldodpq`, `ldodrb`, … (30 operations) | 1–2 | 0,2,3,5 | Object-descriptor accesses of v2's protected mode, gone from v3: `stodwd` is SIGILL `ILL_ILLOPC` on the 4C, while `ldodwd`'s encoding (and `stodwd`'s on the 8C2) runs without a fault, as some later operation (`v2ops.c`). |  |  | v2ops (4C, 8C2); 2 of 30 |
| `ldosrrd`, `ldosrrqp`, `stosrrd`, `stosrrqp` | 7–8 | 0,2,3,5 | v7 accesses, no open implementation. |  |  | v7; no open source |

## Array prefetch and AAU registers

The array prefetch unit runs a program prepared by `ldisp %ctpr2` and started by `bap`; `mova*` read its buffer in channels 0–3 (apb.md, which gives the correctness rules).

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `movab`, `movah`, `movaw`, `movad`, `movaq`, `movaqp` | 1–7 |  | Read the next element(s) of an array prefetch area: base + index + displacement + step × n, zero-extended; a diagnostic value instead of a fault. 5 cycles. **Use:** Software-pipelined loops in `loop_mode` (lcc at -O2+). (apb.md) |  | 11k / 10k | apb-loop, apb-stream, apb (4C, 8C2); 5 of 6 |
| `bap`, `eap`, `fapb` | 1–7 |  | `bap` starts the prefetch program, `eap` stops it; `fapb` is the program's own instruction format (written by `ldisp` targets). (apb.md) |  | 4.1k / 4.0k | apb-loop, apb-stream, apb (4C, 8C2) |
| `aaurr`, `aaurrd`, `aaurrq`, `aaurw`, `aaurwd`, `aaurwq` | 1–8 | 2,5 | Read or write AAU registers (`%aadN`, `%aaindN`, `%aaincrN`, `%aastiN`, `%aabfN`, …); `aaurr*` of positions read 0 in user mode, `aaurrq` of a descriptor reads it back. ALC2/5. **Use:** Loop setup for `staa`/`ldaa`/APB. (apb.md, "AAU registers") |  | 29k / 30k | apb-loop, apb-stream, apb, loops… (4C, 8C2) |
| `incr` | 1–7 |  | Advance an AAU store/load index (`%aastiN += %aaincrN × size`), in the CDS syllable. (apb.md) |  | 696 / 883 | loops (4C, 8C2) |

## Tags, protected mode and pointers

Registers carry tags (diagnostic values, protected-mode pointers). Ordinary C code meets tags only through speculation (speculation.md); these operations matter to protected mode.

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `gettags`, `gettagd`, `puttags`, `puttagd`, `puttagqp` | 1–8 | 0–5 | `gettag*`: the tag bits of a; `puttag*`: a with tags b (SIGILL for some inputs). (semantics.md, "Memory, tag and descriptor operations") |  | 2.4k / 2.4k | abg, apb-loop, apb, semc… (4C, 8C2) |
| `movts`, `movtd`, `movtq` | 1–8 | 0,1,3,4 | Copy keeping tags; `movtd a, %ctprN` prepares a transfer to the address in a (9 cycles to `ct`; ALC0). **Use:** Indirect calls and jumps (`movtd` + `ct`/`call`). (control-transfer.md) | 9 (`movtd` to `ct`) | 7.6k / 7.4k | abg, abi/ctpr.s, abi/dbl.s, abi/elfops/gotplt.s… (4C, 8C2) |
| `movtcs`, `movtcd`, `movtcq`, `movtrs`, `movtrd`, `movtrq`, `movtrcs`, `movtrcd`, `movtrcq` | 1–8 | 0,1,3,4 | Tag-converting moves; plain copies of untagged values. |  |  | semc (4C, 8C2) |
| `gettc`, `gettst`, `invtc`, `puttc`, `puttst` | 1–8 | 0,3 | Tag/trap-cell state: `gettc`, `puttc`, `puttst` SIGILL `ILL_ILLOPC` in user mode, `gettst` leaves its destination, `invtc` returns an undecoded value. |  |  | semc (4C, 8C2) |
| `getpl` | 1–8 | 0,1,3,4 | Prepare a transfer to the offset a from the compilation unit's start (`getpl a, %ctprN`, ALC0) (guide 10.9.2); into a register it is SIGILL `ILL_ILLOPN` in user mode. (control-transfer.md) |  |  | semc (4C, 8C2) |
| `aptoap`, `aptoapb`, `cudtoap`, `gdtoap`, `getsap`, `getva`, `vfsi` | 1–8 | 0,1,3,4 | Protected-mode pointer conversions and checks (v2–v6). In ordinary user mode `aptoap`, `aptoapb` are SIGILL `ILL_ILLOPN`, `cudtoap`, `gdtoap` SIGSEGV `SEGV_BNDERR`, `getsap` SIGILL `ILL_ILLOPC`, `vfsi` SIGBUS `BUS_ADRALN`; `getva a, b` returns the low 48 bits of the pointer a plus b (a = (0x0123456789abcdef, 0x10), b = 0x10 gives 0x456789abcdff). |  |  | semc (4C, 8C2) |
| `cast`, `odtoap`, `tdtomp`, `gettd`, `getsod` | 1–2 | 0,1,3,4 | Object-descriptor operations of v2's protected mode; SIGILL `ILL_ILLOPC` on v3 and v5 (`v2ops.c`). |  |  | v2ops (4C, 8C2) |
| `apincr`, `apincrc`, `cudincr`, `gdincr`, `getind`, `getmi`, `getptr`, `getptrc`, `getspd`, `subarr`, `subarrc`, `vfsid` | 7–8 | 0,1,3,4 | v7 pointer and array operations of protected mode; no open implementation. |  |  | v7; no open source |

## Registers and state

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `rrs`, `rrd`, `rws`, `rwd` | 1–8 | 0 | Read or write a state register: in user mode `%pfpfr`, `%fpcr`, `%fpsr` (FP control and status), `%lsr`/`%ilcr` (loops), `%clkr` (cycle counter), `%ip`, `%idr`, `%dimar0/1`, …; ALC0. `rwd %lsr` needs 4 cycles before `rrd %lsr`. **Use:** `fenv`, the GOT base (`rrd %ip`), loop counts, cycle counting. (registers.yaml; spec/abi/registers.md) |  | 18k / 24k | abg, abi/elfops/gotplt.s, abi/ldouble.s, apb-loop… (4C, 8C2) |
| `rrshd`, `rwshd` | 7–8 | 0 | v7 state accesses (binutils); not implemented in qemu. |  |  | v7; no open source |
| `getsp` | 1–8 | 0 | Allocate (negative b) or release stack in the user stack and return the new stack pointer; `getsp 0` reads it. 2 cycles. The amount must be a window register or `%g16`+. **Use:** `allocate_stack`, frame setup. (spec/abi/stacks.md) | 2 | 10k / 10k | abi/getsp.s, pmu (4C) |
| `ibranchd`, `icalld` | 6–8 | 0 | Branch or call to the address in a register (community qemu). v6+. **Use:** Indirect jumps and calls without a CTPR preparation. |  |  | v6+: no hardware here |
| `mkfsw`, `modbgv`, `vfbgv` | 6–8 | 1,4 | v6 operations on the global register rotation (`%bgr`) and FP state words; not implemented in qemu. |  |  | v6+; no open source |

## Predicates

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `pass`, `andp`, `landp`, `movep` | 1–7 |  | PLS logic on primary predicates `@pN`: `pass` copies a `%pred` (or a loop/compare condition) to `@p` or back, `andp`/`landp` give c0 && c1 (each optionally negated; `landp`'s second operand is lazy for diagnostic values), `movep` moves a predicate conditionally. 1 cycle through a `%pred`, 0 within a wide instruction (two levels at most). **Use:** Branch conditions, if-conversion of `&&`/`\|\|` chains. (semantics.md, "Predicate logic"; bundle.md, "PLS") | 1; 0 within an instruction | 603k / 601k | ctlops, loops, preds (4C, 8C2) |
| `clpandp`, `clplandp` | 1–7 |  | Stand-alone predicate logic outside a wide instruction, `clpandp %predA, %predB, %predC` (gas expands it into PLS syllables); runs in user mode. |  |  | ctlops (4C, 8C2) |
| `srp`, `crp`, `slrp` | 1–7 |  | SS bits that set (`srp`), clear (`crp`) or set a loop (`slrp`) recovery point, with an optional condition `? %predN` (binutils `parse_srp_args`); not used by lcc. In user mode `crp` runs on both machines, `srp` and `slrp` run on the 4C but are SIGILL `ILL_ILLOPC` on the 8C2. |  |  | ctlops (4C, 8C2) |
| `set_mark`, `vfdi` | 1–7 |  | SS bits (`set_mark` the `sw` bit, `vfdi` a verify bit); both run in user mode without visible effect; meaning not documented. |  |  | ctlops (4C, 8C2) |

## Control transfer

A transfer is prepared into `%ctpr1–3` (`disp`, `return`, `movtd`, `getpl`, `sdisp`), then executed by `ct`, `call` or a condition on it; `ibranch` needs no preparation but costs 6 cycles when taken (control-transfer.md).

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `disp`, `ldisp`, `sdisp`, `return`, `gettsd` | 1–7 |  | Prepare a `%ctpr`: `disp` a label (5 cycles to `ct`), `ldisp` an array prefetch program (`%ctpr2`), `sdisp %ctprN, NUM` a system call entry (glibc: `sdisp %ctpr1, 3` + `call`; 1 for 32-bit, 8 for protected mode), `return` the caller (6 cycles); `gettsd %ctprN` (v3+) runs in user mode. **Use:** Branches, calls (`disp` + `call`), returns, system calls (`sdisp` + `call`). (control-transfer.md) | 5 (`disp`), 6 (`return`) to `ct` | 233k / 229k | abg, abi/ctpr.s, abi/dbl.s, abi/elfops/ext.s… (4C, 8C2) |
| `ct`, `call`, `ibranch`, `rbranch` | 1–7 |  | `ct %ctprN ? cond` transfers (free when prepared in time); `call %ctprN, wbs = N` calls with a new window; `ibranch` is a direct branch (taken: 6 cycles); `rbranch` returns into a recovery block on `%MLOCK` or `%dt_alN`. **Use:** `jump`, `cbranch`, `call`, `return`, check-recovery. (control-transfer.md, speculation.md) |  | 263k / 259k | abg, abi/ctpr.s, abi/dbl.s, abi/elfops/ext.s… (4C, 8C2) |
| `pref` | 1–7 |  | Prefetch code at a label into the instruction cache, `pref %iprN, label` (guide 10.9.6); runs in user mode. |  |  | ctlops (4C, 8C2) |
| `ipd`, `nop`, `wait` | 1–7 |  | `ipd N` instruction prefetch depth of a preparation; `nop N` N empty cycles after the instruction; `wait` orders memory (`ld_c`, `st_c`, …: atomics.md, fences). **Use:** `ipd` as lcc; `wait st_c` for C11 seq_cst stores; `nop` for delays under `.ignore strict_delay`. |  | 733k / 508k | abg, abi/ctpr.s, abi/dbl.s, abi/elfops/gotplt.s… (4C, 8C2) |
| `setwd`, `setbn`, `setbp`, `vfrpsz` | 1–7 |  | `setwd` sets the window size, `nfx` and `dbl`; `setbn`/`setbp` the rotating register and predicate areas; `vfrpsz rpsz = N` checks the parameter area size (`rpsz = 0` in a plain function is SIGILL `ILL_ILLOPC`). (spec/abi/registers.md; loops.md) |  | 72k / 72k | abg, abi/ctpr.s, abi/dbl.s, abi/elfops/ext.s… (4C, 8C2) |
| `loop_mode`, `alc`, `abn`, `abp`, `abg` | 1–7 |  | Loop mode and counters: `loop_mode` marks the loop, `alc` advances the loop counters, `abn`/`abp` rotate the register and predicate areas, `abg` the global registers (`%bgr`). (loops.md) |  | 51k / 50k | abg, apb-loop, apb-stream, loops… (4C, 8C2) |
| `abgc`, `rbgc` | 6–7 |  | Global-register rotation forms of v6+ (binutils; gas doesn't accept them). |  |  | v6+; source only |
| `flushr`, `flushc`, `fillr`, `fillc`, `flushts` | 1–7 |  | Spill the register window stack (`flushr`) or the chain stack (`flushc`) to memory, or fill them back (`fillr`, `fillc`, v6+); `flushts` flushes the trap stack. `flushr`, `flushc`, `flushts` run in user mode. **Use:** Stack walking (unwinders, `setjmp`-style code) needs the stacks in memory. |  |  | ctlops (4C, 8C2); 3 of 5 |
| `setsft` | 2–7 |  | Software trap: SIGTRAP `TRAP_BRKPT` in user mode (the kernel's `BUG()` uses it). **Use:** `trap` (`__builtin_trap`) as a breakpoint-style trap; lcc stores to address 0 instead. |  |  | ctlops (4C, 8C2) |
| `settr`, `setei` | 1–7 |  | System settings: `settr type = N` is SIGILL `ILL_ILLOPC` in user mode, `setei` runs without visible effect. |  |  | ctlops (4C, 8C2) |
| `hcall`, `icall`, `ctp_call`, `dtbp` | 6–7 |  | v6/v7 transfers: hypercall, indirect call, call through a prepared CTPR, the v7 `dtbp` (binutils). |  |  | v6+; source only |
| `done`, `iret`, `hret`, `glaunch` | 1–7 |  | Privileged returns from traps and hypervisor, guest launch: `done` is SIGILL `ILL_PRVOPC` in user mode. |  |  | ctlops (4C, 8C2); 1 of 4 |
| `mmurr`, `mmurw` | 1–7 |  | MMU register access, privileged except `mmurw x, %dam_inv`, which clears the data speculation table (lcc writes it at the exits of loops with check loads); `mmurr` of the monitor registers is SIGILL. (speculation.md; monitors.md) |  |  | pmu, spec (4C, 8C2); 1 of 2 |

