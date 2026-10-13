# dejagnu: boards for running GCC's testsuite on E2K

GCC ships no DejaGnu board that runs E2K programs. These boards run the
test programs of GCC's testsuite (or any DejaGnu suite) under MCST's
qemu-e2k user mode or on the Elbrus machines over SSH, and report each
program's output and exit status to DejaGnu. `smoke/` checks that a
board works; `run-gcc-tests.sh` runs GCC's testsuite with them.

| Board | Runs programs | Default lcc (smoke compiler) |
|---|---|---|
| `e2k-qemu` | under MCST's qemu-e2k on the build machine, `-cpu $E2K_CPU` (default `elbrus-v3`) | 1.29.16 for that CPU |
| `e2k-4c` | on the Elbrus 4C (v3, ALT Linux) | `e2k-v3.4c` |
| `e2k-8c2` | on the Elbrus-8C2 (v5, Elbrus Linux) | `e2k-v5.8c2` |
| `e2k-2c3` | on the Elbrus-2C3 (v6, Elbrus Linux); address and port from the environment only | `e2k-v6.2c3` |
| `e2k-ssh` | on any Elbrus machine over SSH (`E2K_SSH_HOST`) | `e2k-v3` |

```
tools/dejagnu/run-smoke.sh e2k-qemu e2k-qemu:elbrus-v5 e2k-4c
                                     # is each board working? (all checks PASS)
tools/dejagnu/run-gcc-tests.sh -b BUILD execute.exp
                                     # GCC's c-torture execute tests under qemu
tools/dejagnu/run-gcc-tests.sh -b BUILD -t e2k-4c -j 4 execute.exp
                                     # the same on the 4C
```

Files: `boards/` (the board files and `e2k-common.tcl`, the procedures
they share), `site.exp` (the global DejaGnu config that adds `boards/`
to the search path), `smoke/` (the smoke suite), `run-smoke.sh`,
`run-gcc-tests.sh`.

## With the GCC port

The port is configured with `--target=e2k-linux-gnu` and lcc's sysroot
(`doc/gcc-port-plan.md`, 4.2). DejaGnu then sees:

- target triplet `e2k-unknown-linux-gnu` (`config.sub e2k-linux-gnu`),
  target alias `e2k-linux-gnu`; `istarget e2k-*-linux*` matches;
- the compiler under test is the build tree's `xgcc` (GCC's own
  `site.exp` in `BUILD/gcc`); the board only runs what it builds.

Point `DEJAGNU` at `tools/dejagnu/site.exp` and name the board:

```
export DEJAGNU=$RELCC2/tools/dejagnu/site.exp
make -C BUILD/gcc -k check-gcc RUNTESTFLAGS="--target_board=e2k-qemu execute.exp"
make -C BUILD/gcc -k -j4 check-gcc RUNTESTFLAGS="--target_board=e2k-4c"
make -C BUILD/gcc -k check-gcc RUNTESTFLAGS="--target_board=e2k-qemu{-O0,-O2} dg.exp=pr1*.c"
```

`run-gcc-tests.sh -b BUILD [-t BOARD] [-l c++] [-j N] [RUNTESTFLAGS...]`
does this and the binutils setup below, and keeps `-j` within each
machine's limit; `-i GCC` tests an installed compiler with the testsuite
of a GCC source tree (`-s`, default `toolchains/gcc`) instead. Results
are GCC's usual `gcc.sum` and `gcc.log` (`BUILD/gcc/testsuite/gcc/`);
compare runs with GCC's `contrib/compare_tests`.

A board variant adds its flags to every compilation:
`'e2k-8c2/-march=elbrus-v5'`, `'e2k-4c{-O0,-O2}'` (two runs). The
boards ask DejaGnu for this (`process_multilib_options`, as its own
baseboards do); before 2026-10-13 they did not, and a variant only ran
the tests again without its flags. Checked with two variants
(`{-march=elbrus-v3,-march=elbrus-v5}`, `execute.exp=20000112*`) on
`e2k-qemu`, `e2k-ssh` and `e2k-8c2`: each compilation has its
variant's flag.

