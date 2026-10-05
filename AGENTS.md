# relcc2: agent guide

This file is the shared context for AI agents working on this repository.
`CLAUDE.md` is a symlink to it. Read it fully before starting work.

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

Hardware: the owner has an Elbrus 4C (v3) and 8CB (v5); they know people
with v6 machines. v7 is in development. Timings are the same for v1..v5 and
differ from v6.

- **4C (v3)**: `ssh -i secrets/id_ed25519 -o IdentitiesOnly=yes
  claude@ringo.lan` (key in untracked `secrets/`, never commit or copy it).
  4× E2S, 16 cores, 61 GiB DDR3 (two memory channels per processor
  instead of three), ALT Linux, native lcc 1.29.16, gdb, perf.
  It is a shared machine, not a container: keep parallelism low (about
  `-j4`) and don't fill `/tmp` (tmpfs). **Several agents use the account
  at once**: work only in your own subdirectory `~/relcc2/<task>` (check
  that it isn't taken), and when done remove only that subdirectory.
  Never remove `~/relcc2` itself or other agents' subdirectories; the
  account's dotfiles stay. `bench.py --remote-dir` and `run.py
  --remote-dir` choose the subdirectory.
- **8C2 (v5)**: `ssh -i secrets/id_ed25519 -o IdentitiesOnly=yes
  claude@geofront.lan` (the same key). Elbrus-8C2 (E8C2, cpu family 5),
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
- Nearly every Elbrus system ships MCST's lcc as the default compiler (OS
  Elbrus and ALT Linux), so tests may assume lcc on the target.
- ALT Linux ships no 32-bit or protected-mode (`e2k128`) runtime. Keep those
  modes in mind when generating code and tests, but leave them as an
  intentionally untested branch; they are low priority.

## Repository layout

```
spec/isa/      ISA spec: bundle.md (wide instruction format), control-transfer.md
               (branches, calls, CTPRs), semantics.md (what operations
               compute), apb.md (array prefetch), speculation.md
               (speculative operations, diagnostic values, DAM),
               loops.md (loop mode, %lsr, rotation),
               monitors.md (performance counters through perf),
               binutils-ops.yaml
               (every operation), registers.yaml (state and AAU registers)
spec/abi/      ABI spec for e2k64: data layout, registers + DWARF, calling
               convention, stacks and unwinding, ELF/relocations/PIC/TLS,
               atomics, source-level conventions (macros, asm, options)
spec/NOTICE.md attribution and licence of spec/
doc/           plans for the owner: gcc-port-plan.md (how to write the GCC port)
docs/          spec-poc-findings.md: spec problems found by toolchains/gcc-claude
               (kept by the agent working there)
tools/setup/   install reference toolchains into toolchains/
tools/isa-extract/  generate spec/isa YAML from binutils; check it against gas
tools/isa-decode/   decoder written from spec/isa; corpus; compare with objdump
tools/isa-timing/   latency/hazard microbenchmarks run on hardware over SSH
tools/isa-score/    scores a compiled program's use of E2K parallelism:
                    the compiler's schedule vs the best schedule of the
                    same operations, per block and loop, static;
                    profile.py weights it by a sampled run on the 4C
                    (README.md)
tools/isa-probes/   operation semantics probe (semantics.c; gen_semantics.py
                    writes semantics.s) and array prefetch probes (apb.c;
                    gen_apb.py writes apb.s and apb-cases.h; apb-loop.c,
                    apb-lcc.c) and speculation probe (spec.c;
                    gen_spec.py writes spec.s and spec-cases.h) and
                    monitor events (pmu.c, gen_pmu.py; pmu.h is a
                    helper for counting in any probe) and loop
                    mode (loops.c; gen_loops.py writes loops.s and
                    loops-cases.h) and memory accesses in one wide
                    instruction (samewi.c, samewi.s) and every v5
                    operation (sem5.c; gen_sem5.py writes sem5.s)
                    and global register rotation (abg.c, abg.s),
                    and combined FP operations (fcomb.c;
                    gen_fcomb.py writes fcomb.s),
                    run on hardware; -DSPEC_V5, -DLOOPS_V5, -DAPB_V5
                    builds add v5 cases
tools/abi/probes/   C probes for the ABI (layout.c must compile; callconv.c is
                    compiled to assembly and read, as are atomics.c,
                    asmops.c and stackprot.c; tagtrap.c, dbl.c,
                    ldouble.c, ctpr.c, dso/run.sh and elfops/run.sh run
                    on hardware, as do litmus.c, getsp.c and
                    reenter.c with their .s files)
tools/abi/compat/   ABI compatibility suite: gen.py writes caller/callee
                    pairs, run.py builds every compiler pair and runs them
                    under qemu-e2k or, with --remote, on hardware over SSH;
                    known.txt lists known differences
OpenE2K/       submodules (see above)
toolchains/    untracked: installed toolchains, corpus builds, scratch output;
               gcc/ (GCC mainline, blobless clone) and gcc-tilegx/ (worktree
               at 54a5f478487, the last commit with TILE-Gx) as references;
               gcc-claude/ (experimental agent-written E2K backend, see
               decision 2)
x86_64/        untracked: Unipro debs
```

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
tools/isa-decode/build_corpus.sh toolchains/corpus
python3 tools/isa-decode/compare_objdump.py toolchains/corpus/*.o
                                                  # 6,826 match, 10 known objdump bug
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
python3 tools/isa-extract/compare_sheet.py <csv>  # 14 documented disagreements
python3 tools/isa-score/selftest.py               # stall rule vs timing-e2s.yaml: ok
python3 tools/isa-score/validate/validate.py --remote claude@ringo.lan \
    --remote-dir relcc2/<task>                    # isa-score vs the 4C,
                                                  # ~1 min: within 1% (one 3%)
```

## Current state

### ISA (spec/isa) — largely complete for code generation

- `binutils-ops.yaml`: every ALU operation (4,271 table entries, 2,957 after
  folding alias spellings such as `plog_128`/`plog_and` → `plog_0x80`),
  with ISA versions, channels, opcode, format and operand formats. Control
  operations from `e2k-opc.c` are in `manual_ops`.
- `bundle.md`: wide instruction layout, every syllable (HS, SS, ALS, ALES,
  CS0, CS1, AAS, LTS, PLS, CDS), operand encodings, operand roles per
  format, channel pairs for quad operations, bundling restrictions,
  assembler syntax notes, known objdump bugs. Each section is marked
  [checked] (byte-exact against gas), [decoded] (our decoder agrees with
  objdump on the corpus) or [source] (read only).
- Still [source] only: SS type 1 (`abgc`/
  `rbgc`), `vfbg`, `puttsd`, `%bgrpred` (gas doesn't accept them), v7 `dtbp`
  and ctcond type 11, `staa`/`ldaa`/`incr` decoding (hidden by an objdump
  bug; encoding is checked).
- Timing measured on the 4C (v3) by `tools/isa-timing/bench.py`
  (`spec/isa/timing.md`, data in `timing-e2s.yaml`, with `measured_on`):
  scheduled latencies match the guide (int 1, FP/mul 4, L1 load 3,
  `disp`→`ct` 5, ...); crossing between the clusters ALC0–2 and ALC3–5
  costs +1 for integer/load results and +2 for FP ones (the guide's
  int→FP +1 is this crossing); a stall costs about a
  whole latency extra (exactly: an operand e cycles early stalls
  4 × ceil(e/4)); an FP-side result routed to an integer op is a cycle
  late when an integer op or load issues in its channel 2 cycles after
  it (write-back conflict); everything user code touches is interlocked except
  `rwd %lsr` → `rrd %lsr` (4 cycles). Also measured: packed integer ops
  (1 cycle, 4 to integer consumers; the guide says FP class), conversions
  (5), FP compares (+2 over integer ones), predicate logic (as the guide;
  `andp` chains within one instruction). Control transfer
  (`spec/isa/control-transfer.md`, hw): a taken `ibranch` costs 6 cycles,
  a prepared `ct`/`call`/return issued in time costs nothing; every call
  and return empties `%ctpr1–3` (a taken `ct` through an empty or
  diagnostic CTPR traps, a false one doesn't); gas forbids preparing and
  transferring through the same CTPR in one instruction before v6.
  Operation semantics (`spec/isa/semantics.md`, hw): `sxt` 4–6 zero-extend
  (the guide is wrong), `merge` picks the first operand when the
  predicate is false, shift counts are modulo the width, division by
  zero is SIGFPE, FP `lt`/`le` signal on quiet NaNs, `fmax`/`fmin` are
  x86-style, `fmul_add` rounds twice, combined operations `X_Y a, b, c` compute `Y(c, X(a, b))` (so `_sub` is c − (a X b), `_rsub` (a X b) − c), `fcmpud*f` is the quiet FP compare into a register (x86 ZF/PF/CF layout; MCST's qemu wrongly raises invalid), `cmpand*` tests `a & b`, `movtd`/`getpl` to a CTPR only in ALC0, a taken `ibranch` hides a stall in its own instruction; memory accesses to one address in one wide instruction take effect ALC2 before ALC5 and a load before a store in its cluster (a load in ALC5 sees an ALC2 store; the ALC5 store of two wins; bundle.md). Speculation
  (`spec/isa/speculation.md`, hw): `,sm` faults give diagnostic values
  that trap (SIGILL `ILL_ILLOPN`) only at a non-speculative use,
  including a diagnostic predicate; `ld,sm` is semi-speculative (pages
  in valid pages, diagnostic for invalid ones, one kernel entry per
  invalid region), `mas=0x3` loads fully speculative; a speculative FP
  op that would set a flag not already set gives a diagnostic instead
  (the kernel starts processes with all flags set); data speculation:
  lock load `mas=0x4,sm` (ALC0/3; without `,sm` no entry), check load `mas=0x3` (ALC2/5, same
  register) that reloads and sets `%MLOCK` for `rbranch` only if the
  entry was lost (overlapping store, trap, no lock load), kept across
  shallow calls and register overwrites, exactly 32 entries; mod 7 is a fully
  speculative lock, mod 2 a check that keeps the entry, stores with mod
  3/5 drop their data register's entry (5 writes nothing), mod 4 stores
  skip the DAM; a fully speculative load recovers through `rbranch ?
  %dt_alN`, taken when the `,sm` operation in ALC N of that instruction
  has a diagnostic operand (lcc's `-fno-semi-spec-ld`); with an
  exception unmasked a speculative FP op always gives a diagnostic
  (SIGILL at the use instead of SIGFPE). Performance monitors
  (`spec/isa/monitors.md`, hw): perf's raw `cpu` events (`0xMMEE`,
  event lists in the kernel's `tools/perf/pmu-events/arch/e2k`), two
  instruction and two data monitors on v3, user-mode counting allowed
  on the 4C, `%dimar0/1` readable with `rrd` in user mode; `EXEC`
  counts wide instructions, `TICKS` cycles, `BUB_E2` 4-cycle stall
  steps, `CONF_ALUn_OUT` write-back conflicts, plus DAM, L1 and spill
  events; sampling is throttled and breakpoints miss hits, so neither
  gives exact per-address counts. Loop mode (`spec/isa/loops.md`, hw):
  a loop with `alc` and `ct ? %NOT_LOOP_END` runs `lcnt + ecnt`
  iterations (`vlc` = 0: endless), `rwd %lsr` needs 4 cycles before the
  loop (else one extra iteration) and also sets `%ilcr`; `%pcntN` is
  true while `pcnt ≤ N`, `%lcntex` only after the loop; `loop_mode`
  stores are dropped while `pcnt > 0` and in the last iteration from
  store number `strmd` on (`semc` turns it off), loads never; `abn`
  makes the old `%b[0]` the new `%b[2]`, `abp` the old `%pred0` the new
  `%pred1`; a one-instruction loop runs one iteration per cycle;
  AAU stores obey the store rules and a dropped one doesn't advance its
  index; a quad store (`staaq`) counts as two for `strmd`, and splitting
  it is SIGILL; `mova` stops in epilogue iterations unless `ldmc`; `vlc` = 0 with an exit predicate makes a while loop; calls
  don't change `%lsr` but a callee's loop does (save it around them);
  counts and rotation survive signals and page faults; lcc peels the
  prologue and uses no `pcnt`/`ecnt` in counted loops. Array prefetch
  (`spec/isa/apb.md`, hw): the program format, what a `mova` reads
  (`base + AAIND + disp + step × n + ind`, zero-extended, diagnostic
  instead of a fault), latency 5, and the correctness rules: a call, a
  return or a `%ctpr2` write stops the program (the next `mova` is
  SIGILL), and after any interrupt the kernel recomputes positions from
  `%ilcr − %lsr`, so APB is correct only in a `loop_mode` loop counted
  by `%lsr` (written before `bap`) with every area advanced once per
  iteration; the buffer doesn't see stores, so nothing may write an
  element a later `mova` reads (lcc versions loops that may alias);
  area size is the prefetch depth (about 1 KiB runs a stream from
  memory at L1 speed); `dcd` 2 (4C) or 3 (8C2) keeps other data cached
  at no cost; a write to `%aabf0` stops the unit at the next trap (lcc's
  loop end). lcc's loops are 2–22× faster with it. Loads
  are scheduled for L1;
  `--memory` measures beyond-L1 latency for reference only.
  Throughput (`bench.py` `tp`): every channel takes an operation every
  cycle at full width (6 integer, 4 FP, 4 loads, 2 stores), except the
  divider in ALC5 shared by all divides (1 cycle apart stalls 4,
  `sdivd` wants 3, `fsqrts` also conflicts at 5) and L1 banks (two
  stores agreeing in address bits 6–7, or a store and a load in one
  32-byte block, can take 2 cycles). The Elbrus-8C2 (v5,
  `timing-e8c2.yaml`, timing.md "Elbrus-8C2") has every v3 latency,
  hazard and control delay; v5 adds 6 FP ops per cycle (ALC2/5),
  128-bit `qp*` ops with scalar latencies; `ldqp` bypasses L1 (served
  by L2: latency 11, one per cycle; `ldq` into a pair goes through L1,
  latency 3; lcc loads 128-bit data as two `ldd`); an APB area delivers
  16 bytes a cycle (also v3); `%aaind` and `%aaincr` are 64-bit, `fmt`
  7 is SIGILL; all 195 v5 ALU operations have measured latencies (1, 2,
  4, 5, 6 or 8 by class, as their 64-bit counterparts) and are fully
  pipelined; FP min/max is 2 cycles (also v3); a 64-bit FP-side result
  feeding a `qp` operation pays the crossing cost; L2 11, L3 about 32,
  memory about 180 cycles. isa-score models these
  and is within 3% on the 8C2's kernels and 1.00 on CoreMark
  (`validate.py --target v5.8c2 --period 30011`). Other v5 hardware results: `%lsr1` is the 64-bit loop
  count (write it after `%lsr`); misaligned APB areas give data; the
  compat suite passes for every pair (no register cleaning); Elbrus
  Linux's libatomic is built for v5, so v3 and v5 code can share
  16-byte atomics there; quad lock/check loads work like doubleword
  ones (entries per register, 32); `stqp`/`staaqp` count as one store
  for `strmd`; `movaqp` reads 16 bytes, also misaligned; all 703 v5 ALU
  operations match the community qemu (MCST's differs in
  `qpslld`/`qpsrld` counts >= 64), packed eq/od compares are quiet,
  round-to-integral mode bit 3 suppresses inexact, and FP ops set the
  denormal flag (also v3; no emulator does); global register rotation
  (`abg`, `%bgr`; loops.md) is an x87-style stack: `abgd` pushes and
  traps on overflow, `abgi` pops, reading an invalid rotating global
  traps only in ALC4/5 and non-speculatively (the same on the 4C). v4,
  v6, v7 are not measured.

### Schedule scoring (tools/isa-score)

`score.py PROGRAM` compares, for every basic block and single-block
loop, the compiler's schedule ("achieved", interlock stalls and
write-back conflicts included, loads in L1) with the best list schedule
of the same operations on the chosen ISA version ("possible", keeping
the compiler's registers or true dependences only; loops by initiation
interval). It is static (owner: no emulator runs). `profile.py PROGRAM
--remote ...` weights the same blocks by a run on the 4C: perf samples
the EXEC and TICKS monitors (spec/isa/monitors.md), whose exact
instruction addresses give per-block execution counts and measured
cycles, and it reports measured against modelled cycles per block plus
a stall breakdown. To compare schedulers use operations per cycle of
the achieved schedule, static or weighted by the run: on CoreMark and
Linpack it ranks lcc > Unipro clang > gcc-claude, as the 4C does; the
possible/achieved score does not. `validate/` (about a minute on the
4C) puts every kernel within 1% (one within 3%); on the benchmarks the
model is within 6% once `profile.py` measures which transfers were
taken (a second sampled run, EXEC − `NO_CT_TAKEN`): fetch bubbles are
taken `ibranch`es (6 cycles) and taken `ct`s waiting for a late
prepare, nothing else (control-transfer.md, "Instruction fetch"). v6/v7 use v3 latencies.

### ABI (spec/abi) — written, checked by the compat suite under qemu-e2k and on the 4C (v3)

Key facts: arguments in 8-byte slots, slots 0..7 in registers (callee
`%dr0..7`, caller `%db[0..7]`), 9–16-byte values on even slots, stack area
always reserved for all slots at the caller's `USD`; results ≤ 64 bytes in
registers, larger ones written to the caller's argument area (no hidden
pointer); variadic calls store the last fixed and all variadic arguments
in memory, and `va_arg` aligns 16-byte values by address;
unprototyped calls pass both ways (Unipro gets this wrong); callers extend
narrow integers to 64 bits and lcc callees re-extend; window units are
pairs of 64-bit registers; DWARF 0–63 = `%r0..63`, 64–127 = `%b[0..63]`;
exceptions use `.eh_frame` only for FDE/LSDA lookup (CFI ignored) and pass
data in DWARF 64/65 (`%b[0]`, `%b[1]`).

ELF (`spec/abi/elf.md`, run on the 4C): `e_flags` top byte is the ISA
version or CPU model, `EF_E2K_INCOMPAT` marks non-forward-compatible code;
64-bit literals hold their halves swapped and PC-relative fields count
from the bundle start; no GOT register (each function does `rrd %ip` +
`_GLOBAL_OFFSET_TABLE_` literal); calls go through `disp` and the PLT,
which runs in the callee's new window and clobbers `%dr8`+ and `%ctpr1`;
lazy binding calls `_dl_fixup(link_map, reloc_offset)` directly; TLS is
variant I with TP in `%g13`, `__tls_get_addr(module, offset)`, GD/IE/LE
only and no linker relaxation. MCST ld problems: `R_E2K_ISLOCAL` can't be
linked, an ifunc referenced with `@GOTOFF` in a PIE fails silently, a
static link with an ifunc crashes ld; gold (not shipped) breaks dynamic
links. A `getsp` amount must be a window register or `%g16`+ (the
kernel can't read `%g0`–`%g15` when the stack grows: SIGSEGV for large
allocations); a `jmp_buf`'s IP (byte 8) can be replaced by a label to
re-enter a frame (nonlocal goto); lcc's `&&label` is an index, not an
address. No stack protector is needed (return addresses live in the
chain stack); lcc 1.31's `-fstack-protector` uses a constant canary
`0x000d0aff` and protects every function, and its `-strong`/`-all` do
nothing.

Atomics (`atomics.md`, lcc output, MAS meanings from MCST qemu): every
read-modify-write is a lock-wait load (MAS 0x7, ALC0) plus wait-unlock
store (MAS 0x2) with `rbranch` (= `ibranch ? %MLOCK`), whatever the
order; fences and seq_cst/release accesses use `wait ld_c, st_c`.
16-byte atomics are libatomic calls with a lock on v3/v4 and inline
`ldqp`/`stqp` on v5+; mixing them on one object breaks. On the 4C
(`litmus.c`) v3 is TSO-like across wide instructions: only a load
passing an earlier store, which `wait st_c` stops; two stores in one
instruction may become visible in either order. Source-level
conventions (`source-compat.md`): define `__e2k__`, `__iset__`,
`__ptr64__`, never `__LCC__` (glibc's headers key lcc-only code on it);
asm `r` = register or even pair, `x` = extended register; lcc treats
`swapcontext` as returns-twice and zero-inits maybe-uninitialised locals
against tag traps.

`long double` (`ldouble.c`, hw): exact x87 extended arithmetic in one
register (mantissa + 16-bit extended part, `movif`/`movfi`); the extended
part survives a window spill only with `nfx = 0`; 80-bit ops use
`%fpcr`/`%fpsr` (rounding, x87 precision control, flags), not `%pfpfr`;
stores write only 10 bytes.

Other measured facts: `setwd dbl` affects only 32-bit integer ALU ops
(loads always write the whole register, FP ops never extend; Unipro
uses `dbl = 1`, lcc `dbl = 0` except at `-O2`+ in functions that need
unsigned 32-bit results zero-extended, about 18% of them); `std,sm` of a register with a tagged upper
half doesn't trap. MCST's libffi (`src/e2k/`) agrees with
calling-convention.md on every rule and filled its gaps (no slot
backfill, callee owns the argument area, large results overlap memory
arguments, `%g12` as static chain).

Compat suite (`tools/abi/compat`): 1,931 generated tests per compiler pair
(scalars, aggregates of 1–1024 bytes, packed/aligned/bit-field/union
types, large results, long argument lists, variadic, unprototyped,
integer extension, `va_list` across compilers, `longjmp` across
compilers, C++ exceptions across compilers, C++ classes that are not
trivially copyable as results and arguments, 300 random signatures). It is
the acceptance test for the owner's GCC port:
`run.py GENDIR --cc gcc='...' --cxx gcc='...'
--pairs lcc:gcc,gcc:lcc,gcc:gcc`. lcc's v3 code also passes it under qemu
as v5, v6 and v7 (`--runner`). Found so far
(in `known.txt` and calling-convention.md): Unipro's unprototyped calls,
Unipro callees not re-extending narrow arguments, Unipro's
`va_arg(__int128)` at odd slots, and a tag trap when a callee spills a
partly written slot (the trap is confirmed on the 4C by
`tools/abi/probes/tagtrap.c`; the suite only hits it under qemu, which
fills fresh registers with tagged values). The `valist` unit showed that
`va_arg` must align 16-byte values by address, because lcc's `static`
variadic functions have an 8-byte-aligned slot area; probes made while
studying it showed that lcc callers store only the last fixed and the
variadic arguments.
C++ exceptions interoperate; the runtime aligns exception objects to 16
bytes only. Still to add: `-march` variants.

### GCC port planning (doc/gcc-port-plan.md)

A plan for the owner, written for someone new to GCC: pipeline and
backend anatomy, files to create (LoongArch's 2022 series as the model),
cross-build commands against lcc's sysroot, milestones M0–M8, design
decisions, VLIW scheduling, and what to take from ia64, TILE-Gx, C6x and
SPARC (with file:line references into `toolchains/gcc`). Owner's
decisions so far: follow lcc's conventions wherever there is a choice
(`setwd dbl` per function; `.ignore strict_delay` with the compiler inserting
every delay, `.accept strict_delay` around user asm); use MCST's
unwinder and libgcc E2K files; default `-march=elbrus-v3`, forward
compatible; `%g12` as static chain. Key facts behind it: MCST gas
assigns channels to ops outside braces, so a first port can print one op
per line, and the hardware interlocks everything but `%lsr`, so such code
is correct without padding, only slow; GCC has no
loop-rotation support and only sel-sched does check-free control
speculation. `tools/setup/build-gcc-docs.sh` builds GCC's manuals
(gccint) into `toolchains/gcc-docs/`.

## Next steps (proposed, owner to choose)

0. `docs/spec-poc-findings.md` (the PoC agent's file, untracked): all
   entries so far are folded into `spec/` and `doc/` and removed from it;
   new ones continue as I16, A18, P12.
1. Extend the compat suite (see Current state). It runs on the 4C with
   `run.py --target v3 --remote ...`; all pairs match qemu except the
   tag-trap tests, which pass on the 4C and on the 8C2 (v5, `--target
   v5.8c2 --remote claude@geofront.lan`).
2. The relcc2 work listed in `doc/gcc-port-plan.md` section 11: DejaGnu
   boards for qemu-e2k and the 4C; timing on other
   generations with `tools/isa-timing/bench.py`; loop-mode and
   speculation leftovers (`spec/isa/loops.md`, `speculation.md`, "Not
   checked yet").
3. APB leftovers: v6+ only (`spec/isa/apb.md`). Protected
   (`e2k128`) and 32-bit modes.
4. ELF leftovers: `@PREF` (`pref %iprN, sym`; source only so far).

## Open questions for the owner

None at the moment.

## Gotchas

- MCST's qemu cannot deliver hardware exceptions as signals: division by
  zero, a taken `ct` through an empty CTPR, a too-small `setwd`, a
  diagnostic-operand trap all stop it with "exception handling is
  unimplemented - aborting". Test those on the 4C.

- The community qemu (the owner's; to be fixed later) differs from the
  4C and MCST's qemu in two ways: under `dbl = 1` it zero-extends 32-bit
  FP results such as `fadds` (`is_alop_affected_by_dbl`), and it starts
  processes with the FP exception flags clear, while the kernel sets all
  six in `%pfpfr` and `%fpsr` (`%pfpfr` 0x1fbf, `%fpsr` 0x3f). Use MCST's
  qemu (`toolchains/qemu-mcst`) as the reference for both.

- MCST's qemu restarts every array prefetch area from its start when a
  signal handler returns, so code built by lcc at `-O2` and above can
  compute wrong results there if the program receives signals (lcc's
  loops in `apb-lcc.c`: 600 wrong of 900). It models none of APB's
  limits either. Test APB on the 4C.

- Checked speculation can't be tested under emulation: MCST's qemu never
  takes `rbranch ? %dt_alN` and has no untouched pages (so fully
  speculative loads never fail there), and the community qemu aborts on
  `%dt_al`. Use the 4C (`tools/isa-probes/spec.c`).

- MCST's qemu applies `%pfpfr` (rounding, flags) to 80-bit operations
  and ignores x87 precision control; the hardware uses `%fpcr`/`%fpsr`.
  Check `long double` rounding and flags on hardware.

- Unipro's assembly needs fixes to assemble with MCST gas: write `%empty`
  instead of `%empty.hi`, compile with `-fno-addrsig` and `-fno-PIC` (gas
  rejects its GOT label arithmetic), and avoid atomics (gas rejects its
  locked-store syntax).
- Store operand order: gas reads *data, address, offset* by default;
  `.ignore ld_st_style` (emitted by both lcc and Unipro) switches to
  *address, offset, data*, the order objdump prints.
- objdump prints any `staa`/`ldaa` without MAS 0x3f as `aaurw`/`aaurr`
  `%unknown_aau_reg` and drops the `incr`; `compare_objdump.py` reports these
  as "known objdump bug".
- binutils' opcode table lists `plog`/`qplog` with 64-bit operand formats;
  the older sheet has 128-bit packed for `qplog`. gas accepts both.
- `gas` rejects a store in ALC2 with a load in ALC3, and a store in ALC5 with
  a load in ALC0, in one bundle.
- Hand-written corpus `.s` files declare their ISA range with
  `isets: N-M` in the first three lines.

## Conventions

- Commit messages: imperative subject, wrapped body explaining why, ending
  with the attribution line the harness provides. Author email is
  configured in the repo (`a1ba.omarov@gmail.com`).
- Prose: plain, direct English; tables for structured facts; mark the
  source of every ABI rule and the verification status of every ISA
  section.
- Keep tools reproducible: generated files record the source commit; never
  hand-edit generated YAML.
