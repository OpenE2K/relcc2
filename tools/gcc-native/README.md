# Native GCC builds on Elbrus machines

`gcc-native.sh` builds a GCC for E2K **on** an Elbrus machine and
rebuilds it with itself, then runs its testsuite there and times the
built compilers as programs. It drives `remote.sh` over relcc2's SSH
wrapper (`tools/setup/e2k-ssh`). Any GCC git checkout with an
`e2k-*-linux-gnu` port will do.

Status: **draft** (2026-10-13; bootstraps below). Checked first, all on the 8C2:
- `check`;
- `push` (21 s for the 1.3 GB tree);
- `bootstrap --stop-after configure`, which guessed
  `e2k-unknown-linux-gnu`, took lcc and `l++ -std=c++14`, and found the
  system's gmp, mpfr and mpc;
- stage 1's libiberty built by lcc `-O3` (80 s);
- `fetch` and `clean`.

The first full bootstrap (2026-10-13, `f5316cda77d`) stopped in stage
1 after 57 minutes. lcc refused one file, `gcc/tree-data-ref.cc`: it
passes a `vec<tree>` through `...` and fetches it with `va_arg` (EDG
error #1291). `native-build` commit `76b0bd07e3c` gives `conflict_fn`
fixed parameters instead. `xcheck` (below) then compiled every host
object with lcc, and the bootstrap was restarted on 2026-10-14.

Bootstraps 2 to 7 (2026-10-14/15) found seven PoC bugs, fixed on
`native-build`, which is v0.4 plus those fixes. Four were miscompiles of
GCC itself, found by object bisection against lcc-built objects:
- a hoisted invariant clobbered the register an `rwd %lsr` reads;
- a predicate written one cycle before a taken return, across an empty
  block;
- while-loop renaming left behind;
- `vec_init` turned a negative constant lane into all ones.

Three made `-g` change the code, which `bootstrap-debug`'s comparison
(stage 2 without debug information, stage 3 with it) caught:
- debug labels triggering gas's CODE_PLACEMENT padding;
- a debug insn counted as room to move;
- `.LFE` in C without unwind tables.

Bootstrap 7's comparison failed on one object (libiberty's
`floatformat.o`, the `.LFE` case). Measured on the 8C2, with checking
compiled in and the same source: the PoC-built cc1plus is within 1% of
the lcc-built one (`cp/pt.cc`, `fold-const.cc`, ... about 1,000 s in
all, identical output), and lcc itself compiling the same files at
`-O3` takes 20% longer.

**Bootstrap 8 passed** (`5ec2d06d909`, 2026-10-16; 7 h 10 min on the
8C2 at `-j 4`): stage 2 and 3 objects are identical. `pack` made the
native tarball from its install:
`gcc-e2k-0.4+native-build.7-e2k-elbrus-8c2-native-elbrus-linux-9.5.5.tar.xz`,
123 MB. Its binaries are marked `elbrus-8c2`, because they link with
the 8C2's system libraries.

Unpacked in a fresh directory on the 8C2, the tarball:
- builds and runs C and C++ programs;
- builds busybox 1.37.0, whose testsuite gives 956 PASS and 6 FAIL. All
  6 also fail with lcc's build (checked on the 4C);
- builds coreutils 9.5, whose `make check` gives 527 PASS, 125 SKIP and
  1 FAIL.

The coreutils failure is `tests/rm/r-root`, which lcc's build passes.
It drives `rm` under gdb with a breakpoint script on `remove.c:391`.
gdb finds 4 locations for that line in the PoC's `-g -O2` build
against 1 in lcc's, so the script sees more hits than calls. This is
a debug-information matter, not a wrong result.

## What it is for

The experimental back end (`toolchains/gcc-claude`, AGENTS.md decision 2)
is built as a cross compiler on x86. A native build exercises it in
three new ways:

1. **Self-hosting.** `bootstrap` runs GCC's `make bootstrap`. lcc
   builds stage 1, stage 1 builds stage 2, and stage 2 builds stage 3.
   The stage 2 and stage 3 object files must then be identical
   ("Bootstrap comparison failure" otherwise). GCC is a large C++
   program, so any miscompilation by the port shows up as a crash, a
   wrong stage 3 or a comparison failure.
2. **Native testing.** `test` runs `make check-gcc` on the machine
   itself. This complements the cross runs through `tools/dejagnu`.