### Binutils in PATH

GCC's testsuite runs the target's binutils by the name
`TARGET_ALIAS-TOOL`, found in `PATH`: `e2k-linux-gnu-objdump` decides
the object format (`lib/file-format.exp`; without it `dg-require-weak`,
`dg-require-alias` and the visibility tests are UNSUPPORTED or
UNRESOLVED), `e2k-linux-gnu-nm`, `-readelf`, `-objcopy` and
`-addr2line` serve other checks. The boards expect MCST's binutils 2.41:
lcc's `binutils/bin` or `tools/setup/build-binutils.sh`'s
`toolchains/binutils/bin`. Both name them `e2k-linux-*`, so
`run-gcc-tests.sh` links `e2k-linux-gnu-*` to them in
`toolchains/dejagnu/bin-e2k-linux-gnu/` and puts that directory first
in `PATH` (`-B BINDIR` chooses the binutils). Running `make check-gcc`
by hand needs the same links in `PATH`. Measured: `weak.exp=weak-1*.c`
gives 34 PASS with the links and 11 UNRESOLVED + 11 UNSUPPORTED
without.

## Settings

Each setting is a Tcl variable (in `site.exp`, or `runtest NAME=value`)
or, failing that, an environment variable of the same name.

| Setting | Boards | Meaning |
|---|---|---|
| `E2K_CPU` | qemu | `-cpu`: `elbrus-v3` (default) … `elbrus-v7` |
| `E2K_LCC` | all | lcc 1.29.16 directory: the smoke suite's compiler, the qemu board's fallback sysroot |
| `E2K_SYSROOT` | qemu | `qemu -L`. Default: the compiler under test's `-print-sysroot`, else `$E2K_LCC/fs` |
| `E2K_QEMU` | qemu | qemu-e2k binary. Default: this checkout's `toolchains/qemu-mcst/bin/qemu-e2k` (`tools/setup/build-qemu.sh mcst`), else `qemu-e2k` in `PATH` |
| `E2K_QEMU_OPTS` | qemu | more qemu options (a Tcl list) |
| `E2K_4C_HOST`, `E2K_4C_PORT` | e2k-4c | default `claude@ringo.lan` |
| `E2K_8C2_HOST`, `E2K_8C2_PORT` | e2k-8c2 | default `claude@geofront.lan` |
| `E2K_2C3_HOST`, `E2K_2C3_PORT` | e2k-2c3 | required; keep them out of the repository |
| `E2K_SSH_HOST`, `E2K_SSH_PORT` | e2k-ssh | required host, optional port |
| `E2K_SSH_KEY` | ssh | identity file, always used with `IdentitiesOnly=yes`. Default `$RELCC2_SSH_KEY`, else `secrets/id_ed25519` of this checkout or of the main checkout (from a git worktree). Without one the board refuses to load: it never lets ssh offer its own keys |
| `E2K_SSH_OPTS` | ssh | more ssh options (a Tcl list) |
| `E2K_REMOTE_DIR` | ssh | work directory on the remote, relative to its home; default `relcc2/dejagnu` |
| `E2K_TIMEOUT` | all | seconds; sets the board's `gcc,timeout` (GCC's limit for compiling and for running a test; GCC's default is 300) |

