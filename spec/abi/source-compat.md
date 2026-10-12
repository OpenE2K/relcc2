# Source-level conventions: macros, inline assembly, options

What a compiler must provide, beyond the binary interface, so that code
and headers written for lcc build unchanged: predefined macros,
inline-assembly constraints and register names, and the lcc options and
behaviours that change generated code. Probes: `tools/abi/probes/asmops.c`,
`tools/abi/probes/execstack.sh` (nested functions, Fortran internal
procedures); macros from `lcc -dM -E -xc /dev/null`.

## Predefined macros (lcc, unipro, lcc-doc)

| Macro | lcc | Unipro | Used by | For a new compiler |
|---|---|---|---|---|
| `__e2k__` | 1 | 1 | glibc (177 files), Linux, gdb | define |
| `__iset__` | ISA version: 3..7 | same | glibc (142 files: ISA-specific code) | define, from `-march` |
| x86 feature macros: `__MMX__`, `__SSE__` … `__SSE4_2__`, `__SSSE3__`, `__AVX__`, `__AVX2__`, `__FMA__`, `__AES__`, `__SHA__`, `__BMI2__`, `__POPCNT__` and more (lcc-builtins.md) | 1 on every target, no option needed | ? | programs choosing an x86 SIMD path; lcc's `<immintrin.h>` emulates the intrinsics | don't define (owner's decision: no x86 intrinsics in the GCC port; lcc-builtins.md) |
| `__elbrus_4c__`, `__elbrus_8c__`, `__elbrus_1cplus__`, `__elbrus_8c2__`, `__elbrus_12c__`, `__elbrus_16c__`, `__elbrus_2c3__`, `__elbrus_48c__`, `__elbrus_8v7__` | with `-mtune=<cpu>` (or a CPU-specific lcc such as `e2k-v3.4c`) | no | glibc (14 files) | define with `-mtune` |
| `__ptr64__` (`__ptr32__`, `__ptr128__` in the other modes; `__PROTECTED__` is a deprecated alias of `__ptr128__`: [protected-mode.md](protected-mode.md)) | 1 | `__ptr64__` | glibc (`__ptr128__`: 137 files), Linux | define |
| `_LP64`, `__LP64__` | 1 | 1 | everything | define |
| `__LCC__` (version: 129, 131), `__LCC_MINOR__`, `__MCST__` | yes | no | glibc's installed headers: see below | **do not define** |
| `_REENTRANT` 1 with `-pthread` (the only macro `-pthread` adds) | yes | ? | configure scripts: autoconf's `AX_PTHREAD` rejects `-pthread` without it (xz) | define with `-pthread` (`CPP_SPEC "%{pthread:-D_REENTRANT}"`, as GCC's aarch64 and riscv Linux targets) |
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