3. **The compiler as a benchmark.** `bench` times `cc1plus` built by lcc
   (stage 1) against `cc1plus` built by the port (stage 2, stage 3, or
   another revision's install) on the same preprocessed compiler sources.
   It also checks that their output is the same. A large real C++
   program is the workload, which tests both the port's code quality and
   its correctness.

`selfbuild --with OTHER` builds one revision with another revision's
installed compiler. It catches a newer revision being miscompiled by an
older one, and the reverse.

## Is GCC miscompiled?

Two builds of one GCC revision must turn the same input into the same
assembly, whoever compiled them. GCC's own bootstrap comparison rests on
this. Here one build is compiled by lcc, taken as correct, and the other
by the port under test. Any difference means the port miscompiled GCC
somewhere on that input's path. Three checks follow from this:

1. **The bootstrap comparison.** Stages 2 and 3 must compile GCC's own
   sources identically.
2. **`diffcc A B`** compiles GCC's C torture tests (`gcc.c-torture`,
   `gcc.dg/torture`) with both compilers and compares the assembly, exit
   status and diagnostics. The default flag sets are `-O0`, `-O2`, `-O3`
   and `-O2 -march=elbrus-v5`. When both compilers are installed
   (`NAME:inst`), it also compiles the C++ torture tests at `-O0` and
   `-O2`. Each compile adds `-S -w -frandom-seed=0`. It is quick and
   catches more than pass or fail does. The pairs to compare:
   - `NAME:stage1` (lcc-built) against `NAME` (stage 3);
   - or, between installed builds, an `lccbuild` against a `selfbuild`
     of the same revision.
3. **`triage REF`** handles build NAME's testsuite failures (`test`).
   For each failing test it compiles the source with the options its
   `.sum` line and its `dg-options` name, by the installed REF (lcc-built)
   and NAME compilers. The same output means the failure is a back-end or
   test problem; a different output points at a miscompiled GCC.

`diffcc.py` does the work of both; its results are in `bench/diffcc-*/`
and `log-NAME/triage-REF/`. Each has `summary.txt`, `diffs.txt` (one
line per difference) and `cases/`, which holds both outputs of the first
200 differences.

To build a second copy of an already pushed revision, give it its own
`--name` and `--source NAME`, for example
`--name X-self selfbuild --with X --source X`. Sharing one source tree
keeps the source paths in the objects equal.

## Where the source comes from

`toolchains/gcc-claude-native` is a git worktree of the PoC repository on
branch `native-build`. It is separate from the PoC agent's checkout
(`toolchains/gcc-claude`, branch `claudes-e2k-compiler`), so native-build
work never touches that agent's files or builds. To follow the PoC:

```
git -C toolchains/gcc-claude-native merge --ff-only claudes-e2k-compiler
```

Fixes that only native builds need go on `native-build`, and the PoC
agent merges them. `push` sends a commit with `git archive`, so
uncommitted changes are not sent (it warns about them).

## Machines

| | 8C2 (default) | 4C | 2C3 |
|---|---|---|---|
| cores, memory, free disk | 8, 125 GiB, ~790 GB | 16, 61 GiB, ~80 GB; at most 512 processes | 2, 15 GiB |
| lcc, binutils | 1.29.16, 2.41.0-29.023 | the same | the same |
| gmp, mpfr, mpc headers | yes | **no**: `push --prereqs` | |
| flex, bison | yes | yes | |
| makeinfo | yes | no (GCC skips the manuals) | |
| expect, runtest | expect only: `push-dejagnu` | **neither**: no testsuite | |
| a detached command after ssh closes | keeps running | **killed** (ALT's logind): `--hold` | |

Use the 8C2. Don't build on the 2C3: it belongs to a contact and has two
cores.

## Checking a revision with lcc on this host first

Stage 1 of a bootstrap is lcc compiling all of GCC's host code: hours
on the 8C2. `xcheck` compiles the same code on this host instead, with
lcc's x86-hosted cross compiler. It is a canadian cross (build x86_64,
host and target e2k) running `make -k all-gcc` in
`toolchains/gcc-native/xcheck`. lcc 1.29.16 `-O3` at `-j6` took 30
minutes for the whole of `all-gcc` (`cc1`, `cc1plus`, `lto1`, `xgcc`).
Later runs rebuild only what changed.

`xcheck` compiles `--src`'s working tree as it is, uncommitted changes
included, so a fix can be tried before it is committed and pushed. It
lists every lcc error by file and line. The one failure every run has is
`specs`, which needs an x86-hosted e2k `gcc` that a canadian cross
doesn't have. The 8C2's gmp, mpfr and mpc headers and libraries are
copied once into `xcheck/deps`, because lcc's sysroot has none.

Run it after merging new PoC commits into `native-build`: the back end's
own files are host code too, and lcc must accept them.

## Commands

