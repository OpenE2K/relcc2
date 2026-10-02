# ELF, relocations, PIC and TLS

How e2k64 objects, executables and shared libraries are laid out, how the
assembler and linker relocate them, and the code a compiler emits to
address data, call functions and reach thread-local storage in each code
model. Marks as in [README.md](README.md); **binutils** is MCST's
`OpenE2K/binutils-gdb` (`include/elf/e2k.h`, `bfd/elfxx-e2k.c`,
`gas/config/tc-e2k.c`).

Run on hardware (Elbrus 4C, v3) by `tools/abi/probes/dso/run.sh`: a
shared library with global and `static` TLS variables, used from a PIE, a
non-PIE executable (with a copy relocation, an IE access and function
pointer equality) and a fully static executable, with lazy and immediate
binding (`LD_BIND_NOW=1`), all built by lcc 1.29.16 for `e2k-v3`.
Unipro's PIC output can't be assembled with gas (see "Unipro" below), so
only lcc's sequences are run.

## ELF header (binutils, kernel)

| Field | Value |
|---|---|
| `EI_CLASS` | `ELFCLASS64` |
| `EI_DATA` | `ELFDATA2LSB` |
| `e_machine` | `EM_MCST_ELBRUS` = 175 |
| `e_flags` bits 24–31 | Machine: the ISA version or a CPU model (below) |
| `e_flags` & `0x10` | `EF_E2K_INCOMPAT`: code runs only on that machine, not on later ones |
| `e_flags` & `0x20` | `EF_E2K_PM`: protected mode (not this document) |
| `e_flags` & `0x40` | `EF_E2K_PACK_SEGMENTS` (protected mode) |
| `e_flags` & `0x3` | `EF_E2K_IPD`: instruction prefetch depth; 0 is the default (readelf: "default ipd") |
| `e_flags` & `0x4`, `0x8` | `EF_E2K_X86APP`, `EF_E2K_4MB_PAGES` (binary compiler, kernel; not used by a compiler) |

Machine codes: 2–7 = `elbrus-v2`…`elbrus-v7` (`-march`); 19 = 8C,
20 = 1C+, 21 = 12C, 22 = 16C, 23 = 2C3, 24 = 48C, 25 = 8V7, 26 = maket32c
(`-mtune` models). 0 and 1 are legacy and rejected by the linker. (An
older encoding put a 3-bit machine in bits 28–30 with
`e_machine` = `EM_E2K_OLD`; binutils still reads it.)

What each lcc target writes (lcc):

| lcc directory | `e_flags` | readelf |
|---|---|---|
| `e2k-v3` | `0x03000000` | elbrus-v3 |
| `e2k-v3.4c` | `0x03000010` | elbrus-4c |
| `e2k-v4.8c` | `0x13000010` | elbrus-8c |
| `e2k-v4.1c+` | `0x14000010` | elbrus-1c+ |
| `e2k-v5.8c2` | `0x05000010` | elbrus-8c2 |
| `e2k-v6.16c` | `0x16000010` | elbrus-16c |
| `e2k-v6.2c3` | `0x17000010` | elbrus-2c3 |

So `-march=elbrus-vN` code is forward compatible (no `INCOMPAT`), and code
tuned for one CPU is marked `INCOMPAT` with either its ISA version or its
model code.

The **kernel** (`arch/e2k/include/asm/elf.h`) execs a 64-bit ELF only if
`EF_E2K_PM` is clear and the machine fits the CPU: versions 2–5 run on that
version and, without `INCOMPAT`, on later ones; versions 6 and 7 on that
version or later; model codes only on that model. The **linker** gives
every input the set of CPUs it runs on, intersects the sets, and fails if
nothing is left; the output gets the most general machine that matches,
with `INCOMPAT` if any input had it. Shared libraries don't take part.

## Sections and segments (binutils, glibc, lcc)

- Ordinary ELF sections: `.text`, `.data`, `.bss`, `.rodata`, `.tdata`,
  `.tbss`, `.init`/`.fini`, `.init_array`/`.fini_array`, `.eh_frame`,
  `.got`, `.got.plt`, `.plt`, `.rela.dyn`, `.rela.plt`. Only `RELA`
  relocations are used.
