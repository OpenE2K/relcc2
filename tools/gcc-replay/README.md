# Replaying GCC's own build: is the port miscompiling GCC?

gcc-replay checks the E2K port by the largest real program we have: GCC
itself. GCC is built twice from one commit:

- **C**, the x86-hosted cross compiler, is compiled by the workstation's
  x86 GCC;
- **N**, the native compiler (hosted on E2K), is compiled by C, that is,
  by the port.

`record.sh` builds N on x86 and keeps every compile C made while doing
it. `replay.py` then runs each of those compiles again with N, on an
Elbrus machine, and compares N's assembly with C's. The two compilers
are one source with the same options and the same input, so they must
give the same output, as GCC's own bootstrap comparison assumes. Each
difference is one of two things:

1. the port miscompiled some part of N; or
2. GCC's output depends on the compiler that built it, which a
   bootstrap never sees (P108 below).

The evidence then holds only x86 GCC, which is taken as correct, and the
port. lcc compiles nothing here, no emulator runs, and no GCC is built
on the machines: they only run N's `cc1` and `cc1plus` on preprocessed
inputs. A revision costs 30 minutes per CPU on the workstation (12
threads; an hour for two at once), 2 minutes to ship, and under two
hours on each machine: 105 minutes on the 4C at `-j 16`, 109 on the
8C2 at `-j 8`. The one bootstrap that passed on the 8C2 took 7 hours
(`tools/gcc-native`).

The recorded compiles are every `-c` of GCC's own build, with that
build's options. At PoC `0.5+37` there were 2,578:

| Part | Compiles |
|---|---|
| `gcc` (C++, checking enabled) | 777 |
| `gmp`, `mpfr`, `mpc` (in-tree) | 859 |
| `libiberty`, `libbacktrace`, `zlib`, `libcpp`, `libcody`, `c++tools`, `lto-plugin` | 198 |
| target libraries: `libgcc`, `libstdc++`, `libatomic`, `libgomp`, which a canadian cross also builds with C | 744 |

Links, `-E` and configure's probes are not recorded.

### Each CPU checks its own compiler

N must be built for the machine it runs on. Build it with that CPU's
cross, configured `--with-tune` (`record.sh --tune`). The code that
runs in a replay is the port's code for that CPU, so a v3 N run on the
8C2 checks only v3 code. P110 below showed on the 8C2 alone: the v3 N
has the same 64-bit read, but never met a diagnostic upper half.

| Machine | N | Cross (`toolchains/poc-cross/cross-cpu.sh`) | `record.sh` |
|---|---|---|---|
| 4C (`claude@ringo.lan`) | v3, generic | `v3` | no `--tune` |
| 8C2 (`claude@geofront.lan`) | 8C2 (v5) | `8c2` | `--tune elbrus-8c2` |
| 2C3 | 2C3 (v6) | `2c3` | `--tune elbrus-2c3`; not run: a contact's 2-core machine (AGENTS.md) |

## Commands

What the host needs:
- `bwrap`, and `$HOME` under `/home`;
- the cross toolchain of the commit under test;
- gmp, mpfr and mpc sources;
- 18 GB of disk per record.

What each machine needs:
- `python3` 3.8 or later, `tar` and `zstd`;
- 11 GB of disk.

REV, VERSION and NAME stand for the commit, its version (`0.5+37`) and
a name of your choice.

```
# once: gmp, mpfr, mpc (checksums verified) in toolchains/gcc-replay/prereqs
toolchains/gcc-claude/contrib/download_prerequisites \
    --directory=toolchains/gcc-replay/prereqs --no-isl

# the crosses of REV, one per CPU (toolchains/poc-cross/cross-VERSION-CPU)
toolchains/poc-cross/cross-cpu.sh v3 --rev REV -j $(nproc)
toolchains/poc-cross/cross-cpu.sh 8c2 --rev REV -j $(nproc)

# record: build N with C (30 minutes on 12 threads; both can run at once)
R=tools/gcc-replay
X=toolchains/poc-cross/cross-VERSION
$R/record.sh v3 $X-v3/gcc-e2k-VERSION-v3 --rev REV
$R/record.sh 8c2 $X-8c2/gcc-e2k-VERSION-8c2 --rev REV --tune elbrus-8c2

# ship: records, N and replay.py to ~/relcc2/gcc-replay/NAME on the machine
$R/ship.py toolchains/gcc-replay/v3 claude@ringo.lan
$R/ship.py toolchains/gcc-replay/8c2 claude@geofront.lan

# replay: N against C on every compile (all cores; -j 4 if others use the machine)
tools/setup/e2k-ssh claude@ringo.lan 'cd ~/relcc2/gcc-replay && python3 replay.py v3'
tools/setup/e2k-ssh claude@geofront.lan 'cd ~/relcc2/gcc-replay && python3 replay.py 8c2'
```