lcc's builtins (`builtin_mcst.html`; [lcc-builtins.md](lcc-builtins.md)
has what each compiles to): `__builtin_cpu_init ()`,
`__builtin_cpu_is ("elbrus-8c2")`, `__builtin_cpu_name ()`,
`__builtin_cpu_arch ()` (deprecated) and `__builtin_e2k_get_cpu_iset ()`
(the ISA version, 0 if unknown; the manual misspells it
`__builtin_e2k_cpu_iset`, which lcc doesn't know). `__builtin_cpu_supports`
is not supported ("unknown built-in function").

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
  %1, %0` in mathinline.h); mathinline.h uses no operand modifiers.
  lcc's modifiers are listed below.
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

### Other constraints (lcc)

lcc 1.29.16 (`e2k-v3`, `e2k-v5.8c2`, `e2k-v6.2c3`) and 1.31.05 (`e2k-v3`)
accept the same letters and print operands the same way
(`tools/abi/probes/asmconv.py` prints every case below; its output is
identical for the four compilers):

| Constraint | lcc | Operand prints as |
|---|---|---|
| `m`, `o` | accepted, also as outputs (`=m`, `+m`) | `%rN, 0x0`: lcc always loads the address into a register and prints base and a zero offset, also for `p->b`, `p[50]`, a global or a local array element. The text fits the address operands of a load or store (`ldd %1, %0`) |
| `i`, `n` | any integer constant | hex, **unsigned at the width of the operand's C type**: `(int) -1` gives `0xffffffff`, `-1L` gives `0xffffffffffffffff`, `(short) -1` gives `0xffff`. A `double` constant prints its bits (`1.5` gives `0x3ff8000000000000`). With `i`, the address of a global prints as gas's literal syntax `[ g ]` (non-PIC; an error with `-fPIC`); the address of a local is an error |
| `I` | 0–31 | as `i` (the range of the 5-bit `src1` literal) |
| `J` | 0–255 | as `i` (the range of an 8-bit ALES literal, `pshufh` and ALOPF11_LIT8; inferred) |
| `g` | accepted | a register in every probe (`%r3`), also for a memory operand |
| `rm`, `mr`, `ri`, `im` | accepted | a constant prints as for `i` |
| `0`–`9` (matching) | accepted | the matched register |
| modifiers `=`, `+`, `&` | accepted | |
| `V`, `<`, `>`, `s`, `X`, `p`, `E`, `F`, `f` | error "asm operand %0 of asm statement have unknown constraint" | |
| every other letter (`K`–`P`, `G`, `H`, `a`–`e`, `h`, `j`–`l`, `q`, `t`–`w`, `y`, `z`, `A`–`D`, `Q`–`U`, `W`, `Y`, `Z`) | error #1121 "unknown asm constraint letter" | |

The unsigned printing matters with 64-bit operations: `(int) -1` passed
with `i` to `addd` reaches gas as `0xffffffff`, which gas encodes as
`_f64 0xffffffff`, that is +4294967295, not −1 ([bundle.md](../isa/bundle.md),
"Literal values"). The installed headers cast such operands to an
unsigned 64-bit type first (`__e2k_syscall_arg_t`).

Constraints in use: glibc's E2K sysdeps use `r` (137), `=r` (111), `+r`
(58), `ri` (21), `=&r` (2), `i` (1) and `g` (1, protected-mode malloc);
the kernel's uapi headers use `ri` (51), `=r` (13) and `i` (8) [source].

### Operand modifiers (lcc)

| Modifier | lcc prints |
|---|---|
| `%#` | the `wbs` of a call made inside the asm (next section) |
| `%=` | a number unique to this asm in the output (`1`) |
| `%%` | `%` |
| `%[name]` | the named operand |
| `%cN` | a constant without decoration: `5` gives `0x5`, `&g` gives `g`. On a register: error "operand modifier 'c' should only be used for constants" |
| `%nN` | the negated constant: `5` gives `0xfffffffb` (`int` width), `-5L` gives `0x5` |
| `%HN` | the second register of a 16-byte `r` operand: an `__int128` in `%r4`+`%r5` prints `%r4` for `%0` and `%r5` for `%H0`. On an 8-byte operand or an `x` operand: error "operand modifier 'H' should only be used with quadro" |
| `%a`, `%l`, `%P`, `%p`, `%w`, `%d`, `%x`, `%X`, `%h`, `%b`, `%q`, `%s`, `%z`, `%k`, `%V`, `%K` | error "unknown operand modifier" |

- In a basic asm (no colons) `%` is not special: `asm volatile ("%#")`
  prints `%#` as written.
- `asm goto` is rejected by lcc 1.29.16 and 1.31.05 ("конструкция asm goto
  пока не поддержана", not supported yet).

### `%#`: the window base for a call inside the asm (lcc)

lcc replaces `%#` with the pair at which `%b[0]` currently starts, `rbs +
rcur` of the function's `setbn`, in hex. lcc's own calls in the same
function use the same `wbs`, and lcc sets `wsz` to `%#` + 4, so `call
%ctpr1, wbs = %#` hands the callee the 4 pairs (`%b[0]`–`%b[7]`) that the
C convention and the kernel's system call entry expect
([registers.md](registers.md), window units; the entries start with
`VFRPSZ_SETWD(4)`, kernel `kernel/trap_table.S:535-555`;
[kernel-interface.md](kernel-interface.md)). `asmconv.py`:

| Function (one `asm volatile`) | lcc `-O` | `setwd` | `setbn` | `%#` |
|---|---|---|---|---|
| leaf, any asm text | `-O0` | `wsz = 0x8` | `rsz = 0x3, rbs = 0x4, rcur = 0x0` | `0x4` |
| leaf, any asm text | `-O1` … `-O3` | `wsz = 0x9` | `rsz = 0x4, rbs = 0x4, rcur = 0x1` | `0x5` |
| asm and a call with 12 arguments | `-O2` | `wsz = 0x9` | `rsz = 0x4, rbs = 0x4, rcur = 0x1` | `0x5` (the call: `wbs = 0x5`) |
| 8 arguments live across the asm | `-O2` (`-O0`: `wsz = 0xc`, `rbs = 0x8`, `%#` 0x8) | `wsz = 0xb` | `rsz = 0x4, rbs = 0x6, rcur = 0x1` | `0x7` |
| the same leaf without the asm | any | `wsz = 0x4` | none | — |

So `wsz − %#` is 4 in every case. The asm may write `%b[0]`–`%b[7]` and
read results from `%b[0]`; lcc reserves no stack argument area for it (no
`getsp`).

When lcc sets up that area: for every `asm volatile`, with or without
`%#` and whatever its clobbers (also `asm volatile ("")`), and for a
non-volatile asm with a `"call"` or `"b[N]"` clobber. A non-volatile asm
without them gets no rotating base (`-O2`: `wsz = 0x8` and no `setbn`;
`-O0`: `wsz = 0x4`), yet `%#` still prints `0x4`, so a call made there
would not pass `%b[]` to the callee. Every `%#` in glibc and the kernel
is in an `asm volatile` or in an asm that lists `"call"` (the kernel's
four non-volatile ones, `asm/e2k_api.h:3094-3206`). With `_Pragma
("asm_inline")` lcc parses the asm itself and emits the same `wbs`.

Users: glibc's `INTERNAL_SYSCALL` (`sysdeps/unix/sysv/linux/e2k/sysdep.h:229`,
`call %ctpr1, wbs = %#`), the uapi `__E2K_SYSCALL_0` … `_7` macros
(`arch/e2k/include/uapi/asm/e2k_api.h:128-331`) and 25 uses in the
kernel's own `asm/e2k_api.h` (also `hcall …, wbs = %#`). (GCC hands `%`
followed by a punctuation character to the target's operand printer when
`TARGET_PRINT_OPERAND_PUNCT_VALID_P` accepts it, `gcc/final.cc:3574`.)

