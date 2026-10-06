# Source-level conventions: macros, inline assembly, options

What a compiler must provide, beyond the binary interface, so that code
and headers written for lcc build unchanged: predefined macros,
inline-assembly constraints and register names, and the lcc options and
behaviours that change generated code. Probes: `tools/abi/probes/asmops.c`;
macros from `lcc -dM -E -xc /dev/null`.

## Predefined macros (lcc, unipro, lcc-doc)

| Macro | lcc | Unipro | Used by | For a new compiler |
|---|---|---|---|---|
| `__e2k__` | 1 | 1 | glibc (177 files), Linux, gdb | define |
| `__iset__` | ISA version: 3..7 | same | glibc (142 files: ISA-specific code) | define, from `-march` |
| x86 feature macros: `__MMX__`, `__SSE__` … `__SSE4_2__`, `__SSSE3__`, `__AVX__`, `__AVX2__`, `__FMA__`, `__AES__`, `__SHA__`, `__BMI2__`, `__POPCNT__` and more (lcc-builtins.md) | 1 on every target, no option needed | ? | programs choosing an x86 SIMD path; lcc's `<immintrin.h>` emulates the intrinsics | don't define (owner's decision: no x86 intrinsics in the GCC port; lcc-builtins.md) |
| `__elbrus_4c__`, `__elbrus_8c__`, `__elbrus_1cplus__`, `__elbrus_8c2__`, `__elbrus_12c__`, `__elbrus_16c__`, `__elbrus_2c3__`, `__elbrus_48c__`, `__elbrus_8v7__` | with `-mtune=<cpu>` (or a CPU-specific lcc such as `e2k-v3.4c`) | no | glibc (14 files) | define with `-mtune` |
| `__ptr64__` (`__ptr32__`, `__ptr128__` in the other modes) | 1 | `__ptr64__` | glibc (`__ptr128__`: 137 files), Linux | define |
| `_LP64`, `__LP64__` | 1 | 1 | everything | define |
| `__LCC__` (version: 129, 131), `__LCC_MINOR__`, `__MCST__` | yes | no | glibc's installed headers: see below | **do not define** |
| `__LCC_MAS_NO` 0, `__LCC_MAS_SPEC` `0x10000000`, `__LCC_MAS_VOLATILE` `0x20000000`, `__LCC_MAS_CLEARTAG` `0x40000000`, `__LCC_CHAN_ANY` −1 | yes | yes | arguments of the `__builtin_loadmas_*`/`storemas_*` builtins; glibc (2 files) | define if those builtins are supported |
| `__SIZEOF_LONG_DOUBLE__` 16, `__SIZEOF_FLOAT80__` 16, `__SIZEOF_FLOAT128__` 16, `__LDBL_MANT_DIG__` 64 | yes | `__SIZEOF_LONG_DOUBLE__`, `__FLOAT128__` | glibc (`bits/floatn.h`) | define (GCC does from the modes) |
| `__LONG_DOUBLE__` 80 | yes (1.31.05 expands it to the undefined name `TARGET_LONG_DOUBLE_BITSIZE`, a bug) | no | nothing found | not needed |
| `__REGISTER_PREFIX__`, `__USER_LABEL_PREFIX__` | empty | `__USER_LABEL_PREFIX__` empty | | empty |
| `__GNUC__` | 11 (`-gcc-version` selects 7, 9 or 11) | 4 | | the GCC version |

glibc's installed headers test `__LCC__` to tell lcc from GCC (`stdio.h`
format attributes, `math.h` and `bits/mathinline.h`, `bits/floatn.h`,
`sys/cdefs.h`, `pthread.h`, ...). Without it they take the plain-GCC
branch, which is what a GCC port wants; that branch expects `__float128`
and, from GCC 7, `_Float128` with the `f128` suffix (`__HAVE_FLOAT128`
= 1). `bits/mathinline.h` (lcc only) shows the inline-asm style
below.

CPUs and ISA versions, as lcc's `-mtune` and `-march` take them
(**lcc-doc**): `elbrus-4c` v3; `elbrus-8c`, `elbrus-1c+` v4; `elbrus-8c2`
v5; `elbrus-12c`, `elbrus-16c`, `elbrus-2c3` v6; `elbrus-48c`, `elbrus-8v7`
v7. `-march=elbrus-v3`..`v7` (v2, `elbrus-2c+`, was dropped in 1.30).
`-mtune` overrides `-march` and marks the binary as runnable on that CPU
only (`EF_E2K_INCOMPAT`, [elf.md](elf.md)). Run-time CPU checks are
below.

## CPU detection at run time (lcc, lcc-doc)

lcc's builtins (`builtin_mcst.html`): `__builtin_cpu_init ()`,
`__builtin_cpu_is ("elbrus-8c2")`, `__builtin_cpu_name ()`,
`__builtin_cpu_arch ()` (deprecated) and `__builtin_e2k_cpu_iset ()`
(the ISA version, 0 if unknown). `__builtin_cpu_supports` is not
supported ("unknown built-in function").

