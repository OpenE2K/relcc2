# Registers

## Register file (guide 9.3.1)

The register file holds 256 64-bit registers. Each also has an extended
part: 16 bits on v1–v4, making an 80-bit register for `long double`, and
64 bits on v5+, which also holds 128-bit packed values (lcc-doc). See
"Extended parts and `long double`" below.

| Part | Registers | Addressed as |
|---|---|---|
| Global | 32 | `%g0`–`%g31` (`%dg`, `%qg`), absolute |
| Stack (windows) | 224 | `%r0`–`%r63` relative to the current window base; `%b[0]`–`%b[127]` relative to the rotating base inside the window |

- `%rN` / `%drN` is the 64-bit register N of the window; `%qrN` (even N) is
  the pair N, N+1, low half first.
- A window can be larger than 64 registers, but `%r` addressing reaches only
  the first 64. `%b[]` reaches up to 128 registers from the rotating base.
- `%g24`–`%g31` can rotate (`abg`/`abgc`).

### Window and rotating-base units

The hardware counts these in **pairs of 64-bit registers** ("quad"
units). (kernel `e2k_wd_t`, `e2k_br_t`; unipro, lcc)

| Operation | Field | Meaning |
|---|---|---|
| `setwd wsz=N` | `wsz` | Window size: N pairs (2N 64-bit registers) |
| `setbn rbs=B, rsz=S, rcur=C` | `rbs` | Rotating area starts at pair B (`%b[0]` = `%r(2B)`) |
| | `rsz` | Rotating area size: S + 1 pairs |
| | `rcur` | Current rotation offset |
| `setbp psz=P` | `psz` | Rotating predicate area size |
| `call ..., wbs=W` | `wbs` | The callee's window starts at pair W of the caller's window |

