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

A callee starts with its caller's `%br` (rotating base and predicate
rotation) until it sets its own with `setbn` or `setbp` (hw:
`tools/abi/probes/stackregs.c`; 4C, 8C2, 2C3; MCST's qemu zeroes it at
every call): [stack-and-unwinding.md](stack-and-unwinding.md), "Stack
registers".

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
- **A function may not call with a `wbs` smaller than what it
  inherited**: one that inherited P pairs from its caller must call with
  `wbs ≥ P`, or its callee's window would start inside the caller's
  caller's window. In a recursion with one window size W and one `wbs`,
  `wbs ≥ W / 2` works and every smaller `wbs` gets SIGSEGV
  (`si_code` 3) at the second nested call (W 16: `wbs` 8 works, 7 dies;
  W 32: 16 works, 15 and 12 die; depth 1 always works; 4C, 8C2, 2C3,
  `tools/isa-probes/pocchk2.c` `rec*`; the proof-of-concept back end's
  I32). The convention (hand over 4 pairs, call with `wbs ≥ 4`) never
  breaks it.
- gas accepts `rbs` 0–63 only.
- Operations in the wide instruction with `setwd` may already use the new
  window: `{ setwd wsz = 8; addd %r0, 5, %r12 }` works (lcc puts `getsp
  ..., %r3` there), but `%b[]` in the instruction with `setbn` still uses
  the old rotating base. (MCST's qemu applies the new base there, so code
  that writes an argument register next to its `setbn` works under qemu
  and fails on hardware: a recursion written so dies with
  SIGSEGV at once on the 4C; `tools/abi/probes/sysabi.s`, `sa_down`.)

