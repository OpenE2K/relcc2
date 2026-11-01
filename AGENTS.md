# relcc2: agent guide

This file is the shared context for AI agents working on this repository.
Read it fully before starting work.

## Goal

A free, copyleft (GPL) **GCC backend for the Elbrus (E2K) architecture**,
built on a written, backend-neutral specification of the ISA and ABI with
machine-readable data and tools that check it against reference toolchains.

MCST already publishes GPL ports of binutils, glibc, gdb, Linux and the GCC
runtime libraries for E2K; the compiler is the missing piece. Nobody has
published a GCC port (MCST had a gccgo port but released no source). Rust is
already covered by Unipro's LLVM, which MCST and Unipro develop together.

relcc2 is a research project: "RE" stands for research and "L" for Elbrus,
as in MCST's lcc. The original relcc (2020) stood for "reverse-engineered
lcc": the owner's earlier attempt to decompile lcc, which stalled on the
codebase's size. relcc2 does not decompile anything (see decision 4).

## Decisions (do not revisit without the owner)

1. **GCC, not LLVM, is the primary backend.** Reason: copyleft. LLVM upstream
   requires Apache-2.0, which would let MCST or Unipro absorb the work into
   proprietary compilers. LLVM is at most a later, secondary option.
2. **The owner writes all GCC code by hand.** GCC does not accept
   AI-generated contributions. Agents write the spec, generated data, tests
   and tools in this repository, and may review, explain and test, but must
   **not write code intended for GCC itself** (no `gcc/config/e2k` patches,
   `.md` files or target hooks). Produce material the owner codes against.

   **Exception: the proof-of-concept backend in `toolchains/gcc-claude`.**
   The owner asked an agent to implement an experimental GCC backend from
   the spec, to find errors and missing data in `spec/` and the port
   plan. In that tree only, agents may write GCC code. It is a proof of
   concept: never submitted upstream and not the owner's port, which is
   still written by hand (how strictly to keep the two apart is the
   owner's call). Its main product is findings about the spec: record
   each in `docs/spec-poc-findings.md`, then fix it in `spec/` or `doc/`
   with the usual source marks, so the owner's port
   benefits through the spec, not through the experiment's code. Nothing
   from `toolchains/gcc-claude` is copied into this repository.
3. **Licensing:** `spec/` is CC BY-SA 4.0 (it adapts MCST's CC BY 4.0
   programming guide; attribution in `spec/NOTICE.md`). Everything else is
   GPL-3.0-or-later; every tool file starts with an SPDX header. Generated
   YAML carries a `CC-BY-SA-4.0` header written by its generator.
4. **Clean sources.** Facts come from openly licensed material. Proprietary
   compilers (lcc, Unipro's LLVM, MCST's lccrt-based LLVM 13) are used only as
   **black-box oracles**: compile probes, read their output, compare. Never
   decompile them into this repository or transliterate their code.
   **Leaked or unpublished MCST material is never used or added**, even if
   GPL-licensed.
5. **lcc is the ABI reference.** It built the system. Where lcc and Unipro
   disagree, follow lcc and note the difference.
6. **Assembler syntax** is that of MCST's GNU binutils port (gas). MCST no
   longer ships a separate assembler; don't call it "las syntax".
7. **The port does what lcc does** wherever there is a choice (code and
   assembly conventions: `setwd dbl` chosen per function, `.ignore
   strict_delay` with compiler-inserted delays, ...). Specs and plans
   should describe lcc's way first.

## Sources

| Source | Where | Use |
|---|---|---|
| MCST programming guide (CC BY 4.0) | http://ataraxia.fwgs.gay/repo/pub/docs/elbrus_prog/html (chapter1..11.html) | Primary spec source. Ch. 9 ABI, ch. 10 ISA, ch. 11 protected mode, ch. 6 optimisation |
| MCST binutils 2.41 | submodule `OpenE2K/binutils-gdb` | Opcode tables, encodings, relocations, gas syntax |
| MCST gdb 15.2 | submodule `OpenE2K/gdb` | DWARF numbering, return values, frames |
| MCST glibc 2.35 | submodule `OpenE2K/glibc` | setjmp, contexts, TLS, ld.so, backtrace |
| MCST Linux 6.1 | submodule `OpenE2K/linux` | Register layouts (WD, BR, CR0/CR1), hardware stack syscalls |
| MCST QEMU e2k 11.0.1 (release 1.2) | submodule `OpenE2K/qemu-e2k-mcst` | Instruction semantics: **the reference** where the emulators disagree. Runs test programs (user mode) |
| Community QEMU e2k 9.2.50 | submodule `OpenE2K/qemu-e2k-community` | Instruction semantics MCST's emulator lacks (the owner co-developed it; development has stalled). Also has `qemu-e2k32` |
| MCST E2K GCC 11 runtime libs (GPL) | `internal-lcc-libs-open-source-1.29.16.tar.xz` in https://setwd.ws/sp/1.29/1.29.16/src/ (`tools/setup/fetch-lcc-libs.sh` unpacks it to `toolchains/src/lcc-1.29.16/`, not tracked) | libgcc (E2K unwinder `config/e2k/unwind-dw2-e2k.c`), libstdc++, libatomic, libquadmath. The GCC port will use the unwinder and libgcc E2K files (owner's decision) |
| MCST libffi 3.3 (MIT) | `libffi-3.3-29.018.tar.xz`, same place and script (`toolchains/src/libffi-3.3-29.018/src/e2k/`) | An independent hand-written implementation of the calling convention (argument slots, results, varargs, closures): cross-check for `spec/abi/calling-convention.md` |
| MCST gcov 4.7.3 | `gcov-4.7.3-29.018.tar.xz`, same place and script | GCC 4.7's gcov tool without E2K changes; only says which coverage format lcc uses |
| lcc cross compilers | https://setwd.ws/sp (owner's archive; 1.19..1.31, stable 1.29.16, preview 1.31.05) | ABI oracle; includes a full sysroot. 1.31.05 (`tools/setup/fetch-lcc.sh 1.31.05 e2k-v3`) is a second lcc oracle |
| lcc's user manual | `doc/lcc/` in every lcc tree (e.g. `/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/doc/lcc`; 1.31.05 has changelogs since 1.24) | MCST publishes it with the compilers (free downloads at dev.mcst.ru/download; setwd.ws is the owner's archive), so it is a usable source (owner's decision); mark **lcc-doc**, quote facts rather than text, and check them with probes where possible. Changelogs and notes gave the atomics, inline-asm, macro and per-generation facts |
| Unipro LLVM 20.1.8 | x86_64 debs in `x86_64/` (untracked) | Second oracle; covers v3..v7 |
| MCST's LLVM 13 port (lccrt) | https://github.com/OpenE2K/llvm-lccrt/releases/tag/v8.2 (x86 host binaries from OS Elbrus 8.2) | Its `liblccopt` plugin is lcc's code generator with full debug info. Planned black-box source for timing tables (run under gdb, dump data); never decompile into the repo |
| Opcode spreadsheet | https://docs.google.com/spreadsheets/d/1p8K5DWBsXTpCDk3TdFwMaTFIadAFuxaH4zfnJXUxK94 | Generated from older binutils that still had the table generator. **Not independent of binutils** |

Hardware: the owner has an Elbrus 4C (v3) and 8CB (v5); a contact lends an
Elbrus-2C3 (v6). v7 is in development. Timings are the same for v1..v5 and
differ from v6 (measured: timing.md, "Elbrus-2C3 (v6)").

- **Connect only with relcc2's key.** Use `tools/setup/e2k-ssh` and
  `tools/setup/e2k-scp` (ssh/scp with `-i <key> -o IdentitiesOnly=yes
  -o BatchMode=yes`; they find the key from any directory and from git
  worktrees, or take `RELCC2_SSH_KEY`, and refuse to run without it).
  A plain `ssh claude@...` offers the owner's personal keys, which the
  machines log as failed logins; so does an `-i` whose path doesn't
  resolve (a relative `secrets/...` outside the repository root, a
  worktree without `secrets/`) or options kept in a shell variable
  (the Bash tool is zsh: `ssh $opts` passes one word). The Python tools
  (`sshkey.py`), the `run.sh` probes and the DejaGnu boards all refuse
  to connect without the key.
- **4C (v3)**: `tools/setup/e2k-ssh claude@ringo.lan` (key in untracked
  `secrets/id_ed25519`, never commit or copy it).
  4× E2S, 16 cores, 61 GiB DDR3 (two memory channels per processor
  instead of three), ALT Linux (kernel 6.1.158-alt1.9.1: MCST's older
  6.1-1.9 tree; the 8C2 and 2C3 run 6.1-1.17 and 6.1-1.18), native lcc
  1.29.16, gdb, perf.
  It is a shared machine, not a container: keep parallelism low (about
  `-j4`) and don't fill `/tmp` (tmpfs). **Several agents use the account
  at once**: work only in your own subdirectory `~/relcc2/<task>` (check
  that it isn't taken), and when done remove only that subdirectory.
  Never remove `~/relcc2` itself or other agents' subdirectories; the
  account's dotfiles stay. `bench.py --remote-dir` and `run.py
  --remote-dir` choose the subdirectory.
- **8C2 (v5)**: `tools/setup/e2k-ssh claude@geofront.lan` (the same
  key). Elbrus-8C2 (E8C2, cpu family 5),
  8 cores, one socket, 1500 MHz (`performance` governor since
  2026-10-04; `%clkr` counts 1500 MHz), 125 GiB
  DDR4 (all four memory channels populated), Elbrus Linux 9.5.5 (OS Elbrus, kernel 6.1), native lcc 1.29.16
  (`e2k-8c2-linux`), gdb, perf (`perf_event_paranoid` 2), numactl.
  Same rules as the 4C: work only in `~/relcc2/<task>`, remove only
  that, keep runs short. Build code for it with the 8C2-tuned
  cross-compiler `/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1` (not v3
  code run forward-compatibly), and for the 4C with `e2k-v3.4c` when
  timing matters (`e2k-v3` is generic, only forward compatible); the
  directory names say which lcc builds are tuned for a CPU. Its system
  libatomic is built for the 8C2 (16-byte atomics in hardware).
- **2C3 (v6)**: a contact's machine on the internet; the owner gives the
  address and port (not recorded here), the same key. Elbrus-2C3
  (E2C3, cpu family 6), 2 cores, 2000 MHz (`performance`), 15 GiB,
  Elbrus Linux 9.5.5, native lcc 1.29.16 (`e2k-2c3-linux`), gdb, perf.
  Same rules as the others (`~/relcc2/<task>`, short runs, remove only
  that). Build with `/opt/mcst/lcc-1.29.16.e2k-v6.2c3.linux-6.1`
  (`v5.8c2` binaries are 8C2-only: "Exec format error"; to ld.so MCST's
  qemu `-cpu elbrus-v6` is a 2C3, so `v6.2c3` builds run there and
  `v6.16c` ones are refused; the community qemu's v6 is the 16C). `bench.py`, `run.py`,
  `profile.py` and `validate.py` take `--ssh-port`; give `validate.py`
  `--cpu 0` (no core 2).
- Timing runs on any of the machines go under their shared lock:
  `flock -w 3600 /tmp/relcc2-claude-timing.lock <command>` on the
  Elbrus machine. Native GCC builds (`tools/gcc-native`) and replays
  (`tools/gcc-replay`) take no lock, so time nothing else on a machine
  while one runs.
- Nearly every Elbrus system ships MCST's lcc as the default compiler (OS
  Elbrus and ALT Linux), so tests may assume lcc on the target.
- The port targets 64-bit unprotected mode only. 32-bit (`-mptr32`) and
  protected (`-mptr128`, `e2k128`) modes are documented for completeness
  (spec/abi/mode32.md, protected-mode.md); Elbrus Linux ships their
  runtimes (`/lib32`, `/lib128`), ALT Linux doesn't (the 4C runs them
  static or with lcc's sysroot libraries).

## Repository layout

```
README.md, COPYING   overview; GPL-3.0-or-later for everything outside spec/
spec/          CC BY-SA 4.0 (spec/NOTICE.md). Every ABI rule carries its source
               mark, every ISA section its status ([checked], [hw], [source]...)
spec/isa/      ISA. README.md: the YAML data and how it is generated and checked
  bundle.md            wide-instruction format, syllables, encodings, gas syntax
  control-transfer.md  branches, calls, CTPRs, fetch costs
  semantics.md         what operations compute (checked against models)
  timing.md            latencies, hazards, throughput, memory; data in
                       timing-e2s.yaml (4C), -e8c2.yaml, -e2c3.yaml
  predicates.md, loops.md (loop mode, %lsr, rotation), apb.md (array
  prefetch), speculation.md (speculation, diagnostic values, DAM, MAS),
  monitors.md (perf counters), x86-support.md (the x86 translator's
  facilities), hwbugs.md (MCST's errata; our low-confidence anomalies)
  operations-v{3,5,6,7}.md  every operation, on the page of the version
                       that added it (v5 = v4 and v5): GENERATED from the
                       hand-written operations-v*.yaml; edit the YAML
  binutils-ops.yaml, registers.yaml, reservations.yaml, lcc-usage.yaml:
                       generated by tools/isa-extract; never hand-edit
spec/abi/      e2k64 ABI. README.md: the file table and the source marks.
               data-layout, registers (+ DWARF), calling-convention (+ C++
               ABI), stack-and-unwinding, elf, atomics, source-compat,
               kernel-interface, kernel-build (what building MCST's Linux
               asks of a compiler); mode32 and protected-mode (for
               completeness only); lcc-builtins.md + lcc-builtins*.yaml
doc/           gcc-port-plan.md: the plan the owner codes the port against
docs/          untracked. spec-poc-findings.md (the PoC agent's list),
               poc-benchmarks.md and poc-roadmap.md (the PoC),
               mcst-coordination.md (owner's: things to raise with MCST)
tools/setup/   install reference toolchains into toolchains/; e2k-ssh,
               e2k-scp, sshkey.py: the only way to reach the machines
tools/isa-extract/  spec/isa YAML from binutils and its checks: extract_*
               (binutils-ops, registers, lcc-builtins.yaml), check_encodings
               and check_literals (against gas), gen_operations
               (operations-v*.md), gen_/check_reservations, count_lcc_ops
               (lcc-usage.yaml), count_lcc_preds and list_pred_ops
               (predicates.md), compare_sheet; docstrings say what each does
tools/isa-decode/   decoder written from spec/isa only (e2kdis.py), corpus,
               compare_objdump.py; hand-written corpus .s files declare
               their ISA range with "isets: N-M" in their first three lines
tools/isa-timing/   bench.py: latency, throughput, hazard and control
               kernels on hardware over SSH (its docstring lists the kinds)
tools/isa-score/    static schedule scoring, profile.py (perf on hardware),
               validate/ (model against hardware); README.md
tools/isa-probes/   ISA probes, run on hardware (below)
tools/abi/probes/   ABI probes: read with -S, run on hardware, or asking lcc
tools/abi/compat/   ABI compatibility suite, the port's acceptance test:
               gen.py, run.py (qemu or --remote), known.txt (expected
               differences); spec/abi/README.md
tools/abi/builtins/ lcc-compatible builtins test; spec/abi/lcc-builtins.md
tools/gcc-native/   native builds and bootstraps of the PoC on the 8C2
               (draft); README.md
tools/gcc-replay/   the PoC checked on GCC itself: the native GCC its cross
               builds replays that build's compiles on the machines;
               README.md
tools/quadmath/     the PoC's libquadmath on the machines, call by call,
               against x86's build of the same source and lcc's; README.md
tools/distro/       a small system built by the PoC alone (MCST's Linux and
               glibc, binutils, gdb, GCC, busybox) as a disk image the 8C2
               boots over AoE; README.md
tools/dejagnu/      DejaGnu boards for GCC's testsuite (qemu, 4C, 8C2, 2C3,
               any SSH host) and run-gcc-tests.sh; README.md
OpenE2K/       submodules (see Sources)
toolchains/    untracked: installed toolchains, builds, all scratch output.
               gcc/ (GCC mainline) and gcc-tilegx/ (last commit with
               TILE-Gx) as references; gcc-claude/ (the PoC, decision 2);
               gcc-claude-native/ (its worktree on branch native-build for
               tools/gcc-native, so native builds never touch the PoC
               agent's checkout)
x86_64/        untracked: Unipro debs
secrets/       untracked: relcc2's SSH key; never commit or copy it
```

### Probes

A probe settles one question by compiling or running a small program.
To find the probe behind a fact, or the facts behind a probe:

- Spec sections name their probe in the status mark or the text
  (`[hw: tools/abi/probes/glibcbuild_cancel.c; 4C, 8C2, 2C3]`):
  `grep -rn NAME spec doc` goes from a probe to what it settled.
- operations-v*.md's "Run on hardware" column lists, per operation, the
  probes whose assembly executes it (from `PROBES` in
  `gen_operations.py`, which also records the machines).
- mode32.md and protected-mode.md have tables of their probes.
- Each probe's header comment gives the build line (which lcc
  directory), where it runs, how to read the output and its spec
  section. Read it before running anything.

Conventions:

- `X.c` is the driver, `X.s` the kernels; `gen_X.py` writes `X.s` and
  `X-cases.h` when they are generated. `-v4`/`-v5`/`-v6` files add cases
  for later CPUs, built in with the define the header names
  (`-DSPEC_V5`, `-DTL_V6`, ...).
- The multi-MB generated files are not in git: run their generators
  first (Conventions).
- A new `.s` in tools/isa-probes needs an entry in `gen_operations.py`'s
  `PROBES`, or the generator stops.
- `check_*` files run on the build host, not the Elbrus: reference
  models to compare with the hardware's output (the `.c` ones need x86
  SSE or x87).
- Timing runs take the machines' shared lock:
  `flock -w 3600 /tmp/relcc2-claude-timing.lock ./probe` on the Elbrus
  machine (`bench.py --wrap` takes the same prefix).
- Name prefixes such as `pocchk`, `pocabi`, `fold`, `callsstack_`,
  `memorycxx_`, `glibcbuild_` and `*left` name the task that wrote the
  probe (PoC findings rechecked before folding, audit leftovers), not its
  subject: go by the header.

tools/isa-probes, by family:

| Family | Probes |
|---|---|
| Operation semantics against models | `semantics`, `semp`, `semx`, `semfx`, `semc`, `sem5`, `sem6`, `ctl6`, `getf`, `divx`, `sqrt`, `fcomb`, `icomb`, `tagx`, `sregs`, `stmqp`, `v2ops`, `ctlops` |
| FP environment | `fpenv`, `fenvlib`, `fpcost`, `specfp` |
| Speculation, DAM, MAS | `spec`, `damwb`, `damlist`, `p28`, `mas_be`, `mas_hint`, `mas_mod`, `mas6` |
| Loops, predicates, rotation | `loops`, `lsrw`, `preds`, `rndpred`, `abg` |
| Array prefetch | `apb`, `apb-loop`, `apb-lcc`, `apb-stream`, `apbst`, `apbst-lcc`, `bapent` |
| Memory and timing | `memx`, `samewi`, `stld`, `stldv`, `stbase`, `pf`, `pf-lcc`, `callsstack_pcall`, `memorycxx_stld`, `memorycxx_dam` |
| Monitors | `pmu`, `pmu2`; `pmu.h` counts events inside any probe |
| x86 translator facilities | `rp`, `native` |
| Leftovers and PoC rechecks | `*left`, `timeleftv`, `aaurwct`, `pocchk*`, `foldisa`, `foldp93` |

tools/abi/probes, by how they are used:

- **Routine**: `layout.c` must compile (a check); `callconv.c` is the
  calling-convention probe, read with `-S`.
- **Read with `-S`** (or `-c` and nm): `atomics.c`, `asmops.c`,
  `stackprot.c`, `sanitize.c`, `ctxtls.c`, `pocplan.c`,
  `memorycxx_abi.cc`, `pocabi*_{atom16,ord,nfx}.c`, `folda33_cpuis.c`,
  `folda35_vec.c`, `folda36_parmalign.c`, `foldp100_blkasm.c`,
  `foldp102_inline_cs.c`, `foldp104_regimage.c`, `foldp106_coldshift.c`
  (`foldp105_asmaddr.c` through its `.sh`).
- **Run on hardware**: the other `.c`/`.cc` files (many with a `.s`;
  `foldp98_split.c` also with the host's GCC and gdb), and `dso/`,
  `elfops/`, `mode32/` and `protected/` through their `run.sh`.
- **Link or assemble on the host**: `execstack.sh`, `prefrel.sh`,
  `debuginfo_loc.sh`, `folddebug_reloc.s`, `gotdata.sh`,
  `foldp105_asmaddr.sh`, `foldp107_gotbase.sh`.
- **Ask lcc, compile only**: `asmconv.py`, `prefetch.py`,
  `kernelbuild.py`, `glibcbuild_lcc.py`, `foldp101_pragma_runs.py`,
  `foldp103_nosemispec.py`; `find_builtins.py`,
  `gen_builtins.py` and `gen_builtins_misc.py` write
  `spec/abi/lcc-builtins-*.yaml`.

## Setup

```
git submodule update --init
tools/setup/setup-unipro.sh          # toolchains/unipro/bin/{clang,llc,...}
tools/setup/build-binutils.sh        # toolchains/binutils/bin/e2k-linux-*
tools/setup/fetch-lcc.sh             # toolchains/lcc-1.29.16-e2k-v5/{lcc,l++,ldis}
tools/setup/build-qemu.sh            # toolchains/qemu-{mcst,community}/bin/qemu-e2k
tools/setup/fetch-lcc-libs.sh        # toolchains/src/lcc-1.29.16/ (MCST libgcc etc.)
tools/setup/build-gcc-docs.sh        # toolchains/gcc-docs/{info,html}: GCC manuals
```

- `build-qemu.sh [mcst|community]` builds Linux user mode only. Run
  programs with `qemu-e2k -cpu elbrus-vN -L <lcc>/opt/mcst/lcc-*/fs ./prog`
  (`-L` is lcc's sysroot, needed for dynamic binaries).
- Neither emulator can be fully trusted. MCST's is the reference where
  they disagree; confirm anything important on hardware.
- Never install well-known host tools (ninja, meson, compilers, libraries)
  yourself, e.g. from PyPI. Ask the owner to install them on the host.

- lcc expects to live in `/opt/mcst/...`. On the owner's workstation the
  cross compilers are installed there globally, one per target:
  `/opt/mcst/lcc-1.29.16.e2k-<target>.linux-6.1/bin/{lcc,l++,ldis}` with
  targets `v3`, `v3.4c`, `v4.1c+`, `v4.8c`, `v5.8c2`, `v6.16c`, `v6.2c3`.
  Choose the target by directory, not with `-march`/`-mtune`. `v3` code runs
  on all later CPUs; `v3.4c` code runs only on the 4C. They run directly
  and link complete programs against their bundled sysroot.
- `fetch-lcc.sh` is only for machines without `/opt/mcst`: it unpacks lcc
  into `toolchains/` and its wrapper runs it under `bwrap` with the tree
  mounted at `/opt/mcst`. No root needed.
- Unipro's debs target Debian; they work on the owner's Gentoo system.
  `setup-unipro.sh` unpacks them with `ar` and `tar` when `dpkg-deb` is
  missing.
- Unipro's tools link against `libxml2.so.2` (only for Windows manifests).
  Hosts with libxml2 2.14+ have only `libxml2.so.16`, so the setup script
  builds a stub in `toolchains/unipro/stub`.
  Its clang needs `-march=elbrus-vN` (not `-mcpu`) and warns that `lcc` is
  missing (it uses lcc as the driver for linking); use `-S`/`-c`.
- Use `toolchains/` for all scratch output, never `/tmp`.

## Checks (all must stay green)

```
python3 tools/isa-extract/extract_binutils.py     # regenerate binutils-ops.yaml
python3 tools/isa-extract/extract_registers.py    # regenerate registers.yaml
python3 tools/isa-extract/check_encodings.py      # 27,268 cases on v2..v7: all ok
python3 tools/isa-extract/gen_operations.py --check
                                                  # every operation has one rule
tools/isa-decode/build_corpus.sh toolchains/corpus
python3 tools/isa-decode/compare_objdump.py toolchains/corpus/*.o
                                                  # 6,890 match, 45 known objdump bug
                                                  # (21 checked with gas)
/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc -c tools/abi/probes/layout.c -o /dev/null
                                                  # or toolchains/lcc-1.29.16-e2k-v5/lcc
python3 tools/abi/compat/gen.py toolchains/compat/gen
python3 tools/abi/compat/run.py toolchains/compat/gen
                                                  # qemu-e2k, v5: 1,931 tests per pair
                                                  # (4 pairs with Unipro), ~10 s:
                                                  # 0 unexpected (lcc->lcc all pass)
python3 tools/abi/compat/run.py toolchains/compat/gen --target v3 \
    --remote claude@ringo.lan -o toolchains/compat/ringo-v3
                                                  # on the 4C, ~30 s: 0 unexpected
                                                  # (also on the 2C3: --target
                                                  # v6.2c3 --ssh-port ...)
python3 tools/abi/compat/run.py toolchains/compat/gen --target v3,v5.8c2
                                                  # mixed targets, qemu as v5:
                                                  # 10 pairs, ~45 s, 0 unexpected
                                                  # (on the 8C2 with --remote;
                                                  # v3,v6.2c3 on the 2C3)
python3 tools/isa-extract/gen_reservations.py --check
                                                  # reservations.yaml up to date
python3 tools/isa-extract/check_reservations.py --selftest
python3 tools/isa-extract/check_reservations.py   # corpus, CoreMark, libc
                                                  # (v3, v5, v6): 0 violations, ~30 s
tools/dejagnu/run-smoke.sh e2k-qemu e2k-qemu:elbrus-v5 e2k-4c
                                                  # every board check PASS, ~1 min
python3 tools/isa-extract/compare_sheet.py <csv>  # 14 documented disagreements
python3 tools/isa-score/selftest.py               # stall rule: ok, latency rules: ok
python3 tools/isa-score/validate/validate.py --remote claude@ringo.lan \
    --remote-dir relcc2/<task>                    # isa-score vs the 4C,
                                                  # ~2 min: within 1% (k_empty aside;
                                                  # the 8C2: --target v5.8c2
                                                  # --period 30011, within 2%)
```

## Current state

### ISA (spec/isa): largely complete for code generation

Measured on the 4C (v3), 8C2 (v5) and 2C3 (v6). v4 is not measured;
what the spec says about v7 (no silicon) comes from binutils, the
kernel and the community qemu. Sections carry source marks ([hw], [checked], [source], ...);
most files end with "Emulators" and "Not checked yet" (the open work).

| Topic | Where |
|---|---|
| Every operation (versions, channels, encoding); register numbers | `binutils-ops.yaml`, `registers.yaml` (generated; `README.md` covers the fields and checks) |
| Wide instruction format, bundling rules, instruction size, objdump bugs | `bundle.md` (`vfbg`, `puttsd`, `%bgrpred`, v7 `dtbp`, ctcond 11 and `staa`/`ldaa` decoding are still [source]) |
| Latencies, crossings, stalls, hazards, throughput, memory | `timing.md`; per-machine data in `timing-e2s.yaml`, `-e8c2`, `-e2c3` |
| Exact results, FP environment, tags, v5 and v6 operations | `semantics.md` |
| Branches, calls, CTPRs, instruction fetch | `control-transfer.md` |
| Speculation, diagnostic values, DAM, MAS, recovery points | `speculation.md` |
| Loop mode, `%lsr`, rotation, `abg`/`%bgr`; array prefetch | `loops.md` ("For a compiler"); `apb.md` |
| Predicates | `predicates.md` ("The short version") |
| Performance monitors through perf | `monitors.md` |
| The x86 translator's facilities usable by native code | `x86-support.md` |
| Per-version resources for a scheduler | `reservations.yaml` (generated) |

Facts that change how code is written or probed:

- **Interlocks** (timing.md, "Hazards"). The hardware interlocks
  everything user code touches, so code without delays is correct,
  only slow. The exceptions are a `%lsr`/`%lsr1` write, which reads and
  the loop's `alc` see only 4 cycles later (`%ilcr` reads 5), and, on v3
  and v5, a compare or predicate logic followed by a predicated op in a
  taken return's instruction (keep them 2 instructions apart).
- **Timing** (timing.md). On v3 and v5: integer 1, FP and multiply 4,
  L1 load 3 (v6: 5), `disp` → `ct` 5. Crossing between ALC0–2 and
  ALC3–5 costs +1 for integer and load results and +2 for FP ones.
  FP-side results, multiplies and divides included, reach integer ops 2
  cycles later. A late operand stalls in 4-cycle steps. An FP-side
  result going to an integer op is a cycle late when an integer op or
  load issues in its channel 2 cycles after it (a write-back conflict,
  decided on the schedule as written; `nop`s count). Under `dbl = 0`, a
  64-bit read of a
  32-bit result waits 4. Every channel issues every cycle except the
  shared ALC5 divider. A callee whose `setwd` grows the window waits for
  all outstanding loads. There is no hardware data prefetcher, and a
  MAS 0x60 prefetch fetches nothing.
- **Control transfer** (control-transfer.md). A prepared `ct`, `call` or
  return issued in time is free; a taken `ibranch` costs 6. Every call
  and return empties `%ctpr1`–`3`, and a taken `ct` through an empty
  CTPR traps. Don't prepare and transfer through one CTPR in one
  instruction: gas refuses it before v6, and v6 falls through.
- **Results** (semantics.md) where the guide is wrong or names mislead:
  - `sxt` 4–6 zero-extend.
  - `merge` gives the first operand when the predicate is false.
  - `movep c, v` writes v when c (the guide swaps the operands).
  - Shift counts are modulo the width.
  - FP `lt`/`le` signal on quiet NaNs.
  - `fmax`/`fmin` are x86's: a NaN or two zeros give b.
  - Combined `X_Y a, b, c` is `Y(c, X(a, b))` rounded twice, so `_sub`
    is c − (a X b). v6 `fma*` is fused.
  - Ordinary FP is SSE/MXCSR bit for bit. 80-bit `fx*` is x87 under
    `%fpcr`/`%fpsr`, not `%pfpfr`.
  - MAS bit 3 byte-swaps aligned accesses for free (misaligned: SIGBUS).
- **Speculation** (speculation.md). A faulting `,sm` operation gives a
  diagnostic value, which traps (SIGILL `ILL_ILLOPN`) only at a
  non-speculative use. So does a speculative FP op that would raise a
  flag not yet set (processes start with every flag set; the
  consequences are under Gotchas). A 32-bit half with tag 1 is diagnostic only with
  bit 30 set. Only the fill MAS (lcc's spill reload) reads memory tags
  back.
- **Loops and APB** (loops.md, apb.md). A callee's loop changes `%lsr`,
  so save it around calls. On v3–v5, array prefetch is correct only in
  a `loop_mode` loop counted by `%lsr` that advances every area once
  per iteration. A call, return or `%ctpr2` write stops it (the next
  `mova` is SIGILL). The buffer doesn't see stores, unless on v3–v5
  1 ≤ `%lsr.ldovl` ≤ the store's distance in iterations (lcc does this;
  the 2C3 follows no such rule). v6 keeps positions across traps
  in any loop.
- **Predicates** (predicates.md). One wide instruction takes at most 4
  compares (ALC0, 1, 3, 4), 4 ELPs, 3 CLP/MLPs and 6 RLP/MRGC entries.
  Predicates are callee-saved for free. An op under a false predicate
  does nothing (no access, fault or wait), except a `movtd` into a
  CTPR, which is SIGILL under any predicate. Readers in the writer's own
  instruction see the old value. Of two writers, the higher channel
  wins, and lcc relies on that.
- **v5 and v6 against v3** (timing.md, "Elbrus-8C2" and "Elbrus-2C3";
  the v6 sections of semantics.md, control-transfer.md and apb.md).
  v5 keeps v3's latencies and rules (except `pfdivd`/`pfsqrttd`, 16, and
  FP results into an 80-bit op's narrow operand, +2), issues 6 FP
  operations a cycle (v4 added ALC2/5; v3: 4), adds 128-bit `qp*` ops, serves `ldqp` from L2 (11 cycles), traps a misaligned
  `ldqp`/`stqp` with SIGBUS, and keeps the 64-bit loop count in
  `%lsr1`. v6 computes everything as v5 does. Its L1 loads take 5
  cycles in either cluster and `ldqp` goes through L1. ALC5 operations
  no longer conflict with a divide. v6 adds fused `fma*` and
  acquire/release MAS forms, and a taken `ibranchd` costs 12.
- **Monitors** (monitors.md). User-mode counting works on all three
  machines with perf's raw `cpu` events. Uncore PMUs need
  `perf_event_paranoid` ≤ 0. Sampling is throttled, breakpoints miss
  hits, and TICKS samples lock onto short code called back to back; a
  random delay between calls removes it (monitors.md).

### Hardware errata (spec/isa/hwbugs.md)

The kernel's errata (`cpu_features.h`) mapped to our machines (4C E2S
rev 2, 8C2 E8C2 rev 3, 2C3 E2C3 rev 1), and our own anomalies (low
confidence). These matter for code:

- `CODE_PLACEMENT` (all three): gas works around it (Gotchas).
- The v6 MAS store-release is broken on E2C3 rev ≤ 2; use `wait` plus
  a store, as lcc and the kernel do.
- `ibranchd`/`rbranch` corrupt an inactive CTPR on every v6 CPU.
- #123567 (8C2, a load right after an intersecting store) has not been
  reproduced. lcc avoids it only with `-ffix-lcc-bug123567`, which
  MCST's kernel passes.

Ours: the 2C3's `mova` SIGILL after a finished counted loop (Gotchas),
and the 4C's kernel shifting `%g24`–`%g31` across a signal taken under
a nonzero `%bgr.cur`.

### Operation reference (spec/isa/operations-v3.md, -v5.md, -v6.md, -v7.md)

`gen_operations.py` generates each page from hand-written rules
(`operations-v{3,5,6,7}.yaml`; edit those, never the `.md`) joined with
`binutils-ops.yaml`, the timing tables, `lcc-usage.yaml` and the probes
that run each operation. Every operation from v2 on has exactly one rule,
in the file of the version that introduced it; `--check` fails on a
missing or duplicate rule. Every v3–v5 user-mode operation has run on
hardware except `mmurr`, and every v6 ALU operation except
`ibranchd`/`icalld`. v7 operations come from the community qemu or by
name only.

### Schedule scoring (tools/isa-score)

`score.py` compares each block's and single-block loop's schedule as
compiled ("achieved") with the best list schedule of the same operations
("possible"). By the owner's rule it is static, with no emulator runs;
execution counts come only from hardware (`profile.py --remote`
weights the blocks by a sampled perf run). Compare schedulers by **operations per cycle** of the achieved
schedule, not by the possible/achieved score. The model is within 1–2%
on `validate/`'s kernels on all three machines (lcc's v6 APB loop entry
aside) and within 6% on whole benchmarks once taken transfers are
measured. The README covers rules, accuracy and limits.

### ABI (spec/abi) — written, checked by the compat suite under qemu and on all three machines

The ABI of 64-bit unprotected mode (`e2k64`). `spec/abi/README.md`
indexes the files and defines the source mark every rule carries. Each
probe in `tools/abi/probes` (with `mode32/`, `protected/`) says in its
header what it checks and whether it is read with `-S` or run on hardware.

Headline facts, with where the details are:

- **Calls** (calling-convention.md): 8-byte slots, slots 0–7 in
  registers (callee `%dr0`–`%dr7`, caller `%db[0]`–`%db[7]`), 9–16-byte
  values on even slots, stack reserved for every slot at the caller's
  `USD`; results up to 64 bytes in registers, larger ones written to the
  caller's argument area (no hidden pointer); variadic calls put the last
  fixed and every variadic argument in memory, `va_arg` aligns 16-byte
  values by address; unprototyped calls work both ways; callers extend
  narrow integers to 64 bits and lcc callees re-extend. A sibling call
  may give its frame back with a positive `getsp` before its `ct` (lcc's
  don't: "Tail calls that release the frame").
- **Registers** (registers.md): window units are pairs of 64-bit
  registers; a `call` needs Q ≤ `wbs` < `wsz` (else SIGSEGV); DWARF 0–63
  are `%r0`–`%r63`, 64–127 `%b[0]`–`%b[63]` (MCST's gdb rejects 128 and
  up); `%g13` is the thread pointer, `%g12` the static chain. Compiler
  temporaries in globals belong in `%g16`–`%g31`, as lcc's do: around a
  signal handler the kernel saves and restores only those, and only for
  code interrupted outside a system call, so a handler's writes to
  `%g0`–`%g15` reach the interrupted code. A system call through entry
  3 returns `%g16`–`%g31` empty (0, tag 5), so the port's `"call"`
  clobber must cover them. A `getsp` amount must be in a window register
  or `%g16`+. `setwd dbl` affects only 32-bit integer ALU results. A
  function holding 80-bit or v5+ 128-bit values in registers needs
  `nfx = 0`, or the extended part is lost when the window spills (on the
  8C2 and 2C3 at every signal handler; on the 8C2 a 128-bit high half
  also at context switches and interrupts after a deep call has
  returned).
- **Floating point** (registers.md): `long double` is x87 extended in
  one register (mantissa + 16-bit extended part); 80-bit operations use
  `%fpcr`/`%fpsr`, not `%pfpfr`. The kernel does not save or restore the
  FP control registers around signal handlers.
- **Stacks, unwinding, debug information** (stack-and-unwinding.md):
  return addresses live in the chain stack (no stack protector needed);
  exceptions use `.eh_frame` only to find the FDE and LSDA (CFI is
  ignored) and pass data in `%b[0]`/`%b[1]`; with lcc's runtime nothing
  unwinds across a signal frame (no `linux-unwind.h`). MCST's gdb reads
  `%b[]` locations through the `%br` in effect, and GCC must write
  `.debug_line` itself ("Debug information"; plan D13).
- **ELF, PIC, TLS** (elf.md): the top byte of `e_flags` is the ISA
  version or CPU, `EF_E2K_INCOMPAT` marks code that isn't forward
  compatible; 64-bit literals hold swapped halves, PC-relative fields
  count from the bundle start; no GOT register (`rrd %ip` + a literal in
  each function that uses it); the PLT clobbers `%dr8`+ and `%ctpr1`;
  TLS variant I, GD/IE/LE only, no relaxation. Only the `e2k-v3` sysroot
  links forward-compatible programs: ld intersects its inputs' CPUs and
  every CPU tree's sysroot is marked for its CPU, while MCST's qemu
  ignores `e_flags` ("Mixing targets in one program"). MCST ld's bugs
  are there.
- **Atomics** (atomics.md): a read-modify-write is a lock-wait load (MAS
  0x7, ALC0) and a wait-unlock store (MAS 0x2) with `rbranch`; any load,
  store or system call between them loses the lock, and a lock left open
  makes the next lock-wait load SIGBUS (compare-exchange stores on both
  outcomes). Message passing reorders on all three machines unless the
  release has lcc's `wait` and the acquire uses the loaded value.
  16-byte atomics are libatomic calls with a lock on v3/v4 and inline
  `ldqp`/`stqp` on v5+; never mix the two on one object.
- **Kernel interface** (kernel-interface.md): system calls are `sdisp`
  entry 3, no vDSO; the state a `_start` meets ("Process entry"); the
  signal frame sits right below `USD` (no red zone); running out of a
  hardware stack is SIGKILL; stack-clash probes must go page by page
  ("Probing"); a context resumed by another thread runs with that
  thread's `%g13`, so TLS addresses must not be kept across a switch.
  The 4C runs MCST's older 6.1-1.9 kernel; the file marks where it
  differs.
- **Source compatibility** (source-compat.md, lcc-builtins.md):
  predefine `__e2k__`, `__iset__`, `__ptr64__`, never `__LCC__`; lcc's
  asm constraints, modifiers and clobbers, options and pragmas; its
  `__builtin_e2k_*` and `<e2kintrin.h>`. The C++ ABI is generic Itanium
  (calling-convention.md, "C++ ABI beyond calls").
- **Building MCST's kernel and glibc** (kernel: kernel-build.md, plan
  D11; glibc: kernel-interface.md "Process entry", source-compat.md,
  stack-and-unwinding.md "C cleanups and cancellation", plan D12):
  what they ask of a compiler beyond the ABI (lcc-only options, reserved
  globals, `check_stack`, asm idioms GCC deletes, the `"FP"` register).

32-bit (`-mptr32`, mode32.md) and protected (`-mptr128`,
protected-mode.md) modes are documented for completeness only. MCST's
qemu loads no ELF32 (the community `qemu-e2k32` does, without entries
3/6 and the GD bound), neither emulator runs protected code, and a
binutils built without `--enable-new-pm-abi` (ours) makes a PLT that
breaks dynamic protected programs.

**Compat suite** (`tools/abi/compat`; usage in `run.py`'s docstring and
spec/abi/README.md): `gen.py` writes caller/callee tests for the rules of
calling-convention.md, `va_list`, `longjmp` and C++ exceptions across
compilers included; `run.py` builds every ordered pair of compilers,
links with lcc and runs under qemu or, with `--remote`, on hardware. It
is the acceptance test for the owner's port:

```
python3 tools/abi/compat/gen.py toolchains/compat/gen
python3 tools/abi/compat/run.py toolchains/compat/gen \
    --cc gcc='...' --cxx gcc='...' --pairs lcc:gcc,gcc:lcc,gcc:gcc
# hardware: --target v3 --remote claude@ringo.lan; mixed targets:
# --target v3,v5.8c2 --cc gcc-v3@v3='... -march=elbrus-v3'
```

`known.txt` lists the known differences (all Unipro's); any other
failure fails the run. A test that times out only under qemu may be qemu
hanging (Gotchas, "Hangs"). The PoC GCC at `-march=elbrus-v3`/`v5`/`v6`
passed every pair with lcc's v3, v5.8c2 and v6.2c3 code on the three
machines (2026-10-13). Builtins have their own acceptance test,
`tools/abi/builtins/run.py --cc gcc=...`: it compares every builtin with
lcc on hardware and lists missing ones.

**GCC compiled by the PoC** (`tools/gcc-replay`, README.md). The PoC's
x86 cross, which x86 GCC compiled, builds a native GCC and keeps every
compile it makes. The native GCC then replays them on the machine whose
code it is (v3 on the 4C, 8C2-tuned on the 8C2) and must match the
cross's assembly byte for byte. Only x86 GCC and the PoC are in the
evidence: no lcc, no emulator, no GCC built on the machines. It found
P108 (C++'s unspecified argument order made GCC's output depend on its
builder), P109 and P110. At PoC `0.5+37` all 2,578 compiles matched on
both machines (2026-10-24).

### GCC port planning (doc/gcc-port-plan.md)

A plan for the owner, written for someone new to GCC: backend anatomy,
files to create (LoongArch's 2022 series as the model), cross-building
against lcc's sysroot, milestones M0–M8, design decisions D1–D13 (D11,
D12: building MCST's kernel and glibc; D13: debug information), VLIW
scheduling, what to take from ia64, TILE-Gx, C6x and SPARC, and in
section 11 what relcc2 provides alongside.

Owner's decisions (plan section 11, "Decisions and open questions"):

- Follow lcc's conventions wherever there is a choice: `setwd dbl` per
  function (D7); `.ignore strict_delay` with the compiler inserting every
  delay, `.accept strict_delay` around user asm (D9).
- Use MCST's unwinder and libgcc E2K files (section 3.3).
- Default `-march=elbrus-v3`, forward compatible (no `EF_E2K_INCOMPAT`).
- `%g12` is the static chain (recorded here; plan D1 lists it among the
  call-clobbered registers).
- `UPSR.fsm` is never changed (2026-10-11).
- Provide lcc-compatible `__builtin_e2k_*` builtins and `<e2kintrin.h>`,
  generated from spec/abi/lcc-builtins.yaml, but no x86 or NEON
  intrinsics and no predefined x86 feature macros (2026-10-06).

A first port can print one operation per line with no braces: MCST gas
assigns the channels, and with its `strict_delay` padding left on (no
`.ignore strict_delay`) it pads after compares; the hardware interlocks
the rest except the cases under "Interlocks" above, so such code is
correct, only slow (plan section 2).
GCC has no loop-rotation support, and only sel-sched does check-free
control speculation (sections 7.6, 7.7).

## Next steps (proposed, owner to choose)

0. `docs/spec-poc-findings.md` (the PoC agent's file, untracked): every
   entry through I56, A36 and P107 is folded into `spec/` and `doc/` and
   removed from it (I55, I56, A32–A36 and P95–P107 on 2026-10-24).
   Open: I57, A37–A52 and P108–P135 (from `tools/gcc-replay`,
   `tools/quadmath` and libffi, 2026-10-24; A40–A43, P114 and P115 from
   the first boots of `tools/distro`, 2026-10-25; A44, A45 and P116 from
   glibc's testsuite on the 8C2, 2026-10-26; A46 from glibc's testsuite
   on the 4C and 8C2, P117 and P118 from GCC's on the 8C2, 2026-10-26;
   A47–A49 and P119–P121 from building OS Elbrus's packages with
   MCST's PDK, 2026-10-30, A50 and P122–P132 on 2026-10-31, A51, P133 and P134 on 2026-11-01, I57, A52 and P135 from GCC's on the 4C the same day); new ones continue as I58, A53, P136.
1. Extend the compat suite (see Current state). It runs on the 4C with
   `run.py --target v3 --remote ...`; all pairs match qemu except the
   tag-trap tests, which pass on the 4C and on the 8C2 (v5, `--target
   v5.8c2 --remote claude@geofront.lan`).
2. The relcc2 work listed in `doc/gcc-port-plan.md` section 11 that is
   left: v4 timing (via the community) with `tools/isa-timing/bench.py`;
   the "Not checked yet" items still in spec/.
3. Protected (`e2k128`) and 32-bit modes: documented for completeness
   (mode32.md, protected-mode.md; their "Not checked yet" lists hold
   the leftovers); the port won't target them.

v7 is out of scope: it is not in silicon yet and we have no access
(owner, 2026-10-12). What the spec says about v7 comes from binutils,
the kernel and the community qemu; don't plan work on it.
4. ELF leftovers: none known (`R_E2K_PREF` is checked by
   `tools/abi/probes/prefrel.sh`).

## Open questions for the owner

None at the moment.

## Gotchas

Traps that cost a day if unknown; details in the files named.

### Emulators

Confirm anything that matters on hardware; MCST's qemu is the reference
where the two disagree. Full difference tables: the "Emulators" sections
of apb.md, speculation.md, predicates.md, loops.md, protected-mode.md,
and `tools/dejagnu/README.md`, "Known limits".

- **No hardware exceptions (MCST's qemu).** A memory fault, division by
  zero, `ct` through an empty CTPR, too small a `setwd` or a diagnostic
  operand stops it ("exception handling is unimplemented - aborting");
  a store past the 1 GiB guest area hangs instead (below). The
  community qemu gives SIGSEGV, but no signal for division by zero.
- **Too lenient (MCST's qemu): broken code passes.** It keeps an
  atomic's lock across loads and stores before the unlock store
  (hardware livelocks; atomics.md), gives `%b[]` `setbn`'s new base in
  the same instruction (hardware: the old one; registers.md), and runs
  a predicated `movtd` into a CTPR conditionally (gas accepts it;
  hardware traps under any predicate; predicates.md, "RLP"). Run
  atomics and hand-written prologues on hardware.
- **Hangs (MCST's qemu).** A plain store of a register with an empty
  tagged half repeats its wide instruction for ever, as does a store
  past qemu's 1 GiB guest area. That empty half is what `setwd` leaves
  in the registers it adds, and what GCC-built code stores when a
  variable set on only some paths is copied or kept across a call, so a
  GCC-built program that hangs only under qemu needs a hardware run
  before it counts as a bug (calling-convention.md, "Unused parts of a
  slot").
- **Array prefetch.** MCST's qemu restarts every area when a signal
  handler returns (lcc `-O2`+ loops then compute wrong results) and
  models none of APB's limits (apb.md).
- **Speculation.** MCST's qemu never takes `rbranch ? %dt_alN`, has no
  untouched pages and computes speculative FP normally; the community
  qemu aborts on `%dt_al`. Use `tools/isa-probes/spec.c` on hardware.
- **Wrong results.** Both emulators raise invalid on quiet FP compares
  (`fcmpud*f`, packed `eq`/`neq`/`od`/`uod`) and never set the denormal
  flag. MCST's qemu takes `qpslld`/`qpsrld` counts modulo 64 (hardware:
  0 from 64 up). The community qemu gets v6 packed FMAs wrong
  (semantics.md).
- **FP state.** MCST's qemu applies `%pfpfr` to 80-bit operations and
  ignores x87 precision control (hardware: `%fpcr`/`%fpsr`). The
  community qemu zero-extends `fadds` results under `dbl = 1` and starts
  processes with the FP flags clear; the kernel sets all six (`%pfpfr`
  0x1fbf, `%fpsr` 0x3f) (registers.md).

### Machines

- **2C3 array prefetch.** After any finished `%lsr`-counted loop
  (glibc's `strncmp` runs one) a `mova` outside such a loop is SIGILL;
  probes write `%lsr` first, 0 will do (hwbugs.md).
- **2C3 clock.** `clock_gettime` moves in ~330 ns steps; time short
  loops with `%clkr` (timing.md).
- **2C3 store-to-load timing depends on the process's history.** Time
  it in fresh processes and read each step's cycles
  (`memorycxx_stld.c trace`; timing.md, "The 2C3's line penalty").

### MCST's runtime and tools (testsuite noise)

- **Sticky FP flags.** After `feclearexcept`, lcc's speculative FP
  code (MCST's libm included: `sqrt` of a negative, `sin`, `exp`,
  `pow`, ...) dies with SIGILL unless built with `-fno-spec-fp`; libm's
  `fmod`/`remainder` hang in `FE_DOWNWARD` on some exact multiples;
  `fesetenv (FE_DFL_ENV)` unmasks the denormal trap and sets x87
  precision to single (registers.md, "MCST glibc's FP bugs";
  speculation.md, "What lcc speculates").
- **MCST's `libgcc_s.so.1`** aborts when a cancellation or
  `pthread_exit` unwinds into a GCC-built C frame with a cleanup; use
  the PoC's `libgcc_s` or `-static-libgcc` (stack-and-unwinding.md, "C
  cleanups and cancellation").
- **MCST's gdb** misreads a 16-byte float given as a bare `DW_OP_regN`;
  `next` over a call ending in `longjmp` runs to the end (registers.md,
  "How MCST's gdb reads them").
- **l++ and GCC objects** defining one guarded inline static: "multiple
  definition of `guard variable`" when l++'s comes first. Link GCC's
  first or use `-fno-gnu-unique` (calling-convention.md).
- **MCST ld.** `R_E2K_ISLOCAL` can't be linked, an ifunc in a PIE fails
  silently, a static link with an ifunc crashes ld; gold breaks dynamic
  links (elf.md, "Rarely used operators, ifunc and gold").
- **Linux built by anything but lcc.** `lcc-option(s)` silently drop
  what the compiler rejects, `-ffixed-g16`…`g25` included, and GCC then
  deletes `barrier()`: check `V=1` lines and object code (kernel-build.md).

### gas, objdump, Unipro

- **Store operand order.** gas reads *data, address, offset*;
  `.ignore ld_st_style` (lcc and Unipro emit it) makes it *address,
  offset, data*, objdump's order.
- **`CODE_PLACEMENT` padding.** gas puts an empty wide instruction, which
  runs, before jump targets at offsets 0x1c0–0x1ff of each 1 KiB block:
  align timing probes around it and check with objdump (hwbugs.md).
- **Debug lines.** A `.loc` without `view` gets its row only at the next
  `.loc`; plain assembly gets no debug sections even with `-g`
  (stack-and-unwinding.md, "Debug information").
- **objdump** shows `staa`/`ldaa` without MAS 0x3f as `aaurw`/`aaurr
  %unknown_aau_reg` and drops `incr` (`compare_objdump.py`'s "known
  objdump bug").
- **Unipro's assembly** needs `%empty` for `%empty.hi`, `-fno-addrsig`,
  `-fno-PIC` and no atomics for MCST gas (isa-decode's `build_corpus.sh`).

## Conventions

- Compare compilers against lcc at `-O3`: it is lcc's base level, about
  GCC's `-O2` (lcc's `-O2` is about GCC's `-O1`).
- Commit messages: imperative subject, wrapped body explaining why, ending
  with the attribution line the harness provides. Author email is
  configured in the repo (`a1ba.omarov@gmail.com`).
- Prose: plain, direct English; tables for structured facts; mark the
  source of every ABI rule and the verification status of every ISA
  section.
- Keep tools reproducible: generated files record the source commit; never
  hand-edit generated YAML.
- Generated probe files are committed unless they are several MB: those
  (the timeleft and memorycxx_stld probes) are listed in `.gitignore`, and
  `gen_operations.py` runs their generators (its `GENERATED` list)
  before it scans the probes.