`record.sh` refuses a cross whose `--version` doesn't name REV. Pass
REV explicitly to both scripts: HEAD moves with every new commit. The
scripts' headers give the remaining options:
- `--machine-dir`, for another task directory than
  `~/relcc2/gcc-replay`;
- `--shard I/N` and `--reverse`, to split one record between machines;
- `--timeout`, 3600 s per compile by default.

`replay.py` writes `NAME/results.txt` on the machine, one line per
compile. Only a run whose lines all read `same` passes. Remove
`~/relcc2/gcc-replay/NAME` when done.

## Reading the results

| Result | Meaning | What to do |
|---|---|---|
| `same` | N's assembly is C's, `.ident` aside (the pkgversion says "native") | nothing |
| `DIFFERENT` | the two outputs differ; N's is kept as `X.n.s` beside `X.s` on the machine | find which half compiled it differently (below) |
| `rc=N` | N failed; 4 is an internal compiler error, usually a signal in N | debug N on the machine (below) |
| `timeout` | slower than `--timeout` | rerun that compile with a longer one |

**A DIFFERENT** is either N miscompiled, or GCC host-dependent. Rerun
the compile with dumps on both sides and find the first pass whose
output differs:
- on the machine, `python3 replay.py NAME --command X.cc1` prints the
  command, to which `-fdump-tree-all` or `-fdump-rtl-all` can be added;
- here, the same command with C's `cc1` or `cc1plus`, inside a `bwrap`
  that mounts `toolchains/gcc-replay/NAME` at its machine directory, as
  `record.sh` does.

P108's signature was identical GIMPLE and RTL from expansion on that
differed only in the order of two insns and their pseudos. That points
at two insn-emitting calls passed as arguments to one call, which C++
leaves in an unspecified order. Fix the source so the outcome no longer
depends on the order (separate statements, in the order N used), and
build a cross with the fix (a build directory's `gcc/`, with its `cc1`
and `cc1plus`, is enough). Then:

```
$R/explain.py toolchains/gcc-replay/NAME FIXED-LIBEXEC claude@HOST
```

It fetches each `X.n.s` and compiles `X` here with the fixed cross. Two
outcomes:
- **explained**: the fixed C gives exactly what N gave. The difference
  was C's own, and N compiled that input correctly.
- **UNEXPLAINED**: N computed something else, which is a miscompile of
  N. Debug it like an `rc=`.

A record of the fixed revision must then replay all `same`.
`abcmp.py BEFORE AFTER STRIDE DIR` compiles every STRIDE-th recorded
input here with two compilers. It shows what a fix costs in wide
instructions, loads and stores, without building anything.

**An rc=** is N failing. `replay.py NAME --command X.cc1` gives the
command; run it under gdb on the machine and look at `p $_siginfo`
(signal, `si_code`, address) and `bt`. SIGILL with
`si_code` 2 (`ILL_ILLOPN`) is a diagnostic value read by a
non-speculative operation (spec/isa/speculation.md). The installed N is
stripped. `NAME/build/gcc/cc1plus` here is not, and it is not PIE, so
gdb's addresses map onto it directly:
`e2k-linux-gnu-addr2line -f -C -e toolchains/gcc-replay/NAME/build/gcc/cc1plus ADDR`,
or `objdump -d` around ADDR. From the function, `-fdump-rtl-all` of its
source file (in `NAME/build`, compiled by C) shows which pass made the
faulting code.

## Pitfalls