- The interpreter is `/lib64/ld-linux.so.2`.
- `PT_E2K_TAG` = `PT_LOPROC` is defined (protected mode); lcc's 64-bit
  output has no such segment. lcc doesn't emit `PT_GNU_STACK`.
- `_GLOBAL_OFFSET_TABLE_` is the start of `.got.plt`, and `DT_PLTGOT`
  points there. Its first three words are reserved: `[0]` = link-time
  address of `_DYNAMIC`, `[1]` = the module's `struct link_map *`,
  `[2]` = address of `_dl_fixup`; ld.so fills in 1 and 2 (lazy binding,
  below). The default linker script puts `.got.plt`, `.igot.plt`, `.got`
  and `.igot`, in that order, into one output section `.got` after the end
  of RELRO, so ordinary GOT entries have positive offsets from
  `_GLOBAL_OFFSET_TABLE_` and the whole table stays writable.

Dynamic tags (binutils, glibc):

| Tag | Value | Meaning |
|---|---|---|
| `DT_E2K_LAZY_DEFECTIVE` | `DT_LOPROC` | Old lazy-binding scheme that patched PLT code; not produced |
| `DT_E2K_LAZY` | `DT_LOPROC + 1` | Lazy binding that patches a literal in each PLT entry; not produced |
| `DT_E2K_LAZY_GOT` | `DT_LOPROC + 3` | Lazy binding through `.got.plt` (current; below). ld.so binds a module lazily only if one of these tags is present |
| `DT_PLTGOTSZ` | `DT_LOPROC + 0x101b` | Size of `.got` |
| `DT_INIT_GOT`, `DT_EXPORT_PL`, `DT_EXPORT_PLSZ`, `DT_REAL_PLTGOT`, `DT_E2K_NO_SELFINIT` | following values | Protected mode |

## Literal fields (binutils, gas; lcc)

Relocations in code patch the literal syllables (LTS) of a bundle, or the
`disp` field of CS0 (see [bundle.md](../isa/bundle.md)):

- A 32-bit literal (`_f32s`) is one LTS.
- A 64-bit literal (`_f64`) occupies two adjacent LTS, **high half at the
  lower address**. Relocations on 64-bit literals (the `*_LIT` types and
  `R_E2K_64_TLS_LE`) therefore store the 64-bit value with its 32-bit
  halves swapped; ld.so does the same for the dynamic `R_E2K_64_ABS_LIT`
  and `R_E2K_64_RELATIVE_LIT`. The 64-bit data relocations
  (`R_E2K_64_ABS`, `R_E2K_64_PC`, `R_E2K_64_GOTOFF`) store it unswapped.
- PC-relative fields count from the **start of the bundle** that holds
  them, not from the field. gas implements this by adding the field's
  offset inside the bundle to the addend: a literal at bundle offset
  `0x10` carries addend `0x10`, and the `disp` field (CS0, offset 8 in that
  bundle) carries addend 8. So `rrd %ip` in the same bundle plus the
  literal gives the target address (checked in lcc objects with objdump).

## Relocation types (binutils)