### Clobber names (lcc)

The same in lcc 1.29.16 (`e2k-v3`, `e2k-v5.8c2`, `e2k-v6.2c3`, `-O0`
and `-O2`) and 1.31.05 (`e2k-v3`) (`asmconv.py`):

| Name | lcc | Effect on a non-volatile asm (`-O2`) | glibc | kernel uapi |
|---|---|---|---|---|
| `"memory"` | accepted | memory values loaded before it are loaded again after it | the spinlocks (`nptl/pthread_spin_*.c`) and every syscall | — |
| `"cc"` | accepted | none seen | — | — |
| `"call"` | accepted | the asm is treated as a call: outgoing area reserved (`wsz = 0x9`), `return %ctpr3` prepared again after it, memory reloaded, operands in window registers instead of `%g16`–`%g31` | `"call", "memory"` on every syscall (`sysdep.h:172-173`) | — |
| `"ctpr1"`–`"ctpr3"`, `"%ctpr1"`–`"%ctpr3"` | accepted; `ctpr0`, `ctpr4` unknown | `"ctpr3"` on a non-volatile asm: `return %ctpr3` prepared after it | `"%ctpr1"`–`"%ctpr3"` (`nptl/pthread_spin_lock.c:40-44`) | `"ctpr1"`–`"ctpr3"` (`E2K_SYSCALL_CLOBBERS`, `e2k_api.h:108-111`) |
| `"b[0]"`–`"b[7]"`, `"%b[0]"`–`"%b[7]"` | accepted; `"b[8]"` and above: error `регистр "%b[8]" нельзя использовать в качетсве clobber'а` (cannot be a clobber; lcc's spelling); `"db[0]"`, `"b0"` unknown | as `"call"`, and `setwd nfx = 0x0` | — | `"b[0]"`–`"b[7]"` (`E2K_SYSCALL_CLOBBERS`) |
| `"pred0"`–`"pred31"`, `"%pred0"`–`"%pred31"` | accepted; `pred32`, `p0` unknown | none seen (lcc kept no predicate live across an asm in any probe) | `"%pred0"` (spinlock) | — |
| `"g0"`–`"g31"`, `"%g0"`–`"%g31"` | accepted; `dg16`, `xg16`, `qg16` unknown | without clobbers lcc may give a non-volatile asm's operands `%g16` and up (leaf functions use them as scratch); with `"g16"` it used `%g17` and reloaded a value it had kept in `%g16`; all of `g16`–`g31`: window registers | — | — |
| `"r0"`–`"r63"`, `"%r0"`–`"%r63"` | accepted; `r64` and above, `dr0`, `xr0`, `qr0` unknown; at `-O0` a clobbered `rN` that holds an operand is an error ("clobber %r0 conflicts with operand %1") | `setwd nfx = 0x0` | — | — |
| state, AAU and special registers (`lsr`, `ilcr`, `pfpfr`, `fpcr`, `fpsr`, `upsr`, `usd`, `wd`, `br`, `psr`, `cr0`, `aasr`, `aad0`, `aasti0`, `empty`, `MLOCK`, `clkr`, `rndpr`, `tst`, `tc`), with or without `%` | error #1118 "unknown register name" | | | |

The kernel's own headers (not uapi) also use `"r0"`–`"r7"`,
`"pred0"`–`"pred31"`, `"%g0"`–`"%g31"`, `"g16"`–`"g31"` and `"%b[0]"`–`"%b[7]"`,
and note that `"lsr"` cannot be named, so they clobber the CTPRs instead
(`asm/e2k_api.h:2786`). Their four non-volatile calls clobber `"call"`,
`"memory"`, `"pred19"` and the global they use (`"g" #greg_no`,
`asm/e2k_api.h:3100-3215`).

GCC accepts a clobber only if it is a register name, an
`ADDITIONAL_REGISTER_NAMES` or `OVERLAPPING_REGISTER_NAMES` alias,
`"memory"`, `"cc"` or `"redzone"`, after stripping one leading `%`;
anything else is "unknown register name" (`gcc/varasm.cc:978-1095`,
`gcc/cfgexpand.cc:3496-3502`). So `"call"` and `"b[N]"` need to be names
the port knows. Both should also clobber `%g16`–`%g31`, as a call does
(lcc treats both as a call, above): a system call through entry 3
returns with those registers emptied, and neither glibc's nor the
uapi's clobber list names them ([kernel-interface.md](kernel-interface.md),
"System calls").