For qemu the default lcc follows the CPU model MCST's qemu 11.0.1
emulates [qemu: `OpenE2K/qemu-e2k-mcst/target/e2k/cpu.c`; checked by
running lcc's builds]: `elbrus-v3` any v3 code (`e2k-v3`), `elbrus-v4`
the 8C (`e2k-v4.8c`), `elbrus-v5` the 8C2 (`e2k-v5.8c2`), `elbrus-v6`
the **2C3** (`e2k-v6.2c3`; a `v6.16c` build stops with "ELF file for
forward incompatible elbrus-16c is incompatible with your elbrus-2c3
host"), `elbrus-v7` the 8V7 (no lcc 1.29.16 build; `e2k-v3` code runs).
The community qemu's `elbrus-v6` is the 16C instead. lcc's v3 code does
not run on `elbrus-v2` (qemu fails an assertion). For the GCC port the
sysroot comes from the compiler (`-print-sysroot`), so v3 code built
against lcc v3's sysroot runs on every `E2K_CPU` from v3 up.

## How the boards work

The test program runs under `timeout(1)` with stdout and stderr merged
into one pipe (no pty, so no `\r`), followed by a status line the board
removes. The board returns pass (status 0) or fail and the program's
exact output, and logs the status (`e2k: ./t.exe: exit 3`, `signal 6`,
`timeout`). No status wrapper is linked into the programs
(`needs_status_wrapper 0`). At the time limit the program is killed
(TERM, then KILL 10 s later) and the test FAILs with "WARNING: program
timed out." The limit is GCC's `timeout_value`: `gcc,timeout`, scaled by
`dg-timeout-factor`.

- `e2k-qemu` is a local target (`isremote 0`): qemu shares the build
  machine's files, so programs run in the test directory, data files and
  `.gcda` files need no copying, and `dg-set-target-env-var` works (qemu
  passes the environment on). GCC's `ld_library_path` (the build tree's
  `libgcc_s`, `libstdc++`) goes to the guest as `qemu -E
  LD_LIBRARY_PATH=...`. Core files are off (`ulimit -c 0`): qemu would
  write `qemu_PROG_DATE.core` into the current directory for every
  program that dies of a signal. `is_simulator` is 1, so GCC skips the
  gdb-based guality and simulate-thread tests (and gives a few long
  tests less work: the `simulator` effective target).
- The SSH boards copy each program to
  `E2K_REMOTE_DIR/run-HOST-PID/` on the remote (one directory per
  runtest process, so parallel `make -j` runs don't collide), run it
  there with stdin forwarded, delete it, and remove the run directory
  (and `E2K_REMOTE_DIR` if empty) when DejaGnu closes the target. One
  SSH master connection per runtest process serves all calls when
  `XDG_RUNTIME_DIR` is set (control socket there, closed at the end);
  without it every call connects anew (about twice as slow).
  `remote_download`, `remote_upload`, `remote_file` and `remote_exec`
  work in the run directory. GCC's profile and gcov tests expect the
  `.gcda` file at the object's absolute path on the build machine,
  which the remote doesn't have: programs run with
  `GCOV_PREFIX_STRIP=1000`, so libgcov writes `NAME.gcda` into the run
  directory, and `remote_upload` and `remote_file` look for a missing
  absolute path there under its last component.

## Smoke suite

`smoke/smoke.exp` builds small programs with the board's lcc and checks
what the board reports; every check PASSes on a working board. It covers
a passing program, `exit 3`, `abort` (signal 6, and no qemu core file),
a null-pointer store and an integer division by zero (signals 11 and 8
on hardware; on qemu the expected SIGKILL, see below), exact output
(stdout and stderr interleaved, no final newline), arguments and stdin
from a file, 1 MiB of output, the environment (qemu only), a data file
copied to the target and a file the program wrote copied back (also by
its absolute build-machine path, as GCC's gcov tests do), `remote_exec`
status and output (SSH boards), and an endless loop killed at the time
limit with no process left behind.

```
tools/dejagnu/run-smoke.sh [-o OUTDIR] BOARD[:CPU]... [-- RUNTEST-ARGS]
DEJAGNU=$RELCC2/tools/dejagnu/site.exp runtest --tool smoke \
    --srcdir $RELCC2/tools/dejagnu/smoke --target=e2k-linux-gnu \
    --target_board=e2k-qemu          # what run-smoke.sh runs
```

`SMOKE_CC` replaces the compiler, `SMOKE_TIMEOUT` the loop's limit
(default 10 s). Tested 2026-10-12: all 25 checks pass on `e2k-qemu`
with `elbrus-v3` to `elbrus-v7`, all 23 on `e2k-4c`, `e2k-8c2`,
`e2k-2c3` and `e2k-ssh` (the 8C2), also without the SSH master
connection; with an unreachable host the checks FAIL at once (GCC's
execution tests are UNRESOLVED), a missing qemu or 2C3 address stops
runtest with an error.

With GCC's own testsuite (2026-10-12; the compiler under test was the
experimental `toolchains/gcc-claude`, only run): with an installed
compiler (`-i`), `execute.exp=20000[0-6]*` gives the same 500 PASS on
`e2k-qemu` and `e2k-4c` (about 0.4 s a test on the 4C, compiling
included); the `dg.exp` tests that check the harness
(`shouldfail-*.c`, `dg-output`, `dg-output-file`) and
`gcov.exp=gcov-1[0-2]*.c` pass on both; board variants
(`'e2k-4c{-O0,-O2}'`) ran, but without their flags (fixed 2026-10-13,
above). In a build tree (`-b`, `make check-gcc`)
`execute.exp=2000[01]*` and the gcov tests ran on the 4C with `-j 4`
(four runtest processes, 1,286 PASS; the only FAILs were a test the
compiler can't build). The remote run directories and control sockets
are gone afterwards.

## Known limits

- **MCST's qemu delivers no hardware exception as a signal.** A memory
  fault (a store to address 0 or 0x1000, or to a read-only page) and an
  integer division by zero stop qemu with "exception at IP = ..." and
  "exception handling is unimplemented - aborting", and it kills itself
  with SIGKILL (logged as `signal 9 (qemu stopped on a hardware
  exception)`). Tests that catch SIGSEGV or SIGFPE (and presumably the
  other hardware traps) FAIL under qemu; run them on hardware, which
  gives signals 11 and 8. Signals sent by software work (`raise`, and the
  time limit's SIGTERM). [qemu, hw: `smoke/segv.c`, `smoke/divzero.c`.]
  The community qemu does deliver SIGSEGV but returns from a division by
  zero with no signal; use MCST's (AGENTS.md: the reference).
- The other differences between qemu and the hardware (AGENTS.md,
  "Gotchas") apply to test results too: qemu restarts array prefetch
  after a signal handler, can't check speculation, applies `%pfpfr` to
  80-bit operations. Confirm a FAIL that looks like a port bug on
  hardware, and build the test with lcc too: some fail because of MCST's
  libm (`doc/gcc-port-plan.md`, section 9).
- **SSH boards**: the program runs with the remote system's
  `libgcc_s.so.1` and `libstdc++.so.6` (MCST's), not the build tree's,
  unless linked statically (`--target_board='e2k-4c/-static-libgcc'`,
  `-static-libstdc++` for C++). GCC marks `dg-set-target-env-var` tests
  UNSUPPORTED on remote targets. Not tried: guality with the remote's
  gdb.
- **The machines are shared** (AGENTS.md, "Hardware"): `make -j4` at
  most on the 4C and 8C2, `-j2` on the 2C3 (two cores);
  `run-gcc-tests.sh` enforces it. Each run works only in its own
  `~/relcc2/dejagnu/run-HOST-PID`. A run that is interrupted leaves that
  directory behind; remove only your own, e.g. `tools/setup/e2k-ssh
  claude@ringo.lan rm -rf relcc2/dejagnu/run-HOST-PID`.
- The 2C3 belongs to a contact: give its address and port in
  `E2K_2C3_HOST` and `E2K_2C3_PORT`, never in a file under version
  control.
- `remote_exec target` on the qemu board runs the command on the build
  machine (DejaGnu does that for every local target); GCC runs test
  programs only through `remote_load`, which goes through qemu.
- 32-bit and protected mode are not set up: ALT Linux ships no runtime
  for them, and only the community qemu has a 32-bit emulator
  (`qemu-e2k32`).