S = symbol value, A = addend, P = address of the field, B = start of the
bundle containing P (so with gas's addend, S + A − P = S − B),
GOT = `_GLOBAL_OFFSET_TABLE_`, G = address of the symbol's GOT entry,
TP0 = start of the module's TLS block minus the TCB (below),
DTP0 = start of the module's TLS segment.

| # | Name | Field | Value | Used for |
|---|---|---|---|---|
| 0 | `R_E2K_32_ABS` | 32 | S + A | data |
| 2 | `R_E2K_32_PC` | 32 | S + A − P | data (`.eh_frame` FDE addresses) |
| 50 | `R_E2K_64_ABS` | 64 | S + A | data; dynamic: GOT entries, absolute data |
| 51 | `R_E2K_64_ABS_LIT` | 64 literal | S + A | absolute address in code (non-PIC) |
| 54 | `R_E2K_64_PC_LIT` | 64 literal | S + A − P | `_GLOBAL_OFFSET_TABLE_` in PIC code |
| 258 | `R_E2K_64_PC` | 64 | S + A − P | data (`.eh_frame` personality, LSDA, `.gcc_except_table`) |
| 69 | `R_E2K_64_GOTOFF` | 64 | S + A − GOT | data |
| 256 | `R_E2K_64_GOTOFF_LIT` | 64 literal | S + A − GOT | address of a local symbol in PIC code |
| 109 | `R_E2K_GOTOFF` | 32 | S + A − GOT (signed) | 32-bit GOT offsets |
| 108 | `R_E2K_GOT` | 32 literal | G − GOT | load of a GOT entry |
| 114 | `R_E2K_GOTPLT` | 32 literal | as `R_E2K_GOT`, or the `.got.plt` slot | `@GOTPLT` |
| 110 | `R_E2K_DISP` | CS0 `disp`, 28 bits | (S + A − P) >> 3; to the PLT entry if the symbol has one | `disp %ctprN, sym` |
| 111 | `R_E2K_PREF` | CS0 | prefetch hint, cache-line relative | `@PREF` |
| 70 | `R_E2K_TLS_GDMOD` | 32 literal | (GOT entry holding the module ID) − GOT | GD |
| 71 | `R_E2K_TLS_GDREL` | 32 literal | (GOT entry holding the DTP offset) − GOT | GD |
| 74 | `R_E2K_TLS_IE` | 32 literal | (GOT entry holding the TP offset) − GOT | IE |
| 75 | `R_E2K_32_TLS_LE` | 32 literal | S + A − TP0 | LE |
| 76 | `R_E2K_64_TLS_LE` | 64 literal | S + A − TP0 | LE |
| 81, 83 | `R_E2K_TLS_32_DTPREL`, `R_E2K_TLS_64_DTPREL` | 32, 64 | S + A − DTP0 | debug info; dynamic: GD offset |
| 80, 82 | `R_E2K_TLS_32_DTPMOD`, `R_E2K_TLS_64_DTPMOD` | 32, 64 | module ID | dynamic only |
| 84, 85 | `R_E2K_TLS_TPOFF32`, `R_E2K_TLS_TPOFF64` | 32, 64 | TP offset | dynamic only (IE GOT entries) |
| 12, 68 | `R_E2K_32_SIZE`, `R_E2K_64_SIZE` | 32, 64 | symbol size + A | `@SIZE` |
| 63 | `R_E2K_64_JMP_SLOT` | 64 | S | dynamic: `.got.plt` slot |
| 64 | `R_E2K_64_COPY` | – | copy | dynamic |
| 65 | `R_E2K_64_RELATIVE` | 64 | load base + A | dynamic |
| 66 | `R_E2K_64_RELATIVE_LIT` | 64 literal | load base + A | dynamic: text relocation of a literal (shared library from non-PIC code) |
| 67 | `R_E2K_64_IRELATIVE` | 64 | resolver(load base + A) | dynamic: ifunc |
| 115 | `R_E2K_ISLOCAL` | 5-bit src1 of a compare | `0xc0` \| 1 if the symbol binds locally, else `0xc0` | `@ISLOCAL`; **cannot be linked** (see below) |
| 118 | `R_E2K_ISLOCAL32` | 32 literal | 1 if the symbol binds locally, else 0 | `@ISLOCAL32` |
| 13, 257 | `R_E2K_32_DYNOPT`, `R_E2K_64_DYNOPT` | 32, 64 | S + A if defined in the same input object file, else 0; no dynamic relocation | `@DYNOPT` (data only) |
| 112 | `R_E2K_NONE` | – | none | **note: not 0**; 0 is `R_E2K_32_ABS` |
| 119, 120 | `R_E2K_ALIGN_RELAX`, `R_E2K_MCSTBUG_140436_RELAX` | – | none (linker relaxation markers) | |

The 32-bit-mode types (8–11: `R_E2K_32_JMP_SLOT`, `_COPY`, `_RELATIVE`,
`_IRELATIVE`) and the protected-mode types (3, 4, 100, 101: `R_E2K_AP_GOT`,
`R_E2K_PL_GOT`, `R_E2K_AP`, `R_E2K_PL`) are not used in e2k64.

There is no `GLOB_DAT`: dynamic GOT entries use `R_E2K_64_ABS` against the
symbol, or `R_E2K_64_RELATIVE` for local ones (lcc output). There is no
TLS relaxation: the linker never rewrites GD or IE code, it only resolves
the GOT entries at link time when it can (below).

## Assembler syntax (gas, lcc)

Relocation operators are written after the symbol inside a literal
operand, `[ _f32s,_lts0 sym@GOT ]` or `[ _f64,_lts0 sym@GOTOFF ]`:
`@GOT`, `@GOTOFF` (32-bit literal → `R_E2K_GOTOFF`, 64-bit literal →
`R_E2K_64_GOTOFF_LIT`), `@GOTPLT`, `@PLT`, `@ABS`, `@PCREL`, `@TLS_LE`,
`@TLS_IE`, `@TLS_GDMOD`, `@TLS_GDREL`, `@TLS_DTPREL` (data), `@SIZE`,
`@ISLOCAL32` (32-bit literal), `@ISLOCAL` (only as the 5-bit first
operand of a compare: `cmpedb,0 [ sym@ISLOCAL ], 0x1, %pred0`),
`@DYNOPT` (data directives only). A bare symbol in a 64-bit literal gives
`R_E2K_64_ABS_LIT`, except `_GLOBAL_OFFSET_TABLE_`, which gives
`R_E2K_64_PC_LIT` (PC-relative to the bundle) unless written
`_GLOBAL_OFFSET_TABLE_@ABS`. `disp %ctprN, sym` gives `R_E2K_DISP`.

## Code sequences (lcc; Unipro the same unless noted)

No register is reserved for the GOT address: each function that needs it
computes it. Calls never need it, because the PLT finds its own GOT slot.

### Non-PIC (`-fno-pic`, executables)

| Access | Code |
|---|---|
| Address of any symbol | `addd 0x0, [ _f64,_lts0 sym ], %r` (`R_E2K_64_ABS_LIT`) |
| Load/store | the literal as the address operand: `ldw %r, [ _f64,_lts0 sym ]` |
| Call | `disp %ctpr1, sym` (`R_E2K_DISP`), `call %ctpr1, wbs = N` |
| Address of an external function | `R_E2K_64_ABS_LIT`; the linker makes the PLT entry the function's canonical address, so it compares equal to the address taken in the library (hw) |

External data referenced from non-PIC code gets a copy relocation
(`R_E2K_64_COPY`, hw).

### PIC and PIE (`-fPIC`, `-fPIE`)

GOT base, in one bundle (B is that bundle):

```
	rrd,0	%ip, %g16                                   ! B
	addd,1	0x0, [ _f64,_lts0 _GLOBAL_OFFSET_TABLE_ ], %g17   ! R_E2K_64_PC_LIT: GOT − B
	...
	addd,0	%g16, %g17, %r3                             ! %r3 = GOT
```

| Access | Code |
|---|---|
| Address of a preemptible or external symbol (data or function) | `ldd %r3, [ _f32s,_lts0 sym@GOT ], %r` |
| Address of a local symbol | `addd %r3, [ _f64,_lts0 sym@GOTOFF ], %r` (`R_E2K_64_GOTOFF_LIT`) |
| Call (any function) | `disp %ctpr1, sym`; the linker points it at the PLT entry if the symbol may be preempted or is undefined |

In a PIE, symbols defined in the executable use `@GOTOFF` and external
ones `@GOT`. In a shared library lcc goes through `@GOT` for every global
symbol (it may be preempted), and calls global functions through the PLT.

### Function pointers and indirect calls

An indirect call loads the address into a CTPR with `movtd %r, %ctprN`
(lcc issues it speculatively, `movtd,0,sm`) and calls it; the address is
the same in every model (taken as above).

## PLT and lazy binding (binutils, glibc)

A call reaches a PLT entry in a **new window**: the caller's `call`
has already allocated it. The entry is (64-bit, PIC):

```
	rrd,0	%ip, %dr8 ; setwd wsz = 0x8, nfx = 0x1
	ldd,0	[ %dr8 + slot − entry ], %dr8        ! .got.plt slot
	movtd,0	%dr8, %ctpr1
	ct	%ctpr1                                 ! jump, not call
```

(non-PIC: `ldd` from the slot's absolute address in a 64-bit literal;
v7: `ibranchd %dr8` instead of `movtd`/`ct`; v6+: `nop 4` instead of
`nop 2` before the use of the load.) The target's own `setwd` then resizes
the window. Consequences for a compiler:

- At function entry only the argument registers are defined; `%dr8` and
  above and `%ctpr1` may have been changed by a PLT entry.
- The PLT jumps rather than calls, so the target returns directly to the
  original caller.

Lazy binding (`DT_E2K_LAZY_GOT`): each `.got.plt` slot initially holds
the address of a secondary entry (link-time address; ld.so adds the load
base when the object is relocated). The secondary entry does
`setwd wsz = 0x8`, puts the byte offset of the `R_E2K_64_JMP_SLOT`
relocation in `.rela.plt` into `%r9`, and branches to the PLT header. The
header loads `.got.plt[1]` (link map) into `%dr8` and `.got.plt[2]`
(`_dl_fixup`) into `%dr10`, and calls `_dl_fixup` with `wbs = 4`, so its
arguments are `%dr8`, `%dr9`; the original arguments in `%dr0`–`%dr7`
stay untouched below the base. `_dl_fixup` returns the target in its
`%dr0` (the header's `%dr8`), and the header jumps there. No assembly
trampoline saves registers: the window does it. `LD_BIND_NOW` and
`-z now` resolve the slots at load time (hw, both ways).

## Thread-local storage (glibc, binutils, lcc, unipro)

- The thread pointer is **`%g13`** (`%g12` in protected mode). The
  kernel preserves it; code must not change it.
- Layout is **variant I** (`TLS_DTV_AT_TP`): at TP is a 16-byte TCB
  `{ dtv_t *dtv; void *private; }`; the executable's TLS block follows at
  TP + align(16, `p_align` of `PT_TLS`). `struct pthread` lies below TP;
  glibc keeps the stack-protector canary at TP − 8 and the pointer guard at
  TP − 16.
- TP offsets of the executable's TLS are fixed at link time:
  TP0 = start of `PT_TLS` − align(16, its alignment).
- `__tls_get_addr` takes **two arguments**, the module ID and the offset
  in the module's block (`void *__tls_get_addr(size_t m, size_t off)`);
  there is no `tls_index`.

Models and code (no local-dynamic model: lcc uses GD for local and
hidden variables too; no relocation for LD exists):

| Model | Used by | Code |
|---|---|---|
| GD | PIC (shared libraries) | load two GOT entries into the argument registers and call: `ldd %r3, [ _f32s,_lts0 x@TLS_GDMOD ], %b[0]`; `ldd %r3, [ _f32s,_lts2 x@TLS_GDREL ], %b[1]`; `disp %ctpr1, __tls_get_addr`; `call %ctpr1`; the address is in `%b[0]` |
| IE | PIE and non-PIC, for TLS defined elsewhere | `ldd %gotbase, [ _f32s,_lts0 x@TLS_IE ], %t`; then `ldw %g13, %t, %r` (the TP offset is added by the load). Non-PIC code gets the GOT base from `addd 0x0, [ _f64,_lts1 _GLOBAL_OFFSET_TABLE_@ABS ], %r` |
| LE | executables, for their own TLS | `ldw %g13, [ _f64,_lts1 x@TLS_LE ], %r` (64-bit offset; `R_E2K_32_TLS_LE` also exists) |

GOT entries the linker creates:

| Entry | In an executable, local symbol | Otherwise |
|---|---|---|
| GD module ID | 1 (one entry shared by all local symbols) | `R_E2K_TLS_64_DTPMOD` (one shared entry for the module's local symbols) |
| GD offset | DTP offset | `R_E2K_TLS_64_DTPREL` |
| IE offset | TP offset | `R_E2K_TLS_TPOFF64` (ld.so: S + `l_tls_offset`; static TLS) |

The module-ID and offset entries are separate, not a pair, so GD loads
two independent GOT offsets.

## Exception tables (lcc, libgcc)

`.eh_frame` CIEs use augmentation `zR` (C) or `zPLR` (C++ with
handlers), with FDE pointer encoding `0x1b` (`pcrel|sdata4`,
`R_E2K_32_PC`), personality `0x9c` (`indirect|pcrel|sdata8`, through a
`DW.ref.__gxx_personality_v0` word, `R_E2K_64_PC`) and LSDA `0x1c`
(`pcrel|sdata8`). `.gcc_except_table` refers to type infos through
`DW.ref.*` words with `R_E2K_64_PC`. Return-address column 0, no CFI
instructions (see [stack-and-unwinding.md](stack-and-unwinding.md)). lcc
emits no `.eh_frame` for C code at `-O2`. (MCST's readelf can't apply
`R_E2K_64_PC` when dumping an object's `.eh_frame` and warns.)

## Stack protector (lcc, unipro, glibc)

Not required. The protector guards the saved return address, and on E2K
that lives in the chain stack, which a program can't write (see
[stack-and-unwinding.md](stack-and-unwinding.md)); spilled registers are
in the procedure stack, also out of reach, and the caller's `USD` is
restored from the chain stack on return. A buffer overflow on the user
stack can still corrupt other arrays and address-taken locals there,
including function pointers, but cannot redirect a return.

Neither lcc nor Unipro emits a check for `-fstack-protector-all` (both
accept the option and ignore it). glibc still stores a canary at TP − 8
(`THREAD_SET_STACK_GUARD`); a compiler that chooses to implement the
protector should read it there.

## Unipro

Unipro's clang uses the same relocations and TLS models, but writes the
GOT base as `_GLOBAL_OFFSET_TABLE_-.L0$pb` against a label on the
`rrd %ip` bundle, and calls `__tls_get_addr@GOTOFF` through `disp`; MCST's
gas rejects the label arithmetic, so its PIC assembly can only be
assembled by its own integrated assembler.

## Rarely used operators, ifunc and gold (binutils, hw)

Nothing in glibc, the kernel, gdb or MCST's runtime libraries uses
`@ISLOCAL`, `@ISLOCAL32` or `@DYNOPT`, and lcc emitted none of them for
our probes. Behaviour of MCST ld 2.41, checked by
`tools/abi/probes/elfops/run.sh` on the 4C:

- **`R_E2K_ISLOCAL` cannot be linked.** Its howto declares a 1-bit field
  while `relocate_section` computes `0xc0 | local`, so every use fails with
  "relocation truncated to fit". Use `@ISLOCAL32`.
- **`@ISLOCAL32`** (1 = binds locally):

  | Symbol | non-PIE executable | PIE | shared library | shared library, `-Bsymbolic` |
  |---|---|---|---|---|
  | `static` | 1 | 1 | 1 | 1 |
  | global, same object | 1 | 1 | **0** | 1 |
  | global, `hidden` | 1 | 1 | 1 | 1 |
  | global, another object of the link | 1 | 1 | **0** | 1 |
  | function from another shared library | 1 | 1 | 1 | 1 |
  | weak undefined | 0 | 0 | 0 | 0 |

  So it is 0 for preemptible definitions and undefined weak symbols. It
  is 1 for functions in other shared libraries (reached through the PLT),
  which matches "a direct call is valid" rather than "defined here".
- **`@DYNOPT`** gives the link-time value of symbols defined in the same
  *input object file* and 0 for anything else, with no dynamic relocation.
  So the value is a usable address only in a non-PIE executable; in a PIE
  or shared library it lacks the load base and only tells "defined in
  this object" (non-zero) apart from "elsewhere" (0).
- **ifunc** (`__attribute__((ifunc))`, `STT_GNU_IFUNC`): works in a shared
  library (exported as an `IFUNC` symbol, resolved by ld.so) and in a
  non-PIE executable (`R_E2K_64_IRELATIVE`), lazily and with
  `LD_BIND_NOW=1`, and the address taken in another object compares equal
  (hw). It **fails in a PIE**: lcc takes the ifunc's address with
  `R_E2K_64_GOTOFF_LIT`, which the ifunc branch of `relocate_section` does
  not handle; it falls into a `default` that returns failure without a
  message (a `FIXME` in the source says so), so ld exits with status 1
  and prints nothing. A **static** link with an ifunc makes ld crash
  (segmentation fault). A compiler should reach ifunc symbols through
  `@GOT` in PIC code.
- **gold** has an E2K backend (`gold/e2k.cc`, enabled in
  `gold/configure.tgt`) but MCST does not ship it. Built from source and
  given lcc's link lines, it links a static program correctly but breaks
  dynamic ones: it has no copy-relocation support (a non-PIE executable
  using a library variable gets a text relocation and computes a wrong
  result) and the PIE dies with `SIGILL`. Its output has no
  `DT_E2K_LAZY_GOT`, so ld.so would bind it eagerly. `ld.bfd` is the only
  usable linker.

## Not covered yet

- `@PREF` and `@GOTPLT` are read from the source only.
- 32-bit and protected modes (separate relocation types, `ldgd`, `%g12`).