### What lcc assumes an asm does (lcc)

| Asm | lcc |
|---|---|
| `asm volatile`, any clobbers | a barrier for everything: reserves the outgoing area, prepares `return %ctpr3` and loop `disp`s again after it (also with no clobbers: CTPRs are never kept across it), and reloads memory as if `"memory"` were given |
| non-volatile | follows the clobber list: keeps `return %ctpr3` prepared across it unless `"ctpr3"`, `"call"` or a `"b[N]"` is listed; may put operands in `%g16`–`%g31` unless `"call"` or that `gN` is listed; reuses a value loaded before it unless `"memory"`, `"call"` or a `"b[N]"` is listed (`asmconv.py`, "Effects") |

The uapi system call macros list neither `"memory"` nor `"call"`, so they
rely on this (inferred). GCC does not treat `asm volatile` as a memory clobber and may
move it relative to other code (`gcc/doc/extend.texi:11797-11801`,
`12423-12434`), so with GCC, values loaded before such a call are
reused after a system call that wrote them. GCC's tree optimizers see
an asm write memory other than its own `m` outputs only when its
clobbers include the literal `"memory"` or it is a non-empty basic asm
(`gimple_asm_clobbers_memory_p`, `gcc/gimple.cc:3058-3074`, which
`gcc/tree-ssa-operands.cc:753-755` asks), volatile or not, and no target
hook changes that: a memory clobber added in `TARGET_MD_ASM_ADJUST`
reaches only RTL [source]. Host GCC 16.2.1 `-O2` (x86-64) reads
`buf[0]` once across `asm volatile ("..." : : "r" (buf))`, after the
asm, and before and after it with `"memory"`. lcc reads it again after a
uapi-style `read` (`tools/abi/probes/pocplan.c` `sys_read_twice`: `ldb`
before and after the asm) [lcc 1.29.16 `-O2` and `-O3`, v3.4c, v5.8c2,
v6.2c3; 1.31.05 `-O3`, v3, v6.2c3].