Window size limits (Elbrus 4C; `tools/abi/probes/winsize.c`, and the
proof-of-concept backend's probe, `docs/spec-poc-findings.md` A2):

- A callee's `setwd wsz` must be **at least the part of the caller's
  window it inherits**, `caller wsz − wbs` pairs; otherwise `setwd` raises
  SIGILL (caller `wsz = 8`: with `wbs = 4` the callee needs `wsz ≥ 4`, with
  `wbs = 6` it needs 2, with `wbs = 7` 1). lcc always hands over exactly 4
  pairs (the 8 argument registers, `wsz = 8`, `wbs = 4`, or `wsz = 0x10`,
  `wbs = 0xc`) and never uses `wsz` below 4, so the convention is: callers
  pass at most 4 pairs, callees use `wsz ≥ 4`.
- The largest window is **112 pairs** (224 registers, the whole stack part
  of the register file), also after spills; 113 traps. Registers above
  `%r63` are reachable only through `%b[]`.
- gas accepts `rbs` 0–63 only.
- Operations in the wide instruction with `setwd` may already use the new
  window: `{ setwd wsz = 8; addd %r0, 5, %r12 }` works (lcc puts `getsp
  ..., %r3` there), but `%b[]` in the instruction with `setbn` still uses
  the old rotating base.

`setwd` also sets two window flags (qemu, kernel):

- **`nfx`**: the kernel's `WD.fx` is its inverse. With `nfx = 0` the
  hardware saves and restores the extended parts of the window's
  registers when spilling; with `nfx = 1` a spilled window comes back with
  them cleared (hw, qemu: `tools/abi/probes/ldouble.c`). Both compilers
  clear `nfx` only in functions that use `long double` (lcc, unipro).
- **`dbl`** (v3+): with `dbl = 1`, 32-bit integer ALU operations
  zero-extend their result into the whole 64-bit register; with
  `dbl = 0`, they write only the low half and leave the upper half
  (including its tag) unchanged. The flag is saved and restored per frame
  (`cr1_hi.wdbl`). Unipro always uses `dbl = 1`. lcc chooses per
  function (lcc 1.29.16 and 1.31.05): `dbl = 0` without optimisation,
  and at `-O2` and above `dbl = 1` in functions whose unsigned 32-bit
  results must be zero-extended (an `unsigned` sum returned or used as an
  index, `unsigned` → `unsigned long`), which saves the extension;
  signed code keeps `dbl = 0` and uses `sxt`. In 400 of GCC's
  `gcc.c-torture/execute` tests at `-O2`, 387 of 2,102 functions had
  `dbl = 1` (386 with 1.31.05); at `-O0` none. Code must not rely
  on the upper half of a 32-bit result either way. Measured on the 4C
  (`tools/abi/probes/dbl.c`, hw; MCST's qemu agrees):

  | Operation | `dbl = 0` | `dbl = 1` |
  |---|---|---|
  | `adds`, `shls`, `muls` (32-bit integer) | upper half kept | zero-extended |
  | `ldw` | zero-extended | zero-extended |
  | `sxt` | 64-bit result | 64-bit result |
  | `fadds` (32-bit float) | upper half kept | upper half kept |
  | `movts` | upper half kept | upper half kept |

  MCST's qemu limits `dbl` to the integer operation group plus `gettags`,
  `puttags` and `rrs` (`operations-descr.h`, `e2k_get_eff_dst_type`). The
  community qemu applies it to every 32-bit ALU result except `movt*s`
  (`translate.c`, `is_alop_affected_by_dbl`), which disagrees with the
  hardware for `fadds`.

## Conventions

| Register | Convention | Source |
|---|---|---|
| Window registers | Saved automatically by the call mechanism, except the parameter area, which a call may change | guide 9.3.1.2 |
| `%g13` | Thread pointer (TLS) | guide 9.3.1.4 |
| `%g12` | Reserved by the guide; used as the **static chain** by MCST's libffi (its closure trampolines load the closure address into `%g12` and jump; Go closures receive it there; its testsuite defines `STATIC_CHAIN_REG "%g12"`). In protected mode it is the thread pointer | guide 9.3.1.4, libffi |
| Other globals | Scratch: not preserved across calls | guide 9.3.1.4 |
| Predicates `%pred0`–`%pred31` | Saved and restored automatically; each call gets a fresh predicate file | guide 9.3.2, 9.5.3 |
| `%ctpr1`–`%ctpr3` | Scratch: not preserved across calls. Returns go through `%ctpr3` | guide 9.3.3 |
| `%lsr`, `%ilcr` (loop status, counter) | Scratch | guide 9.3.4 |
| `%fpfr`, `%pfpfr`, `%upsr` | Special: a function that changes them must restore them | guide 9.3.4 |
| `WD`, `BR`, `USD`, `CUD`, `GD`, `CUIR`, `TSD`, `IP` | Saved and restored automatically | guide 9.3.4 |

The kernel saves `%g12` and `%g13` only on context switch. All other
globals are additionally saved around user signal handlers (guide 9.3.1.4).
Keeping global variables in global registers is only safe in
single-threaded programs without signal handlers.

## Floating-point control and status (kernel, glibc)

Three 32-bit registers, read with `rrs` and written with `rws`
(kernel `cpu_regs_types.h`: `e2k_pfpfr_t`, `e2k_fpcr_t`, `e2k_fpsr_t`;
glibc `sysdeps/e2k/fpu`). The exception bits are in the x87 order in
all three: invalid, denormal, divide by zero, overflow, underflow,
precision (inexact); glibc's `FE_*` values are those bits (`FE_INVALID`
0x01, `FE_DIVBYZERO` 0x04, `FE_OVERFLOW` 0x08, `FE_UNDERFLOW` 0x10,
`FE_INEXACT` 0x20; denormal 0x02 has no `FE_` name).

| Register | Used by | Bits |
|---|---|---|
| `%pfpfr` | `float`, `double`, packed | 0–5 flags (`ie de ze oe ue pe`); 7–12 masks (`im dm zm om um pm`, 1 = masked, i.e. flag << 7); 13–14 rounding (0 nearest, 1 down, 2 up, 3 toward zero: `FE_DOWNWARD` 0x2000 ...); 15 `fz` (flush to zero); 26–31 `die dde dze doe due dpe` |
| `%fpcr` | 80-bit (x87 control word) | 0–5 masks (`im` ... `pm`); 6 always 1; 8–9 precision control; 10–11 rounding (`FE_FPCR_DOWNWARD` 0x400 ...); 12 `ic` |
| `%fpsr` | 80-bit (x87 status word) | 0–5 flags; 7 `es` (error summary); 9 `c1`; 15 `bf` |

glibc keeps the two sets in step: `fenv_t` is `{ __fpcr, __fpsr,
__pfpfr }`; `feholdexcept` saves all three, then sets every mask in
`%pfpfr` and `%fpcr` and clears the flags in `%pfpfr` and `%fpsr`;
`feenableexcept` clears mask bits in both `%pfpfr` (`excepts << 7`) and
`%fpcr`. A compiler that saves and restores the FP environment (GCC's
`TARGET_ATOMIC_ASSIGN_EXPAND_FENV`, for C11 atomic compound assignment
of floating types) must do the same for both pairs, since `long double`
uses `%fpcr`/`%fpsr`. Processes start with all six flags set
(`%pfpfr` 0x1fbf, `%fpsr` 0x3f; speculation.md), so code (and tests)
must not assume clear flags at startup. (Found by the proof-of-concept
backend, `docs/spec-poc-findings.md` P7.)

## DWARF register numbers

| DWARF number | Register | Source |
|---|---|---|
| 0–63 | `%r0`–`%r63` (64-bit `%dr`) | gdb `e2k_dwarf_reg_to_regnum`, lcc, unipro |
| 64–127 | `%b[0]`–`%b[63]` | gdb, libgcc |

No other registers have DWARF numbers: lcc assigns none to `%b[64]`–`%b[127]`,
globals, predicates or control registers. Debug info from lcc and Unipro
uses `DW_OP_regN` with these numbers; arguments appear as `DW_OP_reg0`, … in
the callee.

**Exception handling** uses DWARF 64 and 65 (`%b[0]`, `%b[1]`) as
`__builtin_eh_return_data_regno (0)` and `(1)` (libgcc). A compiler that
numbers them differently produces libraries that are incompatible with
lcc's libgcc and libstdc++ at run time; the libgcc source notes this
happened with an old `e2k-linux-gcc`.

## Extended parts and `long double` (lcc, hw, qemu)

Checked by `tools/abi/probes/ldouble.c` on the 4C (v3) and under MCST's
qemu as v3 and v5.

- lcc keeps a `long double` in one register: the 64-bit mantissa (with
  the explicit integer bit, as on x87) in the ordinary part and the
  sign and 15-bit exponent in the extended part. `movif mant, exp, dst`
  builds it from two registers; the mantissa is then read as the ordinary
  register and `movfi src, dst` reads the 16-bit part. Arithmetic uses the
  `fx*` operations (`fxaddxx`, `fxmulsx`, `fxdivxx`, `fxsqrtixx`, ...;
  the suffix letters give the operand formats: `x` 80-bit, `d` double, `s`
  single), conversions `fxtofd`, `fdtofx`, `idtofx`, `fxtoidtr`,
  compares `fxcmp*xb` into predicates. Loads and stores go through the
  two parts: `ldd`/`ldh`, then `movif`; `std` of the mantissa and `sth`
  of `movfi`'s result. lcc's v5 code is the same.
- **A `long double` kept in a window register across a call needs
  `nfx = 0`** in the function that owns the window: with `nfx = 1` the
  16-bit part reads back as 0 after the window has been spilled to the
  procedure stack (a deep call), though it survives a call that does not
  spill (hw, qemu). lcc sets `nfx = 0` in every function that touches
  `long double` and keeps such values in window registers across calls.
  Global registers are not spilled.
- The arithmetic is x87 extended precision exactly: 64-bit mantissa,
  round to nearest even, denormals down to 2^-16445, overflow to
  infinity; `sqrtl(2)` is correctly rounded. Unnormals and pseudo-NaNs
  raise invalid and give the x87 "indefinite" NaN (`ffff c000…`); a
  pseudo-denormal is normalised (hw, and MCST's qemu agrees).
- 80-bit operations take their rounding mode and precision control from
  **`%fpcr`** (the x87 control word: RC bits 10–11, PC bits 8–9, default
  `0x037f`) and record exceptions in **`%fpsr`** (x87 status word), not
  in `%pfpfr`, which controls `float`/`double` and packed operations.
  With PC = double, `1.0L/3` is rounded to 53 bits, as on x87 (hw). glibc's
  `fesetround`, `feclearexcept` and `fetestexcept` update and read both
  registers, so C code sees one environment. **MCST's qemu gets this
  wrong**: it applies `%pfpfr` to 80-bit operations, ignores precision
  control and sets `%pfpfr` flags.
