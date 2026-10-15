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
- Nearly every Elbrus system ships MCST's lcc as the default compiler (OS
  Elbrus and ALT Linux), so tests may assume lcc on the target.
- The port targets 64-bit unprotected mode only. 32-bit (`-mptr32`) and
  protected (`-mptr128`, `e2k128`) modes are documented for completeness
  (spec/abi/mode32.md, protected-mode.md); Elbrus Linux ships their
  runtimes (`/lib32`, `/lib128`), ALT Linux doesn't (the 4C runs them
  static or with lcc's sysroot libraries).

## Repository layout

```
spec/isa/      ISA spec: operations-v3.md, operations-v5.md (what v4/v5 add:
               the 8C2), operations-v6.md (v6: the 2C3),
               operations-v7.md: every operation, what it does, latency, lcc's use,
               which probe ran it; generated from operations-v*.yaml,
               hwbugs.md (MCST's errata from the kernel source and our
               own low-confidence observations),
               bundle.md (wide instruction format), control-transfer.md
               (branches, calls, CTPRs), semantics.md (what operations
               compute), apb.md (array prefetch), speculation.md
               (speculative operations, diagnostic values, DAM),
               loops.md (loop mode, %lsr, rotation),
               x86-support.md (the x86 translator's facilities and
               what native code can take from them),
               predicates.md (everything about predicates: state,
               every operation that writes or reads one, encoding,
               limits per wide instruction, timing, lcc's use, how to
               use them),
               monitors.md (performance counters through perf),
               binutils-ops.yaml
               (every operation), registers.yaml (state and AAU registers),
               reservations.yaml (what each operation takes in a wide
               instruction per ISA version: channels, pairs, ALES,
               literals, control-operation syllables, bundling
               restrictions, divider issue distances; generated),
               lcc-usage.yaml (operation counts in lcc's system libraries)
spec/abi/      ABI spec for e2k64: data layout, registers + DWARF, calling
               convention, stacks and unwinding, ELF/relocations/PIC/TLS,
               atomics, source-level conventions (macros, asm
               constraints, modifiers and clobbers, options),
               kernel-interface.md (system calls, auxv, contexts,
               also shared between threads, signals and their frames,
               hardware and data stacks, overflow signals, address
               space, -pg, sanitizers, static PIE),
               mode32.md and protected-mode.md (lcc's 32-bit and
               protected modes, for completeness),
               lcc-builtins.md (lcc 1.31's builtins and <e2kintrin.h>:
               the spec for lcc-compatible builtins; lcc-builtins.yaml
               from lcc's manual, -names.yaml every name lcc knows,
               -check.yaml what each operation builtin compiles to,
               -misc.yaml the MAS/barrier/CPU/sysreg/header cases)
spec/NOTICE.md attribution and licence of spec/
doc/           plans for the owner: gcc-port-plan.md (how to write the GCC port)
docs/          spec-poc-findings.md: spec problems found by toolchains/gcc-claude
               (kept by the agent working there)
tools/setup/   install reference toolchains into toolchains/; e2k-ssh and
               e2k-scp (and sshkey.py for Python tools): ssh/scp to the
               Elbrus machines with relcc2's key only
tools/isa-extract/  generate spec/isa YAML from binutils; check it against gas;
                    count_lcc_ops.py (lcc-usage.yaml), gen_operations.py
                    (operations-v{3,5,6,7}.md), count_lcc_preds.py
                    (how lcc's libraries use predicates), list_pred_ops.py
                    (the operations that write or read predicates),
                    check_literals.py (what gas makes of each literal),
                    gen_reservations.py (reservations.yaml) and
                    check_reservations.py (checks it on compiled code,
                    and each wide instruction's size against gas's rule)
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
                    and every v6 operation (sem6.c; gen_sem6.py
                    writes sem6.s) and the v6 control operations
                    (ctl6.c; gen_ctl6.py writes ctl6.s) and store/load
                    values against erratum #123567 (stldv.c;
                    gen_stldv.py writes stldv.s)
                    and global register rotation (abg.c, abg.s),
                    and combined FP and integer operations
                    (fcomb.c, icomb.c; gen_fcomb.py and
                    gen_icomb.py write their .s files),
                    and byte-swapped accesses (mas_be.c,
                    mas_be.s, mas_be-v5.s, mas_be_loops.c),
                    and what lcc's speculative FP code does after
                    feclearexcept (specfp.c, specfp-kern.c)
                    and predicates (preds.c, gen_preds.py: timing,
                    logic, movep, calls, two writers in one
                    instruction, %spred, %pcnt, loop exits, false and
                    diagnostic conditions) and the random predicates
                    (rndpred.c, gen_rndpred.py; check_rndpred.py on the
                    host) and the recovery points of the x86 binary
                    translator (rp.c, rp.s: srp, slrp, crp, %rpr)
                    and the translator's facilities for native code
                    (native.c, native.s; gen_native.py writes
                    native-rot.s and native6.s: srp/crp cost,
                    __clear_cache cost, UPSR.fsm, %bgr rotation delays,
                    the v6 wait on memory)
                    and every v3 packed operation (semp.c;
                    gen_semp.py writes semp.s)
                    and square roots and reciprocals (sqrt.c,
                    gen_sqrt.py, check_sqrt.py on the host)
                    and MAS cache hints and kinds (mas_hint.c,
                    gen_mas_hint.py; mas_mod.c, mas_mod.s) and
                    store-to-load timing (stld.c, stld.s)
                    and the remaining v3-v5 user-mode operations:
                    scalar (semx.c, gen_semx.py, semx-sets.h;
                    check_semx.c on an x86 host; divx.c, divx.s),
                    80-bit (semfx.c, gen_semfx.py; check_semfx.c on
                    an x86 host), memory, tag and descriptor
                    (semc.c, gen_semc.py; stmqp.c, stmqp.s),
                    field extract (getf.c, getf.s), v1-v2 operations on
                    v3+ (v2ops.c, v2ops.s), control operations from user
                    mode (ctlops.c, ctlops.s),
                    and memory accesses: spill and fill of tags,
                    diagnostic addresses, misaligned 16-byte and
                    page-crossing accesses (memx.c; gen_memx.py writes
                    memx.s, memx-v5.s and their -cases.h),
                    and the FP environment: every ordinary FP
                    operation under each rounding mode and fz/daz
                    against SSE (fpenv.c; gen_fpenv.py writes fpenv.s,
                    fpenv-v6.s, fpenv-ops.h; check_fpenv.c on an x86
                    host), glibc's fenv functions (fenvlib.c),
                    denormal costs (fpcost.c, gen_fpcost.py),
                    and software prefetch, cache fill levels and line
                    sizes (pf.c, gen_pf.py; pf-lcc.c for lcc's loops),
                    and tag operations and the last unchecked ALU
                    operations (tagx.c, gen_tagx.py, check_tagx.c), the
                    user state registers and 80-bit status bits
                    (sregs.c, sregs.s, check_sregs.c), AAU and v6 MAS
                    leftovers, UPSR.ac and wait fields (memleft.c,
                    gen_memleft.py), control leftovers: vfrpsz, ipd and
                    pref, ctcond type 9, v6 qpcheck/abgc/fill
                    (ctlleft.c, gen_ctlleft.py), aaurw operands and
                    not-taken ct after return (aaurwct.c,
                    gen_aaurwct.py), and the ISA leftovers of the audit
                    (isaleft.c; gen_isaleft.py: ,sm loads under a
                    diagnostic predicate, predicate tags across calls,
                    UPSR.ac and APB, movaqp against fmt and mrng,
                    %aabfN, %lsr.ldovl, 128-bit registers across a
                    spill, APB programs in sequence), how soon
                    loop-register writes take effect (lsrw.c,
                    gen_lsrw.py) and lcc's APB loop entry (bapent.py,
                    bapent.c),
                    and rechecks of the PoC's findings (pocchk.c;
                    gen_pocchk.py writes pocchk.s and pocchk-cases.h:
                    divide latency by quotient, two writes of one
                    register, false-predicate interlocks, dbl and
                    combined ops, misaligned costs, %g across signals;
                    pocchk2.c, gen_pocchk2.py: fmul_addd operands, loads
                    never read, window and call costs, the wbs rule,
                    atomic spacing, store pairs, APB disp and backward
                    areas, stores of FP and integer results across
                    clusters; pocchk3.c, gen_pocchk3.py: APB halves, the
                    v3 mrng limit, loads beside areas; pocchk4.c,
                    gen_pocchk4.py: flag-form latencies and dbl = 0
                    reads; pocchk5.c, gen_pocchk5.py: rounding, sqrt,
                    flag ops, fcmp latency; damwb.c, gen_damwb.py: a
                    DAM lock and check together, write-back conflicts;
                    stbase.c, gen_stbase.py: I35's daxpy kernels,
                    write-back conflicts as written; damlist.c,
                    gen_damlist.py: I45's list reversal, store-then-load
                    loops; pocchk6.c, gen_pocchk6.py: array prefetch
                    against %lsr states, lcc's APB alignment),
                    the APB leftovers (apbleft.c, gen_apbleft.py: narrow
                    and misaligned AAU accesses and their costs, monitor
                    counts of APB's slowdowns), every other monitor
                    event and the uncore PMUs (pmu2.c, gen_pmu2.py), the
                    timing leftovers (timeleft.c, gen_timeleft.py: store
                    data, narrow 80-bit operands, divider pairs and
                    runs, byte throughput, store banks, store/load
                    aliasing, fetch after a taken transfer, v6 lock MAS
                    and icall; timeleftv.c, gen_timeleftv.py recheck
                    them; their generated files are not kept in git:
                    run the generators first), and calls under a false
                    predicate (callsstack_pcall.c/.s: their cost and
                    chain-stack spill), run on hardware; -DSPEC_V5, -DLOOPS_V5, -DAPB_V5
                    builds add v5 cases
tools/abi/probes/   C probes for the ABI (layout.c must compile; callconv.c is
                    compiled to assembly and read, as are atomics.c,
                    asmops.c and stackprot.c; tagtrap.c, dbl.c,
                    ldouble.c, ctpr.c, dso/run.sh and elfops/run.sh run
                    on hardware, as do litmus.c, getsp.c and
                    reenter.c with their .s files, and litmus_rmw.c;
                    sysabi.c/.s (the kernel interface: FP state across
                    handlers, alternate stacks, frame placement, stack
                    exhaustion and growth, auxv, maps, %clkr, UPSR.ac,
                    system call costs), sigeh.cc with sigeh-c.c
                    (exceptions and cancellation across signal frames),
                    pgprof.c (-pg with gprof) and lockloss.c
                    (gen_lockloss.py: what breaks an atomic's lock),
                    ctxsig.c/.s (contexts, edited signal frames, globals
                    across system calls), ctlcost.cc (what longjmp,
                    exceptions and backtraces cost), hwscan.c/.s (GC
                    root scanning), stackregs.c/.s (the stack registers),
                    tagempty.c/.s (registers a window gains),
                    abileft.c/.s (signals in made contexts, switching
                    from handlers, data-stack overflow) and
                    abileft-san.c (lcc's sanitizers), ctxthr.c
                    (contexts shared between threads, fork, thread
                    exits), accerr.c with getsp.s (si_code and si_addr
                    of stack overflows and bad accesses), kernleft.c/.s
                    (system-call entries, edited signal frames and
                    %bgr, cross-thread context switches), and the PoC
                    rechecks pocabi1_va.cc, pocabi1_valist.cc,
                    pocabi1_fm.c, pocabi1_mp.c/.s (message passing),
                    pocabi1_a16run.c with pocabi1_tear.s, pocabi2.c/.s
                    (a call's wbs bounds; extended parts across
                    signals) and pocplan_tail.c (lcc's tail calls),
                    callsstack_tail.c/.s with callsstack_tailx.cc (tail
                    calls that release the frame, getsp rounding,
                    predicated movtd) and callsstack_clash.c/.s
                    (stack-clash probing) run on hardware; ctxtls.c
                    (TLS across a context switch),
                    pocabi1_atom16.c, pocabi1_ord.c, pocabi1_nfx.c,
                    pocabi2_nfx.c and pocplan.c are read with -S;
                    mode32/ (32-bit mode: layout32.c must compile with
                    -mptr32, callconv32.c is read with -S, the rest,
                    lat32.c with gen_lat32.py, asan32.c, atom32.c and
                    dbg32.* included, runs on hardware through run.sh,
                    with --ship-libs on the 4C) and protected/
                    (protected mode: pmcc.c and pm6464.c read with -S,
                    pmrun.c, pmasm.c and pmcxx.cc run by run.sh, and
                    pmleft.c, pmomp.c, pmmin.s and the pmlib*/pmdl.c
                    library probes by run.sh --left, static on the
                    4C); execstack.sh (GNU-stack census) and
                    prefrel.sh (R_E2K_PREF) link and read; sanitize.c
                    is read with -S);
                    find_builtins.py, gen_builtins.py and
                    gen_builtins_misc.py ask lcc 1.31.05 about its builtins;
                    asmconv.py asks lcc about inline-asm constraints,
                    modifiers, clobbers and %#, prefetch.py what
                    __builtin_prefetch becomes for each target
tools/abi/builtins/ lcc-compatible builtins test: gen.py writes ops.c
                    (a result hash per operation builtin), models.c checks
                    the rest against reference models, run.py compares a
                    compiler with lcc on hardware and reports the
                    builtins it lacks as missing
tools/abi/compat/   ABI compatibility suite: gen.py writes caller/callee
                    pairs, run.py builds every compiler pair (with several
                    --target values, pairs of compilers for different
                    targets, linked and run for the last one) and runs them
                    under qemu-e2k or, with --remote, on hardware over SSH
                    (removing the programs afterwards unless
                    --keep-remote); known.txt lists known differences
tools/gcc-native/   native GCC builds on an Elbrus machine (draft): gcc-native.sh
                    pushes a GCC commit (default: the PoC worktree
                    toolchains/gcc-claude-native) to the 8C2, bootstraps
                    it there (stage 1 by lcc, then by itself), runs
                    check-gcc there and times lcc-built against
                    self-built cc1plus; remote.sh is its machine side
tools/dejagnu/      DejaGnu boards for GCC's testsuite: e2k-qemu (MCST
                    qemu-e2k, any -cpu), e2k-4c, e2k-8c2, e2k-2c3 (address
                    from the environment), e2k-ssh; run-gcc-tests.sh runs
                    GCC's testsuite with them (build tree or installed
                    compiler, -j N either way; board variants pass their
                    flags; remote_spawn runs the remote's gdb for
                    guality); smoke/ and run-smoke.sh check a board, also
                    with -mptr32 and -mptr128 programs
OpenE2K/       submodules (see above)
toolchains/    untracked: installed toolchains, corpus builds, scratch output;
               gcc/ (GCC mainline, blobless clone) and gcc-tilegx/ (worktree
               at 54a5f478487, the last commit with TILE-Gx) as references;
               gcc-claude/ (experimental agent-written E2K backend, see
               decision 2) and gcc-claude-native/ (a worktree of it on
               branch native-build, for tools/gcc-native, so native
               builds never touch the PoC agent's checkout)
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
- Still [source] only: `vfbg`, `puttsd`, `%bgrpred` (gas doesn't accept them), v7 `dtbp`
  and ctcond type 11, `staa`/`ldaa`/`incr` decoding (hidden by an objdump
  bug; encoding is checked).
- Timing measured on the 4C (v3) by `tools/isa-timing/bench.py`
  (`spec/isa/timing.md`, data in `timing-e2s.yaml`, with `measured_on`):
  scheduled latencies match the guide (int 1, FP/mul 4, L1 load 3,
  `disp`→`ct` 5, ...); crossing between the clusters ALC0–2 and ALC3–5
  costs +1 for integer/load results and +2 for FP ones (the guide's
  int→FP +1 is this crossing; a store takes its data like an FP operand:
  FP data +2 only across clusters, integer data no crossing cost); a stall costs about a
  whole latency extra (exactly: an operand e cycles early stalls
  4 × ceil(e/4)); an FP-side result routed to an integer op is a cycle
  late when an integer op or load issues in its channel 2 cycles after
  it (write-back conflict); everything user code touches is interlocked except
  `rwd %lsr` → `rrd %lsr` (4 cycles). Also measured: packed integer ops
  (1 cycle, 4 to integer consumers; the guide says FP class), conversions
  (4; an FP-side result read in another format, e.g. a double as a
  single, costs 5–6), FP compares (+2 over integer ones), predicate logic (as the guide;
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
  x86-style, `fmul_add` rounds twice, combined operations `X_Y a, b, c` are two ordinary operations chained, `Y(c, X(a, b))` (encoded as the first one's opcode plus the second's; `_sub` is c − (a X b), `_rsub` (a X b) − c), MAS bit 3 makes any aligned load or store byte-swapped at no cost (misaligned: SIGBUS; lcc folds `bswap` into it only under `-ffast`; it saves 5 cycles on dependent load paths, nothing in pipelined loops), `fcmpud*f` is the quiet FP compare into a register (x86 ZF/PF/CF layout; MCST's qemu wrongly raises invalid), `cmpand*` tests `a & b`, `movtd`/`getpl` to a CTPR only in ALC0, a taken `ibranch` hides a stall in its own instruction; every other v3 user-mode ALU operation has a checked model (`semx.c`, `semfx.c`, `semc.c`: x86 SSE and x87 references match every line on the 4C and 8C2): `smulx`/`umulx` 32×32→64, `insfs`/`insfd` field insert, `getf` field extract (rotate, keep *len* bits, zero/sign-extend; v1–v4 take the sign bit from a BYTE field of the descriptor, v5 from the field's top; `getf.c` exhaustive on both), 64÷32 `sdivx`/`udivx`/`smodx`/`umodx` (SIGFPE on a quotient that doesn't fit; the remainders by a leading-zero shift rule), `fdtoifd`/`fstoifs` = `roundsd`, `fscale*` (v4) = `ldexp`, the `*_f{b,h,w,d}` forms return x86 EFLAGS and `cctop*` test them, the 80-bit `fx*` operations are exactly x87 code (an `fxadddd` rounds to 64 bits, then to double), the protected-mode, segment and privileged loads and stores fault, `stmqp` (v5) is a byte-masked store that needs 16-byte alignment; memory accesses to one address in one wide instruction take effect ALC2 before ALC5 and a load before a store in its cluster (a load in ALC5 sees an ALC2 store; the ALC5 store of two wins; bundle.md). Speculation
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
  (SIGILL at the use instead of SIGFPE); a `,sm` store under a
  diagnostic predicate is still done, with bit 30 of each data word set,
  and a diagnostic store address traps even with `,sm`; memory keeps a
  tag per 32-bit word and only the fill MAS (mod 4 without `,sm`, lcc's
  spill reload) reads it back, every other load drops it (`memx.c`); an
  `ld,sm` crossing into a bad page is wholly diagnostic, a crossing store
  writes nothing, `si_addr` is the page boundary; misaligned
  `ldqp`/`stqp` are SIGBUS on v5, fine on v6; lcc marks most
  FP operations `,sm` (all of them in pipelined loops, divisions too)
  and stores whose data or predicate are speculative, so lcc `-O3` code
  dies with SIGILL after `feclearexcept` unless built with
  `-fno-spec-fp` (`specfp.c`). Performance monitors
  (`spec/isa/monitors.md`, hw): perf's raw `cpu` events (`0xMMEE`,
  event lists in the kernel's `tools/perf/pmu-events/arch/e2k`), two
  instruction and two data monitors on v3, user-mode counting allowed
  on the 4C, `%dimar0/1` readable with `rrd` in user mode; `EXEC`
  counts wide instructions, `TICKS` cycles, `BUB_E2` 4-cycle stall
  steps, `CONF_ALUn_OUT` write-back conflicts, plus DAM, L1 and spill
  events; sampling is throttled and breakpoints miss hits, so neither
  gives exact per-address counts. Loop mode (`spec/isa/loops.md`, hw):
  a loop with `alc` and `ct ? %NOT_LOOP_END` runs `lcnt + ecnt`
  iterations (`vlc` = 0: endless), `rwd %lsr` (and `%lsr1`) needs 4
  cycles before the loop (iterations issued sooner count down the old
  `%lsr`) and also sets `%ilcr`, whose reads lag 5 cycles; `%lsr` then
  `%lsr1` may be consecutive; `%lsr.ldovl` limits how far ahead the AAU
  fetches (write 255, as lcc does); `%pcntN` is
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
  cycle at full width (6 integer, 4 FP, 4 loads, 2 stores; 64-bit
  packed FP likewise, v4+ also putting the packed adds, `pfmuls` and the
  combined packed forms in ALC2/5), except the
  divider in ALC5 shared by all divides (latency 11–18 by the
  quotient's size; 1 cycle apart stalls 4,
  `sdivd` wants 3, `fsqrts` also conflicts at 5) and L1 banks (two
  stores agreeing in address bits 6–7, or a store and a load in one
  32-byte block, can take 2 cycles, but only when instructions store
  to the same addresses again: in a stream they cost nothing). The Elbrus-8C2 (v5,
  `timing-e8c2.yaml`, timing.md "Elbrus-8C2") has every v3 latency,
  hazard and control delay except `pfdivd`/`pfsqrttd` (16, v3 14) and an
  FP result reaching an 80-bit operation's single or double operand
  (2 cycles later; a same-cluster load 1); v5 adds 6 FP ops per cycle (ALC2/5),
  128-bit `qp*` ops with scalar latencies; `ldqp` bypasses L1 (served
  by L2: latency 11, one per cycle; `ldq` into a pair goes through L1,
  latency 3; lcc loads 128-bit data as two `ldd`); an APB area delivers
  16 bytes a cycle (also v3); `%aaind` and `%aaincr` are 64-bit, `fmt`
  7 is SIGILL; all 195 v5 ALU operations have measured latencies (1, 2,
  4, 5, 6 or 8 by class, as their 64-bit counterparts) and are fully
  pipelined; FP min/max is 2 cycles (also v3); a 64-bit FP-side result
  feeding a `qp` operation pays the crossing cost; L2 11, L3 about 32,
  memory about 180 cycles. isa-score models these
  and is within 2% on the 8C2's kernels and 1.00 on CoreMark
  (`validate.py --target v5.8c2 --period 30011`; until 2026-10-12
  `sum -O2` read 0.92 because perf's TICKS samples lock to a short call
  repeated back to back: validate now puts a random delay between
  calls; monitors.md). Other v5 hardware results: `%lsr1` is the 64-bit loop
  count (write it after `%lsr`); misaligned APB areas give data; the
  compat suite passes for every pair (the registers a window gains are
  empty: 0 with tag 5 on the 8C2 and 2C3, stale bits with tag 5 on the
  4C; `tagempty.c`); Elbrus
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
  traps only in ALC4/5 and non-speculatively (the same on the 4C).
  The Elbrus-2C3 (v6; timing.md "Elbrus-2C3 (v6)", semantics.md "v6
  operations"): every v3–v5 operation computes the same (about 195,000
  probe lines identical to the 8C2's) and every v3/v5 latency, crossing,
  stall rule and CTPR delay holds, except: **L1 loads 5 cycles** in
  either cluster, `ldqp` through L1 (5, four a cycle), integer compare /
  predicate logic → predicated op 1, no divider–ALC5 conflict (a
  store and a load in one 32-byte block still take 1 or 2 cycles by
  run, as on the 4C), `fxsqrtu` 18; L2 13 cycles (2 MiB, no L3),
  memory ~220; APB areas deliver 32 bytes a cycle and keep their
  positions across traps (lcc's v6 APB loops enter 17 cycles after
  `bap` but need 19 with one area, 21 with two); a compare before a
  taken return is seen. The v6 operations
  (`sem6.c`): fused `fma*` (5 cycles, rounded once), `clmul` 4, packed
  compares into a predicate (any/all lane, 4 / 5 to `ct`), `qpsrad`,
  `qpcext`, `pmrgp`, `qpcheck*` (SIGBUS unless equal), x87 helpers
  `mkfsw`/`modbgv`/`vfbgv`; the community qemu gets the packed FMAs
  wrong. Control (`ctl6.c`): a taken `ibranchd` costs 12 cycles
  however early its register is ready, `icalld` + return 23 (`disp` +
  `call` 14), a `disp`/`movtd` and `ct` through one CTPR in one
  instruction falls through, `abgc`/`rbgc` only move `%bgr.cur` (the
  x87 stack checks are `vfbgv`). `pmu.c`, `profile.py` (CoreMark 1.03,
  Linpack 0.99) and the compat suite run there. APB (apb.md, "v6"):
  positions survive traps in any loop shape, so the v3–v5 one-advance
  rule is gone, and a `mova` wider than its area's format reads
  unaligned data (odd strides). MAS (speculation.md, "v6 encodings",
  `mas6.c`): acquire/release forms 0x13/0x53 run (acquire load 10.4
  cycles in a chain, release store ~4 against 15.8 for `wait` + store),
  0x12 `,sm` is the fully speculative load, 0x11/0x13 `,sm` the lock
  loads. lcc's v6 code uses `fmad` only under `-ffast-math`. v4 is not
  measured; v7 is out of scope (no silicon).

  Every v3 operation's latency (`bench.py --only v3op`, 807 chains,
  timing.md "Every v3 operation"; the three machines agree except as
  noted): every FP-side result reaches an integer op 2 cycles later, the
  divider's integer results included (integer divides 11 to FP, 13 to
  integer; `smodx`/`umodx` 10/12); a double `sqrt` is `fsqrtid` 11 +
  `fsqrttd` 14; `frcps`/`frsqrts` 6; 80-bit add/sub/conversions 5, `movx`
  4; reading `%pfpfr`/`%fpcr`/`%fpsr` delivers in 11, `%upsr`/`%ip` in 2;
  `wait` costs 6–7 cycles with nothing outstanding. FP environment
  (semantics.md, `fpenv.c` against SSE on an x86 host): ordinary FP is
  SSE with MXCSR bit for bit, flags included, in all rounding modes and
  with `%pfpfr`'s `fz` (bit 15) and `daz` (bit 6), the 2C3's fused
  `fma*` aside in one NaN case; a denormal operand or tiny result makes
  a multiply 5 cycles, nothing else is slower; lcc links MCST's
  `crtfastmath.o` (fz + daz) for `-ffast-math`. Caches and prefetch
  (timing.md "Memory", `pf.c`): 32-byte L1 and 64-byte L2/L3 lines, no
  hardware data prefetcher on the three; a prefetch with MAS 0 fills L1,
  0x20 (lcc's) only L2, 0x40 only the 8C2's L3, 0x60 (lcc's locality 0)
  nothing, which makes a stream 3–5× slower.

  Predicates (`spec/isa/predicates.md`; `preds.c`, `rndpred.c` on all
  three machines, identical except the v6 return fix): at most 4
  compares (ALC0, 1, 3, 4), 4 ELPs and 3 CLP/MLPs (two levels) and 6
  RLP/MRGC entries (one per cluster and predicate, negation per
  channel) per wide instruction; a callee starts with a copy of the
  caller's predicates and the caller gets its own back, so they are
  callee-saved for free; an operation under a false predicate does
  nothing (no memory access, no fault, no operand check, no
  interlock); readers in the writer's own instruction see the old
  value; two compares writing one predicate leave the higher channel's,
  a CLP beats a compare and the higher CLP another; `movep c, v` writes v only when c (the guide
  has the operands reversed); `%pcnt` and `%pred` on one operation are
  anded; ctcond 6/7/14/15 ignore the predicate while `pcnt` ≠ 0 unless
  `semc`; a diagnostic predicate traps non-`,sm` operations and makes
  `,sm` results diagnostic, either polarity. `%spredK` makes the
  instruction issue without waiting for ALC K's operands (the
  operation then sees a diagnostic operand, and `%spred` is true):
  6 against 10 cycles a load-use step. `%rndpred1`–`15` read two LFSRs
  in the user-writable `%rndpr` (x¹⁷+x¹⁴+1, x¹⁸+x¹¹+1), stepped once
  per instruction that reads them, true 2⁻² … 2⁻²⁴ of the time; the
  model in `check_rndpred.py` reproduces every read. lcc uses
  predicates in 47% of its wide instructions (`landp` trees, opposite
  predicates for both arms, a conditional `ct` per path) and relies on
  the higher channel winning when two enabled writes hit one register.

  Recovery points (speculation.md, `rp.c`): `srp`/`slrp`/`crp` and
  `%rpr` serve MCST's x86 binary translator: `srp` writes its own
  address and BR to `%rpr` (user-readable and writable) and starts
  "generations mode", in which a lock-wait load is SIGILL on v3/v5 and
  a lock-wait load, a fault or an instruction-cache fill is the
  recovery-point exception (SIGBUS) on v6; memory is not rolled back;
  the kernel returns a trap inside a registered region to the
  translator's recovery IP. Nothing for a compiler.

  Leftovers measured on 2026-10-12 (all three machines unless noted):
  a 32-bit half with tag 1 is **diagnostic only with bit 30 set**, else
  empty (a plain use of an empty value doesn't trap); `puttagd`/`puttags`
  refuse to make a diagnostic or pointer tag (SIGILL `ILL_PRVOPC`), their
  `,sm` forms make a diagnostic value, and lcc's `puttagd` only ever
  clears tags; `fxdivt<a><r>` is the single/double src1 ÷ the 80-bit src2
  (semfx.c had them swapped), 16 cycles (8C2 18 through src1); FP traps
  are precise at the operation (`%pfpfr` masks for ordinary ops, `%fpcr`
  for 80-bit), `%pfpfr` bits 26–31 do nothing, only divides and the
  square root write `%fpsr.c1`; `rrs` of a 32-bit register keeps the
  upper half under `dbl = 0`, `%idr` gives the Linux CPU (= sched_getcpu,
  2 cycles), `rrd %clkr` waits for nothing; under `dbl = 0` a 64-bit read
  of a 32-bit result waits 4; AAU loads take 9/11/13 cycles (v3/v5/v6);
  `UPSR.ac` traps every misaligned access, stores included; `wait`
  fields beyond bit 8 (v6/v7) and their costs, `wait trap`; ctcond type
  9 (`%MLOCK || %cmpN`) runs on v3–v6 and can end a test-and-set loop;
  `vfrpsz` checks the handed-over pairs (a system call takes 1 to 4);
  `ipd` and `pref` matter only for cold code; v6 `qpcheck` delays
  nothing, `abgc` acts on the next instruction (timing.md, semantics.md,
  speculation.md, control-transfer.md, atomics.md, x86-support.md).

  The translator's facilities for native code (x86-support.md,
  `native.c`, `pocchk4.c`): every x86 flag operation takes 2 cycles in
  its cluster and 4 across (32×32 multiplies 6 to an integer op, also
  into `cctop`) and `cctop*` reaches a predicated op in 2 (v6 1) and
  `ct` in 3, so an overflow test is two operations and 4 cycles; `srp`/`crp` cost
  nothing; libgcc's `__clear_cache` (MAS 0xf) costs about 140 cycles
  for a line and 7 a line in bulk; `UPSR.fsm` makes predicate FP
  compares read unordered as x86 flags (`lt`/`le`/`eq` true); v3–v5
  rename `%g24`–`%g31` the instruction after `abgd`/`abgi` and 10
  cycles after `rws %bgr`, v6 doesn't rename them at all; the v6
  watch-for-modification `wait` works in user mode, wakes as fast as
  spinning and on every interrupt.

  Measured on 2026-10-13 (all three machines unless noted; timing.md,
  apb.md, monitors.md, control-transfer.md, speculation.md, bundle.md):
  write-back conflicts are decided on the schedule as written (`nop`s
  count; I35: the PoC's daxpy stalled every other iteration, and which
  base instance a store reads costs nothing); a load that a store just
  kept out of L1 is served by L2 (value at 9/11/13 cycles): the stored
  doubleword 1 cycle later on every version, on the 8C2 any load of the
  stored 32-byte line 2–3 cycles later that reads unwritten bytes (I45:
  CoreMark's list reversal; lcc's v5 code pays it too), on the 2C3 the
  stored doubleword at another width up to 4 cycles later, and a load
  16 KiB × k away for up to 2 empty cycles (2C3 3); a store under a
  false predicate sends nothing; an early predicate read stalls in
  2-cycle steps on v3/v5 (4 on v6); the store bank is address bits 6–7
  only; divider pairs conflict only 1 apart, except after
  `smodx`/`umodx` and single-precision roots (by operand and gap
  layout), and runs don't follow from pairs; a callee whose `setwd`
  grows the window waits for every outstanding load (68 against 27
  cycles on the 4C); a store in a DAM check's instruction with the
  same low 12 bits fails the check; the DTLB holds 768 4 KiB pages (a
  miss 15/17/45 cycles), the ITLB 256–383; a prepare fetches `ipd`
  256-byte lines and nothing when its page misses the ITLB; a MAS 0x60
  prefetch reads nothing (lcc's locality-0 loops are slow for their
  shape); `icall` costs 6 cycles more than a prepared call (2C3); AAU
  loads take any MAS, misaligned AAU loads and stores work on every
  version (v5 `ldaaqp`/`staaqp` need 16-byte alignment); a column walk
  with 4 KiB pages is a table walk an element, and L2-bound loads beside
  a fetching area stall (`STALL_AAU`); on v3–v5 epilogue `mova`s need
  `ldmc`; lcc 1.31 versions long v3/v4 APB loops on alignment
  (`-faligned` makes NPB CG 2.0× faster on the 4C); a wide
  instruction's size follows gas's rule (bundle.md, "Counting the
  size"). `pmu2.c` covers the remaining monitor events; the uncore PMUs
  need `perf_event_paranoid` ≤ 0.

### Hardware errata (spec/isa/hwbugs.md)

MCST's errata as its kernel lists them (`cpu_features.h`), with which of
our machines each covers (4C = E2S rev 2, 8C2 = E8C2 rev 3, 2C3 = E2C3
rev 1), and our own anomalies, marked low confidence (they may be our
bugs). The ones that matter for code: `CODE_PLACEMENT` (all three; gas
works around it), the v6 MAS store-release being broken on E2C3 ≤ rev 2
(use `wait` + store, as lcc and the kernel do), `ibranchd`/`rbranch`
corrupting an inactive CTPR on all v6 CPUs, and #123567 (a load right
after an intersecting store, 8C2), which `stldv.c` did not reproduce.
Our own (low confidence): on the 2C3 a `mova` outside `loop_mode` is
SIGILL while `%lsr` says a counted loop has ended (`vlc` 1, `lcnt` 0,
`ldmc` 0: after any finished counted loop, glibc's `strncmp` included;
deterministic; perhaps v6 behaviour rather than a bug); the 4C's kernel
moves `%g24`–`%g31` by `%bgr.cur` places across a signal taken under a
nonzero `%bgr.cur`.

### Operation reference (spec/isa/operations-v3.md, -v5.md, -v6.md, -v7.md)

Split by our hardware: `operations-v3.yaml` has everything up to v3
(the 4C), `operations-v5.yaml` what v4 and v5 add (the 8C2; its header
also covers v5's behaviour changes, lcc's v5 idioms and v3
equivalents), `operations-v6.yaml` and `operations-v7.yaml` what v6
(the 2C3) and v7 (no hardware) add. Every
operation from v2 on (2,149 after leaving out the v1-only ones: no
working Elbrus-3M exists) has a rule in the file of the version that
introduced it (the generator checks this): what it
computes, how a compiler uses it, where the details are;
`gen_operations.py` joins the rules with binutils' ISA versions and
channels, the measured latencies, lcc's use (`count_lcc_ops.py` counts
mnemonics in lcc 1.29.16's v3 and v5 sysroot libraries: about 360
operations) and the probes whose assembly runs each operation, and
fails if an operation has no rule or two. Every v3–v5 user-mode
operation has run on hardware except `mmurr` (monitors.md), and every v6 ALU operation but `ibranchd`/`icalld` ran
on the 2C3; v7 ones come from the community qemu or are known by
name only. The v1–v2-only operations are SIGILL on v3 and v5 (except two
reused encodings); `setsft` is a software trap (SIGTRAP).

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
possible/achieved score does not. `validate/` (under two minutes)
puts every kernel within 1% on the 4C and 2% on the 8C2 (`k_empty`, a
few cycles of call, aside); the model also knows the `dbl = 0` 64-bit
read wait (which explains gcc-claude's `matrix_mul_matrix` step), the
8C2's late narrow 80-bit operand, AAU load latencies, `aaurw` taking its
operand like store data and a not-taken `ct` after `return`; on the benchmarks the
model is within 6% once `profile.py` measures which transfers were
taken (a second sampled run, EXEC − `NO_CT_TAKEN`): fetch bubbles are
taken `ibranch`es (6 cycles) and taken `ct`s waiting for a late
prepare, nothing else (control-transfer.md, "Instruction fetch"). v6
uses the 2C3's latencies (`validate.py --target v6.2c3`: within 1%
except lcc's APB loops, 1.02–1.09: they enter 17–19 cycles after `bap`
and the first `mova`s stall, which the model doesn't carry into the
loop block); v7 uses v6's. Since 2026-10-13 the model decides
write-back conflicts on the schedule as written, charges the store-bank
cycle only for repeated addresses, times loops as the mean of 6 of 10
simulated iterations, and models the store-to-load cases by ISA version
(I35, I45).

### ABI (spec/abi) — written, checked by the compat suite under qemu-e2k and on the 4C (v3)

(Also on the 8C2 and the 2C3: the compat suite passes on both, and the
hardware probes `tagtrap`, `dbl`, `ldouble`, `ctpr`, `getsp`, `reenter`,
`winsize` and `litmus` print the same on the 2C3 as on the 8C2; the
compat run there pairs lcc's 2C3 code with Unipro's `-march=elbrus-v6`.)

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
allocations); after a signal handler the kernel restores `%g16`–`%g31`
(only those; the guide says all globals; registers.md) and only to code
interrupted outside a system call, while a system call through entry 3
returns them empty (0, tag 5), so they can't hold values across one
(kernel-interface.md; a port's `"call"` clobber must cover them); a `jmp_buf`'s IP (byte 8) can be replaced by a label to
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
`ldqp`/`stqp` on v5+; mixing them on one object breaks. What breaks
the lock (`lockloss.c`, all three): any load or store by the thread, to
any address, and any system call between the lock-wait load and the
store (always), an interrupt (about one window in 10^6); `%MLOCK` is
valid only in the store's instruction; a lock left open makes the next
lock-wait load SIGBUS (so compare-exchange stores on both outcomes);
other cores are held off a 64-byte block for 1,024–1,536 cycles; a CAS
loop against fetch-adds nearly starves on one 4C chip; misaligned
lock-wait is SIGBUS. Memory ordering (atomics.md): `litmus.c`'s
setup shows only a load passing an earlier store (which `wait st_c`
stops) and two stores in one instruction becoming visible in either
order, but when the reader already holds the lines, message passing
fails on all three machines (up to 42% of tries with plain accesses;
`pocabi1_mp.c`): a release needs lcc's `wait` and an acquire lcc's use
of the loaded value; with both, nothing reordered. lcc's 16-byte
atomics on v5/v6 are in atomics.md (exchange is its own lock loop, a v6
relaxed load is one `ldqp`, v5's two `ldd` can tear). Source-level
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
`va_arg(__int128)` at odd slots, and timeouts of Unipro `-O0` callees
under qemu only: they spill a partly written slot whose upper half is
qemu's empty value, and qemu loops forever on that store (hardware
stores it without trapping; a real tag trap needs a diagnostic half,
`tools/abi/probes/tagtrap.c`). The `valist` unit showed that
`va_arg` must align 16-byte values by address, because lcc's `static`
variadic functions have an 8-byte-aligned slot area; probes made while
studying it showed that lcc callers store only the last fixed and the
variadic arguments.
C++ exceptions interoperate; the runtime aligns exception objects to 16
bytes only. Mixed targets (`run.py --target v3,v5.8c2`, `v3,v6.2c3`,
`v3,v3.4c`) pass on the 8C2, 2C3 and 4C with only Unipro's known
differences: the calling convention doesn't depend on the target. ld
intersects the CPUs its inputs run on and every file of a CPU tree's
sysroot is marked for that CPU, so only the `e2k-v3` sysroot gives
forward compatible programs; ld.so refuses libraries for another CPU,
and MCST's qemu ignores `e_flags` (elf.md, "Mixing targets in one
program"). With a GCC driver (the experimental GCC at
`-march=elbrus-v3/-v5/-v6`, 2026-10-13) every pair with lcc's v3,
v5.8c2 and v6.2c3 code passes all 1,931 tests on the three machines.

Signals, threads and sanitizers (kernel-interface.md, hw on all three):
a handler runs on the stacks of the made context it interrupts
(`SA_ONSTACK` moves only the data frame); handlers can `swapcontext`/
`setcontext` freely, each context keeping its mask and its own
`[signal stack]`; a thread's `getsp` is accepted into any mapping below
its stack, and glibc stacks sit right above one, so only the one-page
guard catches an overflow (a large frame filled upward writes the
mapping below first; lcc emits no stack-clash probes); lcc's ASan and
LSan work on all three machines, MSan not on the 2C3's lower layout,
and LSan reports blocks held only in window registers as leaks. With
`nfx = 1` the high half of a v5+ 128-bit register is lost when its
window spills (a signal handler included): lcc uses `nfx = 0` in every
function that uses one (registers.md).

Kernel interface (`kernel-interface.md`, `sysabi.c`, hw on all three):
system calls are `sdisp` entry 3 with 4 pairs handed over (`%#` =
`wsz − 4`), fast calls entry 6, no vDSO, `AT_HWCAP` 0; the FP control
registers are **not saved or restored around signal handlers** (the
handler inherits the interrupted state and leaks its changes back); the
signal frame is 4,272 bytes right below `USD` (no red zone), an
alternate stack needs about 4,700 bytes (glibc's `MINSIGSTKSZ` 2,048 is
refused); running out of a hardware stack is SIGKILL, never a catchable
SIGSEGV; the first deep recursion in the main thread is quadratic (the
kernel grows the data stack by each `getsp` and rewrites every chain
frame's `ussz`: 49 s for 250,000 levels on the 4C; threads and a
pre-grown stack are fast); a new thread's hardware stacks start at its
creator's current size; `%clkr` is per core (unsynchronised on the 4C and
2C3); glibc's fast `clock_gettime` costs 1,200–1,400 cycles on the 4C
and 8C2 (the `lt` clock source), 234 on the 2C3; the 2C3's kernel uses
the lower address layout; lcc's `-pie`/`-static-pie` don't imply
`-fPIE`. With lcc's runtime an exception or a forced unwind cannot cross
a signal frame (throwing from a handler and cancelling a C++ thread
blocked in a system call kill the process; backtraces through handlers
work), so the port needs no `linux-unwind.h`
(`stack-and-unwinding.md`, "Signal frames, cancellation"). lcc's inline
asm (`source-compat.md`, `asmconv.py`): `%#`, `%=`, `%c`, `%n`, `%H`
modifiers, `"call"`/`"b[N]"`/`"ctprN"`/`"predN"` clobbers, constants
printed unsigned at the C type's width.