Traps met while the method was worked out:

- **configure takes glibc as 0.0** in a canadian cross without a
  sysroot. With build ≠ host it looks for the target's headers in
  `$prefix/e2k-linux-gnu/sys-include`. N then lacks
  `@gnu_unique_object`, which C has, and lowers `-fhardened`'s
  `_FORTIFY_SOURCE`. `record.sh` passes `--with-build-sysroot` and
  prints `TARGET_GLIBC_MAJOR`/`MINOR` at the end: they must be the
  sysroot's (2.35).
- **Paths.** The recorded commands hold absolute paths of the machine
  (`/home/claude/relcc2/gcc-replay/NAME/...`), and the assembly holds
  them in its debug information. `record.sh` builds inside `bwrap` with
  the record mounted at that path, so C wrote the same paths N writes.
  Don't move the directory on the machine.
- **Pin the revision.** A record and the machine's N come from one
  `record.sh` run. A cross of another commit gives differences that
  mean nothing; `record.sh` refuses one.
- **Records reproduce, binaries don't.** Two `record.sh` runs of
  `0.5+37` gave the same records: the commands, the inputs and C's
  assembly were byte-identical, apart from `cc1-checksum` and
  `cc1plus-checksum`, which hash archives that carry timestamps. All
  2,729 objects matched too, but the linked programs did not. MCST ld's
  output depends on ASLR (A37 in `docs/spec-poc-findings.md`). So
  replay N with the records of its own run, as `ship.py` sends them.
- **Not under qemu.** Under MCST's qemu, N's `cc1plus` took about 70
  minutes on one large input (`rx.ii`), and qemu loops for ever on
  stores of never-written registers, which GCC-built code makes
  (AGENTS.md, Gotchas). The owner has ruled out heavy work under
  emulation.
- **Killing a replay.** `pkill -f replay` also matches the remote
  shell that ssh started, whose command line names `replay.py`. Kill by
  PID, or `pkill -u claude -f '^python3 replay.py'` and then
  `pkill -u claude -x cc1plus`.
- **gas tests without `-mcpu`** assemble for v2 and fail in ways that
  mean nothing. Give `-mcpu=elbrus-8c2` (or `-v3`) when trying one
  wide instruction by hand.

## What it found

| Round | PoC | N on | Result | Found |
|---|---|---|---|---|
| 1 | `0.5+32` | v3 on the 4C and 8C2, split | 2,576 same, 2 DIFFERENT | P108: an unspecified argument order made GCC's output depend on its builder (`floating_to_chars.cc`, `options-save.cc`); explained by `5c8611c427a`, `884271b01d0` |
| 2 | `0.5+32` | 8C2's record (C compiling N) | gas refused two wide instructions of `libcpp/lex.cc` | P109: a field descriptor is a literal; `bf94db7bf47` |
| 3 | `0.5+34` | 8C2 on the 8C2 | 2,543 same, 35 `rc=4`, 0 DIFFERENT | P110: SIGILL in `cv_qualified_p`, a post-RA 64-bit read of a 32-bit result; `05f8cbc2657` |
| 4 | `0.5+37` | v3 on the 4C; 8C2 on the 8C2 | 2,578 same on each | nothing: N matches C on every compile, on both machines |

P108, P109 and P110 are in `docs/spec-poc-findings.md` and the PoC's
`STATUS.claude.md` ("Since v0.5"). None was known before. P108 can't
show in a bootstrap, whose stages are all built by GCC.

## Files

| File | Runs on | Does |
|---|---|---|
| `record.sh` | host | builds N with C in bwrap, through `wrap/` |
| `wrap/driver` | host | stands in for C's `e2k-linux-gnu-{gcc,g++,c++}`; keeps `X.i`/`X.ii`, `X.s` and `X.cc1` (directory and compiler-proper command) for every `-c` |
| `ship.py` | host | sends the records, N and `replay.py` (`--list` only lists) |
| `replay.py` | machine | runs every record with N, writes `results.txt`; `--command` for gdb |
| `explain.py` | host | checks that a fixed C gives N's output for each DIFFERENT |
| `abcmp.py` | host | a compiler change's effect on the recorded inputs |