lcc also has `#pragma asm_inline`, `#pragma no_asm_inline` and
`#pragma asm_length (N)` (the asm's length in cycles, 0–14, E2K only)
(**lcc-doc** `pragma_mcst.html`). glibc uses the first two
(`sysdeps/e2k/dl-machine.h:89-95`, `sysdeps/e2k/fpu/e_coshf.c:44`) and
the kernel all three (`asm/compiler.h:43-67`); MCST's libgcc
`__e2k_clear_cache` uses `no_asm_inline` (`config/e2k/clear-cache.c`). GCC
ignores unknown pragmas (a warning with `-Wunknown-pragmas`).

Register names for `-ffixed-<reg>` and `-fcall-used-<reg>` (as in GCC):
`g0`..`g31`, `r0`..`r63`, `pred0`..`pred31`.

## Options and behaviours that change generated code (lcc-doc)

- **Alignment**: `-mstrict-align` (no unaligned accesses created by
  optimisation) is the default up to v4, which has no efficient unaligned
  access; from v5 the default is `-mno-strict-align`.
- **Data model** (`-mdmodel=`): `medium` (default) assumes no object,
  static or allocated, exceeds 4 GiB, so lcc may compute indices in 32
  bits; `large` allows huge dynamic objects; `huge` (experimental)
  allows both. It changes code, not the binary interface. It does not
  let lcc assume an unsigned 32-bit index doesn't wrap: `p[i + off]`
  with `off = 0xffffffff` reads the wrapped element at `-O1` and `-O3`,
  lcc computing the index with a 32-bit `adds` (zero-extended by
  `dbl = 1`) every iteration [lcc; the proof-of-concept back end's P32,
  on the 4C].
- **Rotating globals**: lcc assumes by default that the rotation base of
  `%g24`–`%g31` may change (`abg`, `abgc`, `modbgv`), and limits
  optimisation of those registers; `-mfixed-gregs-base` (1.31) lifts
  this.
- **Uninitialised variables**: from 1.29, at `-O1` and above lcc
  initialises some variables that may be read uninitialised
  (`-ftrivial-auto-var-init=uninitialized`, its default). This avoids
  "very rare crashes on Elbrus caused by diagnostic tags in registers"
  (see the tag trap in [calling-convention.md](calling-convention.md)).
  A compiler that leaves them alone can store a register holding a
  diagnostic value (an empty one, as in registers a window gains, does
  not trap).
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
- **`setcontext`** is treated as a function that never returns: lcc
  1.29.16 at `-O1` to `-O4` and `-Os`, and 1.31.05 at `-O1` and `-O3`.
  `-fno-builtin`, `-fno-builtin-setcontext` and `-ffreestanding` don't
  change it, and glibc's declaration says only `__THROWNL`. lcc emits
  nothing after the call except the `ct %ctpr3` it prepared before it. A
  call empties `%ctpr1`–`%ctpr3`, so when `setcontext` fails (`ESRCH`
  for an unknown key) and returns, that `ct` traps and the process dies
  with SIGILL [lcc, hw: `tools/abi/probes/ctxsig.c` section 1 on the 4C,
  8C2 and 2C3]. At `-O0`, or when the function is called through another
  name with `__asm__ ("setcontext")`, lcc keeps the code after the call.
  A compiler that wants lcc's behaviour can mark `setcontext` noreturn
  by name; one that doesn't is correct in both cases.
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

## GNU extensions lcc lacks (lcc, lcc-doc, unipro)

lcc 1.29.16 and 1.31.05 (v3, v5.8c2, v6.2c3, `-O2`) reject GNU nested
functions in every form: one that is only called, one whose address
escapes and that uses its parent's variable, and one whose address
escapes but uses nothing from its parent. All three stop at the nested
definition with `error #65: expected a ";"`
(`tools/abi/probes/execstack/nested-*.c`, compiled by
`tools/abi/probes/execstack.sh`). `-fnested-functions`, `-ftrampolines`
and `-ftrampoline-impl=heap` are "unrecognized command line option"s,
and the manual lists `-Wtrampolines` as ignored (lcc-doc,
`lcc_options.htm`). So there is no nonlocal `goto` out of a nested
function either. Unipro's clang rejects nested functions too ("function
definition is not allowed here"). `__builtin_setjmp`,
`__builtin_longjmp` and `__builtin_apply` are "unknown built-in
function"s (lcc 1.29.16 and 1.31.05). No E2K code depends on any of
them and there is no ABI to match, so a new compiler may leave them out
at first. A nested function whose address escapes needs a trampoline
([gcc-port-plan.md](../../doc/gcc-port-plan.md), D1, "Trampolines"),
and a trampoline on the stack needs an `"x"` GNU-stack note
([elf.md](elf.md), "Executable stack"). Nonlocal `goto` and
`__builtin_longjmp` also need the chain stack unwound, which glibc's
`longjmp` does through the kernel. A `jmp_buf` whose saved IP is
replaced by a label's address re-enters the frame there
([stack-and-unwinding.md](stack-and-unwinding.md)).

lcc's output never needs an executable stack (inferred from these
probes and from lcc never writing the note). Its Fortran (`lfortran`,
1.29.16 and 1.31.05) passes an internal procedure as an actual argument
(Fortran 2008) without a trampoline. It passes a static thunk
(`__outer__add__thunk_1`), which reaches the host's variables through a
`.bss` slot (`__outer__add__ext_1`); before the call the host stores
there the address of the variable (`total`). The slot stays static with
`-frecursive` and `-fopenmp`, so two threads in the host share it
(inferred). By default lfortran rejects such an argument in a
`recursive` host ("Internal procedure 'add' is used as actual argument
at (1) in recursive procedure 'outer'"). That is MCST's check
`-ffortran-check-internal-func` (lcc-doc; that it is on by default is
inferred from this error). `-fno-fortran-check-internal-func` accepts
the host, and the code still uses the one static slot, so a recursive
activation would overwrite its caller's (inferred)
(`tools/abi/probes/execstack/internal.f90`, `internal-rec.f90`). GCC's
gfortran has no such error, only an F2008 notice
(`gcc/fortran/resolve.cc:2142-2149`). It lowers internal procedures to
nested functions, so passing one needs a trampoline (inferred).

**Labels as values** are different in lcc: `&&label` is the label's
index among the address-taken labels of its function (0, 1, 2, ...),
not a code address, and `goto *p` dispatches on that index (probed
with lcc 1.29.16 at `-O0` and `-O2`: a table `{ &&a, &&b, &&c }` holds
0, 1, 2 and computed goto through it works). Code that keeps label
values only for `goto *` in the same function works with either
compiler; code that prints them, compares them across functions or
uses them as code addresses (GCC's documented uses for profiling or
`__builtin_return_address` comparisons) gets 0, 1, 2 from lcc.
At `-O3` every `goto *p` of a function branches to one shared block:
a range check of the index (`cmpbedb` against the highest index, an
`ibranch` out when it fails), `sxt` and `shld`, a load of the code
address from a second, compiler-made table (`.T.N`), and `movtd,0,sm`
into `%ctpr1` with the `ct` 9 cycles later. Lua 5.4.7's `vmfetch`
handlers in `luaV_execute` each load the index (from `disptab`, some
under a predicate) and branch to that block: about 28 cycles on v6
from loading the instruction word to the transfer (lcc 1.29.16 `-O3`,
v3 and v6.2c3; the proof-of-concept back end's P72) [lcc]. A GCC port
that keeps `&&label` a code address saves the index load, the range
check and the second table.

## Headers (source)

The e2k `bits/fenv.h` in lcc's sysroot (and in `OpenE2K/glibc`,
`sysdeps/e2k/fpu/bits/fenv.h`) defines `femode_t` only under
`__GLIBC_USE (IEC_60559_BFP_EXT)`, while `fenv.h` declares `fegetmode`
under `__GLIBC_USE (IEC_60559_BFP_EXT_C2X)`, as x86's copy does for
both. So `#include <fenv.h>` fails under `-std=c23` / `gnu23` (GCC's
default) without `_GNU_SOURCE`: "unknown type name 'femode_t'". A GCC
port needs `-std=gnu17` or `-D_GNU_SOURCE` for its test runs and to
build libquadmath, or a fixincludes rule.
