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
| `setwd dbl=1` makes 32-bit ops zero-extend into the full register; `dbl=0` leaves the upper half untouched (including a stale diagnostic tag that traps when stored) | `registers.md`, `calling-convention.md` "Unused parts of a slot" | Recommend `dbl=1` (D7) |
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
paths (e.g. the DWARF register ranges in `_Unwind_SetGR`).

Until libgcc builds (M3), link against the system's `libgcc_s.so.1`
from lcc's sysroot.

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
- Start every file with `.ignore ld_st_style` like lcc and Unipro so
  stores are written *address, offset, data* (`bundle.md`, "Assembler
  syntax notes"). Do **not** copy lcc's `.ignore strict_delay` yet: without
  it, gas pads compares and state-register accesses for you, which avoids
  costly stalls in braceless code.

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

- Split calls/branches into prepare + transfer so the scheduler hoists
  `disp` 5 cycles ahead; `return` early in the epilogue.
- If-conversion to predicated code (`define_cond_exec`).

### M6. Speculation (research-grade)

- Control speculation via selective scheduling (section 7.6).
- Data speculation (load + check) like ia64's `ld.a`/`chk.a`.

### M7. Loops (research-grade)

- Counted loops (`doloop_end`), modulo scheduling with register copies
  (GCC's SMS), then E2K loop mode and rotating registers, then APB.
  Section 7.7.

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
Caveat: rotating loops (M7) also want `%b[]`; at that point outgoing
registers must be printed as `%rN` with the final window size (ia64's
renaming approach), or the loop's rotating area placed elsewhere.

**Static chain** (for nested functions and trampolines): use `%g12`,
which MCST's libffi already uses for its closures
(`spec/abi/registers.md`).

**Call-clobbered**: globals (except `%g13`, fixed; `%g12` as the static chain), the outgoing
registers, `%ctpr1–3`, `%lsr`. Window registers `%r*` are preserved by
hardware: mark them call-saved, and the prologue never saves them.
Predicates are also preserved (each call gets a fresh predicate file).

**Register allocation order**: prefer low window registers so the window
stays small (`REG_ALLOC_ORDER`; ia64.h:446 for the same idea).

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
`%pcnt`/`%LOOP_END` conditions come later.

### D4. Control transfers

| Stage | Jumps | Calls | Returns |
|---|---|---|---|
| M1 | `ibranch label [? %predN]` | — | `return %ctpr3` + `ct %ctpr3` in one pattern |
| M2 | same | one pattern printing `disp %ctpr1, f` and `call %ctpr1, wbs=W` | same |
| M5 | prepare (`disp %ctprN`) and transfer (`ct %ctprN [? pred]`) as separate insns; `%ctpr1–3` an allocatable class; scheduler latency 5 between them | same, prepare hoisted | `return` hoisted 6 cycles |

Interlocks make the M1/M2 forms correct (the hardware waits for the
prepared target, guide 4.4 "неподготовленный переход"); M5 makes them
fast. The same holds for the compare that produces a branch's predicate
(interlocked, latency 3, `spec/isa/timing.md`). C6x's "real insn + shadow" pairs
tied with `record_delay_slot_pair` (c6x.cc:5118, haifa-sched.cc:730) are
the closest existing model of "these two must be N cycles apart", but
for E2K a plain dependence with latency 5 between the `disp` insn and the
`ct` insn (via the CTPR register) is enough, since early is fine.

Indirect calls: `movtd %reg, %ctprN` (latency 9) then `call`.

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

| | `dbl = 0` (lcc) | `dbl = 1` (Unipro) |
|---|---|---|
| `adds`, `shls`, `muls`, other 32-bit integer ALU ops | write the low 32 bits only; the upper half (and its tag) stays as it was | zero-extend into all 64 bits |
| loads (`ldw` etc.), `sxt`, 64-bit ops | whole register | whole register |
| 32-bit float ops (`fadds`), `movts` | low half only | low half only |

lcc always emits `dbl = 0` (every lcc function above starts with
`setwd ..., dbl = 0x0`), which is also how pre-v3 CPUs behave; that is
why the emulator must support both.

Recommend **`dbl = 1`** for GCC:

- GCC's RTL often uses a 32-bit operation and then needs the 64-bit
  value; with `dbl = 1` the upper half is known to be zero, which you can
  tell GCC, saving explicit zero-extensions.
- Writing whole registers avoids the diagnostic-tag trap from partly
  written registers (`calling-convention.md`, "Unused parts of a slot",
  confirmed on the 4C).
- The flag is saved per frame, so mixing with lcc code is safe.

Caveats: 32-bit float results still leave the upper half alone, so
don't assume anything about it there; the ABI still requires callers to
**sign**-extend signed narrow arguments (emit `sxt`), and callees must
not rely on the upper bits they receive (lcc re-extends).

### D8. Floating point

Same registers as integers (no separate FP class). `long double` is
80-bit x87-style in the register's extended part; functions using it set
`nfx=0`. Integer↔FP latency penalties (+1/+2 cycles) go into the DFA's
bypasses, not into register classes.

### D9. Assembly output conventions

- `.ignore ld_st_style` at the top of every file.
- Write `%empty` (gas), not `%empty.hi` (Unipro).

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
  `movtd`→`ct` 9, FP or multiply → integer +2; integer → FP has no penalty
  on the 4C (the guide says +1). These match guide 6.1 otherwise. A stall
  costs about a whole latency extra, so latencies must be exact. v6 and
  later differ (e.g. load→use 5, binutils comment in `bfd/elfxx-e2k.c`
  `build_plt_entry`) and still need measuring.
- **Size**: keep automata small by splitting independent resources into
  separate `define_automaton`s (ALU channels, literals, control); C6x
  uses 7 automata with `ndfa`, `collapse-ndfa`, `no-comb-vect`
  (c6x.md:267). `automata_option "v"` writes a `.dfa` file showing the
  generated automaton.
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

### 7.5 Explicit channels (optional)

gas chooses channels, so the port need not. If gas ever rejects a group
the DFA accepted (the DFA modelled a channel rule too loosely), either fix
the DFA or print channels yourself the way C6x prints units: record after
each issue which `alc` units became reserved (`c6x_variable_issue`,
c6x.cc:4350, using `cpu_unit_reservation_p`), then assign the most
constrained instructions first (`assign_reservations`, c6x.cc:3029) and
print `,N` with a punctuation code in the output templates.

### 7.6 Speculation

- **Control speculation** (moving an operation above the branch that
  guards it): mark it `,sm`; a fault becomes a tagged result that only
  traps if a non-speculative operation uses it. No check instruction is
  needed. GCC's Haifa scheduler always inserts a check for speculative
  instructions (`create_check_block_twin`, haifa-sched.cc:8347); only the
  **selective scheduler** has a mode without checks,
  `SEL_SCHED_SPEC_DONT_CHECK_CONTROL` (sel-sched.cc:3208), used by ia64
  with `-msel-sched-dont-check-control-spec`. The hooks to implement are
  ia64's: `TARGET_SCHED_SET_SCHED_FLAGS` (ia64.cc:8072),
  `TARGET_SCHED_SPECULATE_INSN` (8368, returns the speculative form of a
  load as an `unspec`-wrapped pattern), `TARGET_SCHED_GET_INSN_SPEC_DS`,
  `TARGET_SCHED_SKIP_RTX_P`, and the sched-context hooks (selective
  scheduling keeps several states).
  Caveat: non-load operations (division, FP) can also fault; their `,sm`
  forms are needed too if they are moved.
- **Data speculation** (moving a load above a store that might alias):
  E2K has a load + later check that reloads (guide 4.5; the `%MLOCK`
  branch condition in `bundle.md` "ctcond"). This matches ia64's `ld.a` +
  `chk.a`/`ld.c` (ia64.md:440–708; hooks `TARGET_SCHED_NEEDS_BLOCK_P`,
  `TARGET_SCHED_GEN_SPEC_CHECK`, ia64.cc:8531–8571). The E2K details
  (which MAS/flags, check semantics) are not yet in `spec/`; relcc2 needs
  to work them out first.
- Selective scheduling is enabled by ia64 at `-O3` only (ia64.cc:6182);
  it is maintained (MAINTAINERS lists ISP RAS) but used by few targets, so
  expect bugs.

### 7.7 Loops

- **Counted loops**: a `doloop_end` pattern lets GCC turn a loop into
  "decrement counter and branch". E2K's loop counter (`%lsr`,
  `ct ? %LOOP_END`, `alc`) fits; ia64 does the same with `ar.lc`
  (ia64.md:3957).
- **Software pipelining**: GCC's SMS (`modulo-sched.cc`, `-fmodulo-sched`)
  overlaps iterations and uses register copies instead of rotating
  registers; it runs before register allocation and needs `doloop_end`.
  C6x runs a post-RA modulo scheduler built on Haifa
  (`hwloop_optimize`, c6x.cc:5478, `set_modulo_params`,
  haifa-sched.cc:580) — the closest model for E2K's hardware loop mode.
  Selective scheduling can also pipeline (`-fsel-sched-pipelining`).
- **Rotating registers**: two different things share the name.
  *Window shifting* — each call slides the register window, as SPARC's
  `save`/`restore` and E2K's `call wbs=` do — is handled by hardware and
  GCC models it with `INCOMING_REGNO`/`OUTGOING_REGNO` (D1). *Loop
  rotation* — inside a loop, an instruction advances a base so that the
  register written as `%b[0]` in one iteration is read as `%b[1]` in the
  next (E2K `abn`, ia64 `br.ctop`), which lets overlapped iterations keep
  their own copies without moves — is what software pipelining needs.
  SPARC has no loop rotation. GCC has **no support for loop rotation
  anywhere**; ia64 never used its rotating registers (`n_rotate_regs = 0`, "No rotating register
  support yet", ia64.cc:2769). Supporting E2K loop mode with rotation
  (`abn`/`abp`, `%b[]` rotation, prologue/epilogue predicates) is new
  compiler work — a research project for after the port works.
- **APB** (array prefetch): later still.

### 7.8 Measuring

Correctness with the compat suite and GCC's testsuite; speed with `perf
stat` on the 4C against lcc on the same sources. The guide's chapter 5
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
2. **Timing tables** for more generations (v5 on the 8CB, v4/v6 via the
   community) with `tools/isa-timing/bench.py`; the 4C (v3) table is in
   `spec/isa/timing.md`.
3. **Channel/resource data** from `binutils-ops.yaml` in a form easy to
   turn into reservations (data only; the generator that writes `.md`
   belongs in the port).
4. **Data speculation** spec: the E2K load/check (`%MLOCK`, MAS values)
   semantics, so section 7.6 can be implemented.
5. **Loop mode and rotation** spec details for 7.7.
6. Review of the port's output against `spec/` and lcc, and test
   triage.

### Decisions and open questions

Decided by the owner:

- Unwinder and E2K libgcc files: MCST's (3.3).
- Default target: **`-march=elbrus-v3`**, forward compatible (no
  `EF_E2K_INCOMPAT`; `spec/abi/elf.md`). How MCST keeps v3 code working on
  later CPUs is not public, so forward compatibility is established by
  testing: lcc's v3 code passes the whole compat suite (lcc → lcc,
  1,853 tests) under MCST's qemu as v5, v6 and v7; the owner can test on v5 (8CB), and the community on
  v4 and v6. Known differences to keep in mind: v5 and later may fill
  fresh window registers with tagged values (the tag trap,
  `calling-convention.md`), and v6 has longer load latencies
  (performance only).

Open:

- `dbl = 1` (recommended, D7) or `dbl = 0` like lcc?
