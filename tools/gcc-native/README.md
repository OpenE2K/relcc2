# Native GCC builds on Elbrus machines

`gcc-native.sh` builds a GCC for E2K **on** an Elbrus machine and
rebuilds it with itself, then runs its testsuite there and times the
built compilers as programs. It drives `remote.sh` over relcc2's SSH
wrapper (`tools/setup/e2k-ssh`). Any GCC git checkout with an
`e2k-*-linux-gnu` port will do.

Status: **draft** (2026-10-13). Checked so far, all on the 8C2:
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
$G fetch                         # logs, .sum files, bench results here
$G compare OLD NEW               # two fetched builds, contrib/compare_tests
$G clean                         # remove build NAME; clean --all: everything
$G xcheck                        # here: lcc compiles all host code (above)
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

## Not done yet

- A full bootstrap; how long each stage takes.
- `test` and `bench` against a real build.
- Running the suite from a host-side DejaGnu (`tools/dejagnu`) against a
  natively built compiler. That would need a board that compiles on the
  target: not written.
- The 4C with `--prereqs`. Its 512-process limit allows `-j 4`.
