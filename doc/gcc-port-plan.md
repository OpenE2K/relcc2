# Plan: an E2K backend for GCC

A map for writing the GCC port by hand, for someone who knows C++ but has
never worked on a compiler or on GCC's source. It explains what the pieces
are, in what order to build them, how to compile and test each step, how
the VLIW scheduler should work, and what to take from the Itanium (ia64),
TILE-Gx and C6x ports.

Per decision 2 in AGENTS.md this document contains no code for GCC: it
points at existing GCC code to study and at the E2K facts in `spec/` to
implement. Paths like `gcc/config/ia64/ia64.cc:2691` refer to the GCC
mainline checkout described in [Trees](#1-the-gcc-trees); line numbers are
from commit `ea32419e8c4` (2026-10-02, GCC 17.0.0) and will drift.

Contents:

0. [How GCC turns C into assembly](#0-how-gcc-turns-c-into-assembly)
1. [The GCC trees](#1-the-gcc-trees)
2. [What E2K asks of a compiler](#2-what-e2k-asks-of-a-compiler)
3. [Wiring a new target into GCC](#3-wiring-a-new-target-into-gcc)
4. [Building and running the cross compiler](#4-building-and-running-the-cross-compiler)
5. [Milestones](#5-milestones)
6. [Design decisions](#6-design-decisions)
7. [Scheduling for a VLIW/EPIC machine](#7-scheduling-for-a-vliwepic-machine)
8. [What to take from ia64, TILE-Gx, C6x and SPARC](#8-what-to-take-from-other-ports)
9. [Testing](#9-testing)
10. [Upstreaming](#10-upstreaming)
11. [What relcc2 provides alongside](#11-what-relcc2-provides-alongside)

---

## 0. How GCC turns C into assembly

You need this mental model before any of the files make sense.

### The pipeline

```
C source
  │  front end (c/, cp/): parse, type-check
  ▼
GENERIC (trees) ──► GIMPLE (three-address code, SSA)
  │  ~200 target-independent "tree passes": inlining, constant
  │  propagation, loop optimisations, vectorisation, ...
  ▼
expand: GIMPLE → RTL, using the target's "named patterns"
  │
RTL (register transfer language), ~100 "RTL passes":
  │  combine, if-conversion, register allocation (IRA + LRA),
  │  prologue/epilogue, scheduling (sched1, sched2), machine reorg
  ▼
final: print each RTL instruction using the target's output templates
  ▼
assembly text  ──► GNU as (MCST binutils) ──► object file
```

The target only influences the second half. Everything above "expand" is
shared by all targets and you will rarely touch it.

### What a backend is

A backend is three kinds of things, all under `gcc/config/e2k/`:

1. **Target macros** in `e2k.h`: plain C preprocessor definitions such as
   `BITS_PER_WORD`, `FIRST_PSEUDO_REGISTER`, `REG_CLASS_CONTENTS`. Old
   style, still used for the most basic facts. Documented in
   `gcc/doc/tm.texi` (generated from `tm.texi.in`).
2. **Target hooks** in `e2k.cc`: C++ functions placed in a big structure
   `struct gcc_target targetm`. You override a hook with
   `#define TARGET_FUNCTION_ARG e2k_function_arg` before
   `TARGET_INITIALIZER`. All hooks, with their documentation, are listed in
   `gcc/target.def`; defaults are in `gcc/targhooks.cc`. Anything you
   don't override keeps its default.
3. **The machine description** in `e2k.md` (and files it includes): a
   Lisp-like file of *patterns*. Each pattern says "this RTL shape is one
   machine instruction, its operands must satisfy these predicates and
   constraints, and here is how to print it". Build-time generator
   programs (`genrecog`, `genemit`, `genoutput`, `genattrtab`,
   `genautomata`, ...) turn it into C++ that is compiled into `cc1`.

### Vocabulary

| Term | Meaning |
|---|---|
| RTL | GCC's low-level IR. An expression like `(set (reg:DI 100) (plus:DI (reg:DI 101) (const_int 4)))` |
| insn | One RTL instruction in the instruction stream (`rtx_insn *`) |
| mode | The size/kind of a value: `QImode` 8-bit, `HImode` 16, `SImode` 32, `DImode` 64, `TImode` 128, `SFmode`/`DFmode`/`XFmode` floats, `BImode` 1-bit (predicates) |
| pseudo register | An unlimited virtual register used before register allocation (`reg 100` and up) |
| hard register | A real machine register (numbers `0 .. FIRST_PSEUDO_REGISTER-1`, numbering chosen by you) |
| register class | A set of hard registers an operand may use (`GENERAL_REGS`, `PRED_REGS`, ...) |
| predicate (in .md) | A C test that an operand has the right *kind* (register, small constant, memory) |
| constraint (in .md) | A letter code saying which registers/constants an operand may use *after* allocation (`"r"`, `"I"`, `"m"`) |
| `define_insn` | A pattern that matches one machine instruction and prints it |
| `define_expand` | A pattern used only at expand time to generate one or more insns |
| `define_split` | A rule that rewrites one insn into several, usually late |
| standard names | Pattern names the middle end looks for, like `adddi3`, `movdi`, `call`, `cbranchdi4`; listed in `gcc/doc/md.texi` (node "Standard Names") |
| IRA / LRA | The register allocator and the pass that fixes up operands to satisfy constraints. **New ports must use LRA** (`gcc/target.def`, hook `lra_p`); old "reload" is being phased out |
| prologue / epilogue | Code at function entry/exit, generated by the port's named patterns `prologue` and `epilogue` (usually calling functions in `e2k.cc`) |
| DFA | The scheduler's model of the pipeline, written as `define_automaton`/`define_cpu_unit`/`define_insn_reservation` in .md files |
| machine reorg | A pass near the end (`TARGET_MACHINE_DEPENDENT_REORG`) where a port may do anything; VLIW ports do their final scheduling and bundling there |

### Learning path (about two weeks before writing code)

Read in this order; the line numbers are into the mainline checkout.

1. `gcc/doc/sourcebuild.texi`, "Anatomy of a Target Back End" (around
   line 796): the official list of files a port has.
2. `gcc/doc/md.texi`: Overview (55), Patterns (103), RTL Template (257),
   Output Template (505), Predicates (879), Constraints (1240), Standard
   Names (4732, skim, come back as needed), Expander Definitions (9169),
   Insn Splitting (9383), Insn Attributes (10178), Processor pipeline
   description (10971), Conditional Execution (11491).
3. `gcc/doc/rtl.texi`: RTL objects, machine modes, insns. Skim.
4. `gcc/doc/tm.texi.in`: Storage Layout (864), Registers (1647),
   Register Classes (2012), Stack and Calling (2442), Addressing Modes
   (4166), Condition Code (4433), Costs (4571), Scheduling (4813),
   Assembler Format (5185). Read Stack and Calling carefully.
5. **Read a whole small port end to end**: `gcc/config/or1k/` (OpenRISC),
   about 4,400 lines with a glibc Linux configuration: `or1k.h` (417),
   `or1k.cc` (2345), `or1k.md` (1062), `constraints.md`, `predicates.md`,
   `linux.h` (49), `t-or1k`. Every file of a new port has a counterpart
   there.
6. Then read the parts of `gcc/config/ia64/` listed in section 8.

Hands-on habits that make GCC approachable:

- Compile with `-fdump-tree-all -fdump-rtl-all` and read the dump files
  in order: you see your function change pass by pass. `-dP` annotates
  the assembly with the RTL of each insn and the pattern it matched.
- Run `cc1` directly and under gdb: `gcc -v` shows the `cc1` command line;
  `gcc -wrapper gdb,--args ...` starts it under gdb. GCC has debugging
  helpers callable from gdb: `call debug_rtx(insn)`,
  `call debug_rtx_list(insn, 10)`, `call debug_tree(t)`; the build
  directory's `gdbinit.in` adds `pr`/`pt` commands.
- When something is unclear, find a port that does the same thing and
  read it (see section 8 for which port does what).

---

## 1. The GCC trees

Under `toolchains/` (untracked):

| Path | What | Use |
|---|---|---|
| `toolchains/gcc` | GCC mainline, blobless clone of `https://gcc.gnu.org/git/gcc.git`, `master` at `ea32419e8c4` | Base for the port. Contains ia64 (maintained for Linux; HP-UX/VMS/ELF variants are obsolete, `gcc/config.gcc:272`), C6x, FR-V, Blackfin, SPARC |
| `toolchains/gcc-tilegx` | Worktree of the same clone at `54a5f478487` (2022-06-25), the commit before TILE-Gx/TILEPro were removed (`fc259b522c0`) | Reference only: TILE-Gx prints bundles as `{ ... }` exactly like E2K |

Itanium is still in mainline, so it needs no separate worktree. To start
the port, make your own worktree and branch so mainline stays clean:

```
cd toolchains/gcc
git worktree add -b e2k ../gcc-e2k master
```

and rebase `e2k` on `master` from time to time (`git fetch && git rebase
origin/master`). GCC moves fast; rebasing weekly keeps conflicts small.

---

## 2. What E2K asks of a compiler

Everything here is specified in `spec/`; this section only translates it
into compiler terms and says where it bites.

### An example

lcc 1.29.16, `-O2`, for

```c
long g(long *p, long n);
long f(long a, long b) { long buf[4] = { a, b, a + b, a - b }; return g(buf, 4) + a; }
```

```
f:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0     ! window: 8 pairs = 16 registers
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0    ! %b[0] = %r8, 4 pairs of "outgoing" regs
	  disp	%ctpr1, g; ipd 2                     ! prepare the call target early
	  getsp,0	_f32s,_lts1 0xffffffd0, %r3      ! allocate 48 bytes of user stack
	}
	{ nop 1 ; addd,0 0x4, 0x0, %b[1] }            ! 2nd argument
	{ addd,0 %r0, %r1, %r4 ; subd,1 %r0, %r1, %r5 }
	{ addd,3 %r3, _f64,_lts0 0x30, %r2 }          ! %r2 = entry stack pointer
	{ addd,3 %r2, _f16s,_lts0lo 0xffe0, %b[0]     ! 1st argument = &buf
	  std,5  %r2, _f16s,_lts0lo 0xffe0, %r0 }    ! buf[0] = a
	{ std,2 %r2, ... %r1 ; std,5 %r2, ... %r4 }
	{ std,2 %r2, _f16s,_lts0lo 0xfff8, %r5 }
	{ call %ctpr1, wbs = 0x4 }                    ! callee's window starts at %r8 = %b[0]
	{ nop 5 ; return %ctpr3; ipd 2                ! prepare the return
	  addd,3 %b[0], %r0, %r0 }                    ! result came back in %b[0]; a survived in %r0
	{ ct %ctpr3 }
```

(Braces regrouped onto lines to save space.) Each `{ }` is one wide
instruction, executed in one cycle. Things to notice: no instructions
save or restore registers (the window does it); arguments go out in
`%b[0..]` and the result comes back there; the call target is prepared
by `disp` long before `call`; the return is prepared by `return` and
executed by `ct`.

### Facts that shape the port

| Fact | Where specified | Consequence for GCC |
|---|---|---|
| One register file for integers and floats: 32 globals `%g0–31`, a window of up to 64 directly named `%r0–63`, `%b[]` relative to a movable base | `spec/abi/registers.md` | One main register class for int and FP. No FP/int moves |
| Window sized per function by `setwd wsz=N` (pairs); callee's args arrive in `%r0–7`; caller writes outgoing args into `%b[]` after `setbn` points it past the locals; `call wbs=W` starts the callee's window at pair W | `registers.md`, `calling-convention.md` | ia64-style in/local/out modelling, window size computed after register allocation (section 6, D1–D2) |
| Window registers are preserved across calls by hardware; globals are scratch; `%g12`/`%g13` reserved (`%g13` = thread pointer) | `registers.md` | `CALL_USED_REGISTERS`: globals call-clobbered, window registers not; no callee-save code at all |
| 32 predicate registers, each one bit; compares write them; ALU ops can be predicated through CDS syllables; control transfers take conditions | `spec/isa/bundle.md` (PLS, CDS, ctcond) | `BImode` predicate class, `define_cond_exec`, conditional branches on predicates |
| Control transfer = prepare (`disp`/`movtd`/`return` into `%ctpr1–3`) then `ct`/`call`; prepare→`ct` latency 5 cycles (`disp`), 6 (`return`), 9 (`movtd`); also an immediate `ibranch label ? cond` with no preparation | guide 6.1, ch. 10.9–10.10; `bundle.md` CS0/SS | Simplest correct start: `ibranch` for jumps; later split prepare/transfer so the scheduler can hoist `disp` |
| One wide instruction per cycle: 6 ALU channels, each op allowed only in some channels; ≤ 4 32-bit literal slots; 1 control transfer and 1 prepare; 3 predicate-logic ops; store/load channel restrictions | `bundle.md`, `spec/isa/binutils-ops.yaml` (`channels` per op) | DFA description (section 7) |
| **The pipeline interlocks almost everything, but stalls are expensive**: register results, compare → predicate users, `disp`/`movtd` → `ct`, `%fpcr`/`%pfpfr` were all correct at distance 1 on the 4C; the only non-interlocked case found is `rwd %lsr` → `rrd %lsr` (needs 4 cycles). An operand that arrives `k` cycles late costs about a whole extra latency, not `k` cycles (`spec/isa/timing.md`). gas's `strict_delay` mode (ops outside braces) pads compares and privileged state-register accesses anyway | `spec/isa/timing.md` [hw], guide 6.1, MCST gas | One op per wide instruction is correct. Scheduling with exact latencies is what makes code fast; keep gas's padding (no `.ignore strict_delay`) until the scheduler places `nop`s itself (section 7.4) |
| **gas assigns channels itself** if you omit `,N`, and treats an op outside braces as its own wide instruction (checked: `addd`, `addd`, `ldd` in one brace group were placed in channels 1, 2, 0) | tested with MCST gas | Early milestones print plain ops; channel numbers are never required |
| `setwd dbl=1` makes 32-bit ops zero-extend into the full register; `dbl=0` leaves the upper half untouched (including a stale diagnostic tag that traps when stored) | `registers.md`, `calling-convention.md` "Unused parts of a slot" | per function like lcc: `dbl=1` where unsigned 32-bit results need zero-extending, else `dbl=0` (owner's decision, D7) |
| Speculative mode (`,sm`): faults become a tagged result; a non-speculative use of a tagged value traps | guide 10.2, `calling-convention.md` | Control speculation needs no check instruction (section 7.6) |
| Rotating registers, loop counters, loop mode, array prefetch (APB) | guide 4.5, 10.11 | Advanced; GCC has no rotating-register support anywhere (section 7.7) |
| Calls, results, varargs, extension rules, ELF, relocations, PIC, TLS, `.eh_frame` | `spec/abi/*.md` | Implement as specified; verify with the compat suite |

---

## 3. Wiring a new target into GCC

GCC's tree already recognises the CPU name: `config.sub:1195` lists `e2k`
and `config.guess:980` maps an E2K Linux host to `e2k-unknown-linux-gnu`.
Nothing else mentions E2K yet.

LoongArch is the most recent full port and a good model for the Linux and
build-system parts. It went in on 2022-03-29 as a commit series you can
read with `git show`:

| Commit | Area |
|---|---|
| `c6c0594ef83` | Top-level `configure.ac`/`configure`, `config/picflag.m4` |
| `b44786f6401` | `gcc/config.gcc`, `gcc/configure.ac`, `gcc/common/config/loongarch/`, `linux.h`, `.opt`, `t-*` |
| `bcaf571cf9b` | Machine description: `constraints.md`, `predicates.md`, `loongarch.md`, pipeline `.md`, `-modes.def` |
| `62ec3b5352b` | `loongarch.cc`, `loongarch.h`, `loongarch-protos.h` |
| `bf291a4330d`, `b38100eb08a` | `libgcc/config.host`, `libgcc/config/loongarch/` |
| `34024b71506` | `libgomp/configure.tgt` |
| `4ae54e8dec3` | Testsuite: `target-supports.exp`, `gcc.target/loongarch/` |
| `8766689a78b` | Docs: `contrib/config-list.mk`, `install.texi`, `invoke.texi`, `md.texi` |

### 3.1 Files to create in `gcc/config/e2k/`

| File | Purpose | Model |
|---|---|---|
| `e2k.h` | Target macros: sizes, registers, classes, frame layout | `or1k/or1k.h` |
| `e2k.cc` | Hooks + `targetm`; must `#define IN_TARGET_CODE 1` before including `config.h` (sourcebuild.texi:885) | `or1k/or1k.cc` |
| `e2k-protos.h` | Prototypes of `e2k.cc` functions used from `.md` | `or1k-protos.h` |
| `e2k.md` | Patterns, attributes; includes the others | `or1k.md`, `ia64.md` |
| `constraints.md`, `predicates.md` | Operand constraints and predicates | or1k, ia64 |
| `e2k-modes.def` | Extra modes if needed (e.g. condition modes) | `loongarch-modes.def` |
| `e2k.opt` | `-m` options (`-march=elbrus-vN`, `-mtune=`, ...) | `or1k.opt` |
| `elbrus-v3.md` … | Pipeline (DFA) descriptions per CPU generation (section 7) | `ia64/itanium2.md`, `c6x/c6x.md` |
| `linux.h` | `GLIBC_DYNAMIC_LINKER "/lib64/ld-linux.so.2"`, link spec, OS macros | `or1k/linux.h` |
| `t-e2k` | Makefile fragment (multilibs, generated `.md`) | `or1k/t-or1k`, `c6x/t-c6x` |
| `gcc/common/config/e2k/e2k-common.cc` | Option defaults per `-O` level (e.g. enable scheduling at `-O1`) | `common/config/tilegx/`, `common/config/c6x/` |

### 3.2 Places outside the directory

| File | What to add |
|---|---|
| `gcc/config.gcc` | Two `case` entries. First (around :340–633): `e2k*-*-*) cpu_type=e2k ;;`. Second, OS-specific: `e2k*-*-linux*)` with `tm_file="elfos.h gnu-user.h linux.h glibc-stdint.h ${tm_file} e2k/linux.h"` and `tmake_file` (see `riscv*-*-linux*` at :2551 and `or1k*-*-linux*` at :2946). The header comment (:21–120) explains every variable |
| `gcc/configure.ac` | Add `e2k` to the `cpu_type` list with a valid `nop` for the dwarf2 `.loc` probe (:5727); add a TLS assembler probe (`conftest_s`, near :4038) or `HAVE_AS_TLS` stays off. Regenerate `gcc/configure` with the autoconf version GCC requires |
| `libgcc/config.host` | `cpu_type` (:138 area) and an `e2k*-*-linux*` entry: `extra_parts`, `tmake_file`, `md_unwind_header` (see loongarch at :993) |
| `libgcc/config/e2k/` | `t-e2k`, `sfp-machine.h` (for `__float128` soft-fp), `linux-unwind.h` (signal frames), crt pieces. **The unwinder is special on E2K** (it walks the chain stack, see `spec/abi/stack-and-unwinding.md`); see 3.3 |
| `libatomic/configure.tgt`, `libgomp/configure.tgt`, `libstdc++-v3/configure.host`, `libitm/`, `libsanitizer/` | Later; most have generic fallbacks |
| `contrib/config-list.mk` | Add `e2k-linux-gnu` to the list (lets maintainers build-test the port) |
| `gcc/doc/install.texi`, `invoke.texi`, `md.texi` | Target notes, `-m` options, constraint letters |
| `gcc/testsuite/lib/target-supports.exp`, `gcc.target/e2k/e2k.exp` | Effective-target answers and target tests |
| `MAINTAINERS.yml` | Maintainer entry (MAINTAINERS is generated by `contrib/gen-MAINTAINERS.py`) |

### 3.3 The unwinder and other runtime pieces

MCST publishes the GCC 11 runtime libraries that lcc ships, under the GPL
with the runtime exception: `internal-lcc-libs-open-source-1.29.16.tar.xz`
from https://setwd.ws/sp/1.29/1.29.16/src/ (fetch and unpack with
`tools/setup/fetch-lcc-libs.sh` into `toolchains/src/lcc-1.29.16/`).
It contains libgcc, libatomic, libquadmath and libstdc++ sources (no
compiler), with E2K files in `lib/GNU/gcc-11/libgcc/config/e2k/`:

| File | Lines | What |
|---|---|---|
| `unwind-dw2-e2k.c` | 1192 | The unwinder: walks the chain stack instead of interpreting CFI; copyright AO MCST and FSF |
| `sfp-machine.h`, `t-softfp`, `tf-signs.c`, `sfp-exceptions.c` | ~200 | Soft-float for `__float128` |
| `clear-cache.c` | 79 | `__e2k_clear_cache` |
| `crtalign.S`, `crtfastmath.c` | 29 | Extra crt objects |
| `libgcc-glibc.ver` | 63 | Symbol versions glibc expects from libgcc |
| `t-e2k`, `t-eh-e2k`, `t-linux`, `t-crtalign` | 12 | Makefile fragments; `t-linux` links `libgcc_s.so` with lcc's `-llcc` (for `__builtin_cpu_is`) |

MCST's own triplet there is `e2k-mcst-linux-gnu` (`libgcc/config.host`).
The generated `gcc_objdir/tm.h` that libgcc is built with records two
ABI constraints a GCC port must keep:

- Code that `crtstuff` puts in `.init`/`.fini` must call with
  `call %ctpr1, wbs = 0x6`: glibc's `crti.o` depends on it, so changing
  it breaks binary compatibility (only relevant without `.init_array`).
- The unwinder's comment notes that an earlier MCST `e2k-linux-gcc`
  numbered DWARF registers differently and was incompatible at run time
  with lcc-built libraries; follow `spec/abi/registers.md` (DWARF 0–63 =
  `%r0–63`, 64–127 = `%b[0–63]`).

**Decision (owner): use MCST's unwinder and libgcc E2K files.** MCST
publishes them under GPLv3 with the runtime exception, in a public
release. In the port they keep MCST's copyright notices; the commit that
adds them should say where they come from (the DCO's clause (b) covers
submitting work under a compatible free licence). Adapt rather than
rewrite: drop the `-llcc` link in `t-linux` (GCC's own
`__builtin_cpu_is` support replaces it) and check the `__LCC__`-only
paths (e.g. the DWARF register ranges in `_Unwind_SetGR`). What else
building it takes (`spec/abi/stack-and-unwinding.md`,
`spec/abi/source-compat.md`; the proof-of-concept backend built
mainline libgcc this way, `docs/spec-poc-findings.md` A7, A10, A11):

- MCST also changed the generic `libgcc/unwind.inc` (`#if defined
  __e2k__`: a chain-stack cache in the unwind context, reset before phase
  2). Carry those hunks, not only the `config/e2k/` files.
- The unwinder returns to landing pads normally after `set_backtrace`;
  `__builtin_eh_return` is never used, so no `eh_return` pattern.
- CPU detection: the unwinder picks the procedure-stack layout (v1–v4
  or v5+) with `__builtin_cpu_is`, which lcc backs with `__cpu_info` from
  proprietary `liblcc`. Define and fill `__cpu_info` in libgcc instead
  (from `%idr`; layout and codes in source-compat.md). MCST's static
  `libc.a` references it too, so without it every static link that pulls
  in `dlopen`, NSS or iconv fails.
- `__builtin_return_address (0)` is `rrd %cr0.hi` masked to bits 0–47.
- MCST also adds `_Unwind_GetPCSP` to the `GCC_3.0` node of the generic
  `libgcc-std.ver.in`. libgcc_s must export it: MCST's glibc dlopens
  libgcc_s for thread exit and cancellation and asserts the symbol
  exists (`sysdeps/e2k/unwind-arch.h`), so without it every thread that
  exits through glibc's unwinder aborts.
- Runtime libraries: libatomic builds unchanged; libgomp needs an
  `e2k*-*-linux*` entry (`config_path="linux posix"`) in
  `libgomp/configure.tgt`, else it falls back to the POSIX
  configuration without futexes or pinned memory (lcc ships LLVM's
  `libomp`, not libgomp); libquadmath needs the `__builtin_*q` builtins
  (`fabsq`, `copysignq`, `nanq`, `nansq`, `infq`, `huge_valq`) and
  `-std=gnu17` (glibc's e2k `fenv.h` breaks under C23).
- DWARF: MCST's gdb ignores CFI and misreads `DW_OP_call_frame_cfa`, so
  `DW_AT_frame_base` must be a register expression
  (stack-and-unwinding.md, "Debug information").

Until libgcc builds (M3), link against the system's `libgcc_s.so.1`
from lcc's sysroot.

Source-level and runtime interface rules from lcc's own documentation,
written up in `spec/abi/source-compat.md` and `spec/abi/atomics.md`:

- Define `__e2k__`, `__iset__`, `__ptr64__` and, with `-mtune`, the
  `__elbrus_<cpu>__` macros (`TARGET_CPU_CPP_BUILTINS`), but **never
  `__LCC__`**: glibc's installed headers use it to choose lcc-only code,
  and the plain-GCC branch is the one that suits the port.
- Inline asm: `r` is one register or an even-aligned pair (9–16 bytes),
  `x` one extended register; operands print as the bare register name
  (`%r2`). Register variables use `%xgN` for extended registers.
- Atomics (`sync.md`): read-modify-write = a lock-wait load (MAS `0x7`,
  ALC0) and a wait-unlock store (MAS `0x2`) with `rbranch` back, which
  gas assembles as `ibranch ? %MLOCK`; fences = `wait ld_c = 1,
  st_c = 1`. Under `-march=elbrus-v3` 16-byte atomics must call
  libatomic, like lcc. The 4C orders memory like x86 TSO across wide
  instructions (atomics.md, "Memory ordering on hardware"), so lcc's
  `wait`s around acquire/release accesses are stronger than needed;
  following lcc keeps them. Never bundle a release store with an
  earlier store: stores in one instruction are unordered.
- lcc treats `swapcontext` as returns-twice and zero-initialises
  possibly uninitialised locals at `-O1`+ to avoid tag traps; the port
  should consider both.

---

## 4. Building and running the cross compiler

### 4.1 Tools

- Assembler and linker: MCST binutils, either from lcc
  (`/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/binutils/bin/e2k-linux-{as,ld}`)
  or built from the submodule with `tools/setup/build-binutils.sh`.
- Sysroot: lcc's, which has glibc's `crt1.o`/`crti.o`/`crtn.o`, `libc`,
  `libm`, the dynamic linker, and MCST's `libgcc_s.so.1` and
  `libstdc++.so.6`: `/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/fs`.
- Host compiler: any recent GCC on the build machine; GCC's own
  prerequisites (GMP, MPFR, MPC; `contrib/download_prerequisites` fetches
  them into the source tree).

### 4.2 Configure and build (build directory outside the source tree)

```
mkdir -p toolchains/build-gcc-e2k && cd toolchains/build-gcc-e2k
LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1
../gcc-e2k/configure \
    --target=e2k-linux-gnu \
    --prefix=$PWD/../gcc-e2k-install \
    --with-sysroot=$LCC/fs \
    --with-as=$LCC/binutils/bin/e2k-linux-as \
    --with-ld=$LCC/binutils/bin/e2k-linux-ld \
    --enable-languages=c \
    --disable-bootstrap --disable-multilib \
    --disable-libsanitizer --disable-libquadmath --disable-libgomp \
    --disable-libatomic --disable-libssp --disable-libitm \
    --enable-checking=yes,rtl
make -j16 all-gcc          # builds the driver and cc1 only
make install-gcc
```

Linking with GCC's driver needs two workarounds: lcc's sysroot has
`lib64`, `lib32` and `lib128` (also under `usr/`) but no `lib` or
`usr/lib`, so the `usr/lib/../lib64` search paths GCC derives from
`MULTILIB_OSDIRNAMES = .=../lib64` don't resolve (use a sysroot of
symlinks with empty `lib` and `usr/lib`); and MCST's libgcc build looks
for target tools named `e2k-linux-gnu-*`, while lcc ships `e2k-linux-*`
(symlink them).

`--enable-checking=yes,rtl` makes the compiler verify its RTL constantly:
slow, but it turns silent bugs in your patterns into immediate errors.
`all-gcc`/`install-gcc` are top-level make targets (not described in
`install.texi`, but present in the top-level `Makefile.in`).

First test, compile only:

```
gcc-e2k-install/bin/e2k-linux-gnu-gcc -O0 -S test.c -o -
```

Once `libgcc` builds (milestone M3):

```
make -j16 all-target-libgcc && make install-target-libgcc
```

### 4.3 Running programs

- Link with your GCC (it finds `crt1.o`, `libc` in the sysroot), or with
  lcc during early milestones: compile with GCC (`-c`), link with lcc.
- Run under MCST's qemu user mode (installed as `qemu-e2k` on the owner's
  workstation): `qemu-e2k -cpu elbrus-v3 -L $LCC/fs ./a.out`.
- Run on the Elbrus 4C: see AGENTS.md "Hardware";
  `tools/abi/probes/dso/run.sh` shows the copy-and-run pattern.

### 4.4 Rebuilding after changes

After editing `e2k.md`/`e2k.cc`, `make -j16 all-gcc` in the build
directory rebuilds only what changed (the generators rerun when the `.md`
changes). Keep two build directories: a checking build for development
and a `--enable-checking=release` build for timing.

---

## 5. Milestones

Each milestone ends with something you can run. Earlier ones deliberately
produce slow code: correctness first, then the VLIW work.

### M0. An empty target that builds (1–2 weeks)

- Wire in `config.gcc`, `configure.ac`, and a minimal `gcc/config/e2k/`:
  registers, one class, `movdi`, `adddi3`, `jump`, `return`, `nop`. Copy
  the shape of `or1k`.
- Goal: `make all-gcc` succeeds; `e2k-linux-gnu-gcc -S` on `int f(int
  x){return x+1;}` prints something gas accepts.
- Print **one operation per line, no braces, no channel numbers**: gas
  makes each one a wide instruction and picks the channel.
- Start every file with `.ignore ld_st_style` and `.ignore strict_delay`
  like lcc (owner's decision: follow lcc, D9). The first makes stores
  *address, offset, data* (`bundle.md`, "Assembler syntax notes"); the
  second stops gas from padding compares and state-register accesses, so
  the compiler owns all delays. That is safe from the start because the
  hardware interlocks everything user code touches except `rwd %lsr` →
  `rrd %lsr` (`spec/isa/timing.md`); early braceless code just stalls
  (slow, not wrong) until the scheduler (M4) places `nop`s.

### M1. Straight-line code and branches, `-O0`/`-O1` (3–6 weeks)

- Integer and logical arithmetic in all widths, loads/stores with
  `reg + literal` addressing, constants (literal syllables), compares into
  predicates, conditional branches.
- **Branches**: start with `ibranch label` and `ibranch label ? %predN`
  (immediate, no preparation register). Correct and simple.
- **Prologue/epilogue**: `setwd` in the first wide instruction with a
  window size from a fixed maximum (e.g. 32 pairs) at first; `getsp` for
  the stack frame; epilogue `return %ctpr3` … `ct %ctpr3`.
- Goal: small programs compiled with GCC, linked by lcc, run correctly
  under qemu-e2k and on the 4C.

### M2. Calls and the whole ABI (4–8 weeks)

- `call`/`call_value` as `disp %ctpr1` + `call %ctpr1, wbs=W`; outgoing
  args in `%b[]` (D1), stack argument area always reserved, results in
  `%b[0..7]`, large results through the caller's argument area,
  variadic, unprototyped, narrow-integer extension, `long double`,
  `__int128` even slots. Everything in `spec/abi/calling-convention.md`.
- Window size computed after register allocation (D2).
- Goal: **the compat suite passes against lcc**:
  `python3 tools/abi/compat/run.py GEN --cc gcc='.../e2k-linux-gnu-gcc -O2'
  --pairs lcc:gcc,gcc:lcc,gcc:gcc`, under qemu and on the 4C
  (`--remote`). This is the ABI acceptance test.

### M3. Complete C (2–3 months)

- Floating point (single, double, the unified register file), `long
  double` (80-bit, `nfx=0` in functions that use it), `__float128` via
  soft-fp in libgcc, division (hardware on ALC5), all standard names the
  middle end wants.
  `bswap<mode>2` with a memory operand (or combine patterns for
  `(bswap (mem))` and `(set (mem) (bswap ...))`, HI, SI and DI) can use
  MAS bit 3, a byte-swapped load or store at no cost: `ntohs`, `htonl`
  and friends on network data (`sockaddr_in`, packet headers) become one
  access instead of lcc's mask literal, `sxt` and `pshufb`, 5 cycles
  shorter on a dependent load path. **Only for accesses aligned to their
  size** (`MEM_ALIGN`): a misaligned byte-swapped access is SIGBUS where
  a plain one works, which is why lcc folds only under `-ffast`
  (`spec/isa/semantics.md`, "Byte-swapped memory accesses").
- `spec/isa/operations.md` lists every operation with the standard name
  or C idiom it serves ("Use"), its latency, how often lcc uses it and
  whether it was checked on hardware: the place to look for an
  instruction to match a pattern. `trap` can be `setsft` (SIGTRAP
  `TRAP_BRKPT`, the kernel's `BUG()`); lcc's `__builtin_trap` stores to
  address 0 (SIGSEGV).
- Operations that map onto standard names (`spec/isa/semantics.md`,
  "Other scalar operations" and "80-bit operations", all checked on the
  4C and the 8C2): `smulx`/`umulx` for `mulsidi3`/`umulsidi3` (lcc uses
  them for `(long) a * b`); `insfs`/`insfd` for `insv` (lcc stores a
  bit-field with `insf` and a rotate back, `scl`); `getfd`/`getfs` for
  `extzv`/`extv` and the `zero_extract`/`sign_extract` that combine
  makes of shift-and-mask and shift-pair idioms, and for `and` with a
  low mask wider than 32 bits (a 16-bit descriptor instead of lcc's
  64-bit literal); the descriptor's BYTE field must be ((offset + len −
  1) mod width) / 8 or v3 sign-extends from the wrong bit, and on v5
  `getfz` also covers masks with up to 7 low zeros
  (`spec/isa/semantics.md`, "Field extract"); `fscales`/`fscaled`
  for `ldexp<mode>3` on v4+ with `-fno-math-errno` (lcc: `scalbnf`
  under `-ffast`). The 64 ÷ 32 `sdivx`/`udivx`/`smodx`/`umodx` trap on
  quotients that don't fit, so they suit only proven ranges; lcc
  computes `%` as divide, multiply, subtract. `__builtin_add_overflow`
  could be `adds_fw` + `cctopo` (`addv<mode>4`; subtraction `cmpo*b`
  alone) where lcc spends ten operations; their latencies are not
  measured. The 80-bit operations with a narrower operand or result
  (`fxadddd`, `fxmulxs`, ...) round as x87 code does (to 64 bits, then
  to the result's format), so `(float_truncate:DF (plus:XF ...
  (float_extend:XF ...)))` may be one `fxadddd` without fast-math, as
  lcc does at `-O3`.
- PIC/PIE and TLS exactly as `spec/abi/elf.md` (GOT base via `rrd %ip` +
  `_GLOBAL_OFFSET_TABLE_` literal in the same wide instruction). In PIC
  code reach ifunc symbols through `@GOT`: MCST ld cannot link a
  `@GOTOFF` reference to an ifunc (`elf.md`, "Rarely used operators").
- `libgcc` builds; `gcc.c-torture/execute` and `gcc.dg` pass with a
  DejaGnu board for qemu or the 4C (section 9).
- `.eh_frame` with the encodings in `elf.md` (needed for C++ later).

### M4. Bundling and the basic scheduler (1–2 months)

- DFA description of one generation (v3 first: it runs everywhere and the
  4C can test it), late scheduling in machine reorg, braces printed from
  TImode marks, `nop N` for known gaps. Section 7.1–7.4.
- Goal: same test results, measurably faster code; compare cycle counts
  with lcc on the 4C (`perf` is installed there).

### M5. Prepared control transfers and predication (1–2 months)

- Split calls/branches into prepare + transfer, every branch prepared,
  `disp` → conditional `ct` latency 2 (D4); `return` early in the
  epilogue.
- If-conversion to predicated code (`define_cond_exec`).

### M6. Speculation (research-grade)

- Control speculation, through sched-ebb with the four steps of 7.6
  item 3 and post-RA renaming, or through selective scheduling.
- Data speculation (load + check) like ia64's `ld.a`/`chk.a`.

### M7. Loops (research-grade)

- Counted loops (`doloop_end`), modulo scheduling with register copies
  (GCC's SMS), then E2K loop mode and rotating registers, then APB.
  Section 7.7, which notes that APB needs only a counted loop-mode loop
  and could come right after `doloop_end`.

### M8. C++ and the other runtimes

- `--enable-languages=c,c++`, libstdc++ (or use lcc's `libstdc++.so.6`
  first), C++ exceptions via the E2K unwinder (3.3); the compat suite's
  `eh` unit with `--cxx gcc='...'`.
- libatomic, libgomp (generic Linux fallbacks first).

---

## 6. Design decisions

The decisions you must make early, with a recommendation each. Facts are
from `spec/`; the GCC mechanisms are explained in section 8.

### D1. Registers and windows

**Hard register numbering** (you choose it; it is internal to GCC):

| Range | Registers |
|---|---|
| 0–31 | `%g0–%g31` |
| 32–95 | `%r0–%r63` (window, "locals and incoming args") |
| 96–103 | outgoing argument/result registers, printed as `%b[0]`–`%b[7]` (D1 below) |
| 104–135 | `%pred0–31` |
| 136–138 | `%ctpr1–3` |
| then | fake registers: frame pointer, argument pointer (needed by GCC's frame elimination) |

**Incoming vs outgoing**: the callee sees its arguments in `%r0–7`; the
caller writes them into its own registers that become the callee's `%r0`.
GCC handles this exactly as on SPARC and ia64: `TARGET_FUNCTION_ARG`
returns the outgoing registers, `TARGET_FUNCTION_INCOMING_ARG` the
incoming ones, and `INCOMING_REGNO`/`OUTGOING_REGNO` map between them
(ia64.h:406–415).

**Recommended trick, from lcc**: make the outgoing registers a separate
set of 8 hard registers printed as `%b[0..7]`, and in the prologue
`setbn rbs=W, rsz=3` with `W` = number of pairs used by locals, and call
with `wbs=W`. Then `%b[i]` is the i-th outgoing register *whatever the
window size*, so nothing needs renaming after register allocation.
(ia64 instead renames registers in its printed output, `ia64.cc:3566`;
that is not needed here.) Results come back in the same `%b[0..7]`.
Caveat: rotating loops (M7) also want `%b[]`. lcc keeps `%b[0..7]` as
the outgoing registers and, around a pipelined loop, grows the window
with a second `setwd` and a ring starting at the outgoing pairs, then
shrinks it back before any call; the proof of concept instead puts the
ring between the locals and the outgoing pairs (spec/isa/loops.md "For a
compiler"). Either way the ring must not be above the outgoing pairs at
a call.

**Static chain** (for nested functions and trampolines): use `%g12`,
which MCST's libffi already uses for its closures
(`spec/abi/registers.md`). lcc supports neither nested functions nor
`__builtin_setjmp`/`__builtin_longjmp`/`__builtin_apply`, so nothing
depends on them and they can wait (`spec/abi/source-compat.md`). When
they come, nonlocal `goto` and `__builtin_longjmp` can re-enter the
target frame through glibc: the receiver calls `_setjmp` on a `jmp_buf`
in its frame, the jump writes the label's address over the saved IP
(byte 8) and calls `longjmp`; the frame resumes at the label with the
registers of its latest call (`spec/abi/stack-and-unwinding.md`,
checked on the 4C; the proof of concept implements both this way).

**Call-clobbered**: globals (except `%g13`, fixed; `%g12` as the static chain), the outgoing
registers, `%ctpr1–3`, `%lsr`. Window registers `%r*` are preserved by
hardware: mark them call-saved, and the prologue never saves them.
Predicates are also preserved (each call gets a fresh predicate file).

**Register allocation order**: prefer low window registers so the window
stays small (`REG_ALLOC_ORDER`; ia64.h:446 for the same idea).
`LOCAL_REGNO` (the window registers) alone is not enough: IRA exempts
such registers from the prologue save cost (`calculate_saved_nregs`,
ira-color.cc:1818), but LRA's `find_hard_regno_for_1` (lra-assigns.cc,
the `df_regs_ever_live_p` check) and regrename's `check_new_reg_p`
(regrename.cc:352) treat a not yet used call-saved register as needing a
save, so they prefer call-clobbered globals and the outgoing `%b[0..7]`:
after unrolling, the proof of concept found a leaf daxpy's values in
`%b[]` while `%r4`–`%r63` stayed unused, and values in `%b[]` block a
pipelined loop. It exempts `LOCAL_REGNO` in both places and keeps
regrename out of `%b[]` (`HARD_REGNO_RENAME_OK`); making `%b[0..7]`
fixed instead broke vector results returned in `%b[0]` (pr123753,
pr86363 at `-Os`). Under real pressure (a full window) allocation still
uses `%b[]`.

### D2. Window size

Like ia64's `ia64_compute_frame_size` (ia64.cc:2691): after register
allocation, find the highest window register ever live
(`df_regs_ever_live_p`), add the outgoing area, round to pairs, and emit
`setwd wsz=...` (plus `setbn`) as an `unspec_volatile` insn that must be
first (ia64's `alloc` pattern, ia64.md:4734, attribute `first_insn`).
Leaf functions without calls need no outgoing area. Varargs functions
need their incoming registers (the callee stores them, or reads the
caller's memory copy: see `calling-convention.md`, variadic section, for
which arguments are in memory).

### D3. Predicates

One-bit predicate registers in `BImode`, class `PRED_REGS`. Compares are
patterns setting a `BImode` register; branches and conditional
operations test it. Unlike ia64 (which pairs predicates, ia64.cc:4294),
E2K predicates are single registers, closer to C6x/ARM. A
`define_cond_exec` makes most patterns predicable automatically
(md.texi "Conditional Execution", ia64.md:5144). Note: GCC's if-converter
only creates `cond_exec` after register allocation (ifcvt.cc:5724),
which suits a post-RA scheduler.

Predicate *logic* (`andp`, `landp` in PLS syllables, "½ cycle") and the
`%pcnt`/`%LOOP_END` conditions come later. `landp` is the one to use for
`&&` chains: its second operand may be a diagnostic predicate when the
first is false (`spec/isa/semantics.md`, "Predicate logic"), and CLPs
chain two levels at most in one instruction (bundle.md), so four
conditions are a tree.

What `define_cond_exec` takes in practice (proof-of-concept back end,
P20; GCC behaviour checked in the source): gensupport puts the
cond_exec template *in front of* the insn's (`alter_output_for_insn`,
gensupport.cc:2128), while E2K writes the condition after the operands
(`addd a, b, c ? ~%pred3`), so every predicable template ends in a
punctuation code that prints the condition for a `COND_EXEC` and nothing
otherwise (`*` C templates are left alone); per-alternative `predicable`
(to keep loads and stores out) needs an `enabled` attribute, which
gensupport combines with its `ce_enabled` (md.texi); sched-deps drops
the output dependence between two writes of one register under
opposite predicates (`sched_insns_conditions_mutex_p`) and puts both in
one wide instruction, which the hardware accepts. Post-reload
if-conversion (ce3) is the only user. The gain is small (zlib and xz
4%, CoreMark none, `MAX_CONDITIONAL_EXECUTE` 6), and under an FP
compare's predicate it can lose (the compare's latency joins a loop's
recurrence): lcc's lead on branchy code comes from whole loops turned
into predicated blocks and pipelined (7.7, "Branchy loops"), not from
diamonds.

### D4. Control transfers

| Stage | Jumps | Calls | Returns |
|---|---|---|---|
| M1 | `ibranch label [? %predN]` | — | `return %ctpr3` + `ct %ctpr3` in one pattern |
| M2 | same | one pattern printing `disp %ctpr1, f` and `call %ctpr1, wbs=W` | same |
| M5 | prepare (`disp %ctprN`) and transfer (`ct %ctprN [? pred]`) as separate insns; `%ctpr1–3` an allocatable class; scheduler latency 2 between them (below) | same, prepare hoisted | `return` hoisted 6 cycles |

Interlocks make the M1/M2 forms correct (the hardware waits for the
prepared target, guide 4.4 "неподготовленный переход"); M5 makes them
fast. The same holds for the compare that produces a branch's predicate
(interlocked, latency 3, `spec/isa/timing.md`). C6x's "real insn + shadow" pairs
tied with `record_delay_slot_pair` (c6x.cc:5118, haifa-sched.cc:730) are
the closest existing model of "these two must be N cycles apart", but
for E2K a plain dependence between the `disp` insn and the `ct` insn
(via the CTPR register) is enough, since early is fine. Give it latency
**2**, not 5: a `ct` that is not taken waits only until 2 cycles after
its `disp`, and a taken one `5 − D`, still less than an `ibranch`
(`control-transfer.md`, "Costs"); latency 5 stretches every short block,
the fall-through path included. Prepare **every** branch whose block has
a CTPR free after the jump, taking the CTPRs in turn so each `disp` can
move above the transfer before it, and add an anti dependence of at
least 1 from a transfer to a later `disp` of the same CTPR (gas rejects
both in one instruction before v6). In the proof-of-concept back end
(P15) that took taken-`ibranch` cycles in CoreMark from 171 M to about
4 M (fetch bubbles 16.3% → 3.4% of cycles; CoreMark +14.9%, Linpack
−4.7% time on the 8C2), with no profile. One trap: a predicated op or
`merge` in the wide instruction of a taken **return** reads a predicate
written one instruction earlier as it was before the compare, without
a stall (timing.md, "Hazards"; branches and calls wait), so the bundler
must keep 2 cycles there and not rely on the interlock.

Indirect calls: `movtd %reg, %ctprN` (latency 9) then `call`.

**Conditional returns** (`ct %ctpr3 ? p`, lcc's early exits from leaf
functions, bzip2's `mainGtU`; the proof-of-concept back end, P27,
bzip2 −9 2.6% faster) need three things from the port: (1) GCC accepts
a conditional return only as a plain `(set (pc) (if_then_else c (return)
(pc)))` (`condjump_p`, jump.cc:789; cfgrtl's "return not followed by
barrier"), so the pattern can't carry `(use %ctpr3)`: keep `%ctpr3` live
to the end through `EPILOGUE_USES` in functions that prepare it at
entry (`df_update_exit_block_uses` once known); (2) sched-deps gives
registers live at a jump's target a latency-0 anti dependence, which put
`return %ctpr3` and the conditional `ct` in one instruction, which gas
rejects before v6 (control-transfer.md, "Restrictions"): add a true
dependence; (3) what a return block computes, copied above the branch,
runs on the other path too and must be speculative, and a value written
under the condition into a register still live on the other path needs
its dependence on later reads dropped, or every exit waits for the one
before.

Two hardware facts shape this (`spec/isa/control-transfer.md`, measured
on the 4C):

- **Every `call` and every return empties all three CTPRs**, and a taken
  `ct` through an empty CTPR traps (SIGILL). Model `call` as clobbering
  `%ctpr1–3` (they are call-used anyway, registers.md) and never keep a
  prepared value live across a call; the return must be prepared again
  after the last call. A `ct` with a false condition never traps, so
  prepares may be speculative and hoisted above the condition.
- **A taken `ibranch` costs 6 cycles; a taken `ct` prepared in time costs
  nothing**, calls and returns included. The M1 `ibranch` form is
  correct but slow for loops and likely-taken branches, which is why M5
  matters; lcc uses `ibranch` almost never.

### D5. Literals and constants

Operands can be 5-bit literals in the syllable or 16/32/64-bit literals
in LTS slots (max 4 × 32 bits per wide instruction). In M0–M3 each
wide instruction holds one op, so literals never run out. From M4 the DFA
must count literal slots as a resource (section 7.2).

### D6. Condition codes

E2K has no flags register: compares write predicates. Use the `BImode`
style described in tm.texi.in "Condition Code" (:4433): `cbranch<mode>4`
expands to a compare into a `BImode` pseudo and a branch on it. CC0 no
longer exists in GCC (removed 2021).

### D7. `setwd dbl`

`dbl` is a per-function flag set by `setwd` (v3 and later) that decides
what a 32-bit integer operation does to the upper half of its 64-bit
destination register (measured on the 4C, `spec/abi/registers.md`):

| | `dbl = 0` (lcc's default) | `dbl = 1` (Unipro; lcc in some functions) |
|---|---|---|
| `adds`, `shls`, `muls`, other 32-bit integer ALU ops | write the low 32 bits only; the upper half (and its tag) stays as it was | zero-extend into all 64 bits |
| loads (`ldw` etc.), `sxt`, 64-bit ops | whole register | whole register |
| 32-bit float ops (`fadds`), `movts` | low half only | low half only |

lcc emits `dbl = 0` at `-O0` and in most functions, which is also how
pre-v3 CPUs behave. But at `-O2` and above it sets `dbl = 1` in
functions whose unsigned 32-bit results must be zero-extended (about
18% of functions in GCC's `gcc.c-torture/execute` tests, lcc 1.29.16
and 1.31.05; `registers.md`): `unsigned addu (unsigned a, unsigned b)`
becomes a bare `adds` under `dbl = 1`, while the signed version is
`adds` + `sxt` under `dbl = 0`.

**Decision (owner): choose `dbl` per function, as lcc does**: `dbl = 1`
where the function needs unsigned 32-bit results zero-extended, `dbl = 0`
otherwise. The flag is saved per frame, so it never leaks between
functions, and the ABI is the same either way. How to get there:

- Start with `dbl = 0` everywhere (it is always correct with the rules
  below); add the per-function choice as an optimisation once the port
  works.
- The choice has to be made before instruction selection relies on it,
  e.g. right after expand, by looking for zero extensions of 32-bit
  results, and kept in the function's `machine_function`. Patterns can
  then test it: under `dbl = 1` a zero extension of a 32-bit integer
  operation's result is free.
- Under `dbl = 0`, after a 32-bit integer operation the upper half of the
  register is unknown (old value and tag). Don't tell GCC that SImode
  operations extend; emit `sxt` (or a 64-bit operation) whenever a 64-bit
  value is needed, as lcc does. That alone is not enough: GCC forms
  **paradoxical subregs** by itself, which read the upper half anyway
  (combine's `force_to_mode`, combine.cc:8754–9024, turns a masked
  `(zero_extend:DI (subreg:HI (ashift:SI x 1)))` into
  `(and:DI (ashift:DI (subreg:DI (reg:SI) 0) 1) 0x1fe)`; rtl.texi says the
  upper bits "may or may not be defined"). On E2K they may hold a
  diagnostic tag left by any dead `,sm` result, and the 64-bit operation
  traps (the proof-of-concept back end, P12: CoreMark's `crc16`, SIGILL
  on the 8C2). Reject a paradoxical subreg of a value of 4 bytes or less
  in the operand predicates (the move source included) and zero-extend
  such a source in the move expander.
- With `dbl = 1`, give every integer ALU pattern an extending twin,
  `(zero_extend:DI (op:SI …))`: the whole group extends, `merges` and the
  bit counts included (`registers.md`). Combine then folds an extension
  whose operand has no other use; for the rest enable REE (`-free`, which
  each target turns on itself, e.g. i386-common.cc:2057), which
  eliminates more (when the destination differs it leaves a copy) but
  treats a 32-bit `if_then_else` as a conditional copy and looks through
  it; the proof-of-concept back end (P21) adds a small pass after REE
  that rewrites an extending operation earlier in the block to set the
  64-bit register (zlib's deflate.c: 149 extensions at `-O2`, 90 with REE,
  74 with the pass; lcc 7).
- Under either setting 32-bit FP operations (`fadds`) and `movts` keep
  the upper half, and signed values still need `sxt`.
- Partly written registers can keep a diagnostic tag in the upper half,
  and storing such a register with `std` traps (`calling-convention.md`,
  "Unused parts of a slot", confirmed on the 4C). Spill and pass 32-bit
  values with `stw`, or extend them first; `std,sm` does not trap on the
  4C. lcc also zero-initialises locals that may be read uninitialised
  (`source-compat.md`).

The ABI still requires callers to **sign**-extend signed narrow
arguments (emit `sxt`), and callees must not rely on the upper bits they
receive (lcc re-extends).

### D8. Floating point

Same registers as integers (no separate FP class). `long double`
(`XFmode`) lives in one register: mantissa in the ordinary 64 bits, sign
and exponent in the 16-bit extended part (`movif`/`movfi` pack and
unpack it; loads and stores are `ldd`+`ldh`, `std`+`sth`). A function
that keeps one in a window register across a call must set `nfx=0`, or
the extended part is lost when the window spills (measured on the 4C);
lcc sets `nfx=0` in every function that uses `long double`. 80-bit
operations follow `%fpcr`/`%fpsr` (x87 control and status words), not
`%pfpfr`, so rounding-mode patterns and `fenv` support must handle both
(`spec/abi/registers.md`, "Floating-point control and status", which
also gives the bit layouts). `TARGET_ATOMIC_ASSIGN_EXPAND_FENV` (C11
`_Atomic float x; x += y` must drop the exceptions of failed CAS
iterations) has to hold, clear and update both pairs, as glibc's
`feholdexcept`/`feupdateenv` do. Cross-domain and cross-cluster latency
penalties (FP→integer +2, crossing between ALC0–2 and ALC3–5 +1 or +2,
`spec/isa/timing.md`) go into the DFA's bypasses, not into register
classes.

The one-register model above is what lcc does, and it is what inline
asm's `x` constraint and `%xgN` register variables name
(`spec/abi/source-compat.md`). The proof-of-concept backend instead kept
`long double` in a pair of ordinary registers (mantissa, exponent),
packing with `movif` before and unpacking with `movfi` after each
80-bit operation: no `nfx = 0`, ordinary 64-bit spills, and results
bit-identical to lcc on the 4C, but `x` operands then need an extended
register assembled before the asm and split after it. Pick one before
M3; lcc's model costs `nfx = 0` and two-part spills (`std` + `sth` of
`movfi`'s result), the pair model costs a `movif`/`movfi` (4 cycles)
around every operation.

### D9. Assembly output conventions

Owner's decision: emit what lcc emits.

- `.ignore ld_st_style` and `.ignore strict_delay` at the top of every
  file; the compiler inserts every delay itself (`nop N`, or independent
  work), including the 4 cycles between `rwd %lsr` and `rrd %lsr`.
- Wrap each user `asm` in `.accept strict_delay` … `.ignore
  strict_delay` (lcc also adds `.ias_line N, "file"`), so gas pads
  hand-written code that is not bundled.
- Write `%empty` (gas), not `%empty.hi` (Unipro).

### D10. What GCC needs that its defaults get wrong

Found by the proof-of-concept backend (`docs/spec-poc-findings.md`),
each one cost it a debugging session:

- **`DEFAULT_PCC_STRUCT_RETURN` 0**, or every struct is returned in
  memory and all struct results break.
- **`PROMOTE_MODE` must agree with `TARGET_PROMOTE_FUNCTION_MODE`**
  (narrow integers to `DImode` in both), or expand asserts on call
  results. `TARGET_SUPPORTS_WIDE_INT` 1 (16-byte `__int128` constants).
- **Narrow arguments:** the ABI wants callers to extend and callees to
  extend again, but GCC asks one hook (`promote_function_mode`,
  `for_return` 0) for both sides. The proof of concept answers
  "promoted" except while the current function's own parameters are being
  laid out (`INIT_CUMULATIVE_INCOMING_ARGS` to `function_arg_advance`), so
  GCC re-extends the incoming register.
- **Division traps** (SIGFPE on divide by zero, `spec/isa/semantics.md`),
  but RTL PRE hoists a plain `(set (reg) (div ...))` out of a loop above
  a call that exits, because it refuses to move only insns that clobber a
  hard register (`pr38819` at `-fPIC` on the 4C). The proof of concept
  makes its divide patterns clobber `%ctpr2`. The E2K way to hoist a
  division safely is the speculative form (`sdivs,sm`).
- **Results over 64 bytes** are written by the callee at its entry stack
  pointer, with no hidden pointer, a convention GCC doesn't have. What
  worked: `TARGET_STRUCT_VALUE_RTX` gives the callee the address of a
  local buffer (behind an opaque copy, or `dse1` deletes the stores into
  it) and the epilogue copies the buffer to the entry USD, which also
  avoids the overlap with memory arguments; for the caller it returns a
  global register, and the `call` expander (operand 3 is the result
  size) copies the result from the caller's USD after the call.
  `REG_PARM_STACK_SPACE` grows to the result size so the caller reserves
  enough; `STACK_PARMS_IN_REG_PARM_AREA` keeps stack arguments at their
  slot offsets. A decision for you before M2.
- **PIC jump tables:** GCC emits them as offsets from the table
  (`ADDR_DIFF_VEC`) and leaves adding the table address to the port's
  `tablejump` expander.
- Facts from `spec/isa/semantics.md` that are GCC settings:
  `SHIFT_COUNT_TRUNCATED` 1; `fmul_add*` are not fused (only with
  `-ffp-contract=fast`, never `fma`); `fmax`/`fmin` are x86-style, not C's;
  FP `lt`/`le` signal on quiet NaNs and `eq`/`uod` don't; integer compares
  have no `ne`/`gt`/`ge` forms.
- GCC 17 defaults to C23, where `int f();` takes no arguments: run the
  compat suite's `unproto` unit with `-std=gnu17` in the `--cc` command.
- **FP compare codes:** LTGT must raise invalid on a quiet NaN (PR91323:
  `a < b || a > b` folds to it), UNEQ and the other UN* codes must not,
  so LTGT takes the signaling `fcmpod*f` and UNEQ the quiet `fcmpud*f`
  (`spec/isa/semantics.md`). But without a CC mode,
  `reversed_comparison_code_parts` (`jump.cc`) reverses UNEQ to LTGT
  and back, so an FP compare stored to a register can come out of
  combine with the other code. Either keep the distinction in a CC
  mode, as CC-mode ports do, or lower UNLT/UNLE/UNGT/UNGE/UNEQ/LTGT at
  expand time to `fcmpud*f`/`fcmpod*f` into a register plus an integer
  test of its flags, as the proof of concept does (then
  gcc.dg/torture/pr91323 passes).
- **`getsp` amounts** (alloca, VLAs) must be in a window register or
  `%g16`–`%g31`, never `%g0`–`%g15`: the kernel cannot read those when
  the stack must grow, and the program gets SIGSEGV, but only for
  allocations past the mapped stack (`spec/abi/stack-and-unwinding.md`;
  the proof of concept's 800 KB VLA). lcc uses `%g24`. Give the
  `allocate_stack` pattern a window-register constraint.
- **Short label-difference tables** crash MCST gas when a `.text`
  label follows them (`spec/isa/bundle.md`, "Known gas bugs"): keep
  `CASE_VECTOR_SHORTEN_MODE` at `SImode` or wider.
- **A reload right after its store** (a spill and its fill, a stack
  argument read back) in the very next instruction gets its value about
  8 cycles late; one cycle of distance removes it. Matching is per
  aligned doubleword (`spec/isa/timing.md`, "Memory"); give the
  scheduler that dependence a latency of 2 rather than 1.
- **Memory accesses in one wide instruction** are ordered (ALC2 before
  ALC5, a load before a store in its cluster; `spec/isa/bundle.md`): a
  load in ALC5 sees a store in ALC2 of its own instruction, and of two
  stores to one address the ALC5 one wins. So two accesses that may
  alias must not share an instruction, except a load and a later store
  in a pairing where the load reads the old value. Anti dependences
  can't be trusted for this: once 32 memory references are pending
  (`param_max_pending_list_length`), sched-deps flushes its lists and
  links a later load to the earlier stores with `REG_DEP_ANTI`
  (`flush_pending_lists`, `sched-deps.cc`), although the load must see
  them. A bundler that lets anti-dependent insns share an instruction
  put such a load beside its store in the proof of concept (a
  miscompiled vector shuffle).

---

## 7. Scheduling for a VLIW/EPIC machine

### 7.1 Concepts

- **Dependence graph**: for each block, an edge from an instruction that
  produces a value to each one that uses it, labelled with the latency.
  GCC builds it (`sched-deps.cc`).
- **List scheduling**: fill cycle 0 with ready instructions (all inputs
  available), as many as the machine can issue; advance to cycle 1; and so
  on, choosing by priority (longest path to the end). GCC's "Haifa"
  scheduler (`haifa-sched.cc`) does this.
- **Resource model**: "can this instruction issue in this cycle given
  what is already there?" GCC answers with a finite automaton generated
  from your pipeline description (the DFA, `genautomata.cc`).
- **Bundles**: on E2K one cycle = one wide instruction. The scheduler marks
  the first instruction of each cycle with `TImode` (haifa-sched.cc:4193,
  after reload only, and only when `issue_rate > 1`). The port turns those
  marks into `{ }`.
- **EPIC features** — predication, speculation, rotating registers — are
  ways to give the scheduler more independent instructions: across
  branches (speculation), instead of branches (predication), across loop
  iterations (software pipelining).

### 7.2 The pipeline description

Written in `.md` with `define_automaton`, `define_cpu_unit`,
`define_insn_reservation`, `define_bypass`, `exclusion_set` (md.texi node
"Processor pipeline description", :10971; read it fully). For E2K:

- **Units**: `alc0`…`alc5` (channels), `lit0`…`lit3` (32-bit literal
  slots; a 64-bit literal takes two), `ct` (control transfer), `ctp`
  (preparation), `pl0`…`pl2` (predicate logic), plus the store/load pair
  restrictions from `bundle.md` "Bundling restrictions" as
  `exclusion_set`s.
- **Reservations**: one per class of operation, e.g. "simple integer op in
  any of ALC0–5", "load in ALC0, 2, 3, 5", "store in ALC2 or 5", "divide in
  ALC5", "128-bit op in a channel pair". Which channels each operation may
  use, per ISA version, is in `spec/isa/binutils-ops.yaml` (`channels`).
  That table is large and mechanical: generate the reservations from it
  with a script in the port, as C6x does with `gensched.sh`
  (`c6x/t-c6x:21–37`).
- **Latencies** (`define_insn_reservation` latency and `define_bypass`):
  use the measured table in `spec/isa/timing.md` (data in
  `spec/isa/timing-e2s.yaml`, from `tools/isa-timing/bench.py`): integer
  1, combined integer 2, FP and integer multiply 4, combined FP 8,
  `fdivs`/`fdivd` 11/14, `fsqrts` 12, `sdiv` 11, load 3 (L1 hit),
  compare→predicated op 2, compare→`ct` 3, `disp`→`ct` 5, `return`→`ct` 6,
  `movtd`→`ct` 9, FP or multiply → integer +2. Crossing between the
  channel clusters ALC0–2 and ALC3–5 costs +1 for integer and load
  results and +2 for FP-class ones (the guide's "integer → FP +1" is this
  crossing); model it with `define_bypass` or by tracking the cluster.
  These match guide 6.1. Where the guide is wrong or silent: packed
  integer ops take 1 cycle (multiply 2) to packed/FP consumers and 4 to
  integer ones; conversions 4 (6 to integer), but an FP-side result
  read in another format than it was produced in costs 5 or 6 (a
  double read as a single, a single as a double: timing.md, "Formats";
  a `define_bypass` per format pair); FP compare → predicated op
  4, → `ct` 5; `andp` chains inside one wide instruction (0) and costs 1
  through a `%pred`, 2 to a predicated op and 3 to `ct`. Schedule loads
  for an L1 hit, as lcc does. A stall
  costs about a whole latency extra, so latencies must be exact. v6 and
  later differ (e.g. load→use 5, binutils comment in `bfd/elfxx-e2k.c`
  `build_plt_entry`) and still need measuring.
- **Size**: keep automata small by splitting independent resources into
  separate `define_automaton`s (ALU channels, literals, control); C6x
  uses 7 automata with `ndfa`, `collapse-ndfa`, `no-comb-vect`
  (c6x.md:267). `automata_option "v"` writes a `.dfa` file showing the
  generated automaton.
- **Use `(automata_option "ndfa")`**, as ia64 (ia64.md:204) and C6x
  do: it is needed for correctness of the packing, not only size. The
  channel exclusions (a load in ALC3 with a store in ALC2, a load in
  ALC0 with a store in ALC5; bundle.md) make a deterministic automaton
  reject wide instructions that fit, because it commits to the first
  free alternative: two loads take ALC0 and ALC2 and the store has
  nowhere to go, while loads in ALC0 and ALC5 with the store in ALC2
  fit with three more operations. With `ndfa` the proof of concept's
  daxpy kernel fits one wide instruction (`docs/spec-poc-findings.md`
  P8).
- Declare the channel units with `define_query_cpu_unit` if you want to
  read the chosen channel back (7.5); that also needs
  `#define CPU_UNITS_QUERY 1` in `e2k.h`.

### 7.3 Where scheduling runs

GCC normally runs `sched1` before register allocation and `sched2`
after. VLIW ports instead run the final schedule themselves inside
machine reorg, so nothing changes the order between scheduling and
output:

- Set `TARGET_DELAY_SCHED2` to `true` (stops the normal sched2,
  sched-rgn.cc:3931) and `TARGET_DELAY_VARTRACK` to `true`.
- In your `TARGET_MACHINE_DEPENDENT_REORG`: `compute_bb_for_insn`,
  `df_analyze`, split insns, then `schedule_ebbs ()` (extended basic
  blocks: scheduling across fall-through edges), then bundle, then run
  var-tracking. Models: `tilegx_reorg` (gcc-tilegx tilegx.cc:4847),
  `ia64_reorg` (ia64.cc:9830), `c6x_reorg` (c6x.cc:5879), `bfin_reorg`.
- Hooks to define first: `TARGET_SCHED_ISSUE_RATE` (number of ops per wide
  instruction you allow, e.g. 6 for ALU ops; must be constant),
  `TARGET_SCHED_ADJUST_COST` (special latencies), and
  `TARGET_SCHED_FIRST_CYCLE_MULTIPASS_DFA_LOOKAHEAD` (tries orderings of
  ready instructions to pack wide instructions better; ia64 returns 6).
  `TARGET_SCHED_DFA_NEW_CYCLE` lets you force a new wide instruction
  (e.g. after a call). All hooks: `gcc/target.def:934–1620`.
- Turn scheduling on at `-O1` in `common/config/e2k/e2k-common.cc` (TILE-Gx
  does, `common/config/tilegx/tilegx-common.cc:31`).
- **Operations in one cycle read the old values.** Every operation in a
  wide instruction reads the registers as they were before it, but Haifa
  by default treats insns issued in one cycle as ordered: its
  dependence-breaking transformation moved `std %r1, 0, %r0` after `addd
  %r1, 8, %r1` *in the same cycle* and rewrote the address to `%r1 - 8`,
  which is wrong in a bundle (found by the proof-of-concept backend,
  `gcc.c-torture/execute/loop-2e` at -O1). Set
  `TARGET_SCHED_EXPOSED_PIPELINE` to true (replacements then apply from
  the next cycle), and make output dependences cost at least 1 in
  `TARGET_SCHED_ADJUST_COST` (two writes of one register can't share a
  wide instruction).
- **A taken transfer's whole wide instruction runs.** On ia64 and C6x a
  taken branch skips the rest of its group; on E2K every operation of
  the instruction holding a taken `ct` executes (control-transfer.md;
  `preds.c` shows an `addd` in a taken branch's instruction taking
  effect). Haifa gives a jump's dependences on later insns latency 0
  ("Make latency of jump equal to 0 by using anti-dependence",
  sched-deps.cc:3058), so an insn that must not run on the taken path
  (one setting a register live at the target, a store, anything without
  a speculative form) can land in the jump's cycle. In
  `TARGET_SCHED_ADJUST_COST` make a dependence on a call, or on a jump
  whose consumer has no speculative form or sets a register live at the
  jump's target, cost at least 1; print `,sm` on whatever still shares a
  jump's cycle after it, and on whatever changed blocks, judged by
  block identity, not by positions recorded before scheduling (the
  proof-of-concept back end, P23: zstd's fuzzer and GMP crashed until
  both were fixed; CoreMark −0.75% against the wrong code).

### 7.4 Printing wide instructions

Copy TILE-Gx (gcc-tilegx), which prints the same `{ }` syntax:

1. After scheduling, walk each block (`tilegx_gen_bundles`, tilegx.cc:4453):
   mark an insn "last in its wide instruction" if the next insn starts a
   new cycle (`TImode`), or it is a call; mark the others "inside". Delete
   empty marker insns (blockages).
2. In `FINAL_PRESCAN_INSN` remember the current insn's mark; in
   `ASM_OUTPUT_OPCODE` print `{` before the first insn of a group and `}`
   after the last (`tilegx_asm_output_opcode`, tilegx.cc:5463).
3. Move var-location debug notes out of the middle of groups
   (`reorder_var_tracking_notes`, tilegx.cc:4796).
4. Known pitfalls TILE-Gx handled: a call must end its wide instruction;
   inline `asm` is never put in braces; the profiler call prints its own
   group.

For E2K add the `nop N` field: when consecutive groups are scheduled
more than one cycle apart, print `nop N` (N = gap − 1, maximum 7) inside
the earlier group. C6x computes the same gaps (`reorg_emit_nops`,
c6x.cc:4989), though it emits separate nop instructions because its
pipeline is not interlocked.

Because a stall costs about a whole latency extra (`spec/isa/timing.md`),
`nop N` matters for speed even though the hardware interlocks: within a
block the scheduler already keeps producers and consumers apart by the
DFA latency, but across block boundaries it does not, so a final pass
should check distances and add `nop`s. ia64's write tracker (`rws_sum`,
`group_barrier_needed`, ia64.cc:6255–7096) and C6x's `reorg_emit_nops`
(which pads to `earliest_bb_end`) are models for that pass.

For correctness, the only non-interlocked hazard measured on the 4C is
`rwd %lsr` → `rrd %lsr` (4 cycles); loop-mode code (7.7) must respect it.
Privileged state registers (which gas pads) don't appear in user code.

If later passes could insert labels inside a group, wrap each group in a
`SEQUENCE` as C6x does (`c6x_gen_bundles`, c6x.cc:4622).

### 7.5 Explicit channels (needed for speed)

gas chooses channels, so the port need not for correctness, but it must
for speed: gas gives each operation the first free channel it allows,
so a consumer scheduled one cycle after its producer often lands in the
other cluster and stalls a 4-cycle step (timing.md, "Clusters"), and
the crossing costs in the latencies can only be used once the port knows
the channels. The proof-of-concept back end (P14) prints every
operation's channel (`addd,3`, as lcc does), choosing per wide
instruction the assignment, among the v3 channel sets and gas's
store/load exclusions, with the fewest stall cycles given where each
operand's producer went: interlock stalls in CoreMark 6.0% → 3.4% of
cycles on the 4C (CoreMark +2.8%, Linpack −1% time on the 8C2). Make it
a step of M5. If gas ever rejects a group
the DFA accepted (the DFA modelled a channel rule too loosely), either fix
the DFA or print channels yourself the way C6x prints units: record after
each issue which `alc` units became reserved (`c6x_variable_issue`,
c6x.cc:4350, using `cpu_unit_reservation_p`), then assign the most
constrained instructions first (`assign_reservations`, c6x.cc:3029) and
print `,N` with a punctuation code in the output templates.

### 7.6 Speculation

The machine side is in `spec/isa/speculation.md` (measured on the 4C).
What it means for GCC, from the cheapest step to the hardest:

**1. Speculative forms of instructions.** Every ALU, FP and load pattern
needs a variant with `,sm`. Wrap them the way ia64 does (`unspec`-wrapped
speculative loads, ia64.md:440–708), or give the main patterns an
attribute that the output template turns into `,sm`. Stores, divisions
in the default mode, branches and anything with a side effect never get
one.

**2. What may be speculated without a check.** E2K needs no check
instruction for control speculation: a faulting speculative operation
gives a diagnostic value, and the program fails only if that value
reaches a non-speculative use, which happens only on the path where the
original operation would have run and faulted anyway. So the port can
treat a speculative load as **not trapping**: in GCC's terms, a moved
load needs no `BEGIN_CONTROL` check. The exceptions, all measured:

- Loads: use the semi-speculative form (`ld,sm`, MAS mod 0, lcc's
  default). It returns real data from valid pages the process hasn't
  touched (the kernel pages them in), so its result is always usable
  when the guard holds. The fully speculative form (mod 3) can return a
  diagnostic for such a page and then needs recovery code (item 5);
  leave it out at first.
  Cost to keep in mind: the first speculative load into each invalid
  region traps into the kernel once (about 5,000 cycles), and a load
  from a valid page not touched yet is paged in even if never used.
- `volatile` and atomic accesses, and loads with a MAS (lock loads):
  never speculated.
- FP operations: a speculative FP operation that would raise a flag
  not already set, or an exception that is unmasked, gives a diagnostic
  instead. That is harmless while the flags are sticky-set and all
  exceptions masked (the kernel's start state) but turns into SIGILL
  after `feclearexcept` or `feenableexcept` (instead of SIGFPE in the
  second case). Speculate FP only under `-fno-trapping-math`, and then
  only if we accept that hazard (to be decided; lcc's `-fspec-fp` is on
  by default and lcc speculates FP multiplications in Linpack, but kept
  FP divisions under their branch in our probes). A checked form is
  possible (item 5): a speculative operation reading the result, with
  `rbranch ? %dt_alN`, catches the diagnostic.
- Integer division: `sdivd,sm` by zero gives a diagnostic; safe to
  speculate as such, but lcc doesn't, and it is rarely worth it (11+
  cycles).
- The signal changes: a program that faults in speculated code gets
  SIGILL at the first non-speculative use instead of SIGSEGV/SIGBUS/
  SIGFPE. Tests that catch those signals must be built without
  speculation (lcc: `-fno-semi-spec-ld`, `-fno-spec-fp`).

**3. Where GCC can use it.**

- *If-conversion* (`ifcvt.cc`) is the biggest win: E2K code is mostly
  if-converted (lcc's `-O3` CoreMark issues 2.5 operations per cycle,
  Unipro's clang 1). GCC's `noce_*` paths only convert a branch arm that
  `may_trap_or_fault_p` says cannot trap. Making that answer "no" for
  E2K loads that will be emitted speculatively needs either a GCC
  change (a target hook saying "this insn can be made non-trapping"
  plus a way to mark it `,sm`) or a port-specific if-conversion pass
  after ifcvt. With predication (`define_cond_exec`) the remaining
  stores and calls stay predicated, as lcc does.
- *Interblock scheduling* (`sched-rgn`, `-fsched-spec-load-dangerous`)
  moves loads above branches only when told they can't trap; the same
  "can be made non-trapping" answer plus emitting `,sm` on moved insns
  (`TARGET_SCHED_SPECULATE_INSN`) covers it. GCC's Haifa scheduler
  inserts checks for `BEGIN_CONTROL` speculation
  (`create_check_block_twin`, haifa-sched.cc:8347); E2K needs none, so
  the port should present moved loads as safe rather than as
  control-speculative. How, in sched-ebb (the proof-of-concept back end,
  P16; torture, zstd, GMP, pcre2 and expat suites pass on the 4C and the
  8C2):
  1. In `TARGET_SCHED_DEPENDENCIES_EVALUATION_HOOK` (called after
     `add_deps_for_risky_insns`, sched-ebb.cc:510) delete the jump → load
     dependences (from `deps_may_trap_p`, which decides by address,
     sched-deps.cc:505, and from the risky-insn pass) when the load's
     register is dead on the jump's other path. Don't use `MEM_NOTRAP_P`
     for this: GCC sets it on jump-table loads (`gen_const_mem` in
     `do_tablejump`, expr.cc:14348), which do trap above their range
     check.
  2. Record each insn's block before sched2 and print `,sm` on every
     insn that ended up in an earlier block.
  3. In the same hook, make every insn without a speculative form (FP,
     stores, multi-operation patterns) depend on the jumps before it.
  4. A data-speculation check is built as a copy of the load with the
     load's back dependences, which no longer include the jumps: make a
     load a lock load only once every jump it followed is scheduled, or
     the check rises above the test guarding its address.

  Haifa's own `BEGIN_CONTROL` path is dead code in GCC
  (P13): `haifa_note_dep` rebuilds the dependence with `init_dep`, which
  drops the speculative status (sched-deps.cc:1861, 113), and
  `sched_analyze_2` makes a load's jump dependence speculative only under
  sel-sched (sched-deps.cc:2690); with both fixed,
  `create_check_block_twin` copies the load's forward dependences to the
  check without their `DEP_REPLACE` (`init_dep_1`), turning a breakable
  dependence into a hard one (283 torture ICEs until skipped). Even
  working, a check holds every use of the load below the branch, so it
  isn't worth it here. **Renaming after register allocation** decides
  how much moves (P17): `list = list->next` in a search loop writes a
  register live on the exit path, and regrename, which renames whole
  def-use chains without splitting them or inserting copies, can't free
  it; a small pass that gives such a value, in a block entered only by
  falling through from a conditional jump, a register free in both
  successors (copied back where needed) did (CoreMark's `find` 18 → 12
  cycles a node, +8.1% on the 8C2). Make predicates `LOCAL_REGNO` first
  (as call-saved registers they are live at the function's end), honour
  a call's `CALL_INSN_FUNCTION_USAGE` (argument registers aren't in its
  pattern), and stop at a later use of the register as half of a pair.
- *Selective scheduling* (sel-sched) has a mode without checks,
  `SEL_SCHED_SPEC_DONT_CHECK_CONTROL` (sel-sched.cc:3208), used by ia64
  with `-msel-sched-dont-check-control-spec`; hooks as ia64's
  (`TARGET_SCHED_SET_SCHED_FLAGS` ia64.cc:8072,
  `TARGET_SCHED_SPECULATE_INSN` 8368, `TARGET_SCHED_GET_INSN_SPEC_DS`,
  `TARGET_SCHED_SKIP_RTX_P`, sched-context hooks). Enabled by ia64 at
  `-O3` only (ia64.cc:6182); maintained by ISP RAS but used by few
  targets, so expect bugs.

**4. Data speculation (DAM).** E2K's lock load + check load map onto
ia64's advanced load + check (`ld.a` + `ld.c`/`chk.a`):

| ia64 | E2K |
|---|---|
| `ld.a r` (advanced load, enters the ALAT) | `ldd,0,sm ..., %rN, mas=0x4` in ALC0/ALC3 (enters the DAM) |
| `ld.c r` (check: reload if the entry is gone) | `ldd,5 ..., %rN, mas=0x3` in ALC2/ALC5, same register and address |
| `chk.a r, recovery` (branch to recovery code) | the check plus `rbranch recovery` in the same wide instruction |
| `ld.sa r` (speculative advanced load) | `ldd,0,sm ..., mas=0x7` (fully speculative lock: a failed one just makes the check reload) |
| `ld.c.nc r` (check, entry kept) | `ldd,5 ..., mas=0x2` |
| `invala.e r` (drop one entry) | `std,2 ..., %rN, mas=0x5` (drops the entry of its data register, writes nothing) |
| ALAT invalidation by stores | DAM entry removed by any overlapping store (exact bytes, no false conflicts); a `mas=0x4` store skips the DAM |

GCC's Haifa scheduler already does `BEGIN_DATA` speculation for ia64
with these hooks: `TARGET_SCHED_NEEDS_BLOCK_P` (does a check need a
recovery block: only if consumers were moved above the check),
`TARGET_SCHED_GEN_SPEC_CHECK` (ia64.cc:8531–8571), and the
advanced-load/check patterns (ia64.md:440–708). Constraints the E2K
patterns must express:

- The check's destination register is the lock load's and nothing may
  write it in between (an overwrite keeps the entry, and the check then
  returns the overwritten value: silent wrong data). Model the pair as
  one register that is live from the lock load to the check.
- The check goes in ALC2 or ALC5 (it also occupies a store channel), the
  lock load in ALC0 or ALC3 **with `,sm`** (without it no entry is
  recorded and every check reloads), and `rbranch` in the check's
  instruction, always, as lcc does (a consumer right after a check
  without `rbranch` once saw the early value in the proof of concept).
  An intact check costs nothing, also for a use in the next
  instruction (measured); a failed one about 4 cycles plus the
  compensation code (the proof of concept's measurement).
- Keep stores out of a check's instruction: a check in ALC5 that
  reloads sees a store in ALC2 of the same instruction
  (`spec/isa/bundle.md`).
- The table holds exactly 32 entries and evicts the oldest; keep fewer
  pairs outstanding.
- Traps clear it (a failed check, so correct but slow), and so does a
  call deep enough to spill the register file; shallow calls don't, and
  entries are keyed by physical register, so clear it (`mmurw 0,
  %dam_inv`) on exit from a function that used it, as lcc does, and
  never let a check run without its own lock load on every path.

lcc pipelines loops with it (`-fswp-dam-cc`, default at `-O3`, about +5%
speed for 1–2% more code per its changelog). For GCC it comes after
control speculation and modulo scheduling (M6/M7). lcc 1.29.16 uses only
the `mas=0x4` lock and the `mas=0x3` check.

**5. Checked control speculation (`%dt_al`).** The fully speculative
load (`ld,sm ..., mas=0x3`) never enters the kernel, so it is what code
that must not page in memory speculatively uses (lcc's
`-fno-semi-spec-ld`, which the kernel is built with). It needs recovery,
and E2K has ia64's `ld.s` + `chk.s` in this form:

| ia64 | E2K |
|---|---|
| `ld.s r` (speculative load, NaT on failure) | `ldd,2,sm ..., %rN, mas=0x3` |
| `chk.s r, recovery` | a `,sm` operation reading `%rN` in ALC N (0, 1, 3 or 4), predicated on the guard, writing `%empty`, plus `rbranch recovery ? %dt_alN` in the same wide instruction |
| recovery block | a non-speculative reload and the recomputed users, then a branch back |

This is GCC's `BEGIN_CONTROL` speculation with checks, which the Haifa
scheduler already supports for ia64 (`create_check_block_twin`,
haifa-sched.cc:8347; `TARGET_SCHED_GEN_SPEC_CHECK`;
`TARGET_SCHED_NEEDS_BLOCK_P`). The check must be in a channel the
`%dt_al` condition can name, and it tests operands only: a value that
turned diagnostic in the check's own operation is not caught. A port
can offer it as an option (lcc's `-fno-semi-spec-ld`) after the
check-free semi-speculative form works.

### 7.7 Loops

- **Counted loops**: a `doloop_end` pattern lets GCC turn a loop into
  "decrement counter and branch". E2K's loop counter (`%lsr`,
  `ct ? %LOOP_END`, `alc`) fits; ia64 does the same with `ar.lc`
  (ia64.md:3957). What the pattern must emit (`spec/isa/loops.md`,
  measured on the 4C):
  - `rwd` the trip count with `vlc` (bit 37) into `%lsr` **at least 4
    cycles before the loop** (sooner, the loop runs one iteration too
    many), and `disp %ctpr1, head` 5 cycles before the back edge;
  - `alc alcf=1, alct=1` and `ct %ctpr1 ? %NOT_LOOP_END` in the last
    instruction; the loop runs `lcnt` times (once for 0 or 1, so guard
    a zero trip count before it);
  - either no `loop_mode` bit, or `strmd` at least the number of stores
    in the instruction: with `loop_mode` the last iteration's stores
    from number `strmd` on are dropped (`semc` = 1 turns that off);
  - `%lsr` and `%ilcr` are call-clobbered, so `doloop` must not be used
    around calls without saving `%lsr` (GCC's `doloop` already refuses
    loops with calls on most targets).
- **Software pipelining**: GCC's SMS (`modulo-sched.cc`, `-fmodulo-sched`)
  overlaps iterations and uses register copies instead of rotating
  registers; it runs before register allocation and needs `doloop_end`.
  C6x runs a post-RA modulo scheduler built on Haifa
  (`hwloop_optimize`, c6x.cc:5478, `set_modulo_params`,
  haifa-sched.cc:580) — the closest model for E2K's hardware loop mode.
  Selective scheduling can also pipeline (`-fsel-sched-pipelining`).
- **Modulo scheduling pitfalls** found by the proof-of-concept back end
  (spec/isa/loops.md "For a compiler" has the hardware side: schedule
  with the crossing cost, a kernel with its stores in the last stage
  needs no `loop_mode`, keep the ring away from the outgoing registers
  at calls):
  - *`restrict` cliques hold within one iteration only.* The dependence
    cliques GCC gives an inlined function's `restrict` parameters order
    the references of one call, but RTL's `may_alias_p`, which SMS uses
    for dependences between iterations (ddg.cc:379), believes them
    across iterations too (tree-ssa-alias.cc:2527); tree-data-ref.cc
    trusts only clique 1 and `loop->owned_clique` for cross-iteration
    queries (:3076). `gcc.dg/torture/pr90328.c` failed once pipelined;
    the proof of concept asks again without the cliques for other ones.
  - *Dataflow doesn't know that `setbn` renames `%b[]`.* Between a
    pipelined loop's `setbn` and the one after it, ring names that are
    also `%b[0..7]` look like the outgoing registers, so a call argument
    set up in `%b[0]` before the loop was deleted as dead
    (`pr111048.c`, `va-arg-22.c` at `-Os`). Make the first `setbn` use
    `%b[0..7]` and the second set them, and make the kernel use every
    ring name at its end (a value read in the next iteration under
    another name looks dead).
- **Rotating registers**: two different things share the name.
  *Window shifting* — each call slides the register window, as SPARC's
  `save`/`restore` and E2K's `call wbs=` do — is handled by hardware and
  GCC models it with `INCOMING_REGNO`/`OUTGOING_REGNO` (D1). *Loop
  rotation* — inside a loop, an instruction advances a base so that the
  register written as `%b[0]` in one iteration is read as `%b[2]` in the
  next (E2K `abn`, one register pair; predicates `%pred0` → `%pred1` with
  `abp`; ia64 `br.ctop`), which lets overlapped iterations keep
  their own copies without moves — is what software pipelining needs.
  SPARC has no loop rotation. GCC has **no support for loop rotation
  anywhere**; ia64 never used its rotating registers (`n_rotate_regs = 0`, "No rotating register
  support yet", ia64.cc:2769). Supporting E2K loop mode with rotation
  (`abn`/`abp`, `%b[]` rotation, prologue/epilogue predicates) is new
  compiler work — a research project for after the port works. lcc
  shows a simpler shape that needs no prologue counters: it peels the
  prologue stages before the loop, makes every load in the kernel
  speculative (loads are never suppressed in the epilogue), and lets
  `strmd` drop the stores of the last iteration (`spec/isa/loops.md`,
  "How lcc builds loops").
- **APB** (array prefetch, `spec/isa/apb.md`): measured on the 4C,
  lcc's streaming loops are 2× faster from L1, about 6× from L2 and 22×
  from memory with it than without, because lcc's code without APB
  doesn't prefetch at all. APB doesn't need rotating registers or
  software pipelining: a plain `loop_mode` loop counted by `%lsr`
  (`doloop_end`) with one `mova` per array is enough (`apb-loop.s`,
  loop A). It does need, for correctness:
  - every area advanced exactly once per loop-mode iteration and `%lsr`
    written before `bap`, because after any interrupt the kernel
    recomputes the stream position from `%ilcr − %lsr`;
  - no calls in the loop, and no use of `%ctpr2` (a call, a return or a
    write to `%ctpr2` stops the program, and a later `mova` is SIGILL);
  - on v3/v4, elements aligned to their size (otherwise the `mova`
    returns a diagnostic value), proven or checked at run time as lcc
    does.
  - no store during the loop to an element of a streamed array that a
    later `mova` reads: the buffer doesn't see stores, and how far it
    has prefetched varies. Stores to elements already read are fine;
    lcc versions loops that may alias on a run-time pointer comparison
    (`spec/isa/apb.md`, "Stores into a prefetched array").

  Size each area for the prefetch depth: about 1 KiB (`asz` 4) lets one
  stream from memory run at L1 speed. `dcd = 0`, as lcc writes, is the
  safe default; a large array read once can use `dcd` 2 (v3) or 3 (v5)
  to keep the rest of the cache, at no cost to the stream
  (`spec/isa/apb.md`, "Timing"). End the loop as lcc does, `{ disp
  %ctpr2, disp=0x0; aaurw 0, %aabf0 }`: the `%aabf0` write keeps a later
  trap from restarting the unit.

  So APB could come right after counted loops, before modulo scheduling.
- **While loops** can be pipelined without `%lsr`, `loop_mode` or `abp`
  if the exit test sits in the last stage (the proof-of-concept back end,
  P18, on the 8C2): the kernel ends with `abn abnf=1, abnt=1; ct %ctprN
  ? p`, every other operation of the iteration is no later (stores in the
  last stage), so the iteration the loop leaves on is complete and the
  earlier stages ran only speculative work; prologue and ring naming as
  for counted loops (`spec/isa/loops.md`, "For a compiler"). With the
  compare → `ct` delay of 3 and predicates not rotating, II ≥ 3. Two
  exits merge into one block: the second runs speculatively, the loop
  end tests `landp ~exit1, stay2`, and after the loop the first exit's
  compare runs again on the last iteration's values. Fold register
  copies into their sources first, or a pointer chase pays an ALU op and
  a crossing a node. CoreMark +14% (`find` 3 cycles a node, lcc about 9).
  To go below II 3, give the exit test's predicates **rotating** names
  as the `%b[]` values get (`setbp psz` with the prologue's `setbn`,
  `abp` with `abn`, `setbp psz = 0` on leaving; loops.md "Rotation"):
  strlen at II 1, xz's `lzma_memcmplen` at II 2 (lcc 1) (P24). The loop
  end is the kernel's last insn, so if another predicate takes the name
  it reads in the same cycle, dataflow sees the jump read that write
  (the hardware reads before writing) and deletes the prologue's
  definition: keep that name free in the loop end's cycle.
- **Let doloop count only loops that can use `%lsr`** (P25): GCC's RTL
  doloop converts any loop with a computable count, also one with a
  second exit (`while (len < limit && a[len] == b[len]) len++`), which on
  E2K can't run in loop mode; the counter then stays in a register and
  costs a decrement and a compare per iteration on top of the loop's own
  variable. Fail `doloop_end` for a loop of two non-empty blocks with two
  exits (the PoC's kernel went from 7 operations at II 2 to 6 at II 1,
  lcc's).
- **Copies into and out of a pipelined loop's ring must be
  speculative** (`,sm`; P26): before the loop a register may hold a
  diagnostic value from a speculative load the program never reads on
  that path, and a plain copy traps (xz's `index_tree_next`, SIGILL). In
  general, any operation a compiler adds that reads a register on a path
  where the program didn't must be speculative.
- **DAM with rotating predicates** works on the hardware (P28;
  `spec/isa/speculation.md`, checked with `p28.c` on the 4C and the 8C2:
  a check and `rbranch` one or two stages after the lock, the
  compensation recomputing the predicate under its name at the check,
  the consumer reading it then or one rotation later). The PoC's wrong
  results in GMP's `mpn_bdiv_dbm1c` with both (smaller loops were right)
  must come from its compensation code missing an instance that used the
  early value; it refuses the combination for now.
- **DAM inside a pipelined loop** (a load that may read what an earlier
  iteration stored, `core_list_reverse`; P19, 4C and 8C2): the lock load
  runs stages ahead (ALC0, `,sm`, `mas=0x4`), the check (`mas=0x3`, ALC5,
  `rbranch`) goes in the last stage, after the stores it moved above and
  before those storing what it loaded; DAM entries follow the physical
  register through `abn`, so the check names the register as it is
  called after the rotations. When the check reloads, the compensation
  code redoes, in order and under the check iteration's ring names,
  every instance that used the early value since the lock load (later
  loads through it, the exit compares), then branches back past the
  check; the ring must keep their operands alive until the check, and
  the check's `rbranch` and the loop's `ct` can't share an instruction.
  CoreMark +9% (reverse 8 → 3 cycles a node).
- **Branchy loops** are where lcc's lead on integer code comes from
  (zlib `-9`: lcc 1.18 s, the proof-of-concept back end 2.24 s on the
  8C2; P22): lcc turns a chain of tests (zlib's `longest_match`) into one
  predicated block, every load speculative and the conditions joined by
  `landp` trees, and pipelines the loop. GCC won't get there by itself
  (tree ifcombine merges only blocks without loads or arithmetic, RTL
  if-conversion handles diamonds): it is a target pass after register
  allocation that merges such chains into one block and one branch,
  tail-duplicates the block a rare path re-enters, and hands the result
  to the while-loop pipeliner. Merging alone was 5% slower; with
  pipelining zlib `-9` took 1.26 s.
  It can be tested only on hardware: MCST's qemu gives wrong results for
  APB loops that receive signals.

### 7.8 Measuring

Correctness with the compat suite and GCC's testsuite; speed with `perf
stat` on the 4C against lcc on the same sources. `tools/isa-score`
shows where the cycles go without hardware: for each block and loop it
gives the schedule's cycles and the best possible for the same
operations, so the port's output and lcc's can be compared block by
block (validated on the 4C to within 2%). The guide's chapter 5
lists the E2K performance counters (`BUB_E2` etc. for stalls).

---

## 8. What to take from other ports

### ia64 (`gcc/config/ia64/`, ~26,000 lines: ia64.cc 12,014, ia64.md 5,246, itanium2.md 1,867)

| Concept | Where in ia64 | Use for E2K |
|---|---|---|
| In/local/out registers; `INCOMING_REGNO`/`OUTGOING_REGNO`/`LOCAL_REGNO` | ia64.h:287–299, 406–422; `ia64_function_arg_1` ia64.cc:4760 | D1: same idea, outgoing set printed as `%b[]` |
| Window size after allocation | `ia64_compute_frame_size` ia64.cc:2691 (scan `df_regs_ever_live_p`), `current_frame_info` :158 | D2: compute `wsz` and `rbs` |
| Window allocation insn first in function | `alloc` pattern ia64.md:4734 (`unspec_volatile`, `first_insn`), `ia64_expand_prologue` ia64.cc:3500 | `setwd`/`setbn` insn |
| Printed register renaming | ia64.cc:3566–3597, restored in `ia64_output_function_epilogue` :4462; `HARD_REGNO_RENAME_OK` guard :4269 | Only if `%b[]` is reserved for rotation (D1 caveat) |
| Leaf functions | ia64.cc:2814 onward | No outgoing area, no `setbn` |
| Predication | `define_cond_exec` ia64.md:5144; compares into predicates ia64.md:3830–3920; `cbranch*4` → `ia64_expand_compare` ia64.cc:1816; cond moves → `ia64_emit_cond_move` :1402 | D3, but single (not paired) predicates |
| Speculation hooks | ia64.cc:8072–8571 (`ia64_set_sched_flags`, `ia64_speculate_insn`, `ia64_needs_block_p`, `ia64_gen_spec_check`); speculative load patterns ia64.md:440–708 | 7.6 |
| Late scheduling in reorg | `TARGET_DELAY_SCHED2` ia64.cc:661; `ia64_reorg` :9830; sel-sched at -O3 :6182 | 7.3 |
| Scheduler hooks for wide issue | `ia64_issue_rate` :7311 (6), `ia64_dfa_new_cycle` :7761, lookahead 6 :9646, `ia64_dfa_sched_reorder` :7537 | 7.3 |
| Two automata: one for scheduling, one for packing, switched by a `bundling_p` test | itanium2.md:190, 1095 | Only if packing needs more than "one cycle = one group" (it shouldn't on E2K) |
| Dynamic-programming bundler | `bundling()` ia64.cc:9188 | Not needed: E2K has no bundle templates; a cycle *is* a wide instruction |
| Stop bits `;;` and the predicate-aware write tracker | `rws_sum`, `group_barrier_needed` ia64.cc:6255–7096 | Stop bits are not needed (braces are the groups), but the tracker is a model for the late pass that pads non-interlocked hazards (7.4) |
| `doloop_end` on a count register | ia64.md:3957 | 7.7 |
| **Not reusable** | unwind directives, gp/function descriptors, M/I/F/B templates, NaT/ALAT details, HP-UX/VMS | — |

### TILE-Gx (`toolchains/gcc-tilegx/gcc/config/tilegx/`)

The model for printing `{ }` (7.4): `tilegx_gen_bundles` (tilegx.cc:4453),
`tilegx_asm_output_opcode` (:5463), `reorder_var_tracking_notes` (:4796),
`TARGET_DELAY_SCHED2`/`TARGET_DELAY_VARTRACK` (:5677). Its DFA is tiny
(tilegx-generic.md, 115 lines) and its hardware also interlocks. It never
names pipes in the output: its assembler assigns them, as MCST gas does.

### C6x (`gcc/config/c6x/`, mainline)

The model for explicit unit (channel) assignment and for
"these two instructions must be N cycles apart":
`assign_reservations` (c6x.cc:3029), `c6x_variable_issue` (:4350),
generated DFA from a template (`c6x-sched.md.in` + `gensched.sh`),
real/shadow pairs (`split_delayed_branch` :5118), nop gap computation
(`reorg_emit_nops` :4989), and a post-RA software pipeliner for its
hardware loop buffer (`hwloop_optimize` :5478). C6x has an exposed
(non-interlocked) pipeline, so much of its complexity is not needed for
E2K.

### SPARC (`gcc/config/sparc/`)

The classic register-window port: `INCOMING_REGNO`/`OUTGOING_REGNO`
(sparc.h:1082–1098), `sparc_function_incoming_arg` (sparc.cc:827), leaf
register remapping (`LEAF_REG_REMAP`, sparc.h:976). Its windows are fixed
size, so for E2K ia64's variable-size window logic is the better model.

### Ports that pack instructions but are less relevant

FR-V (`frv_pack_insns`, frv.cc:7532; packing by `.p` suffix and position)
and Blackfin (`bfin_gen_bundles`, bfin.cc:3989).

---

## 9. Testing

1. **ABI**: `tools/abi/compat` (`--cc gcc=... --cxx gcc=...`,
   `--target v3`, `--remote claude@ringo.lan` for hardware). Any failure
   not in `known.txt` is a port bug.
2. **GCC's testsuite** through DejaGnu with a board that runs programs
   under qemu-e2k or over ssh on the 4C. GCC ships no board files; relcc2
   can provide them (section 11). Then:
   `make check-gcc RUNTESTFLAGS="--target_board=e2k-qemu execute.exp"`
   for `gcc.c-torture/execute`, later the whole `check-gcc`, compared with
   `contrib/test_summary` / `contrib/compare_tests` between revisions.
3. **GCC's own compat tests** against lcc: `gcc.dg/compat` and
   `struct-layout-1.exp` with `ALT_CC_UNDER_TEST=<lcc>` (sourcebuild.texi
   :4309–4430): a second, independent ABI check.
4. **ABI probes** in `tools/abi/probes` (`layout.c` must compile with your
   GCC too).
5. **Hardware** for anything involving tags, speculation, or timing:
   qemu differs from the 4C (see the tag-trap note in
   `calling-convention.md`).

---

## 10. Upstreaming

- GCC accepts contributions under the DCO (MAINTAINERS, "Contributing
  under the DCO", :978) or with FSF copyright assignment.
- **GCC does not accept AI-generated contributions** (AGENTS.md decision
  2): every line of the port is written by the owner; this document and
  relcc2's tools are references only.
- A new port needs a maintainer listed in `MAINTAINERS.yml`, an entry in
  `contrib/config-list.mk`, docs, and website updates
  (sourcebuild.texi:896–917).
- The assembler and linker are MCST's binutils, not in upstream binutils.
  Expect the GCC maintainers to ask how others can build and test the
  port; an upstream binutils port (or at least public MCST binutils, as
  now in `OpenE2K/binutils-gdb`) and a public simulator (qemu-e2k) answer
  that.
- MCST code (e.g. the unwinder, 3.3) raises the provenance question.

---

## 11. What relcc2 provides alongside

Agents may write specs, data, tests and tools (AGENTS.md decision 2).
Useful next pieces of relcc2 work for the port, in rough order:

1. **DejaGnu board files** for qemu-e2k and for ssh to the 4C, and a
   script to run GCC's testsuite with them.
2. **Timing tables** for more generations (v4/v6 via the community)
   with `tools/isa-timing/bench.py`; the 4C (v3) and 8C2 (v5) tables
   are in `spec/isa/timing.md`.
3. **Channel/resource data** from `binutils-ops.yaml` in a form easy to
   turn into reservations (data only; the generator that writes `.md`
   belongs in the port).
4. **Speculation leftovers** (`spec/isa/speculation.md`, done for v3
   and v5): v6+.
5. **Loop mode leftovers** (`spec/isa/loops.md`, "Not checked yet"):
   `ldovl`, v6+ (`abg` is done on v5; lcc never emits it).
6. Review of the port's output against `spec/` and lcc, and test
   triage.

### Decisions and open questions

Decided by the owner:

- Follow lcc's conventions wherever there is a choice: `setwd dbl` per
  function (D7), `.ignore strict_delay` with compiler-inserted delays
  (D9).
- Unwinder and E2K libgcc files: MCST's (3.3).
- Default target: **`-march=elbrus-v3`**, forward compatible (no
  `EF_E2K_INCOMPAT`; `spec/abi/elf.md`). How MCST keeps v3 code working on
  later CPUs is not public, so forward compatibility is established by
  testing: lcc's v3 code passes the whole compat suite (lcc → lcc,
  1,853 tests) under MCST's qemu as v5, v6 and v7, and the suite
  passes on the Elbrus-8C2 (v5) for every pair; the community can test
  v4 and v6. The 8C2 does not fill fresh registers with tagged values
  (only qemu does), and v6 has longer load latencies (performance
  only).

### What a v5 mode adds (lcc 1.29.16 for the 8C2, measured)

lcc's v5 code for CoreMark and Linpack differs from its v3 code in only
a few ways, a guide to what an `-march=elbrus-v5` mode needs first:

- **Array prefetch without alignment**: v5 allows unaligned areas, so
  lcc uses APB where its v3 code needed `-faligned` or a run-time check
  (Linpack: 52 APB programs in the v5 build, none in the v3 one).
- **More FP channels**: `faddd`/`fmuld`/`fmul_add*` also in ALC2/5 (6 a
  cycle); lcc uses them sparingly (19 of 399 FP operations in Linpack).
  Watch the one conflicting pattern with the divider (timing.md).
- **`%lsr1`** for loop counts (written after `%lsr`, loops.md).
- **128-bit data**: two `ldd` + `qppackdl` for loads (`ldqp` goes to L2,
  latency 11), `stqp` for 16-byte stores, `qp*` operations with their
  64-bit latencies; `ldqp`/`stqp` loops for 16-byte atomics.
- **`getfzd`/`getfzs`** (v5 field extract) in place of v3 sequences.

Everything else (latencies, hazards, control transfer, the ABI) is the
same as v3 on the 8C2.