`__builtin_cpu_is` compiles to a load from a 64-byte object
**`__cpu_info`** (hidden visibility) and a compare [lcc]:

| Offset | Holds | `__builtin_cpu_is` names |
|---|---|---|
| +0 | ISA version | `elbrus-v2` … `elbrus-v7`: 2 … 7 (deprecated forms) |
| +4 | lcc's CPU code | `elbrus-s` 2, `elbrus-2c+` 3, `elbrus-4c` 4, `elbrus-2cm` 5, `elbrus-8c` 6, `elbrus-1c+` 7, `elbrus-8c2` 8, `elbrus-12c` 9, `elbrus-16c` 10, `elbrus-2c3` 11, `elbrus-48c` 12, `elbrus-8v7` 13 |

These codes are not `%idr`'s model numbers
([semantics.md](../isa/semantics.md), "Other facts"). `__cpu_info` and
`__cpu_init`, which fills it, are defined in lcc's proprietary
`liblcc.a` (`cpuinfo_init.o`; `__cpu_init` runs as a `.ctors.65434`
constructor, priority 101), which lcc's driver always links.

**MCST's static glibc needs it.** glibc is built by lcc and calls
`__builtin_cpu_is` when it checks a library's `e_flags` against the CPU
(`runtime_compatible_host`, `sysdeps/unix/sysv/linux/e2k/ldsodefs.h`).
`ld-linux.so.2` has its own local copy, but `libc.a`'s `dl-load.o`
leaves `__cpu_info` undefined, so a static link by another compiler that
pulls in `dlopen`, NSS or iconv fails ("hidden symbol `__cpu_info'
isn't defined") unless something else defines and fills it. The
proof-of-concept backend's libgcc does that from `%idr`, matching lcc's
values under qemu (v3, v5, v6) and on the 4C
(`docs/spec-poc-findings.md` A10).

## Inline assembly (lcc-doc, lcc)

Constraints, as lcc 1.28+ defines them:

| Constraint | Register | Sizes |
|---|---|---|
| `r` | ordinary ("non-extended") 64-bit registers | 1–8 bytes: one register; 9–16 bytes: an even-aligned pair (`%r2`+`%r3`), for every type |
| `x` | one extended register: 80-bit on v1–v4, 128-bit ("quadro-packed") on v5+ | `sizeof` 16 only: `long double` on any ISA; 16-byte vectors, `__int128` and so on on v5+ |

- Transition (lcc 1.28–1.31): by default (`-mold-asm-constraint-r`)
  lcc still treats `r` on a `long double` or a 16-byte vector as `x` and
  warns. `-mno-old-asm-constraint-r` gives the meaning above, which MCST
  says will become the default. glibc's `bits/mathinline.h` uses `x` for
  lcc ≥ 1.28 and `r` otherwise. A new compiler should implement the new
  meaning.
- Operands print as the base register name whatever their size or
  constraint: `%0` becomes `%r2` for a `long`, an `__int128` pair or an
  `x` operand (`asmops.c`). The operation decides the width (`fxsqrtixx
  %1, %0` in mathinline.h); no operand modifiers are used.
- An `x` value must be copied only with `movx` (or rebuilt with
  `movif` from its two halves): a 64-bit copy (`addd`) drops the 16-bit
  extended part. lcc packs an `x` input with `movif`, emits `movx r, r`
  after an asm with an `x` output before reading it with `movfi`, and
  switches such functions to `setwd nfx = 0` [lcc]. The `movx` after the
  output turned out unnecessary on the 4C and under MCST's qemu
  (`docs/spec-poc-findings.md` A12); why lcc emits it is not known.
  glibc's `bits/mathinline.h`, the only user of `x` in the sysroot, is
  included only under `__LCC__`.
- lcc accepts `_Pragma("asm_inline")` before an `asm` (it allows lcc to
  schedule the asm with surrounding code, `-masm-inline`), and checks the
  asm's text unless `-mno-asm-check`. Other compilers ignore the pragma.

Register variables (`register T v asm("...")`, lcc 1.29+): a name without
a prefix is ordinary registers, so a 16-byte variable at `%g2` occupies
`%g2`+`%g3`; the prefix `x` names one extended register (`%xg5`, odd
numbers allowed). By default (`-mold-register-asm-long-double`) a `long
double` named without `x` is still placed in the extended register, with
a warning.

Register names for `-ffixed-<reg>` and `-fcall-used-<reg>` (as in GCC):
`g0`..`g31`, `r0`..`r63`, `pred0`..`pred31`.

## Options and behaviours that change generated code (lcc-doc)

- **Alignment**: `-mstrict-align` (no unaligned accesses created by
  optimisation) is the default up to v4, which has no efficient unaligned
  access; from v5 the default is `-mno-strict-align`.
- **Data model** (`-mdmodel=`): `medium` (default) assumes no object,
  static or allocated, exceeds 4 GiB, so 32-bit index arithmetic is
  allowed; `large` allows huge dynamic objects; `huge` (experimental)
  allows both. It changes code, not the binary interface.
- **Rotating globals**: lcc assumes by default that the rotation base of
  `%g24`–`%g31` may change (`abg`, `abgc`, `modbgv`), and limits
  optimisation of those registers; `-mfixed-gregs-base` (1.31) lifts
  this.
- **Uninitialised variables**: from 1.29, at `-O1` and above lcc
  initialises some variables that may be read uninitialised
  (`-ftrivial-auto-var-init=uninitialized`, its default). This avoids
  "very rare crashes on Elbrus caused by diagnostic tags in registers"
  (see the tag trap in [calling-convention.md](calling-convention.md)).
  A compiler that leaves them alone can store a tagged register.
- **Signals**: a fault in a speculative operation only tags its result;
  the program gets `SIGILL` when a non-speculative operation (a store, a
  branch) uses the value, instead of `SIGSEGV`, `SIGBUS` or `SIGFPE`.
  lcc keeps the usual signals at `-O0`/`-O1` and with `-fno-semi-spec-ld`
  and `-fno-spec-fp` (older names: `-fcontrol-spec`, `-fno-fp-spec`). A
  port that speculates must expect testsuites that check those signals
  to fail.
- **Speculation switches** (1.26): `-f[no-]semi-spec-ld` (loads moved
  from under a predicate, semi-speculative), with `-glob`, `-stack` and
  `-greg` variants for loads of globals, the stack and through global
  registers; `-f[no-]spec-ld` (speculative with compensation code);
  `-f[no-]spec-fp`; `-f[no-]spec-ctp` (prepares hoisted out of
  conditions); `-f[no-]spec-volatile-st` (1.30, speculative stores
  through `volatile` pointers). `-fkernel` disables
  semi-speculative loads except those of globals, the stack and through
  global registers.
- **`swapcontext`** is treated like `setjmp` (returns twice) since 1.28;
  without that it "could cause runtime errors". GCC marks only `setjmp`,
  `sigsetjmp`, `savectx`, `vfork` and `getcontext` (`gcc/calls.cc`,
  `special_function_p`).
- **`__attribute__((cleanup))`**: lcc runs the cleanup only on a normal
  exit from the scope, not while a C++ exception unwinds through a C
  function (GCC runs it in both cases).
- **`__attribute__((no_caller_stack))`** (lcc only): for functions whose
  arguments and result all fit in registers; the caller may omit the
  argument area the convention otherwise reserves, so the callee must not
  use it.
- **v7**: `-mcheck-nv` makes v7 hardware trap when an operand that must
  be numeric carries another tag (default only in protected mode);
  `-mset-prepare-ct-delays-e48c` applies the 48C's prepare-to-transfer
  delays (default with `-mtune=elbrus-48c`), so those delays differ on
  that CPU.

## GNU extensions lcc lacks (lcc)

lcc 1.29.16 rejects GNU nested functions (and so nonlocal `goto` out of
them; `error #65: expected a ";"`), `__builtin_setjmp` /
`__builtin_longjmp` and `__builtin_apply` ("unknown built-in function").
No E2K code depends on them and there is no ABI to match, so a new
compiler may leave them out at first. Implementing them needs
trampolines (an executable stack, `%g12` loaded before a jump through a
CTPR) and, for nonlocal `goto` and `__builtin_longjmp`, unwinding the
chain stack, which glibc's `longjmp` does through the kernel; a
`jmp_buf` whose saved IP is replaced by a label's address re-enters the
frame there ([stack-and-unwinding.md](stack-and-unwinding.md)).

**Labels as values** are different in lcc: `&&label` is the label's
index among the address-taken labels of its function (0, 1, 2, ...),
not a code address, and `goto *p` dispatches on that index (probed
with lcc 1.29.16 at `-O0` and `-O2`: a table `{ &&a, &&b, &&c }` holds
0, 1, 2 and computed goto through it works). Code that keeps label
values only for `goto *` in the same function works with either
compiler; code that prints them, compares them across functions or
uses them as code addresses (GCC's documented uses for profiling or
`__builtin_return_address` comparisons) gets 0, 1, 2 from lcc.

## Headers (source)

The e2k `bits/fenv.h` in lcc's sysroot (and in `OpenE2K/glibc`,
`sysdeps/e2k/fpu/bits/fenv.h`) defines `femode_t` only under
`__GLIBC_USE (IEC_60559_BFP_EXT)`, while `fenv.h` declares `fegetmode`
under `__GLIBC_USE (IEC_60559_BFP_EXT_C2X)`, as x86's copy does for
both. So `#include <fenv.h>` fails under `-std=c23` / `gnu23` (GCC's
default) without `_GNU_SOURCE`: "unknown type name 'femode_t'". A GCC
port needs `-std=gnu17` or `-D_GNU_SOURCE` for its test runs and to
build libquadmath, or a fixincludes rule.