```
G=tools/gcc-native/gcc-native.sh
$G check                         # what the machine has
$G push                          # HEAD of toolchains/gcc-claude-native
$G bootstrap                     # detached; hours
$G wait                          # status every 10 minutes until done
$G status                        # every build there
$G log [make.log|configure.log|install.log|check.log]
$G push-dejagnu                  # once per machine without runtest
$G test 'execute.exp dg.exp'     # make -k check-gcc RUNTESTFLAGS=...
$G bench-inputs                  # preprocess the benchmark sources once
$G bench NAME:stage1 NAME        # lcc-built against self-built cc1plus
$G pack                          # native tarball from a finished bootstrap
$G fetch                         # logs, .sum files, bench results here
$G compare OLD NEW               # two fetched builds, contrib/compare_tests
$G clean                         # remove build NAME; clean --all: everything
$G xcheck                        # here: lcc compiles all host code (above)
$G diffcc NAME:stage1 NAME       # lcc-built and self-built: same assembly?
$G triage REF                    # NAME's test failures, also with REF?
```

Global options go before the command: `--remote USER@HOST`,
`--ssh-port N`, `--remote-dir DIR` (default `relcc2/gcc-native`),
`--src DIR`, `--rev REV`, `--name NAME` (default: the commit's short
hash), and `-j N` (default 4).

Options for `bootstrap`, `lccbuild` and `selfbuild`:
- `--checking LIST`: default `yes`. Use `release` for builds you time.
- `--stage1-flags FLAGS`: lcc's flags for stage 1, default `-O3` (lcc's
  base level, AGENTS.md).
- `--stop-after configure|stage1`.
- `-- CONFIGURE-ARGS`: extra configure arguments, for example
  `-- --with-arch=elbrus-v5`.

Every build is configured as the PoC's cross build is: C, C++ and LTO;
no multilibs, NLS, sanitizers, SSP or ITM; the machine's own binutils.
libquadmath is also off, because the machines' glibc `<bits/fenv.h>`
fails in C23 (`spec/abi/source-compat.md`, "Headers"). The PoC's cross
sysroot patches that header; a native build can't.

## Layout on the machine

`~/relcc2/gcc-native/` holds:
- `src-NAME/`, `build-NAME/`, `inst-NAME/`.
- `log-NAME/`:
  - `settings`;
  - `times`: when each step started, and when each stage's `cc1plus`
    was linked;
  - `status`: `running`, `ok`, or `failed: configure|make|compare|install`;
  - the step logs, and the `.sum` files after `test`.
- `bench/inputs/*.ii`, the preprocessed sources: `combine.cc`,
  `fold-const.cc`, `tree-vect-stmts.cc`, `cp/parser.cc`, the back end's
  `e2k-bundle.cc` and the generated `insn-recog.cc`.
- `bench/results-*.txt`: the median user time, peak memory and output
  checksum per compiler and input, plus totals relative to the first
  compiler.

## Shared-machine rules

These follow AGENTS.md, "Hardware".
- Builds run under `nice -n 19` with `-j 4` (half the 8C2). They last
  hours and take no lock. Don't time other work on the same machine
  while one runs.
- `bench` takes the shared timing lock
  (`/tmp/relcc2-claude-timing.lock`) and pins one CPU (`--cpu`,
  default 1).
- Remove what you no longer need (`clean`). A bootstrap tree with
  `--enable-checking=yes` is about 10–15 GB (not yet measured).

## Cross toolchain and tarballs

`cross.sh` builds an x86_64-hosted cross toolchain, target
`e2k-linux-gnu`. It contains:
- MCST's binutils from `OpenE2K/binutils-gdb`, with gas's CODE_PLACEMENT
  fix on, as by default;
- the PoC GCC of a `toolchains/gcc-claude-native` commit, built against
  lcc's e2k-v3 sysroot (default lcc 1.29.16: Elbrus Linux 9.5.5, glibc
  2.35-29.018).

It installs into one relocatable prefix and packs it as
`toolchains/gcc-native/gcc-e2k-VERSION-x86_64-cross-OS.tar.xz`.
VERSION is the PoC's tag plus the commits on top of it
(`0.4+native-build.7`).

The tarball leaves the sysroot out: MCST's glibc and libraries are
not ours to ship. `setup-sysroot.sh`, at the top of the tarball, links
one in from an installed lcc. It copies the GNU ld scripts and fixes
MCST glibc's `bits/fenv.h`, which breaks `<fenv.h>` under C23.

The smoke test builds a C and two C++ programs, one with a static and
one with the toolchain's shared libstdc++, and runs them on the 4C. It
doesn't use MCST's qemu, which lacks the `cctop*` operations the PoC
emits.

```
tools/gcc-native/cross.sh -j 8                 # about 20 minutes
tools/gcc-native/cross.sh --stage pack         # repack only
```

## Not done yet

- Native `check-gcc` on the bootstrapped compiler.
- `test` and `bench` against a real build.
- Running the suite from a host-side DejaGnu (`tools/dejagnu`) against a
  natively built compiler. That would need a board that compiles on the
  target: not written.
- The 4C with `--prereqs`. Its 512-process limit allows `-j 4`.