Second half of the 2026-10-12 audit (kernel-interface.md,
stack-and-unwinding.md, elf.md; hw on all three): `makecontext_e2k`
passes every argument, a switch costs about 3,200 cycles round trip,
contexts keep their own FP state and signal mask, `freecontext_e2k`
fails only for the running context, and lcc treats any function named
`setcontext` as never returning; every `longjmp` is a system call
(about 3,000 cycles + 130–175 a frame); a C++ throw costs 26,000–30,000
cycles one frame up and about 3,000 more a frame (7–13 system calls);
glibc's `backtrace` always reads the whole chain stack; a conservative
GC finds callers' register values only in the procedure stack (32-byte
pairs on v3, 16 bytes a register on v5+), and Elbrus Linux's Boehm GC
misses pointers a stopped thread holds only in `%g0`–`%g15`; the stack
registers read in user mode (layouts in stack-and-unwinding.md, "Stack
registers"); no split stacks; an executable without a GNU-stack note
gets a non-executable stack, and lcc rejects nested functions.

Contexts shared between threads (kernel-interface.md, "Contexts and
threads"; `ctxthr.c`, hw on all three): any thread may resume any
suspended context, which keeps its stacks, FP mode and mask but runs
with the resuming thread's `%g13`, so TLS addresses must not be kept
across a switch (GCC folds `&tls` and `__errno_location` across
`swapcontext`; only a value-returning or `noipa` accessor is safe:
`ctxtls.c`). A switch reads the target `ucontext_t` before locking it,
so a switch that lost a race can later resume an old state. A thread
exiting in a context that was freed or remade frees its hardware stacks
twice (and a `[signal stack]` is unmapped twice); on the 4C's 6.1-1.9
kernel any exit after a switch does. Fiber libraries and libgo must
keep their worker threads alive.

Overflow signals (kernel-interface.md, `accerr.c`, hw on all three): a
`getsp` the kernel won't grow the stack for is SIGSEGV at the
interrupted `USD`'s bottom − 1, `SEGV_BNDERR` on the 8C2 and 2C3 and
`SEGV_ACCERR` on the 4C, only because the 4C's ALT kernel is MCST's
6.1-1.9 tree (OpenE2K/linux 6a2673970ea0; 6.1-1.17 changed that
constant; ALT ships MCST's tree unchanged). The 4C's movable trampoline
page has the same cause.

Kernel leftovers (kernel-interface.md, `kernleft.c`, hw on all three):
an `sdisp` to a disabled entry leaves the CTPR empty (SIGILL at the
call); entry 4 runs the old table; `sigreturn` applies an edited
`bgr` and every `g`/`gtag`/`gext` slot; a handler starts with `%bgr`
0xff; a switch into a context another thread ran last costs 500–1,700
cycles more only when that thread ran on another core.

PoC findings folded on 2026-10-13 (calling-convention.md, registers.md,
kernel-interface.md, source-compat.md, speculation.md): a variadic
call's last fixed argument comes after the hidden result pointer and
`this`; `va_list` is `char *` (C++ `Pc`); lcc links `crtfastmath.o`
for `-ffast-math` with `-shared` and `-static` too, and dlopen of such a
library switches the loading thread and its later threads to fz + daz;
a `call` needs Q ≤ `wbs` < `wsz` (Q the pairs its function inherited,
4 in C), else SIGSEGV at the call, system calls included; with
`nfx = 1` the 8C2 and 2C3 lose an extended register part at every
signal handler (the 4C when the handler spills the window), and lcc
sets `nfx = 0` in functions that compute with 80-bit values; lcc's tail
calls don't release the caller's frame, so the stack grows 16 bytes a
call; GCC reuses loads across an asm without `"memory"`; lcc speculates
conditional loads but never volatile or atomic ones; lcc's 64-bit
objects use the 64-bit DWARF format, and MCST's gdb rejects DWARF
register numbers from 128 up.

Calls and the data stack (2026-10-16; calling-convention.md, "Tail
calls that release the frame", kernel-interface.md, "Probing",
predicates.md; hw on all three, false-call timing on the 4C and 2C3): a
tail call may give its frame back with a positive `getsp` before its
`ct` (or in its instruction), and the callee then behaves as after a
direct call (stack pointer, locals, rewritten stack arguments, large
results, signals, `backtrace`, MCST's unwinder; 10^6 chained calls in
constant stack), so GCC's sibling calls fit; `getsp` rounds its sum
down to 16, and nothing bounds a release. A false `call` costs what a
false `ct` costs, and the one chain frame it spills when the chain
register file is full costs nothing measurable. Stack-clash probes
every 4 KiB, allocated page by page (`getsp -4096` and a store), make
glibc's one-page guard enough for a large frame; 8 KiB probes can miss
it, a `getsp` is no probe, and probes after one big `getsp` let a
signal frame land below the guard; an E2K call probes nothing
(inferred). A `movtd` into a CTPR under a predicate is SIGILL.

32-bit mode (`spec/abi/mode32.md`, for completeness; hw on all three):
ILP32 with 8-byte `long long` and `double`; every data access is
`ldgd*`/`stgd*` (two 32-bit operands summed mod 2^32 within GD =
[0, 0xf000_0000), beyond it `SEGV_BNDERR`) and indirect calls use
`getpl`; argument slots as in 64-bit mode, but 32-bit values fill only
the low half and lcc extends narrow integers neither as caller nor in
results; ELFCLASS32 with the same `e_flags`, `/lib32/ld-linux.so.2`,
`R_E2K_32_*` relocations, TLS at TP + 8; system calls through entries 1
and 5 (the entry, not the task, picks the compat ABI), no time64 calls;
the modes cannot be mixed in one process; lcc's 32-bit ASan stops at
startup on the 8C2 and 2C3 (MCST's maps parser skips only three
mappings above 4 GiB and `[trampolines]` is a fourth); `ldgd*`/`stgd*`
cost what `ld*`/`st*` do and `getpl` → `ct` is 9; gdb and gcore work. MCST's qemu loads no ELF32; the community
qemu-e2k32 runs it without entries 3/6 and the GD bound.

Protected mode (`spec/abi/protected-mode.md`, for completeness; hw on
all three, the 4C static): pointers are 16-byte tagged descriptors
(base, size, curptr; a v6 procedure label carries the module index),
checked on every access (`SEGV_BNDERR`, `ILL_ILLOPN`); every call hands
over 8 pairs, `%qr0` is an AP to the argument area, slots are 16 bytes,
slots 1–7 in registers, results up to 128 bytes in registers and larger
ones through a hidden AP in slot 1; no stack-pointer check (the kernel
sets `%core_mode.no_stack_prot` before v7); loads keep memory tags and
empty values trap; the machines' lib128 reuses freed memory at once; a
binutils built without `--enable-new-pm-abi` (the default, ours
included) makes a PLT that breaks dynamic programs; lcc 1.31.05 makes
`intptr_t` a descriptor. Neither emulator runs protected code. Only
`cudtoap` makes a read-only AP (a store through it is `SEGV_ACCERR`),
and lcc never emits `cudtoap` or `ldcud`; psl overflows at 0xffe0;
`rrd %usd.lo` sees a `getsap` from 3 cycles on; user shared libraries,
OpenMP and static ELF32 protected programs work; root can clear
`no_stack_prot` (sysctl `debug.no_stack_prot`).

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
compatible; `%g12` as static chain; provide lcc-compatible
`__builtin_e2k_*` builtins (`<e2kintrin.h>`, generated from
spec/abi/lcc-builtins.yaml) but no x86 or NEON intrinsics and no
predefined x86 feature macros (2026-10-06). spec/abi/lcc-builtins.md
specifies them for lcc 1.31.05 (2026-10-07): 497 names (a sweep of about
60,000 candidates found only `__builtin_e2k_get_cpu_iset` missing from
the manual, which misspells it), 357 of 448 operation builtins are the
operation with the arguments in order, the rest and every header helper
were checked against models on the 4C, 8C2 and 2C3
(`tools/abi/builtins/run.py` is the port's acceptance test; it lists
the builtins a compiler lacks, so it can be used while they are added). Key facts behind it: MCST gas
assigns channels to ops outside braces, so a first port can print one op
per line, and the hardware interlocks everything but `%lsr`, so such code
is correct without padding, only slow; GCC has no
loop-rotation support and only sel-sched does check-free control
speculation. `tools/setup/build-gcc-docs.sh` builds GCC's manuals
(gccint) into `toolchains/gcc-docs/`.

## Next steps (proposed, owner to choose)

0. `docs/spec-poc-findings.md` (the PoC agent's file, untracked): every
   entry through I52, A29 and P91 is folded into `spec/` and `doc/` and
   removed from it (I35, I45, I52, A24–A29 and P86–P91 on 2026-10-13;
   I35 and I45 are explained); P92 is open; new ones continue as I53,
   A30, P93.
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

- On the 2C3 an array prefetch probe outside a `%lsr`-counted loop must
  write `%lsr` first (0 will do): after any finished counted loop
  (glibc's `strncmp` runs one) its `mova` is SIGILL (hwbugs.md).

- On the 2C3 `clock_gettime` advances in about 330 ns steps (clock source
  `sclkr`): time short loops with `%clkr` there (`mas_be.c`'s ns figures
  read 0).

- MCST's qemu cannot deliver hardware exceptions as signals: a memory
  fault (null or read-only page), division by zero, a taken `ct` through
  an empty CTPR, a too-small `setwd`, a diagnostic-operand trap all stop
  it with "exception handling is unimplemented - aborting". Test those
  on the 4C. (The community qemu delivers SIGSEGV but returns from a
  division by zero with no signal, and its `,sm` loads under a
  diagnostic predicate read memory.)

- gas inserts an empty wide instruction, which runs, before any jump
  target at offsets 0x1c0–0x1ff of each 1 KiB block (its
  `CODE_PLACEMENT` workaround): align timing probes so it can't
  lengthen a measured distance, and check distances with objdump.

- MCST's qemu is too lenient in two ways that hide broken code: it
  keeps an atomic's lock across loads and stores between the lock-wait
  load and the wait-unlock store (hardware always fails the store, so
  such a loop livelocks; atomics.md), and it applies `setbn`'s new base
  to `%b[]` operands in the same instruction (hardware uses the old one;
  registers.md). Run atomics and hand-written prologues on hardware.

- MCST's libm needs the sticky FP flags set: lcc built it with
  speculative FP, so after `feclearexcept` many functions (`sqrt` of a
  negative, `sin`, `exp`, `pow`, `fmod`, ...) die with SIGILL inside
  libm, and its `fmod`/`remainder` hang in `FE_DOWNWARD` on some exact
  multiples (registers.md, "MCST glibc's FP bugs";
  `tools/abi/probes/libmchk.c`). Expect testsuite noise from both.

- MCST's glibc `fesetenv (FE_DFL_ENV)` unmasks the denormal exception
  and sets x87 precision control to single (registers.md): after it a
  denormal operand traps (SIGFPE, or SIGILL in speculative lcc code) and
  `long double` rounds to 24 bits. Expect noise from tests that call it.

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

- gas accepts a predicated `movtd` into a CTPR (`movtd,0 %dr1, %ctpr1 ?
  %pred0`) and MCST's qemu runs it conditionally, but the hardware traps
  (SIGILL `ILL_ILLOPC`) whatever the predicate's value
  (spec/isa/predicates.md, "RLP"). Prepare unconditionally.

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
- Generated probe files are committed unless they are several MB: those
  (the timeleft probes) are listed in `.gitignore`, and
  `gen_operations.py` runs their generators (its `GENERATED` list)
  before it scans the probes.
