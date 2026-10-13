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
| `linux.h` | `GLIBC_DYNAMIC_LINKER "/lib64/ld-linux.so.2"`, link spec, OS macros; `CPP_SPEC "%{pthread:-D_REENTRANT}"` (lcc's `-pthread` defines it, and autoconf's `AX_PTHREAD` rejects `-pthread` without it; `riscv/linux.h:40`, `aarch64/aarch64-linux.h:39`; the PoC's P39); `TARGET_ASM_FILE_END file_end_indicate_exec_stack` (as `or1k/linux.h:47` and `loongarch/linux.h:58`): the GNU-stack note, `"x"` only in objects with a stack trampoline (`spec/abi/elf.md`, "Executable stack"; D1, "Trampolines"); `ENDFILE_SPEC` = a math-file spec followed by `GNU_USER_TARGET_ENDFILE_SPEC`, as `loongarch/gnu-user.h:59-61, 69-72`, with libgcc's `extra_parts="$extra_parts crtfastmath.o"` (`libgcc/config.host:993-994`) and MCST's `crtfastmath.c` (3.3), which sets fz and daz. GCC's ports write `%{Ofast\|ffast-math\|funsafe-math-optimizations:%{!shared:crtfastmath.o%s}}`; lcc links it into shared libraries too, and loading such a library switches the loading thread and its later threads to flush-to-zero (`spec/abi/registers.md`, "Flush to zero"; A28). GCC removed exactly that on x86 (PR 55522, `88b531d0323`; invoke.texi:18862-18864); doing what lcc does (decision 7) means dropping `%{!shared:...}`: a decision for you | `or1k/linux.h` |
| `t-e2k` | Makefile fragment (multilibs, generated `.md`) | `or1k/t-or1k`, `c6x/t-c6x` |
| `gcc/common/config/e2k/e2k-common.cc` | Option defaults per `-O` level (e.g. enable scheduling at `-O1`) | `common/config/tilegx/`, `common/config/c6x/` |

### 3.2 Places outside the directory

| File | What to add |
|---|---|
| `gcc/config.gcc` | Two `case` entries. First (around :340–633): `e2k*-*-*) cpu_type=e2k ;;`. Second, OS-specific: `e2k*-*-linux*)` with `tm_file="elfos.h gnu-user.h linux.h glibc-stdint.h ${tm_file} e2k/linux.h"` and `tmake_file` (see `riscv*-*-linux*` at :2551 and `or1k*-*-linux*` at :2946). The header comment (:21–120) explains every variable |
| `gcc/configure.ac` | Add `e2k` to the `cpu_type` list with a valid `nop` for the dwarf2 `.loc` probe (:5727); add a TLS assembler probe (`conftest_s`, near :4038) or `HAVE_AS_TLS` stays off. Regenerate `gcc/configure` with the autoconf version GCC requires |
| `libgcc/config.host` | `cpu_type` (:138 area) and an `e2k*-*-linux*` entry: `extra_parts`, `tmake_file`, `md_unwind_header` (see loongarch at :993) |
| `libgcc/config/e2k/` | `t-e2k`, `sfp-machine.h` (for `__float128` soft-fp), crt pieces. **The unwinder is special on E2K** (it walks the chain stack, see `spec/abi/stack-and-unwinding.md`); see 3.3. No `linux-unwind.h`: MCST's unwinder interprets no CFI and skips signal frames like any frame without an FDE, and with it an exception cannot be thrown out of a signal handler (`spec/abi/stack-and-unwinding.md`, "Signal frames, cancellation") |
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
| `clear-cache.c` | 79 | `__e2k_clear_cache`, which `__clear_cache` calls through `CLEAR_INSN_CACHE` in `gcc_objdir/tm.h` (D1, "Trampolines") |
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
  `-std=gnu17` (glibc's e2k `fenv.h` breaks under C23). libgo:
  `-fsplit-stack` needs nothing. GCC's default `supports_split_stack`
  hook already rejects it (`gcc/common/common-target.def:65-74`,
  `gcc/opts.cc:1282-1291`), lcc rejects it too, and libgo's configure
  then builds without split stacks (`libgo/configure.ac:454-499`). E2K
  cannot support split stacks (`spec/abi/stack-and-unwinding.md`, "No
  split stacks"). libgo's fallback starts goroutines with `makecontext`
  (`libgo/runtime/runtime.h:494-500`), which E2K's glibc lacks: it needs
  `makecontext_e2k` and `freecontext_e2k`, each goroutine gets its own
  kernel hardware stacks, and every switch is a system call
  (`spec/abi/kernel-interface.md`, "User contexts"). Goroutines move
  between threads; the kernel's rules for that (keep the threads that
  run goroutines alive, reach TLS only through functions that read it)
  are in the same file, "Contexts and threads".
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
  (`%r2`). Register variables use `%xgN` for extended registers. The
  system-call asm of glibc and the uapi headers needs `%#`, the
  `"call"` and `"b[N]"` clobbers and a memory clobber the tree
  optimizers see: D10, "lcc's system-call asm".
- Atomics (`sync.md`): read-modify-write = a lock-wait load (MAS `0x7`,
  ALC0) and a wait-unlock store (MAS `0x2`) with `rbranch` back, which
  gas assembles as `ibranch ? %MLOCK`; fences = `wait ld_c = 1,
  st_c = 1`. Under `-march=elbrus-v3` 16-byte atomics must call
  libatomic, like lcc. No machine keeps loads or stores in order across
  wide instructions in general: message passing fails on all three when
  the reader already holds the data's line, up to 42% of tries with
  plain accesses (atomics.md, "Memory ordering on hardware"; found
  rechecking A29). Each side needs lcc's form: a `wait ld_c = 1, st_c =
  1` before a release store (`atomic_store` pattern) and, after an
  acquire load, a dummy use of the value (`adds,sm r, 0x0, %empty`) or
  a `wait ld_c = 1` (`atomic_load` pattern); with both, no setup
  reordered anything on any machine, with either alone some did. A
  read-modify-write as lcc emits it needs no `wait` (its exchange used
  as a release, read with lcc's acquire, never failed). Never bundle a
  release store with an earlier store: stores in one instruction are
  unordered.
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
  `libstdc++.so.6`: `/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/fs`. Take
  the `e2k-v3` one: every file in a CPU tree such as `e2k-v5.8c2` is
  marked for that CPU, so a program linked against it runs only there
  whatever `-march` its objects have (`spec/abi/elf.md`, "Mixing targets
  in one program"; the same holds for native links on Elbrus Linux).
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
  the stack frame; epilogue `return %ctpr3` … `ct %ctpr3`. Once the
  scheduler runs (M4), fill that first instruction as lcc does (P73;
  lcc 1.29.16 `-O3`, Lua 5.4.7's 724 functions, v3 and v6 alike;
  registers.md). Besides `setwd` it holds `setbn`/`setbp`, `getsp`
  (ALC0 only), a prepare (a `disp` to the first callee or `return
  %ctpr3`, in 706 of the 724), loads through the argument registers (up
  to three beside `getsp`, four without it) and constants needed later
  (up to five `adds`/`addd`). Operations beside `setwd` already use the
  new window; none can read `getsp`'s result, which reaches an integer
  operation 2 cycles later (timing.md). Leaf functions prepare `return
  %ctpr3` in the first or second instruction (67 of Lua's 88). The
  proof of concept put nothing beside `setwd`/`getsp`, and with one
  prepare a cycle its prepares all started two cycles late (Lua's
  `luaH_get`: first compare in the sixth instruction instead of the
  fourth); packing as lcc does gave Lua 0.4–1.3% fewer cycles on the
  2C3. What stops a scheduler from doing it: 7.3, "The frame setup is
  no scheduling barrier".
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
- `spec/isa/operations-v3.md` (and `operations-v5.md` for what v5 adds)
  list every operation with the standard name or C idiom it serves
  ("Use"), its latency, how often lcc uses it and whether it was checked
  on hardware: the place to look for an instruction to match a pattern.
  `trap` can be `setsft` (SIGTRAP `TRAP_BRKPT`, the kernel's `BUG()`);
  lcc's `__builtin_trap` stores to address 0 (SIGSEGV).
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
  (`spec/isa/semantics.md`, "Field extract"). Use `getfs` for fields
  of SImode values: its descriptor has offset and *len* in 5 bits each
  and BYTE in bits 13–14, so it covers fields of up to 31 bits, while
  `getfd` reads all 64 bits of the register, which under `dbl = 0`
  arrive 4 cycles after a 32-bit producer instead of 1 and can hold a
  diagnostic upper half that traps (D7). lcc does this: `getfs` where
  the field stays 32-bit, `getfd` only after `sxt 6` under `dbl = 0`,
  after a load, or on a result that `dbl = 1` zero-extended [lcc
  1.29.16 `-O3`, v3 and v5]. The proof of concept printed `getfd` for
  SImode fields until P83. `fscales`/`fscaled` serve `ldexp<mode>3` on
  v4+, and `scalbn` and `scalbln` reach the same pattern
  (builtins.cc:2226-2229). Under the default `-fmath-errno`, though,
  `ldexp` is not const (`ATTR_MATHFN_FPROUNDING_ERRNO`,
  builtins.def:287-289, 514), so expand keeps the call (it uses an
  internal function only for a call without side effects,
  cfgexpand.cc:3155-3163) and tree-call-cdce has no range test for it:
  the pattern serves `-fno-math-errno` code (P79). lcc likewise keeps
  `ldexp`, `ldexpf` and `scalbn` as calls even with `-ffast`; it
  inlines only `scalbnf`, at `-O2` and up [lcc 1.29.16, v4+]. The 64 ÷
  32 `sdivx`/`udivx`/`smodx`/`umodx` trap on quotients that don't fit,
  so they suit only proven ranges; lcc computes `%` as divide,
  multiply, subtract. The 80-bit operations with a narrower operand or
  result (`fxadddd`, `fxmulxs`, ...) round as x87 code does (to 64
  bits, then to the result's format), so `(float_truncate:DF (plus:XF
  ... (float_extend:XF ...)))` may be one `fxadddd` without fast-math,
  as lcc does at `-O3`.
- **Round to integral** (P76): `fdtoifd`/`fstoifs` serve `floor`,
  `ceil`, `btrunc`, `roundeven`, `nearbyint` and `rint<mode>2` with
  immediates 9, 0xa, 0xb, 8, 0xc and 4; use `pfstoifs` for `V2SF`, and
  v5's `qpfdtoifd`/`qpfstoifs` once 128-bit vector modes exist. Bit 3
  keeps inexact off [hw: `tools/isa-probes/pocchk5.c`, all three
  machines; `spec/isa/semantics.md`, "Other scalar operations"]. C23
  requires that, and GCC asks for it by default in C:
  `-fno-fp-int-builtin-inexact` (set for C23, c-opts.cc:941-945)
  means these patterns may not raise inexact (md.texi:5616-5621), as
  i386's SSE4.1 forms don't (`ROUND_NO_EXC`, i386.md:26366-26388).
  lcc and MCST's libm use 1, 2, 3, which raise inexact
  (`spec/abi/registers.md`, "MCST glibc's FP bugs"), so don't take
  libm as the reference for flags. `round<mode>2` (halfway cases away
  from zero) has no immediate: expand it only under
  `-fno-trapping-math`, as i386 does (i386.md:26177-26186,
  `ix86_expand_round_sse4`: `trunc (x + copysign (nextafter (0.5, 0),
  x))`). In the proof of concept a `floor` loop went from 16.9 ns an
  element (libm calls) to 0.70 on the 8C2 (lcc 0.43).
- **Overflow builtins** (P78, I50): `__builtin_add_overflow`, `_sub_`
  and `_mul_` (and ubsan's checks) expand through `addv<mode>4`,
  `subv<mode>4`, `mulv<mode>4` and the unsigned `uaddv`, `usubv`,
  `umulv<mode>4`, whose operand 3 is the label to jump to on overflow
  (internal-fn.cc:1497, 1275, 2096; optabs.def:312-317); `-ftrapv`
  goes through `addv<mode>3` and its relatives or libgcc's `__addvsi3`
  (optabs.def:124-139). On E2K `addv` is `adds_fw`/`addd_fd` beside
  the `adds`/`addd` (ALC0 or ALC3; the two can share an instruction),
  then a branch on OF; `uaddv` the same on CF. `subv` is `cmpo<s,d>b`
  alone and `usubv` `cmpb<s,d>b` (`spec/isa/semantics.md`, "Integer
  compares"). SImode `mulv`/`umulv` are `smulx_fw`/`umulx_fw`, which
  set OF together with CF. There are no 64-bit multiply flags, and
  GCC's DImode expansion already uses `smulhd`/`umulhd` and a compare
  (internal-fn.cc:2189-2240). Write the overflow test as an ordinary
  RTL bit test, `(ne (and f (const_int 0x800)) 0)`, with the AND in
  its own insn and the mask in a register, and let a splitter turn it
  into `cctopo`: OF is bit 11 of the flags word, which is what
  `cctopo` tests (semantics.md, "x86 flag operations"). If-conversion
  handles a jump on a register compared with zero, but an unspec
  condition is "complex" and is only tried as a store-flag pattern
  without clobbers (ifcvt.cc:951-1010), so `ovf +=
  __builtin_add_overflow (…)` would rebuild its 0 and 1 every
  iteration and the loop would not pipeline. Let the expanders FAIL
  for a constant operand: GCC's own code then needs a single compare
  (internal-fn.cc:1530-1545). Put the `cctopo` in the flag operation's
  channel: it gets the flags in 2 cycles there and in 4 in the other
  one, and a multiply's (`smulx_fw`) in 6. A predicated use then comes
  4 cycles after the operands, 8 for a multiply
  (`spec/isa/x86-support.md`, `spec/isa/timing.md`); `smulx_fw` is an
  FP-side result for the write-back conflict (7.5). With `,sm` the flag
  forms and `cctop` speculate like compares: a diagnostic operand gives
  a diagnostic flags word or predicate [hw: `tools/isa-probes/pocchk5.c`
  `dg`]. Neither qemu runs the flag operations or `cctop`
  (semantics.md), so keep them behind an option for emulator runs. lcc
  uses none of this: its `int` add check is ten operations in 6 wide
  instructions, and it checks a multiply by dividing the product by
  each operand, two `sdivs` (`sdivd` for `long`) in 12 instructions
  [lcc 1.29.16 `-O3`, v3–v6; 1.31.05 for v6 and v7 alike].
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
with `wbs=W`. `W` is at least 4, also in a function whose locals need
fewer registers: the 4 pairs of incoming arguments (`%r0`–`%r7`) stay
below the callee, used or not, because a `call` whose `wbs` is smaller
than the number of pairs the function was itself handed traps (SIGSEGV
`SEGV_BNDERR` at the `call`, system calls included;
`spec/abi/registers.md`, "Window size limits"). The proof of concept's
small leaves called `getpid` with `wsz = 5`, `wbs = 1` and died. Then
`%b[i]` is the i-th outgoing register *whatever the window size*, so
nothing needs renaming after register allocation.
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

**Trampolines.** MCST's libffi has the model (`src/e2k/ffi.c:316-342`):
two wide instructions, 40 bytes. The `__ptr32__` form uses `getpl` and
32-bit literals and is 32 bytes.

```
	{
	  movtd,0	_f64,_lts0 0x0, %ctpr1		! the nested function
	  addd,1	0x0, _f64,_lts2 0x0, %dg12	! the static chain
	}
	{
	  ct	%ctpr1
	  ipd	2
	}
```

gas assembles this text, with zero literals or with real ones, to
exactly libffi's words on v3, v5 and v6
(`tools/abi/probes/execstack/tramp-text.s`; `execstack.sh` compares the
bytes). So `TARGET_ASM_TRAMPOLINE_TEMPLATE` can print it. The chain
literal is bytes 16–23 and the function's is bytes 24–31, each with its
high half first (`spec/abi/elf.md`, "Literal fields").
`TARGET_TRAMPOLINE_INIT` therefore copies the template and stores each
value rotated by 32 bits as one doubleword, which gives libffi's two
words. It must then flush the 40 bytes itself, as
`loongarch_trampoline_init` does (`loongarch.cc:8525-8582`).

MCST's libgcc builds `__clear_cache` from GCC's `libgcc2.c`, with
`CLEAR_INSN_CACHE` defined as a call to `__e2k_clear_cache` (GCC 11
runtime tarball, `gcc_objdir/tm.h:71-80`; its comment says that a fast
version, if ever needed, should be inline code for v3 and later).
`__e2k_clear_cache` is `wait st_c`, a MAS 0xf store per 256-byte line,
then `wait fl_c` (`config/e2k/clear-cache.c:55-77`; on v1–v2 it is a
kernel call; `spec/isa/x86-support.md` has its cost). lcc's
`__builtin___clear_cache` is a call to `__clear_cache` (`execstack.sh`).
The port needs the same `CLEAR_INSN_CACHE` definition or a `clear_cache`
pattern, because without either GCC expands `__builtin___clear_cache` to
nothing (`builtins.cc:6056-6083`).

- `TRAMPOLINE_SIZE` 40 and `TRAMPOLINE_ALIGNMENT` 64 bits, the
  alignment of a wide instruction. libffi reserves 56 bytes
  (`ffitarget.h:54`).
- The `movtd` and the `ct` must be in separate instructions. In one
  instruction, v3 and v5 jump to the CTPR's old target and v6 doesn't
  jump at all, yet gas accepts the pair on every version. It rejects
  only `disp` + `ct` through one CTPR before v6
  (`spec/isa/control-transfer.md`, "Restrictions"). With the `ct` one
  instruction after the `movtd`, the pair costs max(1, 9) + 1 = 10
  cycles instead of 2, so each call through a trampoline loses 8 cycles
  (control-transfer.md's rule; not measured for a trampoline).
- The trampoline writes only `%ctpr1` and `%g12`, both call-clobbered,
  and touches no window register. Like a PLT entry (`spec/abi/elf.md`),
  it runs in the callee's new window and passes the arguments through
  unchanged.
- (inferred) The kernel doesn't restore `%g12` after a signal handler
  (`spec/abi/registers.md`). Suppose a handler calls through another
  trampoline or a libffi closure between this trampoline's `addd` and
  the nested function's first read of `%g12`: the nested function then
  gets the wrong chain. libffi has the same window.
- A trampoline on the stack needs an executable stack. With
  `TARGET_ASM_FILE_END file_end_indicate_exec_stack` (`linux.h`, 3.1),
  every object that builds one gets an `"x"` note
  (`trampolines_created`, `builtins.cc:6183-6185`), and ld marks the
  program `RWE` with a warning (`spec/abi/elf.md`, "Executable stack").
  Without such a note, or with `-z noexecstack`, the stack is not
  executable and the call is `SIGSEGV` (kernel
  `arch/e2k/mm/fault.c:3224-3226`; inferred, not run).
- (inferred) `CODE_PLACEMENT` (`spec/isa/hwbugs.md`): a trampoline's
  entry is a call target written at run time, out of reach of gas's NOP
  padding. On the CPUs the erratum lists, it can therefore land at a bad
  page offset: 0xdc0–0xdff, and the kernel's BPF JIT also avoids
  0xee0–0xfff. The listed CPUs are the v3–v6 models E2S, E8C, E1C+,
  E8C2, E12C, E16C and the E2C3 up to rev 1, which covers all three of
  our machines. A safe port reserves some slack in the frame and lets
  `TARGET_TRAMPOLINE_ADJUST_ADDRESS` and `TARGET_TRAMPOLINE_INIT` both
  move the entry off those offsets; a heap allocator skips those slots.
  MCST's libffi doesn't bother (`ffi.c:276, 316-325`), and no failure
  has been seen.
- **Heap trampolines** (`-ftrampoline-impl=heap`, GCC 14) need no
  executable stack. The compiler side is generic: the function calls
  `__gcc_nested_func_ptr_created (chain, func, &slot)` on entry and
  `__gcc_nested_func_ptr_deleted ()` on every exit
  (`tree-nested.cc:3607-3640`), and no `"x"` note is written. The
  runtime is in libgcc, but only for x86 (Linux and Darwin) and AArch64
  (Linux and GNU/Hurd) (`libgcc/config.host:429, 439, 734, 741, 791,
  809`). E2K would need three things. First,
  `libgcc/config/e2k/heap-trampoline.c`, modelled on
  `config/aarch64/heap-trampoline.c`: per-thread pages from
  `mmap (PROT_WRITE | PROT_EXEC)`. The E2K kernel allows such private
  write-execute mappings (`arch/e2k/mm/init.c:71-72`), and generic
  libffi tries the same mapping for its closures
  (`src/closures.c:840-852`); neither was run. Each slot holds the
  40-byte template with both literals filled in, followed by
  `__builtin___clear_cache`. Second, a `t-heap-trampoline` that adds the
  file to `LIB2ADDEH` and `LIB2ADDEHSHARED`. Third, a `config.host` line
  for E2K Linux. The two symbols are already exported under
  `GCC_14.0.0` (`libgcc-std.ver.in`). Heap trampolines stay opt-in
  unless `config.gcc` sets `HEAP_TRAMPOLINES_INIT` for E2K, which today
  only Darwin does (`config.gcc:1229-1237`). As GCC's manual warns, they
  leak when `longjmp` skips the cleanup.
- MCST's `libgcc_s` also exports `__enable_execute_stack`
  (`GCC_3.4.2`), but it is the empty generic one (MCST's libgcc
  `config.host:363-365`; its code is only a return). On Linux,
  `PT_GNU_STACK` does that job.

**Call-clobbered**: globals (except `%g13`, fixed; `%g12` as the static chain), the outgoing
registers, `%ctpr1–3`, `%lsr`. Window registers `%r*` are preserved by
hardware: mark them call-saved, and the prologue never saves them.
Predicates are also preserved: the caller's come back whatever the
callee writes, and the callee starts with a copy of the caller's,
which it must not read before writing (spec/isa/predicates.md).

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
regrename out of `%b[]` (`HARD_REGNO_RENAME_OK`) and, in a function that
calls, inside the window allocation left (D2, P51); making `%b[0..7]`
fixed instead broke vector results returned in `%b[0]` (pr123753,
pr86363 at `-Os`). Under real pressure (a full window) allocation still
uses `%b[]`.

**Registers above `%r63`** (P59): a window has up to 112 pairs, but `%r`
names reach 64 registers and the rest only through `%b[]`
(registers.md). lcc uses them in big FP kernels: NPB LU's `jacld`,
`jacu` and `rhs` run at `wsz = 0x60` with all of `%b[0..127]` as plain
names (`setbn rsz = 0x3f, rbs = 0x20`, no `abn`), `buts` at 0x5b (lcc
`-O3`, v3–v6). It keeps such a window also at calls that are few or go
to small leaves (SP's `x_solve`, BT's `initialize`, LU's `error`):
`setbn rsz = wsz − 0x21, rbs = 0x20, rcur = rsz − 3` puts `%b[0..7]` at
the top four pairs and `%b[8 + i]` at `%dr(64 + i)` whatever the size,
and the calls use `wbs = wsz − 4`, so those registers stay below the
callee and survive calls like `%r`. Where hot calls go to big callees
(LU's `ssor`) it keeps 0x22 and grows only around pipelined loops, whose
rings (`rcur = 0`) take the bottom of that area. The proof of concept,
with only `%r` and `%b[0..7]`, spilled (`jacld` 223 frame accesses,
lcc's 0–6) and ran `jacld` 3.4 times as long as lcc on the 2C3; making
`%b[8..63]` call-clobbered lost, as IRA's caller saves
(`-fcaller-saves`, on at `-O2`) put values crossing calls there and
saved them at every call, and values crossing pipelined loops took the
rings' names. (lcc's LU `rhs` spills too: 126–263 frame accesses.)

### D2. Window size

Like ia64's `ia64_compute_frame_size` (ia64.cc:2691): after register
allocation, find the highest window register ever live
(`df_regs_ever_live_p`), add the outgoing area, round to pairs, and emit
`setwd wsz=...` (plus `setbn`) as an `unspec_volatile` insn that must be
first (ia64's `alloc` pattern, ia64.md:4734, attribute `first_insn`).
Leaf functions without calls need no outgoing area. Varargs functions
need their incoming registers (the callee stores them, or reads the
caller's memory copy: see `calling-convention.md`, variadic section, for
which arguments are in memory). A function that calls puts its outgoing
area at pair 4 or above (D1), so with the convention's 4 outgoing pairs
its `wsz` is at least 8; a leaf needs `wsz ≥ 4`, the pairs its caller
handed over (a smaller `setwd` is SIGILL, registers.md).

What a window costs (registers.md, "What windows cost"): nothing while
the active calls' windows fit the register file; beyond that a call and
return cost about 15 + 1.6 × `wbs` cycles. So window size is free in
shallow call trees, and in deep ones what costs is the pairs each frame
keeps below its callee; but a `setwd` grow and shrink around a region
costs about 3 cycles, about what the spills cost. The proof of concept
tried lcc's way (grow only around pipelined loops) and limiting
register renaming to keep windows small: xz's spill stalls went from
60 M cycles to 0 and its run time stayed the same, and bzip2 -9 got 2.6%
slower (P31's companion finding I31): there, renaming freedom is worth
more than window size. Not in deep chains of hot calls (P51): once D1's
`LOCAL_REGNO` exemption lets regrename into unused window registers, it
spreads values over all 64 `%r`, since it takes the register with the
oldest `tick` and one it never took has tick 0 (regrename.cc:449, :492),
then marks it ever live (:552). Lua's interpreter chain (`luaV_execute`,
`luaD_precall`, `precallC`, `str_sub`, `lua_pushlstring`,
`luaS_newlstr`) then kept 32, 25, 32, 5, 5 and 32 pairs below its calls
(7, 5, 7, 5, 5, 7 after allocation alone; lcc 53, 9, 9, 5, 4, 6, `-O3`
for the 8C2) and spent 5% of k-nucleotide's cycles on register-file
spills. The proof of concept keeps renaming inside the window
allocation left (`HARD_REGNO_RENAME_OK`) in a function that calls,
unless its calls are rare next to its hottest block (1/16) or go only to
leaves and functions calling only leaves (GCC expands callees first
within a file, cgraphunit.cc:1978–2000): spills 602 M → 10 M on the 8C2,
set 2 geomean 0.991 (8C2) and 0.986 (2C3), CoreMark −0.2% (renaming
everywhere matters in CoreMark's `matrix_test` and bzip2's `mainSort`,
whose calls are rare or shallow). regrename runs only with
`-frename-registers`, which `-funroll-loops` turns on (common.opt:2836).
A `getsp` in the `setwd` instruction is free.

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
4%, CoreMark none, `MAX_CONDITIONAL_EXECUTE` 6). Under an FP compare's
predicate it lost while the arms' loads were predicated (each waits 4
cycles for the predicate, then its own latency: Linpack 7% slower in
the proof of concept), and it gains in lcc's form, a speculative load
and a predicated copy (7.6, item 3, "Loads in a converted arm"; P86).
Where the select feeds the next compare (`idamax`'s maximum) the
compare's latency joins the loop's recurrence, which pipelining the
loop outweighed there (the proof of concept). lcc's lead on branchy
code comes from whole loops turned into predicated blocks and pipelined
(7.7, "Branchy loops"), not from diamonds.

**Predicated calls** (P67; lcc 1.29.16 `-O3`, control-transfer.md,
"How lcc uses it"). lcc predicates most calls: 12,999 of 20,519 in
SQLite for v5 and 2,592 of 5,921 in Lua for v3. Its `sqrt` with
`errno` is `fsqrtid,sm` plus `fsqrttd,sm ? p`, with `call %ctpr2 ? ~p`
to libm's `sqrt` (the `disp` sits in the function's first instruction).
Its `if (f == 2) lng (l); else dbl (d)` prepares `lng` and `dbl` in two
CTPRs before the compare, then does `call %ctpr2 ? p` and `call %ctpr1
? ~p` in consecutive instructions. A call under a false predicate leaves
the CTPRs (D4), and a prepare takes no predicate (CS0 has no condition
field, bundle.md), so preparing unconditionally is harmless. ce3
predicates call insns once their patterns are predicable
(`cond_exec_process_insns` accepts `CALL_P`, ifcvt.cc:374, and adds a
`REG_BR_PROB` note, :429), so give `call` and `call_value` the
`predicable` attribute. Three things follow:

1. ce3 leaves identical tails of the two arms unconditional
   (`flow_find_cross_jump`, ifcvt.cc:550). Two calls through `%ctpr1`
   are identical RTL, so they become one unconditional call after both
   arms' prepares, and since prepares stay unconditional, the second
   prepare wins: the proof of concept called `dbl` on both arms. Prepare
   each arm's callee in its own CTPR, as lcc does, or reject such a
   prepare in `IFCVT_MODIFY_INSN` (ifcvt.cc:422), as the proof of
   concept does.
2. GCC's dataflow makes a predicated call's clobbers conditional
   definitions (`DF_REF_CONDITIONAL`, df-scan.cc:3216), each with a use
   (`df_get_conditional_uses`, df-scan.cc:3070, called at :3233), and
   LR treats every conditional definition as a use as well
   (`df_lr_bb_local_compute`, df-problems.cc:888; `df_simulate_defs`,
   :3898). So every call-clobbered register (globals, `%b[]`, the CTPRs)
   is live up to such a call. In NPB CG a predicated `sqrt` at the end
   of `conj_grad` kept `%ctpr2` live around six loops, which lost their
   array prefetch (7.7, "Keep `%ctpr2` free"). The uses aren't needed:
   where the call runs, the values are gone, and where it doesn't,
   later reads keep them live anyway (inferred). The proof of concept
   drops them for call clobbers in df-scan.cc and df-problems.cc.
3. A CTPR prepared before a predicated call is usable after it only
   under a predicate that excludes the call's, because a taken call
   empties all three. lcc relies on that, and a pass that deletes
   repeated prepares (D4, "In functions that call") must count a
   predicated call as a call. sched-deps already treats a predicated
   call's clobbers as unconditional (`deps_analyze_insn`,
   sched-deps.cc:3870), which is safe.

In the proof of concept this made NPB EP.W 1.5–1.8% faster (8C2 7.28 →
7.17 s, 4C 14.36 → 14.10, 2C3 5.47 → 5.37; lcc 6.29, 12.49 and 4.77,
which also predicates its `log` calls and unrolls the loop by two), and
SQLite got 879 predicated calls and 3.6% fewer wide instructions.

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

Indirect calls: `movtd %reg, %ctprN` (latency 9) then `call`. v6 adds
`ibranchd` and `icalld`, a jump or call through a register with no
CTPR, but a taken `ibranchd` costs 12 cycles however early its register
is ready, and `icalld` plus a return 23 cycles against 14 for `disp` +
`call` (`spec/isa/control-transfer.md`, "v6"). So `movtd` (or `disp`)
ahead plus `ct`/`call` stays the choice for v6, as in lcc's v6 code.
lcc 1.31.05 uses the new forms only for v7 (`-march=elbrus-v7`): jump
tables as `ldd` + `ibranchd`, indirect calls as `icalld`, indirect tail
calls as `ibranchd`, and direct calls and tail calls as `icall f` and
`ibranch f`, without `disp` [lcc; `spec/isa/operations-v6.md`,
`icall`]. Decide v7's transfers when a v7 machine can time them (P79).

**Switches** (P65, P72; lcc 1.29.16 `-O3`, v3, v5 and v6 alike, and lcc
1.31.05 for v3; control-transfer.md, "How lcc uses it"). A jump table
costs a bound check, a load of the target (3 cycles, 5 on v6) and 9
cycles from `movtd` to the `ct`: about 14 cycles before the arm's first
instruction (16 on v6). lcc uses a table only for 18 arms or more
(consecutive values with one target count as one arm), or from 6 arms
when 6 or more of them end in a tail call. A smaller switch becomes
predicated code: the selector is compared against the values into
predicates, `landp` trees form each arm's condition, the arms' loads and
arithmetic run speculatively, and stores, calls and returns go under the
arms' predicates (range compares first split a switch of 14 arms or
more, or one with calls, into a few such blocks). SQLite's
`sqlite3VdbeSerialGet` (12 case values in 9 arms) is 34 wide
instructions there; the proof of concept, with a table, made it 84. GCC
makes a table from 5 case labels when the target has no `casesi` pattern
(`default_case_values_threshold`, targhooks.cc:1946;
`jump_table_cluster::is_beneficial`, tree-switch-conversion.cc:1787;
`--param case-values-threshold` overrides the hook,
tree-switch-conversion.h:520), and below that a tree of compares and
branches. Raising `TARGET_CASE_VALUES_THRESHOLD` (target.def:6485)
toward lcc's 18 alone gains nothing, because the tree stays branches:
the proof of concept measured SQLite's speedtest1 0.3–0.4% faster (with
16 or 32) and Lua's queen and k-nucleotide 0.7–1.1% slower. Take lcc's
rule only together with a pass that turns the whole compare tree into
one predicated block (the target pass of 7.7, "Branchy loops", fits).
Until then keep GCC's default.

**Conditional returns** (`ct %ctpr3 ? p`, lcc's early exits from leaf
functions, bzip2's `mainGtU`; the proof-of-concept back end, P27,
bzip2 −9 2.6% faster) need three things from the port: (1) GCC accepts
a conditional return only as a plain `(set (pc) (if_then_else c (return)
(pc)))` (`condjump_p`, jump.cc:789; cfgrtl's "return not followed by
barrier"), so the pattern can't carry `(use %ctpr3)`: keep `%ctpr3` live
to the end through `EPILOGUE_USES` (df-scan.cc:3638) in every function
with conditional returns, also those that call
(`df_update_exit_block_uses` once known); (2) sched-deps gives
registers live at a jump's target a latency-0 anti dependence, which put
`return %ctpr3` and the conditional `ct` in one instruction, which gas
rejects before v6 (control-transfer.md, "Restrictions"): add a true
dependence; (3) what a return block computes, copied above the branch,
runs on the other path too and must be speculative, and a value written
under the condition into a register still live on the other path needs
its dependence on later reads dropped, or every exit waits for the one
before.

**In functions that call** (P56; lcc `-O3` on v3, v5 and v6): lcc
prepares `return %ctpr3` right after the last call before an exit (one
instruction later when that one already holds a prepare or a `ct
%ctpr3`), and every later exit shares that prepare, even across a call
under a predicate that excludes the exit's (`call %ctpr1 ? %pred1` …
`ct %ctpr3 ? %pred3`; a call under a false predicate leaves the CTPRs,
control-transfer.md). The proof of concept copies the return block's
prepare into each block that returns through it (only where `%ctpr3`
is dead on the other path; a prepare needs no `,sm`) and, in machine
reorg, deletes a prepare that an earlier one already holds on every
path in with no call between; no GCC pass does this, since the prepare
appears with the epilogue, after postreload CSE. 127 of Lua's 1,398
return prepares went, each a 6-cycle wait: Lua 0.7–1.0% fewer cycles,
speedtest1 1.4%, LAME 0.2% on the 8C2.

**Sibling calls** (P91; `spec/abi/calling-convention.md`, "Tail calls").
lcc makes `return g (x)` a jump at `-O2` and `-O3`: `disp %ctpr1, g`,
the arguments in the caller's own `%dr0`… (a `ct` keeps the window) and
`ct %ctpr1`, also from functions with a frame or stack arguments. It
passes stack slots in an area of its own frame even where the incoming
one would do, and releases neither that area nor its frame before the
jump, so a chain of tail calls grows the data stack until the last
callee returns (16 bytes a call for one argument, measured on the 4C,
8C2 and 2C3). GCC's `gcc.dg/sibcall-3.c` and `sibcall-9.c`, which check
that a local's address stays put across sibling calls, therefore abort
with lcc's code; `sibcall-4.c` and `-10.c`, without arguments, pass.
GCC's sibling calls reuse the incoming area (a callee needing more stack
slots than the caller got is refused, calls.cc:2631-2641) and need a
`sibcall_epilogue` pattern or the `emit_epilogue_for_sibcall` hook
(calls.cc:2564-2571); a function with a frame gives it back before the
jump with a positive `getsp`, which lcc uses to free a VLA at the end of
its scope [lcc 1.29.16 `-O2`] (before a jump: inferred, not checked);
that would pass all four tests (inferred). The proof of concept turns
sibling calls off (`TARGET_FUNCTION_OK_FOR_SIBCALL`, target.def:3147),
so sibcall-3, -4, -9 and -10 fail for it: they are xfailed only on the
targets they list (gcc.dg/sibcall-3.c:8).

Two hardware facts shape this (`spec/isa/control-transfer.md`, measured
on the 4C):

- **Every `call` and every return empties all three CTPRs**, and a taken
  `ct` through an empty CTPR traps (SIGILL). Model `call` as clobbering
  `%ctpr1–3` (they are call-used anyway, registers.md) and never keep a
  prepared value live across a call; the return must be prepared again
  after the last call. A call under a false predicate leaves them
  (`preds.c` `callf_*`), which lcc uses (above). A `ct` with a false condition never traps, so
  prepares may be speculative and hoisted above the condition.
- **A taken `ibranch` costs 6 cycles; a taken `ct` prepared in time costs
  nothing**, calls and returns included. The M1 `ibranch` form is
  correct but slow for loops and likely-taken branches, which is why M5
  matters; lcc uses `ibranch` almost never.

### D5. Literals and constants

Operands can be 5-bit literals in the syllable or 16/32/64-bit literals
in LTS slots (max 4 × 32 bits per wide instruction). In M0–M3 each
wide instruction holds one op, so literals never run out. From M4 the DFA
must count literal slots as a resource (section 7.2), and a size check
after bundling closes the groups the DFA can't judge (section 7.4,
"Count each wide instruction's size").

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
  value is needed, as lcc does. That is also the fast way: under
  `dbl = 0` a 64-bit reader of an unextended 32-bit result waits 4
  cycles, whether it is `addd`, `getfd`, `cmpedb`, `merged`, `faddd`,
  an address or `std`'s data, while `sxt` takes 1 and a 32-bit reader
  (`getfs`, `cmp*sb`, `stw`) waits nothing extra
  (`spec/isa/timing.md`, "Formats"). So use the 32-bit forms on SImode
  values: `getfs`, not `getfd`, for a field of one (the
  proof-of-concept back end, I51: a pipelined loop taking `getfd` of an
  `xors` result ran at 3 cycles an iteration, with `getfs` at 1). That
  alone is not enough: GCC forms
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
- Partly written registers can keep a diagnostic value in the upper
  half, and storing such a register with `std` traps
  (`calling-convention.md`, "Unused parts of a slot", confirmed on the
  4C, 8C2 and 2C3; the empty value that registers a window gains hold
  does not trap). Spill and pass 32-bit values with `stw`, or extend
  them first; `std,sm` does not trap on any of the three. lcc also
  zero-initialises locals that may be read uninitialised
  (`source-compat.md`).

The ABI still requires callers to **sign**-extend signed narrow
arguments (emit `sxt`), and callees must not rely on the upper bits they
receive (lcc re-extends).

### D8. Floating point

Same registers as integers (no separate FP class). `long double`
(`XFmode`) lives in one register: mantissa in the ordinary 64 bits, sign
and exponent in the 16-bit extended part (`movif`/`movfi` pack and
unpack it; loads and stores are `ldd`+`ldh`, `std`+`sth`). A function
that holds one in a window register, across a call or not, even for a
few instructions between a `movif` and a `movfi`, must set `nfx=0`.
With `nfx=1` the extended part is lost when the window spills in a deep
call, whenever a signal handler runs meanwhile on the 8C2 and 2C3 (on
the 4C when the handler's frames push the window out), and now and then
at an interrupt; system calls and page faults keep it
(`spec/abi/registers.md`, `nfx`, measured on all three machines; the
proof of concept's A26). lcc sets `nfx=0` in every function that
computes with an 80-bit value, leaves included; one that only copies or
passes a `long double` as its two parts keeps `nfx=1`, unless it
directly follows such a function in the file (an lcc artifact,
registers.md).
80-bit operations follow `%fpcr`/`%fpsr` (x87 control and status
words), not `%pfpfr`, so rounding-mode patterns and `fenv` support must
handle both (`spec/abi/registers.md`, "Floating-point control and
status", which also gives the bit layouts). `TARGET_ATOMIC_ASSIGN_EXPAND_FENV` (C11
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
80-bit operation: ordinary 64-bit spills and results bit-identical to
lcc on the 4C, but `x` operands then need an extended register
assembled before the asm and split after it. The pair model needs
`nfx = 0` too: between an operation's `movif` and its `movfi` the value
is in an extended register, and a signal arriving then loses the 16-bit
part under `nfx = 1` (the proof of concept counted 104 and 143 wrong
results in 8.2 million on the 4C and 8C2 with its operands kept in
window registers, none with `nfx = 0`; A26), so every function that
holds an extended value in a window register, however briefly, needs
it. Building the packed operands in `%g16`–`%g31` would avoid the loss
at signals, since the kernel restores those registers' extended parts
after a handler (`spec/abi/registers.md`, `long double`), but not
across calls and system calls, which clobber them. lcc puts short-lived
80-bit values there and still sets `nfx = 0`: in every function that
computes with one, also in a leaf without calls whose 80-bit value only
ever sits in `%g16` [lcc 1.29.16 `-O2` and `-O3`, v3.4c, v5.8c2,
v6.2c3, and 1.31.05: `tools/abi/probes/pocplan.c` `ld_leaf`,
`pocabi2_nfx.c`]. Pick one model before M3; both cost `nfx = 0`; lcc's
adds two-part spills (`std` + `sth` of `movfi`'s result), the pair model
a `movif`/`movfi` (4 cycles) around every operation.

The port's programs link MCST's libm, which misbehaves once a program
clears the sticky FP flags or rounds down (`spec/abi/registers.md`,
"MCST glibc's FP bugs"). This matters when `sqrt` is expanded inline
with `-fmath-errno`, GCC's default: `sqrt` of a negative number must
then set `errno` to `EDOM`, which only the library call does.

Compute the root on every path with plain `fsqrtid`/`fsqrttd` (not
`,sm`, unless FP speculation is allowed; section 7.6), and call `sqrt`
after it for errno only, ignoring its result. Keep the call after the
root explicitly: after reload the scheduler orders a call against an
operation on registers only through the registers they share and the
call's clobbers (sched-deps.cc:3836-3907), and orders a trapping
operation (any FP arithmetic under `-ftrapping-math`,
rtlanal.cc:3259-3262) only after an *earlier* call that may not return
(sched-deps.cc:2951-2955). So once ce3 has predicated the call into the
root's block (D3), sched2 may issue it first, and with `FE_INVALID`
clear libm then traps (the proof of concept; registers.md, "MCST
glibc's FP bugs"). The proof of concept makes the call read the root's
result: a copy into `%b[1]`, which `sqrt` ignores, listed as a use of
the call (P91). The root then raises
invalid before the call, so libm's own 0/0 is an ordinary NaN and the
call returns; the root's NaN for −1.0 is the one libm returns [hw:
`tools/abi/probes/libmchk.c` `rootfirst`, 4C, 8C2, 2C3]. The inline
root is right on every path: `fsqrtid` + `fsqrttd` and `fsqrts` give
MCST glibc's results bit for bit in all four rounding modes, NaNs
included, and raise invalid exactly for a negative operand or a
signalling NaN [hw: `tools/isa-probes/pocchk5.c` `sqrt`, 4C, 8C2,
2C3], so the call is there for `errno` alone. The shape is also faster
than GCC's (below), in which if-conversion makes the root wait for the
operand test (`fcmpuddf` and an integer compare, 6 cycles before
`fsqrtid` can issue): the proof of concept, keeping `.SQRT`
unconditional through a target macro it reads in tree-call-cdce.cc, ran
NPB EP class A about 1% faster on all three machines (P85).

GCC's default is the other shape. The pass that makes it is call DCE
(`tree-call-cdce.cc`; its header comment, :96–103, shows the result:
`if (__builtin_isless (x, 0)) y = sqrt (x); else y = IFN_SQRT (x);`,
a quiet test). `use_internal_fn` (:1185–1260) adds the internal
function `.SQRT` (only when the port has a `sqrt<mode>2` pattern,
which then expands it: `internal-fn.def:408`, `builtins.cc:2304`) and
keeps the libm call for out-of-range operands only;
`shrink_wrap_one_built_in_call_with_conds` (:918; its `bi_newcall`
part, :1050–1088) moves `.SQRT` into the in-range arm and joins the
two results with a PHI. On the error path nothing raises invalid
before the call, so with `FE_INVALID` clear `sqrt (-1.0)` dies with
SIGILL inside libm (`libmchk.c` `rootlast`). The pass reads no target
hook, so the shape above needs a change to it (inferred).

lcc's own expansion has the same problem. Its call path traps the same
way, and at `-O3` its root is `fsqrttd,sm`, which returns a diagnostic
for `sqrt (2.0)` when `FE_INEXACT` is clear [lcc 1.29.16 `-O3`, v3.4c:
`fsqrtid,5,sm`, then `fsqrttd,5,sm … ? %pred0` and `call … ? ~%pred0`
for `return sqrt (x);`].

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
- **Extensions in loop recurrences** (P45): with `SImode` promoted
  (above), each 32-bit operation on an `int` variable is followed by an
  `sxt` back into its `DImode` register, and in a loop that `sxt` sits
  in the recurrence (the proof of concept: nussinov's `max` at II 3,
  lcc's 2, on the 2C3; `s += a[i]` at II 2, lcc's 1). ext-dce (`-O2`,
  before combine) removes none of them: a promoted subreg read by a
  compare or an `if_then_else` (`cmplsb`, `merges`) counts as reading
  all 64 bits (ext-dce.cc:1400–1416; such codes are not
  `safe_for_live_propagation`, :151–153), and a 64-bit use after the
  loop (a returned sum) keeps the extension inside it, since ext-dce
  deletes extensions but never moves them. lcc keeps such values in 32
  bits (`adds`, `cmplsb`, `merges`) and extends once after the loop, or
  into another register where a 64-bit use in the loop needs it (lcc
  `-O3`, v3–v6). The proof of concept does it after register
  allocation: an extension whose uses all read 32 bits becomes a copy,
  one whose 64-bit uses all follow the loop moves to the loop's exits.
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
- **The last fixed argument of a variadic call counts the hidden result
  pointer** (A24; `spec/abi/calling-convention.md`, "Variadic
  functions"). The caller stores the last fixed argument and every
  variadic one to its slot as well, and when a C++ class comes back
  through the hidden pointer in slot 0, "last fixed" is the last
  declared parameter one slot further on (`S f (long a, long b, ...)`:
  from slot 2; with no declared parameter, from slot 0). GCC already
  gives you that count: `INIT_CUMULATIVE_ARGS` receives `n_named_args`,
  which includes the pointer when it is passed as an argument
  (tm.texi:4441-4444; calls.cc:2996-3006, :3014), and `this` too, since
  it is in a method's `TYPE_ARG_TYPES`. Keep it in `CUMULATIVE_ARGS` (it
  is one too large for a function that isn't variadic, harmless there)
  and store argument `max (n_named_args - 1, 0)` and every later one. A
  call without a prototype stores every argument (`n_named_args` is then
  the number of actual arguments: test `prototype_p`). Don't count
  `TYPE_ARG_TYPES` yourself: the proof of concept did, stored one
  argument too early, and an lcc callee read garbage. Don't rely on
  `function_arg_info::named` either: once the port defines
  `TARGET_SETUP_INCOMING_VARARGS`, `pretend_outgoing_varargs_named`
  defaults to true (targhooks.cc:249-253) and `expand_call` marks every
  argument named (calls.cc:3035-3047). The callee side needs nothing:
  GCC puts `.result_ptr` first among the parameters
  (function.cc:2474-2496) and calls `setup_incoming_varargs` after the
  last one (:3809-3810), so `va_start` already skips it.
- **`va_list` is `char *`** (A27; `spec/abi/calling-convention.md`). GCC's
  default `__builtin_va_list` is `void *` (`std_build_builtin_va_list`,
  builtins.cc:5276-5282), and C++ mangles a `void *` parameter `Pv` where
  lcc's names say `Pc`, so no C++ function taking a `va_list` would link
  against lcc-built code. Define `TARGET_BUILD_BUILTIN_VA_LIST` to return
  `build_pointer_type (char_type_node)`, as C6x does (c6x.cc:676-679,
  :6732-6733).
- **PIC jump tables:** GCC emits them as offsets from the table
  (`ADDR_DIFF_VEC`) and leaves adding the table address to the port's
  `tablejump` expander.
- **Packed shifts read the whole 64-bit count** (a count of the lane
  width or more gives 0 or the sign, semantics.md), but under
  `SHIFT_COUNT_TRUNCATED` GCC's `expand_shift_1` hands a shift the wider
  register of a lowpart subreg: a `V2SI` holding the count in both lanes
  made `psllw` read `c | c << 32` (`gcc.dg/vect/pr88903-1.c`, P29). Mask
  the count to the lane in the vector shift expanders and keep the
  packed shifts as unspecs, so combine can't drop the mask again;
  `SHIFT_COUNT_TRUNCATED` is right only for the scalar shifts.
- **Byte-swapped accesses** (MAS 0x8, semantics.md) need proven
  alignment: fold `bswap` into a load or store only where
  `get_object_alignment_2 (MEM_EXPR, …, addr_p = true)` proves it, not by
  `MEM_ALIGN`, which under `STRICT_ALIGNMENT` is the pointer type's, so
  `*(uint32_t *) (buf + 1)` would pass and SIGBUS (P34; lcc folds only
  under `-ffast`, and then also traps there). A MAS goes in CS1, which
  `setwd`, `setbn`, `setbp`, `wait` and `call` also use (bundle.md).
  But CS1 has one MAS field for each of ALC0, 2, 3 and 5, so MAS
  accesses never exclude each other, only those control operations.
  Model a `cs1` unit that the control operations reserve, with an
  `exclusion_set` between it and the MAS accesses (7.2), not one CS1
  unit that every MAS access reserves. That single unit held the proof
  of concept's NPB IS histogram kernel to II 4, where lcc's runs at II 2
  with a lock load, a check and an `rbranch` in one instruction (P69;
  7.6, item 4).
- **Costs** (P37): price a division by a constant as `expand_divmod`'s
  sequence (a high-part multiply and shifts, about 6; a remainder 11;
  powers of two 1–4), not as a divide: doloop rejects a loop whose
  iteration count costs more than `max-iterations-computation-cost` (10
  instructions, `loop-doloop.cc`), so `while (len > 2) { …; len -= 3; }`
  (zlib's `inflate_fast`) otherwise stays a branch loop. `expand_divmod`
  itself prices a division by a register, so this doesn't change how
  divisions expand. Integer divides take 11–18 cycles by the quotient's
  size, 2 more to an integer operation (timing.md).
- **Combined integer operations** (`shr_andd`, `xor_xors`, … in ALC1/4,
  latency 2) only where a loop is bound by the six ALU channels and the
  pair is off its recurrence; formed by default they made the PoC's
  LZMA and gunzip benchmarks 0.7–1.3% slower on the 8C2 (P38). Under
  `dbl = 1` the 32-bit ones zero-extend their result like the plain
  32-bit operations (registers.md).
- Facts from `spec/isa/semantics.md` that are GCC settings:
  `SHIFT_COUNT_TRUNCATED` 1 (scalar shifts only, above); `fmul_add*`
  are not fused: they round twice, as RTL's `(plus (mult a b) c)`
  means, so they need no `-ffp-contract` (lcc forms them at `-O3` with
  `-ffp-contract=off` too) and never implement `fma` (v6's `fma*` are
  fused, "What a v6 mode adds"); where to form them is a per-loop
  choice (7.7, P61); `fmax`/`fmin` are
  x86-style, not C's;
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
  gcc.dg/torture/pr91323 passes). The register compares' flags reach an
  integer consumer in 4 cycles in either cluster
  (`spec/isa/timing.md`, "Clusters"): unlike other FP-side results
  they pay no crossing cost, so don't pad after them (the proof of
  concept's channel assigner had padded 2 cycles after every quiet
  compare), and pipeline them like any FP operation (P82). lcc's `c +=
  isless (a[i], b[i])` is a one-instruction kernel on v5 (`fcmpuddf,sm`,
  `cmpesb,sm r, 1`, `merged,sm`, the `addd` and two `movad`); its
  `isunordered` count runs two elements a cycle with `fcmpuoddb` into
  predicates; in `x = isless (x, a[i]) ? b[i] : x` it loads `b[i]`
  speculatively and selects with predicated moves [lcc 1.29.16 `-O3`,
  v5.8c2].
- **FP min and max from conditional moves** (P77): `fmaxd a, b` and
  `fmind a, b` are exactly `a > b ? a : b` and `a < b ? a : b` (they
  return b for a NaN or two zeros; `spec/isa/semantics.md`). Recognise
  those selects, in either arm order and for GT/LT only, in the
  `mov<mode>cc` expander and emit them as unspecs, as i386 does
  (`ix86_expand_sse_fp_minmax`, i386-expand.cc:4166: LT, UNGE with the
  arms swapped, LE only without NaNs). Use plain `smax`/`smin` only
  under `-ffinite-math-only -fno-signed-zeros`. lcc `-O3` also makes
  `a >= b ? a : b` and `a <= b ? a : b` into `fmaxd`/`fmind`, which
  gives −0 for `+0 >= −0 ? +0 : −0` where C gives +0 [hw: 4C; lcc
  1.29.16 and 1.31.05; `-O2` and below keep a compare and a select]:
  don't copy that. C99's `isless` … `isunordered` reach the back end
  as UN* codes (`islessgreater` as an inverted UNEQ,
  builtins.cc:11018-11020) and take the quiet compares (above). lcc's
  forms: `isless (a, b)` is `fcmpuddf` and `r == 1`, `islessequal` two
  compares of r and `landp`, `islessgreater` the quiet predicate
  compares `fcmpoddb` and `fcmpneqdb` with `andp`, `isunordered`
  `fcmpuoddb`, and `long double` uses `fxcmpudxf` (semantics.md,
  "Floating-point compares") [lcc 1.29.16 `-O3`, v3 and v5].
- **`getsp` amounts** (alloca, VLAs) must be in a window register or
  `%g16`–`%g31`, never `%g0`–`%g15`: the kernel cannot read those when
  the stack must grow, and the program gets SIGSEGV, but only for
  allocations past the mapped stack (`spec/abi/stack-and-unwinding.md`;
  the proof of concept's 800 KB VLA). lcc uses `%g24`. Give the
  `allocate_stack` pattern a window-register constraint.
- **Short label-difference tables** crash MCST gas when a `.text`
  label follows them (`spec/isa/bundle.md`, "Known gas bugs"): keep
  `CASE_VECTOR_SHORTEN_MODE` at `SImode` or wider.
- **A load soon after a store to the same place** (a spill and its
  fill, a stack argument read back, a field beside one just written)
  is served by L2 and gets its value 6–8 cycles late
  (`spec/isa/timing.md`, "Store to load"). v3: a load of the stored
  aligned doubleword in the very next instruction; give the scheduler
  that dependence a latency of 2 rather than 1. v5 (the 8C2) adds any
  load of the store's 32-byte line 2 or 3 cycles after it that reads a
  byte the store didn't write; v6 (the 2C3) a load of the stored
  doubleword at another width up to 4 cycles after it. sched-deps links
  a load to an earlier store only when they may overlap
  (`true_dependence`, sched-deps.cc:2675), so the v5 case (another
  field of the line, through the same base) has no dependence to carry
  a latency; it needs a check where instructions are formed. lcc
  doesn't avoid it either: its v5 `core_list_reverse` kernel pays it on
  CoreMark's lists, and so did the proof of concept's (I45).
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
- **lcc's system-call asm** (P87; `spec/abi/source-compat.md`, "Inline
  assembly"; `spec/abi/kernel-interface.md`, "System calls"). glibc's
  `INTERNAL_SYSCALL` (`call %%ctpr1, wbs = %#`, clobbers `"call",
  "memory"`) and the kernel's uapi `__E2K_SYSCALL_*` macros (clobbers
  `"ctpr1"`–`"ctpr3"` and `"b[0]"`–`"b[7]"`, no `"memory"`) need five
  things that GCC's defaults don't give:
  1. **`%#`**: accept `#` in `TARGET_PRINT_OPERAND_PUNCT_VALID_P`
     (final.cc:3572-3575 then calls `TARGET_PRINT_OPERAND` without an
     operand) and print, in hex, the window base the function's own
     calls use. Inside an asm, final sets `this_is_asm_operands`
     (final.cc:2632, cleared at :2657) and leaves `current_output_insn`
     null (it is set only for recognised insns, :2780, and cleared after
     each, :2869), so a printer that must tell an asm's `%#` from a `%#`
     in the port's own templates tests `this_is_asm_operands`; simpler
     to keep `#` out of the port's templates. A function with such an
     asm needs the 4 outgoing pairs and the `setbn` that lcc gives it
     (`wsz = %# + 4`; lcc 1.29.16 `-O3`: `wsz = 0x9`, `setbn rsz = 0x4,
     rbs = 0x4, rcur = 0x1`, `wbs = 0x5`; `tools/abi/probes/pocplan.c`
     `sys_getpid`). A system call, like any call, needs a `wbs` of at
     least the pairs the function was handed, 4 for a C function, so
     `%#` is never below 4 (the `call` itself is SIGSEGV `SEGV_BNDERR`
     otherwise: D1; the proof of concept's first leaves, small windows
     with `wbs` 1, died so, A25). Decide which
     functions need the area from the asm, not from its RTL, which
     doesn't show the `%b[]` the text writes or the call it makes: lcc
     sets the area up for every `asm volatile` and for any asm with a
     `"call"` or `"b[N]"` clobber (source-compat.md, "`%#`"); a `%#` in
     `ASM_OPERANDS_TEMPLATE` finds the rest.
  2. **`"call"`**: GCC accepts as a clobber only a register name, an
     `ADDITIONAL_REGISTER_NAMES` or `OVERLAPPING_REGISTER_NAMES` alias,
     `"memory"`, `"cc"` or `"redzone"` (varasm.cc:1013-1093); anything
     else is "unknown register name" (cfgexpand.cc:3499-3504). The proof
     of concept makes `"call"` an `ADDITIONAL_REGISTER_NAMES` alias of a
     register no asm names otherwise (the argument pointer; any register
     in `accessible_reg_set` other than the PIC register may be
     clobbered, cfgexpand.cc:3347-3371), and its `TARGET_MD_ASM_ADJUST`
     (target.def:4573-4590, called at cfgexpand.cc:3830-3833 with the
     clobbers and `clobbered_regs`) replaces that clobber with what a
     call clobbers: every call-clobbered register that isn't fixed
     (`%g16`–`%g31` included, which a system call empties),
     `%ctpr1`–`%ctpr3`, and a memory clobber (`(mem:BLK (scratch))`,
     what `"memory"` makes, cfgexpand.cc:3506-3509), leaving out the
     registers the asm's own operands are in. Give `b[0]`–`b[7]` as
     register names or aliases so that `"b[0]"` and `"%b[0]"` decode
     (GCC strips one `%`, varasm.cc:978-987), and treat a `"b[N]"`
     clobber like `"call"` with `nfx = 0`, as lcc does.
  3. **Memory at the GIMPLE level**: the tree optimizers see an asm write
     memory other than its `m` outputs only when its clobbers include
     the literal `"memory"` or it is a non-empty basic asm
     (`gimple_asm_clobbers_memory_p`, gimple.cc:3058-3074, read by
     tree-ssa-operands.cc:753-755), volatile or not, so FRE reuses a
     value loaded before it (host GCC 16.2.1 `-O2` reads `buf[0]` once
     across such an `asm volatile`); a memory clobber added by
     `TARGET_MD_ASM_ADJUST` reaches only RTL, too late
     (source-compat.md, "What lcc assumes an asm does"). glibc lists
     `"memory"`; the uapi macros rely on lcc treating every `asm
     volatile` as a memory barrier (lcc loads `buf[0]` again after a
     uapi-style `read`, `pocplan.c` `sys_read_twice`). Either change GCC
     (a target hook consulted by `gimple_asm_clobbers_memory_p`; the
     proof of concept added a macro there) or accept that the uapi
     macros don't clobber memory with GCC, as GCC's manual says of any
     asm.
  4. **The return prepare**: a leaf whose `return %ctpr3` is hoisted to
     its first instructions (D4, M5) must not have it before an asm that
     clobbers `%ctpr3`, `"call"` or a `"b[N]"`: the prepare is then dead
     to dataflow, DCE deletes it, and the `ct %ctpr3` traps (SIGILL; the
     proof of concept). lcc prepares the return after every `asm
     volatile` (`sys_getpid`: `return %ctpr3` in the instruction after
     the asm; source-compat.md, "What lcc assumes an asm does").
  5. A port pass that moves values out of `%b[]` (D1) must leave an
     asm's `%b[]` clobbers and the `setbn` they need alone (the proof of
     concept's pass renamed them and dropped the `setbn`).

  With these the proof of concept's uapi and glibc-style `getpid`,
  `read` and `write` returned the right results at `-O0`, `-O2`, `-O3`
  and `-O2 -fPIC` on the 4C, 8C2 and 2C3, reloaded memory a `read`
  wrote and kept values live across the call in window registers (its
  runs, not rechecked).

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
  `exclusion_set`s. Add a `cs1` unit for `setwd`/`setbn`/`setbp`, `wait`
  and `call`, excluded with every load or store that carries a MAS, and
  let `ibranch` and `rbranch` take both `ctp` (CS0 holds their
  displacement) and `ct` (D10; bundle.md). `setwd`, `setbn` and
  `setbp` share one CS1 (opcodes 0, 1 and 4 carry the `setbn`/`setbp`
  bits, bundle.md), as in lcc's first instructions (M1), so they must
  not exclude one another: emit them as one insn (D2) or give each its
  own unit.
- **Reservations**: one per class of operation, e.g. "simple integer op in
  any of ALC0–5", "load in ALC0, 2, 3, 5", "store in ALC2 or 5", "divide in
  ALC5", "128-bit op in a channel pair". Which channels each operation may
  use, per ISA version, with its channel pairs, ALES, literal slots and
  load/store/MAS class, and which syllable or field each control
  operation takes, is in `spec/isa/reservations.yaml` (section 11, item
  3; generated from `binutils-ops.yaml`'s `channels` and bundle.md's
  rules). That table is large and mechanical: generate the reservations
  from it with a script in the port, as C6x does with `gensched.sh`
  (`c6x/t-c6x:21–37`).
- **Latencies** (`define_insn_reservation` latency and `define_bypass`):
  use the measured table in `spec/isa/timing.md` (data in
  `spec/isa/timing-e2s.yaml`, from `tools/isa-timing/bench.py`): integer
  1, combined integer 2, FP and integer multiply 4, combined FP 8,
  `fdivs`/`fdivd` 11/14, `fsqrts` 12, a double square root `fsqrtid` 11
  + `fsqrttd` 14, `sdiv` 11, load 3 (L1 hit), compare→predicated op 2,
  compare→`ct` 3, `disp`→`ct` 5, `return`→`ct` 6, `movtd`→`ct` 9, FP,
  multiply or divide (integer divides too) → integer +2; every other v3
  operation is in timing.md, "Every v3 operation", by latency class.
  Crossing between the channel clusters ALC0–2 and ALC3–5 costs +1 for
  1-cycle integer and load results and +2 for FP-class ones and for
  the 2-cycle integer ones (x86 flag forms, combined integer
  operations; timing.md, "Clusters"); the guide's "integer → FP +1" is
  this crossing. Model it with `define_bypass` or by tracking the
  cluster.
  These match guide 6.1. Where the guide is wrong or silent: packed
  integer ops take 1 cycle (multiply 2) to packed/FP consumers and 4 to
  integer ones; conversions 4 (6 to integer), but an FP-side result
  read in another format than it was produced in costs 5 or 6 (a
  double read as a single, a single as a double: timing.md, "Formats";
  a `define_bypass` per format pair); FP compare → predicated op
  4, → `ct` 5; `andp` chains inside one wide instruction (0) and costs 1
  through a `%pred`, 2 to a predicated op and 3 to `ct`. Schedule loads
  for an L1 hit, as lcc does outside pipelined loops (inside them, 7.7,
  "Plain loads in a pipelined loop"). A stall
  costs about a whole latency extra, so latencies must be exact. v6
  differs (measured on an Elbrus-2C3, timing.md "Elbrus-2C3 (v6)"):
  load→use 5 in either cluster, `ldqp` 5, integer compare or `andp` →
  predicated op 1; everything else as v3. So `-mtune` needs per-version
  latencies at least for loads and predicates; a v3 schedule run on
  v6 stalls 4 cycles at every load consumed 3–4 cycles later.
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
- **The frame setup is no scheduling barrier** (P73). sched-deps makes
  an `unspec_volatile` a `TRUE_BARRIER` (`sched_analyze_2`,
  sched-deps.cc:2758–2774), and every later insn gets a true dependence
  on it (`sched_analyze_insn`, sched-deps.cc:3388). A volatile `getsp`,
  or a `blockage` after it, makes the whole body wait `getsp`'s 2
  cycles and keeps prepares and argument loads out of the first
  instructions; the proof of concept also had a rule that kept CTPR
  operations a cycle after `setwd`. Describe `getsp` as setting the
  stack pointer register (plus its result), so that only accesses
  through the new stack pointer depend on it, and let a `disp` or
  `return` share `setwd`'s wide instruction (CS0 against CS1). If
  `setwd` stays volatile, as ia64's `alloc` is (D2), give its
  dependences latency 0 in `TARGET_SCHED_ADJUST_COST`. lcc's first
  instructions are in M1, "Prologue/epilogue".
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

**Waits longer than 7 cycles**: the HS `nop` field holds 7 at most, so
print the rest of a longer wait as empty wide instructions (`{ nop K }`;
gas accepts `nop 0`). Never shorten a wait: an operand early by e
cycles stalls 4 × ceil(e/4), so the PoC, which capped waits at `nop 7`,
ran `x = x / d + k` at 21 cycles an iteration instead of 16 (P35).

For correctness, the only non-interlocked hazards measured on the 4C
are the loop-register writes: `rwd %lsr` (and v5+ `%lsr1`) → `rrd` of
either or the loop's first instruction (4 cycles), → `rrd %ilcr` (5;
timing.md, loops.md); loop-mode code (7.7) must respect them.
Privileged state registers (which gas pads) don't appear in user code.

**Count each wide instruction's size** (I52). At most 16 words fit
(`spec/isa/bundle.md`, "Counting the size"), and the DFA can't hold that:
two operations with the same literal value share an LTS, a constant 0–31
is free as `src1` but half an LTS as `src2`, half-syllables pair into
words, and the total rounds up to an even count. Compute the size the
way gas does, from each insn's channel and literals, in the pass that
marks the groups (step 1 above), and start a new group when an insn
would take one past 16 words. It rarely bites: 0.02–0.13% of lcc's wide
instructions are 16 words, mostly six ALU operations with a predicate
tree of two or three PLS words, CDS words and three or four literal
words, and on v3 also function entries (`setwd` and `setbn`, a prepare
with `ipd`, `getsp` and five more operations with literals: 15 words,
so one more CDS word or `mova` fits, two more words don't). Print a
commutative operation's small constant first, since gas never swaps
operands (`addd 0x14, %dr0, %dr1` needs no LTS, `addd %dr0, 0x14, %dr1`
half of one), and leave the literal slots to gas (an explicit `_ltsN`
with a lower slot left empty costs that slot's word, rounding aside).
`tools/isa-extract/check_reservations.py` computes the same size for
compiled code (the size part of `Checker.bundle`). The proof of concept's count after
channel assignment matched gas's for 95% of the 71,299 instructions of
its Lua build and was 2–4 words high for the rest, mostly from counting
every constant above 15 as a literal (its figures).

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

Some crossings can't be avoided by any channel assignment: with four
loads in ALC0/2/3/5 an instruction's two ALU operations sit in ALC1 and
ALC4, and an operation reading both in the next cycle crosses one way or
the other; one result with four readers in the next cycle needs four
channels of one cluster. Since an operand early by 1–3 cycles stalls a
whole 4-cycle step, **pad 1–3 empty cycles after channel assignment**
where an operand would still be early (after a wide instruction that
falls through, with no label between): zlib's crc32 braid block 46 → 26
cycles, CoreMark +3.4% on the 4C (P36).

Size the assignment for a whole wide instruction (P80). lcc's pipelined
kernels fill all six ALU channels and add `mova`s, predicate logic and
the loop's transfer. In lcc 1.29.16's SQLite for v5 at `-O3` the
fullest instruction holds 12 operations, counted as a port's insns: 6
ALU, 3 `mova`, 2 `landp` and a `ct` with `alc`/`abn`/`abp`. 0.35% of
the instructions hold more than 8, nearly all in loop kernels. The
proof of concept's assigner gave up above 8 insns and left exactly
those instructions to gas, which put consumers in the wrong cluster, so
the modulo scheduler's crossing costs didn't hold where they matter
most. Only the ALU operations (at most 6) and the `mova`s (at most 4,
in the AAS) have a channel to choose; prepares, transfers,
`set*`/`wait`/`call` and predicate logic have fixed syllables
(bundle.md). Search over those two groups only (at most 9 operations in
that SQLite build, 10 by the encoding), and treat an instruction the
search can't assign as a bug to report, not a case to leave to gas.

Check write-back conflicts per issue cycle (P81; timing.md, "Write-back
conflicts"). An integer operation that writes a register, or a load,
placed in channel N at cycle t makes the FP-side result issued in
channel N at t − 2 a cycle late, when its consumer is an integer
operation or an FP one in the other cluster: with the consumer at the
producer's latency, a 4-cycle stall step. FP-side here means FP
arithmetic, integer multiplies and the multiply flag forms (`smulx_fw`,
`umulx_fw`). The delay holds whatever channel N issued at t − 1
(measured on all three machines with `damwb.c`), so keep, per channel,
the FP-side operations of the last two cycles. Keeping only the
channel's latest one missed the conflict in the proof of concept: with
two `smulx_fw` back to back in ALC0, an `addd` placed in ALC0 two cycles
after the first stalled every iteration 4 cycles, 1.5 instead of 1 cycle
an element. The cycles are those of the schedule as written
(instructions and their `nop`s): a stall the hardware inserts between
the two, or in the integer operation's own instruction, doesn't remove
the conflict, so a scheduler needs no stall model to check it. The
proof of concept's Linpack daxpy kernel (I35) lost a third of its speed
this way: at II 4 the bundler put the x pointer's `addd,1` 2 cycles
after an `fmuld,1` whose product a `faddd` in the other cluster read
exactly at its latency, and every other iteration stalled (6 cycles an
iteration instead of 4; timing.md, "Write-back conflicts").

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

- Consumers of a speculative value need `,sm` only where they run
  unconditionally: a non-speculative operation predicated on the guard
  doesn't trap on a diagnostic operand when its predicate is false
  (speculation.md; lcc's `if (c) r = *q + 7` is `ldw,sm` + a predicated
  plain `adds`).
- Stores have no speculative form for their address: a `,sm` store to
  NULL or an unmapped page faults like any store (speculation.md), so a
  store can never run before its guard; a predicated store removes the
  branch.
- Loads: use the semi-speculative form (`ld,sm`, MAS mod 0, lcc's
  default). On v3 a speculative load that misses the caches is not free
  even when its value turns out dead (about 8 cycles a miss on the 4C,
  22 with a TLB miss; nearly free on the 8C2 and 2C3: timing.md, "Loads
  whose value is never read"), so on v3 don't hoist loads likely to miss
  into paths that mostly don't use them. It returns real data from valid pages the process hasn't
  touched (the kernel pages them in), so its result is always usable
  when the guard holds. The fully speculative form (mod 3) can return a
  diagnostic for such a page and then needs recovery code (item 5);
  leave it out at first.
  Cost to keep in mind: the first speculative load into each invalid
  region traps into the kernel once (about 5,000 cycles), and a load
  from a valid page not touched yet is paged in even if never used.
- `volatile` and atomic accesses, and loads with a MAS (lock loads):
  never speculated, in any of the steps of item 3 (P88). lcc predicates
  them instead: `if (c) s = *v` with `volatile long *v` is `ldd ... ?
  ~%pred0` after the compare where an ordinary pointer gets `ldd,sm` in
  the function's first instruction, a relaxed atomic load the same, and
  in a pipelined loop too (`ldd,5 %r3, 0x0, %b[5] ? ~%pred1`)
  (`spec/isa/speculation.md`, "What lcc speculates"). GCC's own passes
  leave them alone: noce refuses operands with side effects
  (`noce_operand_ok`, ifcvt.cc:3844-3854), the interblock schedulers
  class a volatile load as risky (`may_trap_exp`, haifa-sched.cc:445-449;
  sched-ebb's `add_deps_for_risky_insns` then keeps its jump
  dependence, sched-ebb.cc:417) and Haifa's speculation refuses side
  effects (`sched_insn_is_legitimate_for_speculation_p`,
  sched-deps.cc:682). `may_trap_p` does not count a volatile access as
  trapping (only stack-check probes, rtlanal.cc:3148-3156), so only the
  port's own steps can break this: with step 1 below unguarded, the
  proof of concept read `*v` in `for (...) if (i & 1) s += *v` in the
  instructions of the jumps that skip it, and loaded the stack
  protector's guard, a volatile global (`__stack_chk_guard`,
  targhooks.cc:956), with `ldd,sm`. Test with `volatile_refs_p`
  (rtlanal.cc:2980) and the port's atomic and MAS patterns.
- FP operations: a speculative FP operation that would raise a flag
  not already set, or an exception that is unmasked, gives a diagnostic
  instead. That is harmless while the flags are sticky-set and all
  exceptions masked (the kernel's start state) but turns into SIGILL
  after `feclearexcept` or `feenableexcept` (instead of SIGFPE in the
  second case). Speculate FP only under `-fno-trapping-math`, and then
  only if we accept that hazard (to be decided). lcc accepts it: its
  `-fspec-fp` is on by default, and at `-O3` it makes every FP operation
  of a pipelined loop `,sm`, divisions under a condition included, and
  if-converts some straight-line FP code; a program that clears the
  flags with `feclearexcept` then dies with SIGILL in lcc's loops
  (spec/isa/speculation.md, "What lcc speculates", 4C/8C2/2C3), and
  lcc users build such code with `-fno-spec-fp`. MCST's libm is built
  that way too, so such a program can also die inside libm, whatever
  the port does (D8). A checked form is
  possible (item 5): a speculative operation reading the result, with
  `rbranch ? %dt_alN`, catches the diagnostic.
- Integer division: `sdivd,sm` by zero gives a diagnostic; safe to
  speculate as such (lcc does in loops: 64–72% of its integer divisions
  are `,sm`), but outside loops rarely worth it (11+ cycles).
- Stores: never moved above their guard, but a store whose data or
  predicate comes from speculative operations must be `,sm` (a
  diagnostic operand otherwise traps). A `,sm` store under a diagnostic
  predicate is still done, with corrupted data, so a store may take a
  speculative predicate only where that predicate cannot be diagnostic
  on a path the program really runs (lcc does this in pipelined loops
  and before data-speculation checks; speculation.md).
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
- *Loads in a converted arm* (P86; lcc 1.29.16 `-O2` and `-O3` for
  v3.4c, v5.8c2 and v6.2c3, lcc 1.31.05 for v3 and v6.2c3;
  `tools/abi/probes/pocplan.c`). lcc loads an arm's value before the
  compare, speculatively, and predicates only the copy: `if (a < b) x =
  *p` is `ldd,sm` in the function's first instruction, `fcmpltdb` into
  `%pred0`, and at `-O3` `addd,3,sm 0x0, %g16, %r0 ? %pred0` four cycles
  later (an FP compare reaches a predicated operation in 4,
  `spec/isa/predicates.md`, "Timing"), so `x` is ready one cycle after
  the predicate. `c ? *a : *b` loads both before the compare. A
  predicated load instead issues only once its predicate is ready and
  delivers 3 cycles later (5 on v6) (inferred from the latencies). In a
  pipelined loop the predicate's latency overlaps other iterations, and
  lcc's one-instruction kernels for `x = a[i] < t ? b[i] : x` use a
  `merged`, predicated copies or (v3 `-O2`) a predicated `ldd,sm`
  (`spec/isa/predicates.md`, "How lcc uses predicates"). ce3 makes the
  predicated load once FP loads are predicable; a pass after it can
  rewrite each predicated load that is its arm's last write of a
  register live after the arm as `ld,sm` into a free register (one that
  nothing later in the block writes, or the next load into it waits for
  the copy) plus a predicated copy, as the proof of concept does.
  Volatile and atomic loads stay plain predicated loads, as in lcc (item
  2). Such an arm raises no FP flag (the compare runs on every path as
  the branch's condition, and the load and the copy are not FP
  operations), so it needs no FP speculation.

  lcc predicates under an FP compare whatever the arm's frequency:
  Linpack's `idamax` (`if (fabs (dx[i]) > dmax) { itemp = i; dmax = fabs
  (dx[i]); }`, a new maximum being rare) keeps both updates under the
  `fcmpltdb` predicate in every build (`merged`/`merges`; v3 `-O3`
  unrolls by two with predicated copies). The proof of concept had
  refused such arms because predicated loads made Linpack 7% slower; in
  lcc's form it converts them, using an FP compare's predicate only
  where GCC estimates the arm at 30% or more (its own threshold, not
  lcc's). Its select chains (ns an element on the 8C2 / 4C / 2C3):
  `isless` 7.71 / 15.47 / 6.78 → 6.19 / 12.42 / 4.76 (lcc 4.69 / 9.40 /
  3.01), `<` 6.70 / 13.46 / 6.03 → 4.85 / 9.74 / 4.14; Linpack 19.47 →
  18.85 s on the 8C2, where `idamax`'s update became two merges under
  the compare and the loop pipelined (the proof of concept's figures,
  not rechecked). ce3's predication path weighs no probability: it
  counts insns against `MAX_CONDITIONAL_EXECUTE` (ifcvt.cc:5852) and
  only copies the branch probability into the `REG_BR_PROB` notes of
  predicated calls (ifcvt.cc:637-646, :429-433); only its fallback for
  an arm that ends in a jump elsewhere (`find_if_case_1`/`_2`) scales
  the arm's cost by it (`cheap_bb_rtx_cost_p`, ifcvt.cc:142, :6331,
  :6447). A rule like that threshold belongs in `IFCVT_MODIFY_TESTS`
  (ifcvt.cc:627-635), which sees the test block, whose jump carries the
  `REG_BR_PROB` note, and cancels the conversion by clearing the test.
- *Selects of two loads* (P91): phiopt in mainline GCC (17) turns `c ?
  *a : *b` into one load from a PHI of the two pointers when both arms
  load from the same memory state (`factor_out_conditional_load`,
  tree-ssa-phiopt.cc:4187-4460, tried on diamonds at :4507-4511;
  volatile loads excepted, :4212-4215; two different pointers only after
  the early pass, and before vectorization not when a pointer is an
  induction variable of the loop, :4314-4322). No option or param turns
  off this transformation alone (`-fno-ssa-phiopt` turns off all of
  phiopt, :5078). On E2K that load then waits for the compare and the
  pointer select (4 cycles from an FP compare to a `merge`, then 3 or 5
  for the load). lcc loads both before the compare with `ldd,sm` and
  selects the values (`tools/abi/probes/pocplan.c` `sel_ab`, every
  build). The proof of concept adds a param (`phiopt-factor-loads`) to
  switch the transformation off; a port needs such a GCC change (or a
  later pass that splits the load again, inferred).
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
     check. Keep the dependences of volatile and atomic loads: the
     risky-insn pass gives a volatile load its jump dependence for that
     reason (item 2).
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
- *Loads at the head of a branch's target* (P71): sched-ebb grows an
  extended block only through a fall-through edge into a block without a
  label (sched-ebb.cc:640-650), so it never moves a load from a branch's
  target into the branch's block. lcc does: Lua's `luaH_getshortstr`
  loads a node's key value (`ldd,sm`) in the block that tests the key's
  type [lcc 1.29.16 `-O3`, v5.8c2 and v6.2c3], so the value is ready
  when the test resolves. The proof of concept, loading it after the
  branch, counted 9 cycles a test on the 2C3 (5-cycle loads) against
  lcc's 3. It now copies the loads (never a volatile or atomic one) that
  start a block reached by conditional branches into those branches'
  blocks as `,sm` loads, where their register is dead on the other edge:
  Lua's k-nucleotide 2% and spectral-norm 1.3% faster on the 2C3.
- *Interpreter dispatch* (P72; Lua 5.4's `vmfetch`, the proof of concept
  on the 2C3). A handler that ends in `if (trap) {...} i = *pc++; goto
  *disptab[op]` reaches the shared dispatch block by a conditional
  branch after cross-jumping. GCC's `duplicate_computed_gotos` copies a
  computed goto's block only into predecessors with a single successor
  that fall through or end in a simple jump
  (`maybe_duplicate_computed_goto`, bb-reorder.cc:2733–2736), and only a
  block no longer than 8 unconditional jumps (`uncond_jump_length` ×
  `max-goto-duplication-insns`, bb-reorder.cc:2783, params.opt:618). It
  runs after register allocation, just before ce3 (passes.def:535, 538),
  which is where a port's own copy fits (inferred). So every interpreted
  instruction waits after the branch for the fetch (5 cycles on v6), the
  opcode, the table load (5) and `movtd` (9). Copy the block into the
  conditional predecessors as well, and start the fetch, the opcode
  arithmetic and the table load above the branch, speculatively; only
  `movtd` and `ct` then stay after it. That gave the proof of concept
  2–7% fewer cycles, putting Lua at 0.97–1.06 of lcc's cycles. lcc's own
  dispatch is slower, because its `&&label` is an index
  (spec/abi/source-compat.md, "Labels as values"): each handler loads
  the index and branches to one block that range-checks it, loads the
  address from a second table and does `movtd`, about 28 cycles from the
  instruction word's load to the transfer on v6, against the proof of
  concept's 21. When two blocks hoist into one predecessor, the second
  must see the first's hoisted registers as live into it: with liveness
  computed once, the proof of concept miscompiled Lua's `luaO_tostring`
  and gcc.dg/vect's vect-cond-7/8.
- *What pays to move above a branch* (P72, the proof of concept's
  measurements). Independent operations pay their way along any branch:
  bzip2's compression was 3.4% slower without them. A chain, in which a
  moved operation reads another moved one's result, holds the branch
  until it issues, on the other way too, so move chains only along
  likely branches (80% or more, such as `if (l_unlikely (trap))`); along
  any branch they made CoreMark 1% slower. Move nothing along a branch
  taken less than a fifth of the time: CoreMark's list search, with a
  load moved for its 11% "not found" arm, was 0.4% slower.
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

A load moved above both its guard and a store gets its check **under the
guard's predicate**: a check under a false predicate neither accesses
memory nor faults nor sets `%MLOCK`, while a check with `,sm` is no
check at all (it always reloads; speculation.md, the PoC's I29). A
check may follow its lock load in the next instruction and needs no
latency of its own; users of the register go at the lock load's
latency.

**Version straight-line code for aliasing before speculating it** (P47;
lcc's NPB bt.c at `-O3` for v3, v5 and v6; the PoC on the 8C2 and 2C3).
Each check takes ALC2 or ALC5 and an `rbranch`, so a block with many
loads after stores through other pointers runs out of instructions:
NPB BT's `binvcrhs` (5×5 updates through `lhs`, `c`, `r`) took the PoC
973 wide instructions a call with 295 checks. lcc compares the 200-byte
ranges of `lhs` and `c` (`cmpledb` of each base + 200 against the other
base, joined by `landp`) and only where they overlap branches to a copy
with lock/check pairs; it repeats the test before each of three parts
of the function and never tests the 40-byte `r` (65 pairs in all, all
in the overlap copies, 749 wide instructions). GCC versions only loops
for aliasing (`create_runtime_alias_checks`, tree-data-ref.cc:2685);
basic-block SLP doesn't. The PoC versions blocks outside loops in a
late GIMPLE pass (pointers set before the block, constant offsets, at
least 8 loads after a store through another pointer) and gives the
disjoint copy a clique with a base per pointer: 112 wide instructions,
BT.W 10.47 → 6.18 s on the 8C2 (lcc 6.34), 13.25 → 9.66 s on the 2C3.
Inside a loop RTL would believe such a clique across iterations (7.7,
"`restrict` cliques hold within one iteration only").

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
  instruction (a consumer right after a check without `rbranch` once
  saw the early value in the proof of concept; lcc leaves it out only
  where nothing read the register since the lock load, 6 checks in NPB
  BT and SP). Two checks, in ALC2 and ALC5, may share one `rbranch`
  (lcc: 63 instructions in SP, floyd-warshall's kernel; speculation.md).
  An intact check costs nothing, also for a use in the next
  instruction (measured); a failed one about 4 cycles plus the
  compensation code (the proof of concept's measurement).
- Keep stores out of a check's instruction: a check in ALC5 that
  reloads sees a store in ALC2 of the same instruction
  (`spec/isa/bundle.md`).
- A lock load and the check of an earlier lock may share a wide
  instruction, and so may two locks and two checks with one `rbranch`
  (speculation.md, "Distances"; `damwb.c` on the 4C, 8C2 and 2C3). lcc
  does this when the initiation interval needs it: NPB IS's histogram
  runs at II 2 with the lock in ALC3, the check in ALC5 and both stores
  in the other instruction. The check's `rbranch` takes CS0 and the SS
  transfer (bundle.md), so neither a prepare nor the loop's `ct` fits
  in the check's instruction. In a two-instruction kernel the check's
  row is therefore fixed, and a greedy placer must not fill it with a
  store first (the proof of concept did, P69). Where the II leaves
  room, the proof of concept keeps the check apart from its lock:
  CoreMark's list reversal ran 2.9% slower on the 8C2 with lock and
  check in one instruction and the store in the next (its finding I45,
  not reproduced in isolation; low confidence).
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
    cycles before the loop** (sooner, the iterations issued before the
    write lands count down the old `%lsr`, so the loop runs a count
    that depends on what `%lsr` held), and `disp %ctpr1, head` 5 cycles
    before the back edge;
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
- **Hardware loops after register allocation: `hw-doloop.cc`** (P66,
  P68). C6x, Blackfin, ARC and Xtensa keep a `doloop_end` counter in an
  ordinary register through allocation and turn the loop into a
  hardware loop in machine reorg by calling `reorg_loops`
  (hw-doloop.cc:618; c6x.cc:5870, bfin.cc:3920, arc.cc:8334,
  xtensa.cc:5341) with three hooks (hw-doloop.h:141–158):
  `end_pattern_reg` names the counter of a `doloop_end` insn, `opt`
  converts a loop (for E2K: `rwd` of the count into `%lsr` at least 4
  cycles ahead, `ct %ctpr1 ? %NOT_LOOP_END`), and `fail` splits the insn
  back into a decrement and a branch. C6x's `opt` hook is also its
  modulo scheduler (above). Two things the framework leaves to the
  port:
  - *A spilled counter hides the loop* (P66). `discover_loops` skips a
    `doloop_end` whose `end_pattern_reg` is NULL (hw-doloop.cc:369–371),
    and every port's hook returns NULL when the counter isn't a register
    (c6x.cc:5384, bfin.cc:3901, arc.cc:8315, xtensa.cc:5319), so neither
    `opt` nor `fail` sees such a loop. Allocation spills the counter of
    a loop that wants every register, since it is live throughout and
    read once an iteration (the proof of concept's v3 `jacld` in NPB LU:
    609 operations using all of `%g16`–`%g31` and `%r0`–`%r63`). The
    ports give their `loop_end` a memory alternative and split it after
    reload into a load, a decrement, a store and a branch
    (bfin.md:1977–2018, c6x.md:1482–1519, arc.md:5163–5193); with a
    register-only pattern LRA reloads the counter around the jump
    instead, with the store on the successor edges (lra.cc:2012–2030,
    whose critical edges IRA splits for it, ira.cc:5686–5728), and the
    loop is lost just the same (inferred). Either way it counts in
    memory and isn't pipelined. E2K needs the count only on the way in
    (`%lsr` counts, loops.md), so give the pattern a memory alternative
    and convert such loops in the port: find a `doloop_end` whose
    counter is in memory, load the count from the slot into a register
    dead at the loop's head, write `%lsr` from it and drop the memory
    counter (checking that nothing reads the slot after the loop). The
    proof of concept's `jacld` then pipelined (II 103, 4 stages): LU.W on
    the 4C 23.63–23.79 → 22.19–22.68 s (lcc 17.88; gcc-claude
    b28002c9f27); the v5 code of that loop got a register for its
    counter. lcc keeps the count only in `%lsr`, written before the loop
    (`rwd %g17, %lsr` in `jacld`'s preheader; lcc `-O3`, v3, v5, v6).
  - *The hook must check where the loop is entered* (P68). A loop's
    blocks are those reachable from its head in which the counter
    register is live (`discover_loop`, hw-doloop.cc:225–289), so when
    allocation reuses that register for another value next to the
    loop, outside blocks join, and the "loop" can be entered away from
    its head. `process_incoming_edges` (:158–184) accepts entries that
    share a source or a destination, which need not be the head, and
    when it fails a second time, after forwarder blocks are added, the
    loop is only dumped ("can't find suitable entry", :328–334), not
    marked bad, with `incoming_src` and `incoming_dest` both NULL (which
    hw-doloop.h:51–56 says can't be optimized). `opt` is called for
    every loop not marked bad (:589). ARC and Xtensa return false when
    `incoming_dest` is NULL or not `loop->head` (`hwloop_optimize`,
    arc.cc:8016–8030, xtensa.cc:5173–5187); C6x converts only one-block
    loops (c6x.cc:5502) and asserts the entry (:5505); Blackfin jumps to
    `incoming_dest` (bfin.cc:3764). Check as ARC does: the proof of
    concept's hook sent every entry to the head and deleted the loop's
    code while its setup still named the head's label (xz's `main`:
    `.L21` undefined at link time, once predicated calls joined the
    blocks around a short loop).
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
    only those of `%b[0..7]` that are live after the loop, the second set
    all eight, and the kernel use every ring name at its end (a value
    read in the next iteration under another name looks dead). Uses of
    all eight make every `%b` register look live into every pipelined
    loop, and so live out of any loop before one: a pipeliner working
    loop by loop then refused NPB SP's hottest flux loop for a `%b[1]` it
    used only as a temporary (P48; 117 operations at II 17 once
    narrowed). A value can stay in `%b[0..7]` across the loop only with
    the proof of concept's layout (ring below the outgoing pairs); with
    lcc's (ring starting at the outgoing pairs, D1) none may, and lcc
    sets them again after the loop (LU's `ssor`).
  - *The unroller's split induction variables are a recurrence* (P62).
    With `-fsplit-ivs-in-unroller` (the default, common.opt:3039),
    `loop2_unroll` gives the copies `i0 = i + 8; …; i = i0 + 8`
    (loop-unroll.cc:1472, `split_iv` :1710): two adds in a chain every
    iteration, so II ≥ 2 however few the operations (the PoC's gemm
    unrolled by two). On x86 fwprop folds the `+ 8` into the addresses
    and leaves `i = i + 16`; E2K addresses have two terms, so the chain
    reaches the final code (`addd %g16, 8, %g29` … `addd %g29, 8,
    %g16`). Before pipelining, rewrite the last add as `i = i + 16` with
    `i0` derived from `i` (the PoC's gemm: II 1), and compare accesses
    through `i0` with those through `i` in `i`'s terms: SMS's DDG asks
    `may_alias_p` (ddg.cc:379), which skips offset-based disambiguation
    (alias.cc:3225), so it would order every store of one copy before
    every load of the other.
- **Dependence cliques for arrays updated in place** (P46; NPB SP, the
  PoC on the 8C2): SP's sweeps update plane `i` of a global array from
  planes `i1 = i + 1` and `i2 = i + 2`, set outside the inner loop.
  GCC's dependence test for that loop sees `i` and `i1` as unrelated
  invariants and answers "don't know" (`analyze_ziv_subscript` needs
  their difference to fold to a constant, tree-data-ref.cc:3752);
  expanding their SSA definitions (`tree_to_aff_combination_expand`,
  tree-affine.cc:976) shows they differ by 1, which in-bounds indexing
  makes disjoint. Give the RTL pipeliner that answer as a dependence
  clique valid for the whole loop (`loop->owned_clique`, which
  tree-data-ref.cc:3076 trusts across iterations). A clique sits only on
  a `MEM_REF` (tree.h:1455), so a declared array's reference must become
  `MEM[(T *)&decl]`; the alias oracle reads it there
  (tree-ssa-alias.cc:2506), but ivopts' `copy_ref_info` looks for it
  through `get_base_address`, which returns the decl for `MEM[&decl]`
  (tree.cc:13291), and drops it (tree-ssa-address.cc:1021, 1050): walk
  the old reference's components instead. With both, the PoC's SP loops
  pipeline at II 2 with array prefetch (SP.W 22.35 → 15.56 s; lcc
  11.3). Relating the planes in RTL (registers a constant apart) works
  only for pointers set from one base (PolyBench's seidel-2d), not for
  SP's offsets, which multiply loop counters.
- **Two rows of one array at an unknown distance** (P70; NPB FT's
  butterfly, `fftz2`): the stores go to `y[i21 + k][j]` and
  `y[i22 + k][j]`, with `i22 = i21 + lk` and `lk` computed from a
  parameter, so expanding the SSA definitions (P46) gives no constant
  difference. GCC sees two row objects that are either the same or
  disjoint, and gives the pair a "could be independent" relation
  (`DDR_COULD_BE_INDEPENDENT_P`, tree-data-ref.cc:3481-3510,
  tree-data-ref.h:430-487) whose distance vector (0) holds only if the
  rows coincide. The vectorizer can live with that, since a distance
  of 0 needs no check (tree-vect-data-refs.cc:3938-3970; host GCC
  16.2.1's vect dump of the butterfly). A modulo scheduler that treats
  the same-row case as possible, however, chains every store to the
  one before. Version such a loop on a run-time check of the two rows'
  address ranges, as for two arrays, instead of refusing the pair as
  dependent. lcc keeps `fftz2` out of line and has DAM versions of it
  (lock loads `mas=0x4,sm`, checks `mas=0x3` with `rbranch`) beside the
  array-prefetch one [lcc 1.29.16 `-O3`, NPB FT, v5.8c2]. With the
  versioning and combined operations (P61, below) the PoC runs the
  butterfly at II 2 instead of 5.
- **Where alias information goes in the pass pipeline** (P50; the PoC on
  the 8C2 and 2C3):
  - *Tag loops where they are innermost, twice.* `cunroll` runs after
    `vect` and `predcom` (passes.def:322–327), so a loop whose small
    inner loops it unrolls becomes innermost only then: SP's sweeps have
    `for (m = 0; m < 3; m++)` loops inside the `k` loop (at `-O2`
    `cunrolli` may not grow code, tree-ssa-loop-ivcanon.cc:1786, and
    unrolls only the first; `cunroll` does the rest when allowed to grow,
    opts.cc:1435; at `-O3` `cunrolli` takes them all). Tagging after
    `vect` gave each `m` loop a clique and the `k` loop none; tagging only
    after `cunroll` and `slp` lost PolyBench's adi, jacobi-2d, heat-3d
    and fdtd-2d, which predictive commoning leaves unpipelinable once
    versioned (adi 3× slower). The PoC tags after `vect` and again after
    `slp` for loops that became innermost in between, both before ivopts
    (SP.W 14.47 → 12.74 s on the 8C2, lcc 11.3; BT.W 6.16 → 5.62 s).
    The second run must take the first run's `MEM[&decl]` references as
    the same objects as plain ones: `operand_equal_p` treats different
    cliques as different trees (fold-const.cc:3635).
  - *LIM's unsigned rewrite loses points-to precision* (mainline; GCC
    15.3 keeps the pointer arithmetic). LIM rewrites arithmetic it
    hoists out of a conditional block as unsigned
    (tree-ssa-loop-im.cc:1460; `POINTER_PLUS_EXPR` becomes `PLUS_EXPR`
    and a conversion, gimple-fold.cc:11158): FT's `evolve` gets
    `(dcomplex (*)[256][256]) ((unsigned long) &u0 + k * 1048576)`. PRE
    starts by recomputing points-to (`TODO_rebuild_alias`,
    tree-ssa-pre.cc:4634), which merges the operands of integer
    arithmetic (gimple-ssa-pta-constraints.cc:2332) and takes any
    non-zero constant for an address of non-local memory (:908), so the
    row pointer may alias every global array, and ivopts copies that to
    its addresses. Restoring `{ u0 }` for a pointer converted from
    `&decl` plus an integer computed from no pointer let the loop
    pipeline (II 9 for four elements, 122 cycles an iteration before):
    FT.W 0.64 → 0.54 s on the 8C2, 0.58 → 0.49 s on the 2C3.
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
    recomputes the stream position from `%ilcr − %lsr` (v3–v5; v6 keeps
    the positions, so there any loop shape and any number of advances
    per iteration work, measured on the 2C3; a `mova` wider than the
    area's format also reads unaligned data there, which allows strides
    that aren't a multiple of the element);
  - no calls in the loop, and no use of `%ctpr2` (a call, a return or a
    write to `%ctpr2` stops the program, and a later `mova` is SIGILL);
  - on v3/v4, elements aligned to their size (otherwise the `mova`
    returns a diagnostic value and the loop dies with SIGILL at the
    first non-speculative use of it), proven, tested at run time (lcc
    1.31, long loops) or assumed ("APB alignment on v3/v4", below);
  - movas in epilogue iterations (`lcnt` 0, `ecnt` > 0) only with
    `ldmc` = 1 in `%lsr`: with `loop_mode` and `ldmc` 0 they read
    nothing (the destination keeps its value), without `loop_mode` they
    read, but on v3–v5 an interrupt there puts the stream back an
    element (the kernel counts epilogue iterations only with `ldmc`,
    `iset_v3.c:194-196`: a few runs in a million on the 4C and the
    8C2), and on the 2C3 the first one is SIGILL
    (`spec/isa/hwbugs.md`). lcc's APB loops have no epilogue
    iterations; a pipelined kernel that drains through `ecnt` must set
    `ldmc` or keep its movas out of the drain stages.
  - no store during the loop to an element of a streamed array that a
    later `mova` reads: the buffer doesn't see stores, and how far it
    has prefetched varies. Stores to elements already read are fine;
    lcc versions loops that may alias on a run-time pointer comparison
    (`spec/isa/apb.md`, "Stores into a prefetched array").

  Size each area for the prefetch depth: about 1 KiB (`asz` 4) lets one
  stream from memory run at L1 speed. Spread the areas over both halves
  of the buffer, as lcc does: it gives arrays to the halves by turns
  and sizes each half's areas to sum to 2 KiB. Each half has its own
  2 KiB, so with one or two arrays every area can be 2 KiB, which on
  the 4C is 5–13% faster from memory than 1 KiB. With 4 KiB pages, two
  areas in one half make a column walk (bound by TLB misses) 9–40%
  slower than one area in each half, on all three machines and at any
  area size (I47; `spec/isa/apb.md`, "Which half an area is in").
  `dcd = 0`, as lcc writes, is the safe default; a large array read
  once can use `dcd` 2 (v3) or 3 (v5) to keep the rest of the cache, at
  no cost to the stream (`spec/isa/apb.md`, "Timing"). End the loop as
  lcc does, `{ disp
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
- **A while loop's trip count is a guess** (P52): GCC's static profile
  gives a loop's exits 11% between them (`PRED_LOOP_EXIT`,
  predict.def:119; split per exit, predict.cc:2183), so a hash-chain
  walk with a "found" exit stays 94.5% at its NULL test and the guessed
  profile says about 13 nodes a lookup (Lua's `internshrstr`), where hash
  chains hold one or two. Pipelined, that walk paid its prologue on
  nearly every lookup and stalled on speculative loads of the nodes past
  the one it stopped at. The PoC lowers the profile of a walk whose
  pointer starts at an array element at a computed index and is replaced
  by a field of what it points to (`scale_loop_profile`,
  cfgloopmanip.cc:698) and keeps it from the pipeliner: k-nucleotide
  7.39 → 6.92 s on the 8C2 (lcc 6.0); CoreMark's long list walks stay
  pipelined. lcc pipelines no walk with a second exit, bucket or list
  alike (it peels the first nodes with speculative loads, then runs a
  plain loop), but does pipeline one-exit walks such as `for (; p; p =
  p->next) n++` (lcc `-O3`, v3–v6). Chains linked by offsets are the
  same (P71). Lua 5.4's `luaH_getshortstr` walks `n += gnext (n)`, and
  GCC's static profile gives its loop header 13.8 executions a call
  (two exits at 5.5% each; host GCC 16.2.1 `-O2`, profile_estimate
  dump), so neither the guessed trip count nor a pointer-only pattern
  (`p = p->next`) keeps it from the pipeliner. lcc doesn't pipeline it:
  it loads the first node's key type, next offset and key value (the
  last two `,sm`), and the second node's, before the first type test,
  then runs a plain loop [lcc 1.29.16 `-O3`, v5.8c2 and v6.2c3]. The
  PoC pipelined it at II 10 with 3 stages until it treated offset walks
  from a hashed array element as chains too: Lua k-nucleotide 1.6%
  faster on the 2C3. Don't pipeline a while loop whose expected trip
  count is below its stage count, and since GCC's guess won't tell you
  that for a chain walk, recognise the walk.
- **Copies in the latch lose the count** (P43): out-of-SSA leaves a
  rotation of loop state (`ym2 = ym1; ym1 = y`, PolyBench's deriche) as
  register copies in the latch after the exit test, and
  `doloop_optimize` passes `entered_at_top` false unless the latch has
  no active insn (`loop-doloop.cc:724`), so the loop isn't counted and
  isn't pipelined. Move such copies in front of the exit jump first (each
  copied-to register dead at the exit and not read by the jump): deriche
  went from 0.52 of lcc to just ahead of it on the 8C2 and the 2C3. Then
  don't unroll a one-block counted loop the modulo scheduler will take,
  but do unroll one whose recurrence leaves it nothing to overlap
  (seidel-2d: a divide and six adds, 38 of 49 cycles an iteration).
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
  CoreMark +9% (reverse 8 → 3 cycles a node). Values the compensation
  reads must survive the check's instruction itself: the compensation
  runs after that whole instruction, its writes included
  (speculation.md; P30, `vect-alias-check-18.c` gave wrong bytes when a
  redone add's operand was overwritten there; possibly also P28's GMP
  failure). Only the iterations between a lock and its check hold an
  entry: count those against the 32, not all the kernel's stages. lcc's
  NPB IS lock runs 28 iterations ahead of its check (`? %pcnt28`), at
  the distance of the loop's other far loads, so the histogram's cache
  misses overlap (lcc 1.29.16 `-O3`, v3, v5 and v6; speculation.md).
  Combine DAM with array prefetch as lcc does: return from the
  compensation code with `ibranch`, which writes no CTPR, where a
  `disp %ctpr2` + `ct` back would stop the APB program (apb.md; "Keep
  `%ctpr2` free" below). With MAS accesses no longer excluding one
  another (D10, one MAS field per channel), the lock far ahead and APB,
  the proof of concept's IS.A timed part took 0.17 s on the 8C2 (lcc
  0.18), 0.32–0.35 s on the 2C3 (lcc 0.30–0.32) and 0.61 s on the 4C
  (lcc 0.53–0.61), against 0.74 s on the 8C2 at II 2 without the last
  two (P69).
- **Several lock/check pairs in one loop** (P58; lcc's PolyBench
  floyd-warshall at `-O3`, v3, v5 and v6 alike): in `path[i][j] = min
  (path[i][j], path[i][k] + path[k][j])` the store may hit `path[i][k]`
  (at `j == k`) and `path[k][j]` (when `i == k`). lcc's kernel is three
  wide instructions: `path[i][j]` through array prefetch, `path[i][k]`
  and `path[k][j]` each with a lock load every iteration (the invariant
  one too: a check removes its entry), both checks (ALC2, ALC5) in one
  instruction with one `rbranch`, and compensation code that redoes the
  add and the compare from both checked values, since `%MLOCK` doesn't
  say which failed. On v3/v5 a check runs 14 or 15 iterations after its
  lock, so about 29 of the 32 DAM entries are in use (v6: 2 iterations).
  Don't version for such an element: `path[i][k]` lies in the row the
  loop stores, so the run-time test always fails. Split the `j` loop
  after the iteration whose store writes `path[i][k]` (`j == k`, found
  at run time from the two addresses): `path[i][k]` is then invariant in
  both halves and needs no check. GCC's loop splitting can't find that
  point: it splits only at a test in the body, relational or `==` with
  the first or last value (tree-ssa-loop-split.cc:120-160), and here the
  point comes from aliasing, not from a test. The CFG work is GCC's
  `loop_version` (cfgloopmanip.cc:1888) and `connect_loops` (static in
  tree-ssa-loop-split.cc:369), the test the port's own. The proof of
  concept does this (`-mav-split`; P91): both halves pipeline at II 1,
  17.6 s on the 8C2 (lcc 47.2) and 13.3 s on the 2C3 (lcc 35.6);
  forwarding the stored element through a register instead, its earlier
  way, held the kernel to II 5 (74.7 s on the 8C2) (the proof of
  concept's figures, not rechecked).
- **Alias queries after register allocation** (P31): `may_alias_p` in
  machine reorg reads `reg_base_value`, which only `init_alias_analysis`
  computes; `end_alias_analysis` keeps the old vector, so a pass there
  sees values from before later copies and renaming. Call
  `init_alias_analysis` around a reorg pass that asks.
- **Auto-vectorization with 64-bit vectors** (P32): V8QI, V4HI, V2SI and
  V2SF in the ordinary registers (moves and logic as 64-bit integer
  operations; `padd`/`psub`, saturating, min/max, compares and `pmerge`
  selects, `psign` abs, shifts (above), `pmullh`/`pmulhh`/`pmulhuh`,
  `pmaddh` dot products, `psadbw` sums, `phadd` reductions, `punpck`
  widening, `pshufb` permutes and narrowing (its bits 4–6 only on v5,
  semantics.md), `pfadds`/`pfmuls`). On the 4C the PoC's vectorized
  kernels run 1.7–69 times faster than its scalar code and 1.9–9.4 times
  faster than lcc where lcc doesn't vectorize (widening, narrowing, dot
  products, sums of differences), but lcc is 1.5–3 times faster on plain
  loops, which it runs at two vectors a cycle through APB (`movad`,
  `staad`, `pshufb` to realign) where `ldd`/`std` allow one (four loads
  and two stores don't fit one instruction's channels). GCC's `-O2` cost
  model vectorizes nothing that needs versioning for alignment (on v3/v5
  misaligned accesses cost several cycles, timing.md; v6 none), so the
  gains come at `-O3` or `-fvect-cost-model=dynamic`. CoreMark's
  `A[i*N+j]` with unsigned 32-bit indices isn't vectorized because the
  index may wrap; lcc keeps the wrap too (`-mdmodel=medium` lets it
  compute indices in 32 bits, source-compat.md, not drop the wrap).
- **Rotating registers only for values that need them** (P41): a value
  whose last use issues no later than the next iteration's definition
  (lifetime at most II, not carried, not read after the loop) can stay
  in an ordinary register, since operands are read at issue. Large FP
  loops need this: NPB SP's flux loops have about 135 values against at
  most 62 rotating names, and pipelined only once short-lived values
  shared window registers and `%g16`–`%g31` (SP 26.9 → 25.6 s, LU 16.5 →
  14.2 s). Not where code after the kernel reads older iterations'
  instances (while loops, DAM).
- **Stores in stream loops cost nothing extra** (I28): the two-stores
  bank rule and the store–load block rule of timing.md apply only to
  instructions that store to the same addresses again; with addresses
  moving every instruction, two stores (`std` or AAU `staad`) or a store
  and a load share an instruction at no cost. Modelling the rule there
  made the PoC's Linpack 6.5% slower on the 8C2.
- **Combined FP operations read the addend at issue** (I34): through
  any operand `fmul_addd` takes 8 cycles, so combining takes away the
  four cycles a separate `faddd` gives a load into the addend; where
  such a load may miss L1, schedule it earlier or keep the operations
  apart (the PoC's L2-sized daxpy: 1.19 cycles an element combined, 1.06
  apart, 1.11 with the load 4 cycles earlier).
- **Form combined FP operations per loop, never on a recurrence** (P61).
  Combine forms a non-fused `(plus (mult))` pattern wherever it matches
  (`-ffp-contract` gates only `convert_mult_to_fma`,
  tree-ssa-math-opts.cc:3402). lcc combines where the FP channels bound
  the kernel: PolyBench heat-3d on v3, II 12 with 20 `fmul_addd` (60
  operations apart on 48 slots); gemm on v5, nine elements in five wide
  instructions; NPB FT's `fftz2`. It keeps them apart in reductions
  (bicg's `q[i]`, Linpack's `ddot`) and in its DAM daxpy versions, though
  it combines its other daxpy kernels (lcc `-O3`). Through the addend
  the combined operation takes 8 cycles to the add's 4, so on a
  recurrence it lengthens the II (the PoC's bicg: 12 cycles for two
  elements instead of 8). The PoC forms them in its pipeliner, in loops
  known to run 64 times or more and in any other loop where combining
  lowers the II, for a product that one add or subtract alone reads and
  an addend not carried or derived from a carried value: heat-3d 5.84 →
  4.96 s on the 8C2; formed everywhere, Linpack's short daxpy loops
  lost. Give the pattern an insn condition that is false outside the
  pass that forms them. The trip count alone is the wrong test. NPB
  FT's butterfly runs 16 elements at a time: kept apart, its 10 FP
  operations and an address add held the PoC to II 3; with two
  combined operations and AAU stores (`staad` with `incr`, no address
  arithmetic) it reaches II 2, as lcc's kernel does. Decide AAU stores
  the same way, by what they do to the II (P70).
- **Keep `%ctpr2` free across a loop that may use array prefetch**
  (P42): only `%ctpr2` can hold the APB program, so a branch prepared in
  `%ctpr2` and live through an inner counted loop costs that loop its
  prefetch (an outer latch jumping straight back to the inner header,
  which GCC's loop tree makes one loop with two latches: 4 loops in NPB
  SP). lcc ends its hardware loops through `%ctpr1` (1,442 of 1,450 in
  our benchmark builds) and loads APB programs into `%ctpr2`, though it
  uses `%ctpr2` for other branches too. Prepare such a branch after the
  loop only for a loop that will use APB (re-preparing one already
  placed cost the PoC's CoreMark 0.64%), so decide which loops those are
  before branches get CTPRs: once prepares are shared and hoisted, the
  jumps reached through `%ctpr2` from a loop's exit need not go to one
  place (P60). Short loops count: NPB FT's butterfly (`fftz2`, 16
  iterations at run time) is a two-instruction kernel in lcc's v5 and
  v6 code, 4 `movad`, 4 `staad` with `incr` and 8 FP operations (2
  combined) filling all six channels of both instructions, where the
  PoC, its outer back edge in `%ctpr2`, takes II 5. lcc keeps an outer
  branch prepared in `%ctpr3` across that loop and prepares the back
  edge in `%ctpr1` again after it. The way into such a short loop
  matters as much as its kernel. lcc sets the butterfly's nine AAU
  registers two per wide instruction, in ALC2 and ALC5, beside
  compares, adds, `insfd` and the `%lsr`/`%lsr1` writes: five
  instructions before the peeled loads and `bap`. The nine are three
  descriptors (one per output row, one for the loads), two store
  indices, two increments, and two index registers, `%aaind1` and
  `%aaind2`, for the two prefetch areas that read the two input rows
  through the one descriptor `%aad0` [lcc 1.29.16 `-O3`, v5.8c2]. Don't
  make AAU writes scheduling barriers: the PoC wrote one descriptor
  every two instructions and took 66–77 cycles to enter the loop,
  against lcc's 33–46, and letting ALU operations share an AAU write's
  instruction made FT.W 3–5% faster (P70).
- **APB areas: `disp` at 0 or more, backward areas based at their
  lowest element** (I41): `disp` is zero-extended, so a negative offset
  goes into the descriptor's base or `%aaind`; on v3 an area may not go
  below its base (diagnostic values, SIGILL in gcc.dg/vect tests), on
  v5+ it may; write a negative increment with `aaurwd` on v5+
  (apb.md). AAU stores (`staa*`) need no alignment on any version, only
  APB's loads do on v3 and v4 (I43; lcc's v3 PolyBench fdtd-2d uses 19
  `staad`). Two AAU stores with literal offsets in one instruction need
  explicit literal slots (`_f32s,_ltsN`) or gas rejects them
  (bundle.md). On v3 an area's `mrng` may be at most 24 + *g*, where
  *g* is the largest of 1, 2, 4 and 8 dividing both the area's start
  and its step in bytes, and a `mova`'s `ind` must be a multiple of its
  size; otherwise the movas return diagnostic values (I48, measured on
  the 4C; `spec/isa/apb.md`, "Diagnostic results"). 24 + the element
  size is always safe there, because v3 already needs element-aligned
  starts. Where a group of loads needs a larger `mrng` than that, lcc
  splits it into more areas. v5 and v6 have neither limit.
- **APB alignment on v3/v4: prove it as lcc does, or assume C's**
  (P90). An area whose start isn't aligned to its element gives
  diagnostic values on v3 (and v4, by lcc's changelog), and the loop
  dies with SIGILL `ILL_ILLOPN` at the first non-speculative use; v5
  and v6 read the misaligned data (`spec/isa/apb.md`, "Diagnostic
  results"). lcc's way first (decision 7): it uses APB on v3/v4 only
  where it proves the alignment (global arrays,
  `__builtin_assume_aligned`) or with `-faligned`, and lcc 1.31 also
  versions a loop with a large known count on `(p & (size − 1)) == 0`
  at `-O3` (`-floop-apb-align-versioning`). GCC's middle end has both
  halves: `get_pointer_alignment` proves (builtins.cc:423, from the
  alignment CCP and `__builtin_assume_aligned` record in
  `ptr_info_def`, tree-ssanames.cc:735), and the vectorizer's
  versioning for alignment builds the run-time test
  (`vect_create_cond_for_align_checks`, tree-vect-loop-manip.cc:4216,
  with `loop_version`, cfgloopmanip.cc:1888). C's rule is the other
  choice: `get_object_alignment` gives a memory reference its type's
  alignment unless the pointer's is known (builtins.cc:323–333), so
  `MEM_ALIGN` of `*p` is already the element's
  (`set_mem_attributes_minus_bitpos`, emit-rtl.cc:2164). It breaks
  only code that reads misaligned words through cast pointers
  (undefined in C, but it works with lcc and with plain loads, which
  cost cycles and don't trap), and that code then dies on the 4C
  (`tools/isa-probes/pocchk6.c` `align`: lcc's own `-faligned` and
  `__builtin_assume_aligned` loops on data 1 or 4 bytes off; the 8C2
  and the 2C3 run the same v3 code right). What lcc's rule costs it
  (`spec/isa/apb.md`, "What lcc's alignment rule costs"): in NPB class
  W its v3 code gets 7, 5, 14 and 73 programs in CG, FT, MG and LU
  against 23, 12, 22 and 78 with `-faligned`, and runs CG 2.0× and FT
  1.48× slower on the 4C. The proof of concept assumes C's alignment
  (`-mapb-type-align`, on by default) and reports 18, 25, 50 and 54
  programs against 8, 12, 4 and 52 with its own proof, which is weaker
  than lcc's (MG's arrays passed as pointers; its figures). Following
  lcc means carrying the alignment from the arrays' declarations
  through the pointers ivopts makes and adding the run-time test for
  long loops; an option that assumes C's alignment, as lcc's
  `-faligned` does, gets the rest for code that keeps to C's rules.
- **Read all of a loop's streams through APB, backward ones too**
  (I49). On the 4C, plain loads that miss L1 lose 1.25 cycles an
  iteration in L2 while an area streams beside them: II 4 becomes
  5.25, forward or backward, in any load channel. The 2C3 loses
  0.5–1.25 cycles, the 8C2 at most 0.2. From memory such a loop runs
  no faster than its plain stream alone. With every stream in an area
  the loop keeps its II from memory on all three machines
  (`spec/isa/apb.md`, "Plain loads beside an area"). A backward stream
  takes an area based at its lowest element, with the start in
  `%aaind` (I41), as lcc does. When one stream must stay plain on v3
  (alignment unknown, a gather), APB for the other streams gains
  little. lcc keeps APB there anyway (`g[i] * p[n - 1 - i]` with `p` a
  pointer), while the proof-of-concept back end drops it from v3 loops
  with a backward plain load (PolyBench durbin 5.40 → 3.96 s on the
  4C). Measure before choosing either way, and include forward plain
  streams in the test, since they pay the same.
- **Array prefetch in a modulo scheduler** (P33, the PoC on the 4C and
  the 8C2): a load stepping one aligned element an iteration, through an
  address the loop steps and that no store of the loop writes before
  it is read, becomes a `mova` node before scheduling (latency 5, an AAS
  channel, no ALU or load channel, no memory dependences), up to four;
  its peeled prologue instances stay loads, and each area starts at the
  element its first kernel instance reads (stage U reads iteration
  S − 1 − U), so every area advances once per kernel iteration, which
  is all apb.md requires (`loop_mode` is not needed, as long as no
  `mova` runs in an epilogue iteration without `ldmc`: the APB item
  above; such loops also run on the 2C3, P89). Channels and areas
  are given per kernel row after scheduling (0/1 the left half, 2/3 the
  right; area = program instruction). The setup (`aaurwd` of each
  start, `ldisp %ctpr2`, `bap`) goes on each way in after `rwd %lsr`, in
  a block of its own; the exit gets `disp %ctpr2, disp=0x0` and a write
  of a zeroed register to `%aabf0` (gas rejects a literal there); a
  `mova` pattern uses `%ctpr2`, which keeps branch preparation off it.
  Cost: about 20 cycles a run (the setup and `bap`'s 20–22 cycles to the
  first `mova`, which the prologue partly covers): 2–9% slower on short
  loops in L1, 5–12 times faster from memory (as fast as lcc's), and
  shorter kernels where the load channels bound them (II 2 → 1). Use it
  when it shortens the kernel or the loop runs 256 times or more; a
  run-time test on the count, as lcc does, would get the memory gains for
  loops of unknown length.
- **Plain loads in a pipelined loop: read them far ahead** (P44): a load
  array prefetch doesn't serve (v3/v4 without proven alignment, gathers,
  the DAM versions of loops), scheduled for L1, stalls the whole kernel
  on every miss (the PoC's v3 mvt: 278 cycles an element, lcc's 45). lcc
  issues such loads 40–90 cycles before their use in 31 of the 155 v3
  PolyBench kernels that have them (mvt's column walk: 22 iterations at
  II 4), 40–76 on v5 and v6, and 22–47 even in CoreMark's `matrix_test`
  (lcc `-O3`; timing.md, "Memory"). SMS takes edge latencies from
  `dep_cost` (ddg.cc:222), so from `TARGET_SCHED_ADJUST_COST`, which can
  tell SMS by `common_sched_info->sched_pass_id == SCHED_SMS_PASS`
  (sched-int.h:28). The PoC gives a load no recurrence goes through 48
  cycles there: its v3 2mm, 3mm, lu, ludcmp and syrk ran 3–4 times
  faster, mvt 0.28 → 0.91 of lcc's speed, doitgen on the 8C2 0.65 →
  0.95. It does so only in loops known or bounded to run long (a
  profile, a constant count, or `get_max_loop_iterations_int`,
  cfgloop.cc:2049, between 64 and 2^20): with every loop reading far,
  CoreMark lost 18% and xz 28% on the 8C2 (the stages cost every way
  in). Never raise II for it; when the ring runs out, halve the distance
  (deriche's II-1 loops need 56 stages at 48 cycles and fit at 24).

  Gathers in loops of unknown length (P74; NPB CG's row loop, `sum +=
  a[k] * p[colidx[k]]`, 72.6 elements a row in class W, II 4 from the
  FP add's recurrence in both compilers): lcc uses `p[colidx[k]]` 13
  iterations (52 cycles) after loading it on v5 and v6 and 8 (32
  cycles) on v3, and its way into each row costs little because its
  peeled prologue is packed: the first 14 iterations' loads (45, up to
  four a wide instruction) take 23 wide instructions, 27 cycles, and
  the row 34 cycles from its head to the kernel (lcc `-O3`, v5; v6
  alike; spec/isa/loops.md, "How lcc builds loops"). The proof of
  concept's kernel read the gather one iteration (4 cycles) after its
  load, for an L1 hit, and stalled on every miss (4.7 cycles an
  iteration against lcc's 4.2, measured on the 8C2); at 48 cycles the
  loop went from 6 to 17 stages and the way into each row cost more
  than the misses (CG.W 0.75 → 0.78 s); at about an L2 hit's latency
  (it used 12 cycles up to v5 and 16 on v6; L2 takes 9 cycles on the
  4C, 11 on the 8C2 and 13 on the 2C3, timing.md) it gained: CG.W 0.75
  → 0.72 s on the 8C2 (lcc 0.68), 0.81 → 0.77 on the 2C3 (gcc-claude
  9dbb3274d4f). The same in while loops that often leave early
  (CoreMark's list searches) cost CoreMark 1%. So give a gather in a
  counted loop of unknown length at least an L2 hit's latency, and the
  far distance where the loop is known to run long or the prologue is
  packed as lcc's is.
- **Big counted loops want loop mode and array prefetch even when
  overlap gains nothing** (P53): in a loop of hundreds of operations the
  II can't be much below one iteration, so a pipeliner that wants a
  clearly shorter kernel refuses it, and the list-scheduled loop stalls
  on L2 (NPB LU's blts, buts and jacld at 1.5 times their modelled
  cycles in the PoC on the 8C2). lcc runs them as `loop_mode` loops that
  don't overlap iterations (no `abn`; only induction and recurrence
  values cross the back edge) with APB where it can: jacld and jacu in
  kernels of 105–106 wide instructions with 20 `mova`s on v5 and v6
  (116 on v3), blts in 112 with 85 (129 on v3, with plain loads) (lcc
  `-O3`, NPB LU class W; their values in `%b[]`, D1, "Registers above
  `%r63`"; spec/isa/loops.md, "How lcc builds loops"). So decide APB
  per counted loop and count it when deciding whether pipelining pays;
  SMS keeps a single-stage schedule only with `--param sms-min-sc=1`
  (default 2, modulo-sched.cc:1660). The PoC pipelined such loops at an
  II up to 5/4 of one iteration (blts at II 284: LU.W 11.93 → 11.41 s);
  its two resource-bound loops then had about 260 values live at once,
  more than the ring and the window hold, and spill slots stored and
  reloaded across iterations: overlapping loops that big needs placement
  that keeps lifetimes short, and spill code turned back into values
  (next item).
- **A pipeliner for loops of hundreds of operations** (P63, P64, P75;
  NPB LU, the proof of concept on all three machines). GCC's SMS lacks
  four things such loops need:
  - *A register-pressure limit* (P64). SMS never checks pressure ("???
    If (success), check register pressure estimates",
    modulo-sched.cc:2260); its swing order places each node next to its
    scheduled predecessors or successors (:1812–1828), which keeps
    lifetimes short but doesn't bound them, and it runs before
    allocation (passes.def:512, IRA at :518), which then spills what
    doesn't fit. A ring holds at most 128 registers (`setbn rsz` ≤
    0x3f, 2 (rsz + 1) registers; loops.md, "Rotation"), and in a loop at
    the machine's width the values alive at the worst cycle, not the
    six channels, are the limit: in the proof of concept `jacu` (575
    operations) at II 106 in 2 stages has 116 values alive on average
    but 166 at the worst cycle, for 126 ring names and 49 free
    registers, and `jacld`'s 2-stage schedules peak at 204–224 against
    175. One iteration already nearly fills the register file: lcc's v5
    `jacld` kernel names 177 distinct registers (98 `%b[]`, 63 `%r`,
    `%g16`–`%g31`) of the 192 in its window and the 16 globals, and lcc
    doesn't overlap iterations of these loops at all (above). The proof
    of concept (gcc-claude 2ced4f36492, 02a87b004a2, 1f82859c980) skips
    IIs a lower bound rules out (half the peak, and the lifetimes over 2
    II, in pairs), moves nodes within their dependences to cut the peak
    and then the lifetimes (not loads: moving them nearer their uses
    cost NPB BT 9% on the 2C3), tries II up to 1.5 times one unpipelined
    iteration, and runs a loop in one stage, as lcc does, when its MII
    is within a tenth of one iteration and the overlapped schedule would
    be more than an eighth longer: `jacld` at II 124, where lcc's kernel
    takes 106 cycles (105 wide instructions) on v5 and v6 and 118 on v3
    (lcc `-O3`).
  - *Stores spread over the schedule* (P64). Only ALC2 and ALC5 store
    (bundle.md), two a cycle. `jacld` stores 100 elements an iteration
    (lcc: 75 `staad` and 25 `std`); a forward list schedule of one
    iteration piled the proof of concept's 115 (its spills included) up
    at the end, two a cycle; a backward pass from the last cycle spreads
    them. lcc's v5 kernel stores 29 of its 34 constant zeros in its
    first 52 wide instructions and 51 of its 66 computed values in its
    last 26, two a cycle.
  - *Spill code read as values* (P63). A pipeliner after allocation
    (C6x's model) meets the allocator's spill code, and a value carried
    to the next iteration through a slot (stored below, reloaded at the
    top) is a dependence through memory from the store to the next
    iteration's load, so every stage must come after the store: the
    proof of concept's `jacld` (618 operations) didn't schedule until it
    read through it. LRA's slots are easy to recognise: their
    `MEM_EXPR` is the artificial `%sfp` decl of `get_spill_slot_decl`,
    with a known offset and an alias set nothing else has
    (`set_mem_attrs_for_spill`, emit-rtl.cc:2818–2869, called from
    lra-spills.cc:161), and LRA shares a slot only between pseudos whose
    lifetimes don't conflict (lra-spills.cc:24–33). So read a reload of
    a slot the loop body stores as a copy of the stored value: of the
    same iteration when the store comes first, of the previous iteration
    when the load does, and in the first iteration the value loaded from
    the slot before the loop (gcc-claude b90c3b67c22: `jacld` then
    schedules at II 105–134). Predictive commoning makes such values: at
    `-O2` it runs, without unrolling, whenever the loop vectorizer is on
    (tree-predcom.cc:3571–3588), and in `jacld`'s `j` loop it carries
    `u[i][j-1][k][0..4]` over from the previous iteration's
    `u[i][j][k][0..4]` (five chains of distance 1; host GCC 16.2.1 `-O2
    -fpredictive-commoning -fno-tree-vectorize
    -fdump-tree-pcom-details`). lcc carries none of them: its kernel
    reads all 20 `u` elements it needs (four neighbours × 5) with 20
    `movad` and divides four times every iteration, with no frame
    access (lcc `-O3`, v3, v5, v6). With this and the pressure limit
    above the proof of concept's LU.W ran 10.07 → 8.0 s on the 2C3,
    11.40 → 9.94 s on the 8C2 and 24.39 → 22.48 s on the 4C (lcc 6.51,
    8.39, 17.88).
  - *Eviction* (P75). SMS places each node once, in swing order; a node
    with no free cycle in its window gets an empty row inserted, raising
    II by one, and after ten such splits SMS starts over at the larger
    II (modulo-sched.cc:2225–2258), up to `max (max_asap, 2 × MII)`
    (:1621; `sms-max-ii-factor`, params.opt:1088); a placed node never
    moves. NPB LU's `blts` (329 operations in the proof of concept,
    about 115 loads, a recurrence through `v[i][j-1][k][*]` with five
    divides and 33 dependent FP operations) is bound by that recurrence
    to II 207, yet every one-pass order the proof of concept tried (body
    order, earliest starts, least room first) failed below 274: the
    loads that start an iteration have no predecessors, so every order
    puts them in the first rows, and the recurrence's last nodes (its
    stores, in the last stage) wrap into those rows at any II below the
    chain's length. Rau's iterative modulo scheduling (place the highest
    node, evict what is in its way and what its dependences no longer
    allow, place those again; six placements a node) reaches II 207;
    over 712 pipelined loops of NPB, PolyBench, CoreMark, Lua, zlib,
    bzip2 and Linpack it shortened 15 (SP's `x_solve` 48 → 30,
    `z_solve` 72 → 55) and lengthened 2: LU.W 4.4% faster on the 8C2
    and 7.8% on the 4C, SP 2–2.6%, PolyBench adi 3–6% (gcc-claude
    62bdc8316cf, 11506a4c3a6, e451fb3964a). lcc doesn't overlap `blts`:
    a one-stage `loop_mode` kernel of 112 wide instructions taking 253
    cycles on v5 and v6 (85 `movad`), 129 and 239 on v3 (plain loads),
    whose operations isa-score bounds at 209 cycles by the recurrence
    (lcc `-O3`; loops.md, "How lcc builds loops"). So plan for iterative
    modulo scheduling, or SMS with an eviction step, for loops of
    hundreds of operations, and compare each loop's II with its
    recurrence bound to tell a scheduling failure from a real
    recurrence. The bound is the least II at which no dependence cycle
    has positive weight when an edge weighs LAT − II × DIST (a
    longest-path test); SMS's own `rec_mii` assumes one back edge per
    cycle and rounds `length / distance` down (ddg.cc:780–803), so it
    can understate it.
- **Unroll a column walk so each copy has its own area** (P54): on v3–v5
  one area fetches at most a 64-byte line every 2 cycles from L1
  (apb.md, "Timing"), so a load stepping a line or more an iteration
  holds the loop to 2 cycles whatever its II (PolyBench cholesky's
  init_array, `A[s][t]` down a column: 2.1 at II 1 on the 8C2). lcc
  unrolls such loops with an area per copy (two for a plain column walk
  on v3, v5 and v6; nine in cholesky's init_array on v5/v6), though not
  syrk's (one area, 1.05 s on the 8C2). The PoC returns 2 from
  `TARGET_LOOP_UNROLL_ADJUST` (loop-unroll.cc:371; the RTL unroller runs
  only under `-funroll-loops`, loop-init.cc:570, which rs6000 turns on at
  `-O2`, rs6000-common.cc:41) for such a one-block loop: syrk 0.66 s,
  cholesky 1.96 → 1.38 s (lcc 1.49). Reading the columns with plain
  loads instead lost 6.4% of PolyBench: from memory they need the
  prefetch.
- **An area's base can be an induction register plus invariants**
  (P55): ivopts addresses a plane of a 3-D array as the inner induction
  variable plus outer-loop values (NPB SP's xi fluxes: `_368 = ivtmp.89
  + ivtmp.37; MEM[_368 + ivtmp.90]`), so the load's base is a register
  the loop computes. The setup can still compute the area's start (the
  induction register's start plus the invariants and the constant);
  leaving such loads out kept 127 of SP's loads on memory (taking them:
  SP.W 25.40 → 23.71 s on the 2C3). Compare stores in the same terms:
  lcc reads all 37 loads of that loop through APB on v5 and v6, the five
  `rhs` planes it updates in place included (a store to an element
  already read is fine, apb.md, "Stores into a prefetched array").
- **Address chains after combine** (P91; the proof of concept's
  observation, not rechecked): combine can leave an address as a chain
  of dependent additions inside a loop; the proof of concept's
  `e2k_addr_height` pass after combine rebuilds such a chain from its
  terms, adding the loop-invariant ones once on the loop's entry, after
  which bzip2's `generateMTFValues` loop pipelined again.
- **After register allocation, analyse addresses per definition**
  (P57): one hard register holds several values of a loop body
  (addresses and doubles share one register file), so a post-RA
  pipeliner like C6x's that wants one definition per register in the
  loop loses loads: in SP's xi fluxes `_368`'s register later held three
  sums of doubles, and 21 of 38 loads stayed plain. Take a definition
  when every address through it, directly or through registers derived
  from it, comes before the register's next definition; use-def chains
  give that (`df_chain_add_problem (DF_UD_CHAIN)`, df.h:1038, as
  `mips_df_reorg` does, mips.cc:19813), or analyse before allocation.
  The PoC: 6 plain loads left, SP.W 23.65 → 21.10 s on the 2C3.
- **If-convert loops before RTL, loads included** (P49): for `ix[k] =
  (xr[k] >= roundfac) ? ix[k] : 0` (LAME's `count_bits`), GIMPLE loads
  `ix[k]` only under the condition and stores a PHI of it and 0. RTL
  if-conversion won't run a load that may trap (`noce_try_cmove_arith`,
  ifcvt.cc:2683), so the loop stays two blocks and isn't pipelined.
  GCC's tree if-conversion knows the load is safe, because the same
  reference is written on every path (`ifcvt_memrefs_wont_trap`,
  tree-if-conv.cc:903), and turns the PHI into a `COND_EXPR`, but it runs
  only for the vectorizer: it versions the loop under
  `.LOOP_VECTORIZED` and the converted copy is deleted when the loop
  isn't vectorized (tree-vectorizer.cc:1350); `-ftree-loop-if-convert`
  does nothing (common.opt:2040). So the port needs its own GIMPLE step,
  or a GCC change that keeps the conversion. The PoC's step runs such
  loads always and makes the PHIs selects; the loop then pipelines at II
  1 (LAME 9.34 → 7.64 s on the 8C2, lcc 7.44). lcc makes the loop one
  `loop_mode` block: the load `,sm` every iteration, `merges`, an AAU
  store (lcc `-O3`, v3/v5/v6).
- **A loop that calls: the errno call and the next iteration** (P84;
  NPB EP's acceptance loop, `t2 = sqrt (-2.0 * log (t1) / t1)` for the
  points inside the unit circle, about 78.5%). GCC's scheduler keeps
  every memory access after a call, const and pure calls included: a
  call becomes the last memory flush, on which every later load or
  store depends (sched-deps.cc:1795, :2687–2688, :3909), and it waits
  for every earlier store (:1769–1771); before reload nothing crosses a
  call (:3843–3848). So `-fno-math-errno`, which makes `log` const
  (builtins.def:279–289, :548), doesn't free the loads after it.
  `tree-call-cdce` turns `y = sqrt (x)` into `if (__builtin_isless (x,
  0)) y = sqrt (x); else y = .SQRT (x)` (tree-call-cdce.cc:96–103), so
  the inline root sits in the arm opposite the call and if-conversion
  predicates it (D8 gives the shape to use instead: the root on every
  path, then the call for errno only). lcc (1.29.16, `-O2` and `-O3`,
  v3, v5, v6)
  issues `fsqrtid,sm` on every path with `fcmpledb 0, x` into a
  predicate in the same or the next wide instruction, calls `sqrt` when
  that test fails (for errno, taking its result on that path) between
  `fsqrtid` and `fsqrttd,sm`, and only then loads the next point: at
  `-O3` the next two points' loads and multiplies run beside `fsqrttd`
  and the tally, at `-O2` beside the tally. The divide overlaps nothing
  on that path (at `-O2` the next point's work shares the divide's wide
  instructions only when the current point was rejected and the divide
  is speculative). lcc's order is the one D8 warns about: for a
  negative operand nothing raises invalid before its call, which then
  traps in MCST's libm when `FE_INVALID` is clear. The proof of concept
  runs each point as one chain (`log` → `fmuld` → `fdivd` → `fcmpuddf`,
  `cmpesb` → `fsqrtid ? ~p` → `fsqrttd ? ~p` → `call sqrt ? p` → the
  next point's loads): 39% more samples than lcc in the loop on the
  8C2, EP.W at 1.10–1.12 of lcc's time on all three machines. D8's
  shape (P85) gains 1%; `-fno-math-errno`, which removes the `sqrt`
  call and with it the barrier before the next point's loads, gains
  nothing more (class A: 55.15 against 55.23 s on the 8C2, 41.27
  against 41.37 on the 2C3), so neither the call's place nor memory
  order at the calls explains the rest of the gap; not settled. Keep
  D8's order.
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
hardware loop buffer (`hwloop_optimize` :5478, its `opt` hook for
hw-doloop.cc; 7.7, "Hardware loops after register allocation"). C6x
has an exposed (non-interlocked) pipeline, so much of its complexity is
not needed for E2K.

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
   under qemu-e2k or over ssh on the Elbrus machines. GCC ships no board
   files; relcc2's are in `tools/dejagnu` (`e2k-qemu`, `e2k-4c`,
   `e2k-8c2`, `e2k-2c3`, `e2k-ssh`; README.md there). With
   `DEJAGNU=tools/dejagnu/site.exp` and the binutils named
   `e2k-linux-gnu-*` in `PATH` (`tools/dejagnu/run-gcc-tests.sh -b
   BUILD` does both):
   `make check-gcc RUNTESTFLAGS="--target_board=e2k-qemu execute.exp"`
   for `gcc.c-torture/execute`, later the whole `check-gcc`, compared with
   `contrib/test_summary` / `contrib/compare_tests` between revisions.
   Under MCST's qemu a memory fault or a division by zero kills qemu
   instead of raising a signal, so tests that catch SIGSEGV or SIGFPE
   need a hardware board.
3. **GCC's own compat tests** against lcc: `gcc.dg/compat` and
   `struct-layout-1.exp` with `ALT_CC_UNDER_TEST=<lcc>` (sourcebuild.texi
   :4309–4430): a second, independent ABI check.
4. **ABI probes** in `tools/abi/probes` (`layout.c` must compile with your
   GCC too).
5. **Hardware** for anything involving tags, speculation, or timing:
   qemu differs from the 4C (see the tag-trap note in
   `calling-convention.md`).
6. **Failures that come from MCST's libm**, not from the port
   (`spec/abi/registers.md`, "MCST glibc's FP bugs"):
   - A test that clears the FP flags and then calls libm can die with
     SIGILL inside it. `gcc.dg/torture/pr68264.c` dies in `acos`, also
     when lcc builds it [hw: 8C2].
   - A test that calls `fmod` or `remainder` in `FE_DOWNWARD` can hang
     (mainline's `gcc/testsuite` has none).
   - A test that checks the flags libm raises for domain errors fails
     (inferred).

   Build such a test with lcc too before debugging the port.

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

1. **DejaGnu board files** for qemu-e2k and for ssh to the 4C, 8C2
   and 2C3, and a script to run GCC's testsuite with them: done,
   `tools/dejagnu` (a smoke suite checks each board).
2. **Timing tables** for more generations with
   `tools/isa-timing/bench.py`: the 4C (v3), 8C2 (v5) and 2C3 (v6)
   tables are in `spec/isa/timing.md`; v4 (via the community) is left
   (v7 is out of scope: not in silicon, no access).
3. **Channel/resource data** from `binutils-ops.yaml` in a form easy to
   turn into reservations (data only; the generator that writes `.md`
   belongs in the port). Done: `spec/isa/reservations.yaml`, written by
   `tools/isa-extract/gen_reservations.py`: per operation and ISA
   version (v2–v7) the channels, channel pairs, ALES use, literal sizes,
   load/store and MAS class; the syllables and fields of every control
   operation (CS0, SS, CS1 groups, PLS, CDS, AAS, LTS); the restrictions
   of bundle.md and gas with their sources; and the ALC5 divider's
   measured issue distances on v3, v5 and v6.
   `tools/isa-extract/check_reservations.py` checks it against every
   wide instruction of compiled code: none of the 7.6 million in lcc
   1.29.16's system libraries (all for v3, v5 and v6, `libc.so.6` for
   v4), its CoreMark builds and the decoder corpus breaks it (bundle.md,
   "Bundling restrictions"). It also counts what lcc puts together: a
   prepare and `setwd` in one instruction about 19,800 times in its v3
   code, a prepare and a `ct` 16,000, two MAS accesses 20,300.
4. **Speculation leftovers** (`spec/isa/speculation.md`): done for v3,
   v5 and v6 (the 2C3, including its MAS forms).
5. **Loop mode leftovers** (`spec/isa/loops.md`): `ldovl` and the
   loop-register write timing are done on all three machines, `abg`
   on v5 and v3 (lcc never emits it).
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
  v4. The suite also passes on an Elbrus-2C3 (v6, lcc's 2C3 build,
  every pair), and lcc's v3 code linked with its 8C2 or 2C3 code, or
  with Unipro's `-march=elbrus-v3`/`v5`/`v6` code, passes it on those
  machines (`run.py --target v3,v5.8c2`, `v3,v6.2c3`: the convention
  doesn't depend on the target, so `-march` variants of the port can
  call each other). Registers a window gains hold an empty value on all
  three machines (0 with tag 5 on v5 and v6, as under qemu; on the 4C the
  stale bits with bit 30 cleared, tag 5), which a plain `std` stores
  without a trap (`spec/abi/calling-convention.md`, "Unused parts of a
  slot"). v6 has longer load latencies (performance only).
- `UPSR.fsm` is never changed (2026-10-11): with it set the FP predicate
  compares give x86 results for NaN operands (semantics.md), and code
  may assume it clear (registers.md).

### lcc's builtins and intrinsics (source compatibility)

Code written for Elbrus uses lcc's `__builtin_e2k_<op>` builtins (448,
one machine operation each with a few exceptions), the MAS, barrier,
CPU-detection and system-register builtins and `<e2kintrin.h>`'s short
forms and helpers; and lcc predefines the x86 feature macros
(`__SSE2__` … `__AVX2__`) on every target and emulates the x86
intrinsics, so x86 SIMD paths of portable programs build on E2K.
spec/abi/lcc-builtins.md specifies all of it for lcc 1.31.05: every
name lcc knows (checked against about 60,000 candidate names), what each compiles
to, and the behaviour of everything that is not one operation, verified
on the 4C, 8C2 and 2C3. lcc's and Unipro's headers are "all rights
reserved", so the port writes its own: 357 operation builtins are the
operation with the arguments in order and can be generated from
spec/abi/lcc-builtins.yaml (with lcc's corrected signatures from
lcc-builtins-check.yaml) and binutils-ops.yaml; the page lists the rest.
The port also needs x86's six `__float128` builtins (`__builtin_fabsq`,
…), which lcc supports. `tools/abi/builtins/run.py --cc gcc=…` is the
acceptance test: it compares every builtin's results with lcc's on
hardware, and reports the ones the compiler lacks as missing, so it
can be run while the port adds them (it finds a builtin with
`__has_builtin` or as a macro, so one that the port's header defines
as an inline function, as lcc's does `puttagd`, counts as missing).
**Owner's decision (2026-10-06): the port provides
these compatible builtins, and no x86 or NEON intrinsics and no
predefined x86 feature macros**; programs with x86 SIMD paths take their
generic ones. (Upstream GCC's precedent for an emulation layer, should
one ever be wanted, is PowerPC's opt-in x86 intrinsic headers in
`gcc/config/rs6000/`, which stop with `#error` unless built with
`-DNO_WARN_X86_INTRINSICS` and come with no `__SSE__` macros.)

### What a v5 mode adds (lcc 1.29.16 for the 8C2, measured)

lcc's v5 code for CoreMark and Linpack differs from its v3 code in only
a few ways, a guide to what an `-march=elbrus-v5` mode needs first:

- **Array prefetch without alignment**: v5 allows unaligned areas, so
  lcc uses APB where its v3 code needed `-faligned` (1.29.16 has no
  run-time test; 1.31 versions long loops) (Linpack: 52 APB programs in
  the v5 build, none in the v3 one; NPB class W: CG 22, FT 13, MG 22,
  LU 78 against 7, 5, 14 and 73).
- **More FP channels**: `faddd`/`fmuld`/`fmul_add*` also in ALC2/5 (6 a
  cycle); lcc uses them sparingly (19 of 399 FP operations in Linpack).
  Watch the one conflicting pattern with the divider (timing.md).
- **`%lsr1`** for loop counts (written after `%lsr`, loops.md; it
  takes effect after 4 cycles like `%lsr`, and the two writes may be in
  consecutive instructions).
- **128-bit data**: two `ldd` + `qppackdl` for loads (`ldqp` goes to L2,
  latency 11), `stqp` for 16-byte stores, `qp*` operations with their
  64-bit latencies; for 16-byte atomics lcc's `ldqp`/`stqp` sequences
  (`spec/abi/atomics.md`, "16-byte atomics"), but with every atomic load
  one `ldqp`: lcc's relaxed load is two `ldd`, which tear once they are
  a few cycles apart (and once in 4 million reads even in consecutive
  instructions). Define `atomic_loadti`: without it
  `can_atomic_load_p` (optabs-query.cc:675-688) takes 16-byte loads for
  non-atomic, and `expand_atomic_exchange` won't inline an exchange
  (optabs.cc:7509-7513). Give exchange its own pattern, lcc's
  two-instruction lock loop; GCC's fallback for it is a compare-exchange
  loop (A29).
- **`getfzd`/`getfzs`** (v5 field extract) in place of v3 sequences.

Everything else (latencies, hazards, control transfer, the ABI) is the
same as v3 on the 8C2.

### What a v6 mode adds (lcc 1.29.16 for the 2C3, measured)

Measured on an Elbrus-2C3 (timing.md "Elbrus-2C3 (v6)", semantics.md
"v6 operations", operations-v6.md). For `-march=elbrus-v6` and
`-mtune` for it:

- **Scheduling**: loads 5 cycles (any cluster), `ldqp` 5 and four a
  cycle (so 128-bit loads become single `ldqp`, as lcc's 2C3 libraries
  do: 5,747 of them), integer compare → predicated op 1, packed compare
  into a predicate 4 (5 to `ct`), FMA 5. The divider no longer delays
  ALC5 FP operations; the store bank rule stays.
- **Fused multiply-add**: `fma`/`fms`/`fnma`/`fnms` scalar and `qp`
  patterns (`fma<mode>4`, `fms<mode>4`, `fnma<mode>4`, `fnms<mode>4`),
  rounded once, 5 cycles against `fmul_add`'s 8; contraction under
  `-ffp-contract=fast` (GCC's default for GNU C) then uses them. lcc
  differs: at `-O3` for the 2C3 it keeps `a * b + c` as the twice-rounded
  `fmul_addd` (the same result as separate operations), calls libm for
  `fma()` and emits `fmad` only for `__builtin_fma`; its
  `-ffp-contract=fast` fuses recurrences (Horner) and `restrict` loops
  (`fmsd`, `fnmad`) and drops `fmul_addd` in other loops; `-ffast-math`
  also fuses `fma()` and loops without `restrict`; neither fuses a
  returned `a * b + c` or a reduction (operations-v6.yaml; the
  proof-of-concept back end's P40, rechecked). lcc's 2C3 libraries use
  `fmad` 857 times. Decide whether the port follows GCC's default
  (fused, results differ from lcc's in the last bit) or lcc's (no
  contraction unless `-ffp-contract=fast` or `-ffast-math`, reductions
  unfused, as the proof-of-concept back end now does);
  `__builtin_fma` should become `fmad` either way.
- **Vector exits**: `qpcmpeqbop`/`qpcmpeqbap` (a compare straight into
  a predicate) for `cbranch` on vectors and the string functions,
  `qpmrgp` for a select by a predicate, `qpsrad` for 64-bit arithmetic
  shifts, `clmull`/`clmulh` for carry-less multiplies.
- **APB**: 32 bytes a cycle per area, so two `movaqp` from one area per
  iteration cost nothing extra. Areas keep their positions across traps,
  so any loop shape works, but write `%lsr` (0 will do) before a program
  that isn't counted by it: on the 2C3 a `mova` outside `loop_mode` is
  SIGILL while `%lsr` says a counted loop has ended (`vlc` 1, `lcnt` 0,
  `ldmc` 0), the state glibc's `strncmp` leaves (`spec/isa/hwbugs.md`).
- **Atomics**: lcc's v6 code is its v3 code (`wait` + plain accesses).
  v6's load-acquire / store-release MAS forms (0x13) run, and a release
  store that way costs about a quarter of `wait` + store on the 2C3,
  but their ordering wasn't checked, lcc doesn't use them, and
  MCST's kernel lists the store-release as broken on the E2C3 up to
  revision 2 and the E12C/E16C (spec/isa/hwbugs.md): follow lcc, and
  keep them as a later option (speculation.md, "v6
  encodings"). The new fully speculative MAS 0x12 is what v3's 0x3 was.
  16-byte atomics load with one `ldqp` at every order, and their
  compare-exchange tests with `qpcmpeqdap` into a predicate and selects
  with `qpmrgp` (`spec/abi/atomics.md`, "16-byte atomics"). The 2C3 needs
  the release store's `wait` most of all: there a reader that orders its
  loads still sees two plain stores out of order (atomics.md, "Memory
  ordering on hardware").

The ABI is unchanged (the compat suite passes on the 2C3). What tuning
buys there: lcc's 2C3-tuned code runs CoreMark 1.19× and Linpack 1.35×
faster than its generic v3 code on the same machine, and interlock
stalls fall from 30% of CoreMark's cycles to 2%, so `-mtune=elbrus-v6`
(5-cycle loads) is worth having even with `-march=elbrus-v3`.

