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

The project is called relcc2. relcc (2020) was the owner's earlier attempt to
decompile MCST's lcc; it stalled on the codebase's size.

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
| Community QEMU e2k | submodule `OpenE2K/qemu-e2k-community` | Instruction semantics (the owner co-developed it) |
| MCST QEMU e2k | submodule `OpenE2K/qemu-e2k-mcst` | Instruction semantics, second opinion |
| MCST E2K GCC 11 runtime libs (GPL) | `internal-lcc-libs-open-source-1.29.16.tar.xz` in https://setwd.ws/sp/1.29/1.29.16/src/ (unpacked in `toolchains/src/`, not tracked) | libgcc (E2K unwinder `config/e2k/unwind-dw2-e2k.c`), libstdc++, libatomic, libquadmath |
| lcc cross compilers | https://setwd.ws/sp (owner's archive; 1.19..1.31, stable 1.29.16, preview 1.31.05) | ABI oracle; includes a full sysroot |
| Unipro LLVM 20.1.8 | x86_64 debs in `x86_64/` (untracked) | Second oracle; covers v3..v7 |
| Opcode spreadsheet | https://docs.google.com/spreadsheets/d/1p8K5DWBsXTpCDk3TdFwMaTFIadAFuxaH4zfnJXUxK94 | Generated from older binutils that still had the table generator. **Not independent of binutils** |

Hardware: the owner has an Elbrus 4C (v3) and 8CB (v5) and may provide a
limited SSH account; they know people with v6 machines. v7 is in
development. Timings are the same for v1..v5 and differ from v6.

## Repository layout

```
spec/isa/      ISA spec: bundle.md (wide instruction format), binutils-ops.yaml
               (every operation), registers.yaml (state and AAU registers)
spec/abi/      ABI spec for e2k64: data layout, registers + DWARF, calling
               convention, stacks and unwinding
spec/NOTICE.md attribution and licence of spec/
tools/setup/   install reference toolchains into toolchains/
tools/isa-extract/  generate spec/isa YAML from binutils; check it against gas
tools/isa-decode/   decoder written from spec/isa; corpus; compare with objdump
tools/abi/probes/   C probes for the ABI (layout.c must compile; callconv.c is
                    compiled to assembly and read)
OpenE2K/       submodules (see above)
toolchains/    untracked: installed toolchains, corpus builds, scratch output
x86_64/        untracked: Unipro debs
```

## Setup

```
git submodule update --init
tools/setup/setup-unipro.sh          # toolchains/unipro/bin/{clang,llc,...}
tools/setup/build-binutils.sh        # toolchains/binutils/bin/e2k-linux-*
tools/setup/fetch-lcc.sh             # toolchains/lcc-1.29.16-e2k-v5/{lcc,l++,ldis}
```

- lcc expects to live in `/opt/mcst/...`; the wrapper runs it under `bwrap`
  with the tree mounted there. No root needed. It compiles, assembles and
  links complete programs against its bundled sysroot.
- Unipro's tools need a `libxml2.so.2`; the setup script builds a stub.
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
toolchains/lcc-1.29.16-e2k-v5/lcc -c tools/abi/probes/layout.c -o /dev/null
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

### ABI (spec/abi) — written, checked against lcc and Unipro output, not yet run on hardware

Key facts: arguments in 8-byte slots, slots 0..7 in registers (callee
`%dr0..7`, caller `%db[0..7]`), 9–16-byte values on even slots, stack area
always reserved for all slots at the caller's `USD`; results ≤ 64 bytes in
registers, larger ones written to the caller's argument area (no hidden
pointer); variadic calls store every argument (fixed ones too) in memory;
unprototyped calls pass both ways (Unipro gets this wrong); callers extend
narrow integers to 64 bits and lcc callees re-extend; window units are
pairs of 64-bit registers; DWARF 0–63 = `%r0..63`, 64–127 = `%b[0..63]`;
exceptions use `.eh_frame` only for FDE/LSDA lookup (CFI ignored) and pass
data in DWARF 64/65 (`%b[0]`, `%b[1]`).

## Next steps (proposed, owner to choose)

1. **ABI compatibility test suite** (like GCC's `compat` tests): generated
   caller/callee pairs for every rule in `calling-convention.md`, one side
   built with lcc and the other with the compiler under test, linked with
   lcc, run on hardware or qemu-e2k user mode. Becomes the acceptance test
   for the owner's GCC port. Needs SSH access or a qemu-e2k build.
2. **ELF, relocations, TLS models, PIC sequences** spec (binutils
   `bfd/elfxx-e2k.c`, glibc `ld.so`, guide 9.1).
3. **"What a GCC port must decide"** document: register numbering and
   classes, fixed/scratch registers, window sizing, and which existing
   ports to study (SPARC for windows; ia64 for bundling, speculation,
   `sel-sched`). A map for the owner, not code.
4. Timing/scheduling data; APB; protected (`e2k128`) and 32-bit modes.

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