What windows cost (`pocchk2.c`, `%clkr`; 4C, 8C2 and 2C3 alike; the
proof-of-concept back end's I31):

| Case | Cycles |
|---|---|
| `disp` + `call` + `return`/`ct` of an empty leaf, caller window 16 pairs, callee 4, 8, 16, 32 or 64 pairs | 14.1 |
| the same, callee 112 pairs (caller and callee no longer fit the register file) | 32.4 |
| recursion, a level (call and return), handing over 4 pairs (`wbs` = W − 4), depth 8 / 32: W 8 | 17.6 / 18.1 |
| W 16, 32, 48, 64, 112 | 20.4 / 30.2, 47 / 55, 74 / 80, 101 / 104, 182 / 192–208 |
| W 48 or 40 with `wbs` 28 (keeping 28 pairs below the callee, as W 32) | 48.5–50 / 55.5 |
| `setwd` to the same size twice in a function | free |
| `setwd` 16 → 32 pairs and back | about 3 cycles for the pair |

Window size is free while the windows of the active calls fit the
register file; once they don't, a call and its return cost about
15 + 1.6 × `wbs` cycles, the part of the caller's window below the
callee being spilled and filled; the callee's own size doesn't matter.
So in deep call chains what costs is the pairs each frame keeps below
its callee; a function that calls should keep its window small at its
calls, but each grow and shrink costs about what the spills do (lcc
grows the window with a second `setwd` only around a pipelined loop).
In real programs the spills are small: the proof-of-concept back end
measured 0.54% of xz -6's cycles in `BUB_B_RF_SPILLFILL` on the 8C2
(windows of 48–55 pairs on its hot chain; lcc's 17–27 never spill) and
0.30% of 7-Zip's benchmark for lcc.

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
  | the whole 32-bit integer ALU group: `adds`, `ands`, `andns`, `shls`, `sars`, `scls`, `muls`, `sdivs`, `udivs`, `getfs`, `merges` (either predicate value), `popcnts`, `lzcnts`; also predicated (`? %pred`, when it writes) and speculative (`,sm`) | upper half kept | zero-extended |
  | the 32-bit combined integer operations (`add_adds`, `shr_ands`, `and_ors`, `xor_xors`, …) | upper half kept | zero-extended (`tools/isa-probes/pocchk.c`; 4C, 8C2, 2C3) |
  | `ldw` | zero-extended | zero-extended |
  | `sxt` | 64-bit result | 64-bit result |
  | FP-unit operations with a 32-bit result: `fadds`, `fstois`, `fdtois`, `fstoistr`, `movts` | upper half kept | upper half kept |
  | the tag operations with a 32-bit result, `gettags`, `gettagd`, `puttags` [hw: `tools/isa-probes/tagx.c` `gts_upper`, `gtd_upper`, `ps_upper`; 4C, 8C2, 2C3] | upper half kept | not checked |
  | `rrs` of a state register [hw: `tools/isa-probes/sregs.c` `width`; 4C, 8C2, 2C3] | low half written, upper half kept | 32-bit registers (`%pfpfr`, `%fpcr`, `%fpsr`, `%upsr`, `%core_mode`) and `%idr` (whose value fits in 32 bits): zero-extended. 64-bit ones (`%clkr`, `%ip`): the low half zero-extended on the 8C2 and 2C3, but **the whole 64-bit value on the 4C** (`rrs %ip` gives a PIE's full address there) |
  | `rrd` of a 32-bit state register | zero-extended | zero-extended |

  This holds for every register the operation writes, the globals
  `%g0`–`%g31` included (`adds`, `muls` into `%g20`). The table is from
  `tools/abi/probes/dbl.c` (generated by `gen_dbl.py`) on the 4C and the
  Elbrus-8C2, which agree (the proof-of-concept back end's findings I16,
  A18 rechecked), except the rows that name another probe.

  `rrs` and `rrd` of `%pfpfr`, `%fpcr`, `%fpsr`, `%upsr`, `%core_mode`,
  `%idr`, `%clkr` and `%ip` assemble and run in user mode on all three
  machines, and their results have no tags [hw:
  `tools/isa-probes/sregs.c` `width`, plus a PIE build for `%ip`]. To
  get a zero-extended FP control or status word, use `rrd` (any `dbl`)
  or `rrs` under `dbl = 1`. Under `dbl = 0`, `rrs` leaves the caller's
  upper half in place. MCST's qemu and the community qemu zero-extend
  under `dbl = 1` (as the 8C2 does).

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
| `%g0`–`%g11`, `%g14`, `%g15` | Scratch across calls, but **not for temporaries**: a signal handler's writes to them reach the interrupted code (below) | kernel, hw |
| `%g16`–`%g31` | Scratch: not preserved across calls, and **emptied (0, tag 5) by system calls** through entry 3; fast calls may change `%g26`–`%g31` ([kernel-interface.md](kernel-interface.md), "System calls"). Saved and restored around signal handlers that interrupt code outside a system call, so usable for temporaries between calls (lcc's code uses these and never `%g0`–`%g15`) | kernel, hw, lcc |
| Predicates `%pred0`–`%pred31` | Saved and restored automatically: the caller's come back whatever the callee writes. The callee starts with a copy of the caller's values (hw: `tools/isa-probes/preds.c` `call_*`; the guide says only "a new predicate file"), so it must write a predicate before reading it ([predicates.md](../isa/predicates.md)) | guide 9.3.2, 9.5.3, hw |
| `%ctpr1`–`%ctpr3` | Scratch: not preserved across calls. Returns go through `%ctpr3` | guide 9.3.3 |
| `%lsr`, `%ilcr` (loop status, counter) | Scratch | guide 9.3.4 |
| `%fpcr`, `%fpsr`, `%pfpfr`, `%upsr` | Special: a function that changes them must restore them. **`UPSR.fsm` (bit 8) is never changed**: processes start with it clear, and with it set the FP predicate compares give x86 results for NaN operands (semantics.md), so code may assume it is clear (owner's decision, 2026-10-11) | guide 9.3.4 |
| `WD`, `BR`, `USD`, `CUD`, `GD`, `CUIR`, `TSD`, `IP` | Saved and restored automatically | guide 9.3.4 |

The kernel saves all globals on context switch, but around a user signal
handler it saves and restores only `%g16`–`%g31`, which it calls "local"
(`arch/e2k/include/asm/glob_regs.h`, `kernel/signal.c`:
`sig_save_local_gregs`, `sig_restore_local_gregs`); `%g0`–`%g15` are
"global for user applications according to ABI" and keep whatever the
handler leaves in them. The guide (9.3.1.4) says all other globals are
saved around handlers, which is wrong for the 6.1 kernel: on the 4C,
the 8C2 and the 2C3 a handler that writes `%g1` and `%g16` changes the
interrupted code's `%g1` and not its `%g16` (`tools/isa-probes/pocchk.c`;
the proof-of-concept back end, which kept temporaries in `%g0`–`%g11`,
crashed under signals until it moved them). So compiler temporaries
belong in `%g16`–`%g31`, as lcc does, and `%g0`–`%g15` are for values
meant to be global (as `%g12`, `%g13`; the kernel also decodes a
`getsp` amount only from `%g16` on, elf.md). A signal delivered at the
end of a system call returns to code whose `%g16`–`%g31` the system call
has already emptied: `sigreturn` puts the frame's values back only for
code interrupted outside a system call [hw: `tools/abi/probes/ctxsig.c`
section 2; [kernel-interface.md](kernel-interface.md), "Signals"].

## Floating-point control and status (kernel, glibc)

Three 32-bit registers, read with `rrs` and written with `rws`
(kernel `cpu_regs_types.h`: `e2k_pfpfr_t`, `e2k_fpcr_t`, `e2k_fpsr_t`;
glibc `sysdeps/e2k/fpu`). Which bits each one keeps was found by
writing and reading them on the 4C, 8C2 and 2C3 [hw:
`tools/isa-probes/sregs.c` `write` (with `es2`) and `fp`]; MCST's qemu
keeps the same bits of `%pfpfr` and `%fpcr`, but stores `%fpsr`'s `es`
and `bf` and never computes them or C1 [source: qemu
`target/e2k/cpu-sregs.h`]. The exception bits are in the x87 order in
all three: invalid, denormal, divide by zero, overflow, underflow,
precision (inexact); glibc's `FE_*` values are those bits (`FE_INVALID`
0x01, `FE_DIVBYZERO` 0x04, `FE_OVERFLOW` 0x08, `FE_UNDERFLOW` 0x10,
`FE_INEXACT` 0x20; denormal 0x02 has no `FE_` name).

| Register | Used by | Bits |
|---|---|---|
| `%pfpfr` | `float`, `double`, packed | 0–5 flags (`ie de ze oe ue pe`); 6 `daz` (denormals are zero; the kernel's `e2k_pfpfr_t` calls it `zero1`, MCST's qemu `daz`) [hw]; 7–12 masks (`im dm zm om um pm`, 1 = masked, i.e. flag << 7); 13–14 rounding (0 nearest, 1 down, 2 up, 3 toward zero: `FE_DOWNWARD` 0x2000 ...); 15 `fz` (flush to zero) [hw]; 16–25 read 0; 26–31 `die dde dze doe due dpe` can be written, but nothing sets, clears or reads them ([speculation.md](../isa/speculation.md), "Speculative operations"). Bits 0–15 are x86 MXCSR's layout, and the operations follow SSE exactly ([semantics.md](../isa/semantics.md), "Rounding modes, flush to zero and denormals are zero") |
| `%fpcr` | 80-bit (x87 control word) | 0–5 masks (`im` ... `pm`); 6 always 1; 7 and 13–15 read 0; 8–9 precision control; 10–11 rounding (`FE_FPCR_DOWNWARD` 0x400 ...); 12 `ic` can be written but has no effect: infinities stay affine (with `ic` set `fxcmpudxf` still orders them: `inf` > 1, `-inf` < `inf`), as on x87 since the 387 |
| `%fpsr` | 80-bit (x87 status word) | 0–5 flags; 9 `c1` ([semantics.md](../isa/semantics.md), "80-bit operations"). 7 `es` (error summary) and 15 `bf` are **not stored**: both read 1 exactly when a flag in bits 0–5 is set whose mask in `%fpcr` is clear (checked with each mask cleared alone), and they follow `%fpcr` writes alone (`%fpsr` 0x3f reads 0x80bf under `%fpcr` 0x340 and 0x003f under 0x37f). x87's C0, C2, C3 and TOP (8, 10–14) do not exist and read 0. Bit 6 (x87's stack fault) is stored on the 2C3 only, does not set `es`, and no operation sets it |

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

**Traps** [hw: `tools/isa-probes/sregs.c` `fp`, the three machines;
kernel `arch/e2k/kernel/traps.c:1845-1885`]. `float`, `double` and
packed operations raise SIGFPE when the exception's mask in `%pfpfr` is
clear, whatever `%fpcr` says. 80-bit operations raise it when the mask
in `%fpcr` is clear, whatever `%pfpfr` says. Neither kind reads or sets
the other register's flags. The trap is precise: `si_addr` is the wide
instruction that holds the operation, and the handler finds the new
flag set. There is no x87-style deferred trap: an exact `fx*` operation
runs normally when an unmasked flag is already set in `%fpsr`. With an
underflow unmasked, an 80-bit operation traps on a tiny result even if
it is exact, as x87 does. The kernel's `do_fp` derives `si_code` from
every flag that is set, not from the one that trapped. If `%fpsr.es` is
set it decodes `%fpsr`; otherwise it decodes `%pfpfr`:

| Exception | `%pfpfr` trap, flags clear before | `%fpcr` trap, flags clear before | any trap, all six flags set before |
|---|---|---|---|
| invalid | `FPE_FLTINV` 7 | 7 | `FPE_FLTRES` 6 |
| divide by zero | `FPE_FLTDIV` 3 | 3 | 6 |
| overflow (the trap leaves inexact clear) | `FPE_FLTOVF` 4 | 4 | 6 |
| underflow | 6 (`fmuld` sets underflow and inexact) | `FPE_FLTUND` 5 | 6 |
| denormal | 5 | 5 | 6 |
| inexact | 6 | 6 | 6 |

A `%pfpfr` trap taken while `%fpsr` holds an unmasked flag gets
`%fpsr`'s code (`faddd` inexact with `%fpsr` = 0x01 and `%fpcr` 0x340
reports `FPE_FLTINV`). So `si_code` is reliable only when the flags
were clear before the operation. Processes start with all of them set,
so a handler that needs the cause should read the flags itself
(inferred).

**Flush to zero and denormals are zero** [hw: `tools/isa-probes/fenvlib.c`,
all three machines]. MCST's libgcc has a `crtfastmath.c` whose
constructor sets `%pfpfr` bits 6 and 15 ("mcstbug #149583"); lcc links
`crtfastmath.o` for `-ffast-math` and `-Ofast` (not for its own
`-ffast`), so such programs start with `%pfpfr` 0x9fff instead of
0x1fbf. glibc's `fesetround`, `feclearexcept`, `feholdexcept`,
`feupdateenv`, `feenableexcept`, `fegetenv` and `fesetenv` of a saved
environment keep both bits; `fesetenv (FE_DFL_ENV)` and `fesetenv
(FE_NOMASK_ENV)` clear them.

**glibc's `fesetenv (FE_DFL_ENV)` is wrong** [hw, glibc
`sysdeps/e2k/fpu/fesetenv.c`]: it means to mask every exception
(`/* … все их маскируем */`) but writes `FE_ALL_EXCEPT << 7` to
`%pfpfr` (0x1e80) and `FE_ALL_EXCEPT` to `%fpcr` (0x3d, read back as
0x7d), and `FE_ALL_EXCEPT` lacks the denormal bit. Afterwards the
denormal exception is unmasked in both registers (`1.0 + 4.9e-324`
raises SIGFPE with si_code 5, `FPE_FLTUND`, or SIGILL in lcc `-O2` code,
whose speculative FP operations turn it into a diagnostic value), and
`%fpcr`'s precision control is 0, single: `long double` `1.0L / 3.0L`
gives `0xa.aaaabp-5` instead of `0xa.aaaaaaaaaaaaaabp-5` until the
environment is restored (processes start with `%fpcr` 0x37f). A test
suite that calls `fesetenv (FE_DFL_ENV)` sees both. A compiler's
`-ffast-math` startup should do what lcc's does (link MCST's
`crtfastmath.o`).

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
  16-bit part is lost when the window is spilled to the procedure stack
  (a deep call): it reads back as 0 on the Elbrus-8C2 and as 0 or stale
  data (`0x1f98` in one run) on the 4C, though it survives a call that
  does not spill (hw, qemu). lcc sets `nfx = 0` in every function that touches
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
