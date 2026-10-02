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
| lcc cross compilers | https://setwd.ws/sp (owner's archive; 1.19..1.31, stable 1.29.16, preview 1.31.05) | ABI oracle; includes a full sysroot |
| Unipro LLVM 20.1.8 | x86_64 debs in `x86_64/` (untracked) | Second oracle; covers v3..v7 |
| MCST's LLVM 13 port (lccrt) | https://github.com/OpenE2K/llvm-lccrt/releases/tag/v8.2 (x86 host binaries from OS Elbrus 8.2) | Its `liblccopt` plugin is lcc's code generator with full debug info. Planned black-box source for timing tables (run under gdb, dump data); never decompile into the repo |
| Opcode spreadsheet | https://docs.google.com/spreadsheets/d/1p8K5DWBsXTpCDk3TdFwMaTFIadAFuxaH4zfnJXUxK94 | Generated from older binutils that still had the table generator. **Not independent of binutils** |

Hardware: the owner has an Elbrus 4C (v3) and 8CB (v5); they know people
with v6 machines. v7 is in development. Timings are the same for v1..v5 and
differ from v6.

- **4C (v3)**: `ssh -i secrets/id_ed25519 -o IdentitiesOnly=yes
  claude@ringo.lan` (key in untracked `secrets/`, never commit or copy it).
  4× E2S, 16 cores, 61 GiB RAM, ALT Linux, native lcc 1.29.16, gdb, perf.
  It is a shared machine, not a container: keep parallelism low (about
  `-j4`), work in `~/relcc2`, and don't fill `/tmp` (tmpfs).
- Nearly every Elbrus system ships MCST's lcc as the default compiler (OS
  Elbrus and ALT Linux), so tests may assume lcc on the target.
- ALT Linux ships no 32-bit or protected-mode (`e2k128`) runtime. Keep those
  modes in mind when generating code and tests, but leave them as an
  intentionally untested branch; they are low priority.

## Repository layout

```
spec/isa/      ISA spec: bundle.md (wide instruction format), binutils-ops.yaml
               (every operation), registers.yaml (state and AAU registers)
spec/abi/      ABI spec for e2k64: data layout, registers + DWARF, calling
               convention, stacks and unwinding, ELF/relocations/PIC/TLS
spec/NOTICE.md attribution and licence of spec/
doc/           plans for the owner: gcc-port-plan.md (how to write the GCC port)
tools/setup/   install reference toolchains into toolchains/
tools/isa-extract/  generate spec/isa YAML from binutils; check it against gas
tools/isa-decode/   decoder written from spec/isa; corpus; compare with objdump
tools/abi/probes/   C probes for the ABI (layout.c must compile; callconv.c is
                    compiled to assembly and read; tagtrap.c and dso/run.sh
                    run on hardware)
tools/abi/compat/   ABI compatibility suite: gen.py writes caller/callee
                    pairs, run.py builds every compiler pair and runs them
                    under qemu-e2k or, with --remote, on hardware over SSH;
                    known.txt lists known differences
OpenE2K/       submodules (see above)
toolchains/    untracked: installed toolchains, corpus builds, scratch output;
               gcc/ (GCC mainline, blobless clone) and gcc-tilegx/ (worktree
               at 54a5f478487, the last commit with TILE-Gx) as references
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
                                                  # qemu-e2k, v5: 1,853 tests per pair
                                                  # (4 pairs with Unipro), ~10 s:
                                                  # 0 unexpected (lcc->lcc all pass)
python3 tools/abi/compat/run.py toolchains/compat/gen --target v3 \
    --remote claude@ringo.lan -o toolchains/compat/ringo-v3
                                                  # on the 4C, ~30 s: 0 unexpected
python3 tools/isa-extract/compare_sheet.py <csv>  # 14 documented disagreements
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
- Still [source] only: APB array-prefetch programs, SS type 1 (`abgc`/
  `rbgc`), `vfbg`, `puttsd`, `%bgrpred` (gas doesn't accept them), v7 `dtbp`
  and ctcond type 11, `staa`/`ldaa`/`incr` decoding (hidden by an objdump
  bug; encoding is checked).
- No timing data yet (latencies, bypasses). Planned sources: lcc's lccopt
  (debug build, as a black box), Unipro's scheduling model, microbenchmarks
  on hardware, with a `measured_on` field per value.

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
only and no linker relaxation.

Compat suite (`tools/abi/compat`): 1,853 generated tests per compiler pair
(scalars, aggregates of 1–1024 bytes, packed/aligned/bit-field/union
types, large results, long argument lists, variadic, unprototyped,
integer extension, `va_list` across compilers, `longjmp` across
compilers, C++ exceptions across compilers, 300 random signatures). It is
the acceptance test for the owner's GCC port:
`run.py GENDIR --cc gcc='...' --cxx gcc='...' --pairs lcc:gcc,gcc:lcc,gcc:gcc`. Found so far
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

## Next steps (proposed, owner to choose)

1. Extend the compat suite (see Current state). It runs on the 4C with
   `run.py --target v3 --remote ...`; all pairs match qemu except the
   tag-trap tests, which pass on the 4C. Run it on v5 (8CB) when
   available, which may clean registers like qemu.
2. The relcc2 work listed in `doc/gcc-port-plan.md` section 11: DejaGnu
   boards for qemu-e2k and the 4C; the full list of non-interlocked
   hazards (compare → predicate use, state registers; gas's
   `strict_delay` rules are a start); measured timing tables per
   generation; data-speculation and loop-mode specs.
3. APB; protected (`e2k128`) and 32-bit modes.
4. ELF leftovers (`spec/abi/elf.md`, "Not covered yet"): `@ISLOCAL`,
   `@DYNOPT`, ifunc, gold.

## Open questions for the owner

- None right now.

## Gotchas

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
