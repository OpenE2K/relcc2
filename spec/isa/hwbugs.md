# Hardware errata and anomalies

What is known about E2K hardware bugs, from two kinds of source that
must not be confused:

- **[source]**: errata MCST names in its GPL Linux kernel,
  `arch/e2k/include/asm/cpu_features.h` (`CPU_HWBUG_*`, with MCST's bug
  numbers, the CPU models and revisions affected and the kernel's
  workaround). These are MCST's own statements, but each is one comment
  line: the exact trigger is rarely given, and we have reproduced none
  of them.
- **[hw?]**: things our probes saw on our machines that look wrong or
  undocumented. **Low confidence.** Each could be a bug in our probe, an
  interaction with the kernel, a misreading of what the operation is
  meant to do, or a fault of that one machine, rather than a CPU
  erratum; none is confirmed by MCST. Treat them as leads, not facts,
  and recheck before relying on any.

Our machines and their revisions (`/proc/cpuinfo`): the 4C is an
**E2S revision 2** (v3), the 8C2 an **E8C2 revision 3** (v5), the 2C3 an
**E2C3 revision 1** (v6). "Ours" below says which of them an erratum
covers.

## Errata that matter to a compiler [source]

| Erratum (MCST #) | CPUs | What (the kernel's words, condensed) | Kernel's workaround | Ours | For a compiler |
|---|---|---|---|---|---|
| `CODE_PLACEMENT` (146897, 140436, 77417) | E2S, E8C, E1C+, E8C2, E12C, E16C; E2C3 rev ≤ 1 | jumps to the end of a 4 KiB page, and the placement of asynchronous (APB) program code, can misbehave | keep jump targets and call return addresses out of page offsets 0xdc0–0xdf8 and 0xee0–0xff8 | all three | MCST gas does it by default for v2–v6 (`ENABLE_FIX_LCC_BUG_142105`, apb.md); a port that emits its own code layout must keep gas's fix on. The rule for APB programs is not published |
| `INTERSECTING_L1_ACCESSES` (123567) | E8C2, E12C, E16C | a load after an intersecting store can read wrong data from L1 | at least one real wide instruction between them (not `nop N`) | 8C2 | **not reproduced**: `stldv.c` (a store and an overlapping load in the next instruction, 10 size/offset shapes × 6 channel pairs, 1,000,000 times each) read the right value every time on the 4C, the 8C2 and both 2C3 cores. Such a reload is slow anyway (timing.md, "Store to load"); keep one instruction between, as lcc's scheduling against that cost does |
| `STORE_RELEASE`, `SOFT_WAIT`, `STORE_MAS` (130066, 134351, 133605, 159721) | E12C rev 0, E16C and E2C3 rev ≤ 2 (store-release, store MAS); E12C, E16C, E2C3 rev 0 (soft waits); E8C2 its own soft-wait variant | L1/L2 don't respect the `lal`/`las`/`sas`/`st_rel` barriers; a pair of stores with certain MAS doesn't work | don't use `las`/`sas`/store-release; `nop` before `lal`/`sal`; four instructions between such stores. The kernel's release store is `wait st_c, ld_c` + `st` on these, the MAS store-release (0x13) only where the bug is absent | 2C3 (store-release, store MAS), 8C2 (soft wait) | the v6 MAS release store that `mas6.c` timed (speculation.md, "v6 encodings") is **not to be used** on our 2C3 or any E2C3 up to rev 2; lcc doesn't, and its v6 atomics are `wait` + plain stores (atomics.md) |
| `BRANCH_ACTIVATES_CTPR` (rm 27719) | E2C3, E12C, E16C (all) | `ibranchd`/`rbranch` can corrupt an inactive `%ctpr` | don't use them while any `%ctpr` is inactive | 2C3 | `rbranch` ends every DAM check and lcc's atomic read-modify-write loops (atomics.md); on v6 keep all CTPRs valid (prepared) around them, or avoid them, until the meaning of "inactive" is known |
| `TAGGED_LDW` (142262) | E16C rev ≤ 2, E2C3 rev ≤ 1, E12C rev 0 | a tagged `ldw` doesn't work after a possibly aliasing `stw` | four bubbles between | 2C3 | only code that loads tags with `ldw` (protected mode, tag-preserving copies) |
| `PREFETCH_EMPTY` (129870, 131465) | E12C, E16C, E2C3 rev 0 | prefetches into `%empty` don't work | give the prefetch the "L1 disabled" MAS (prefetch into L2) | none | lcc's `ldb,sm …, %empty, mas=0x20` prefetches (timing.md, "Memory") are no-ops there |
| `WC_DAM` (95860) | E2S rev ≤ 2, E8C rev ≤ 1, E8C2 rev 0 | write-combining memory conflicts with the DAM | `wait st_c` between WC writes and cacheable loads | 4C | only programs writing WC mappings (graphics memory) |
| `AAU_AALDV` (99302) | E2S, E8C, E1C+, E8C2 rev 0 | `%aaldv` is sometimes not restored properly | `wait ma_c` in the kernel's AAU restore | 4C | none in user code; the kernel covers APB across traps |
| `UNALIGNED_LOADS` (160330) | E8V7 rev 0 (v7) | fully speculative loads crossing a page boundary lose tags | no speculative mode for unaligned loads | none | v7 only: semi-speculative (`mas=0x0`) for loads that may cross a page (speculation.md) |
| `WRITE_MEMORY_BARRIER` (89495) | E8C rev ≤ 1 | the write barrier doesn't work, even for atomics | a special sequence after every read-acquire | none | v4 (E8C) only |
| `RWD_LSR` (165225) | listed, enabled for no CPU in this kernel | some instructions are not allowed after `rwd %lsr`/`%lsr1` | `{nop}` after the `rwd` | — | lcc already leaves 4 cycles between `rwd %lsr` and the loop (loops.md, where a too-early loop also misbehaves, below) |
| `GENERATIONS_L2_PREF` (rm 35505) | E16C rev 2, E8V7 rev 0 | load/store "generations" conflict with the L2 prefetcher | the kernel disables the prefetcher while the binary compiler runs | none | binary translation only |

## Errata for debuggers, profilers and the OS [source]

These don't change generated code but explain what tools see:

- `BREAKPOINT_INSTR` (90514; E2S, E8C, E8C2 — the 4C and the 8C2):
  the hardware hangs after code is modified for a breakpoint unless
  the patched instruction keeps its `HS.lng`; the kernel handles it.
- `SPURIOUS_EXC_DATA_DEBUG` (117649; up to v5, rev 0 of v6 CPUs) and
  `EXC_DEBUG` (143157, 141618; v6 and earlier): data breakpoints fire on
  the first 31 bytes of a 32-byte block falsely, and user debug
  exceptions can arrive in the kernel. gdb watchpoints on our machines
  can trigger spuriously.
- `KERNEL_DATA_MONITOR` (88644; E2S, E8C, E1C+ — the 4C): data-monitor
  events are lost when they overflow under masked interrupts; the kernel
  doesn't profile with the data monitors. `PIPELINE_FREEZE_MONITORS`
  (125405; E8C2, and E2C3/E12C/E16C rev > 0 — the 8C2 and the 2C3): the
  pipeline-freeze feature conflicts with performance monitoring, so the
  kernel turns pipeline freeze off while monitors run. Both bear on
  `profile.py` and `pmu.c` (monitors.md): cycle counts taken under
  monitoring are of a CPU with that feature off.
- `ITLB_LARGE_PAGES` (152622; up to v6): the ITLB for large pages is
  small, so the kernel maps less of itself with large pages.
- OS-only, listed for completeness: `CLW`, `CLW_STALE_L1_ENTRY`,
  `CLW_LOW_RESTORE`, `CLW_ASYNC*` (the stack-clearing hardware), `SS`,
  `FALSE_SS` (single step), `TRAP_CELLAR_S_F`, `TAGGED_STRQP`,
  `TLB_FLUSH_L1D`, `GIGANTIC_FLUSH`, `USD_ALIGNMENT`, `CR_*` (chain
  register writes), `C3*` (idle states), `WAIT_INT`, `SCLK*`, `LEVEL_EOI`,
  `IOMMU`, DMA, PCIe and memory-controller entries, `HWPOISON`, `LDRD_*`
  (privileged loads), `DIMCR1`, and the virtualization entries (`VIRT_*`,
  `GUEST_*`, `HRET_INTC_CU`, `HCALL_EXC_ILL_INSTR_ADDR`, `INTC_*`).

## What our probes saw [hw?, low confidence]

| Observation | Where | Why it may be a bug | Why it may not be |
|---|---|---|---|
| Stand-alone APB programs (`apb.c`'s `cold*` cases: `bap`, a `nop` gap, a few movas, no loop) die with SIGILL on **core 1** of the 2C3, intermittently and in runs, right after the process has faulted in fresh memory; the same binary never fails on core 0, and on core 1 it often passes too | 2C3 (v6), `apb.c`, `apbm` variants in `toolchains/` | core-dependent, timing-dependent failure of correct-looking code; E2C3 rev 1 is under `CODE_PLACEMENT`, whose APB part is unpublished | the APB program addresses of failing cases (page offsets 0xc28, 0xda8, 0x290, …) are mostly outside the erratum's ranges; the cases use APB outside any loop, which no compiler does; every loop-based APB test (`apb-loop.c`, `apb-lcc.c`, lcc's own loops) passes on both cores, also under signals and page faults |
| An APB area whose increment carried a `mova` 32 GiB past its array (into unmapped memory) made the process spin in the kernel until killed, instead of giving a diagnostic value | 8C2, `apb.c` `incr64` (since changed) | a user program could tie up a core | might be the kernel's fault handling, not the CPU; seen once, not repeated on purpose (apb.md) |
| A compare one instruction before a taken return is not interlocked with a predicated operation or `merge` in the return's instruction: it reads the old predicate, with no stall | 4C, 8C2 (`preds.c` `i17_*`); fixed on the 2C3 | every other predicate consumer waits; v6 behaves as one would expect | reproducible and maybe intended on v3–v5 (the guide doesn't say); timing.md lists it as a hazard to schedule around |
| A prepare (`disp`, `movtd`) and a `ct` through the same CTPR in one instruction: the `ct` doesn't transfer at all on v6 (v3/v5: `movtd` + `ct` goes to the old target), although gas accepts the pair from v6 | 2C3 (`ctl6.c`, `bench.py` `movtd_ct`) | gas allowing it suggests it should work | the intended v6 semantics are unknown; perhaps related to `BRANCH_ACTIVATES_CTPR` |
| A loop entered 1–3 cycles after `rwd %lsr` runs one iteration too many on v3/v5, and only one iteration on v6 | all three (`loops.c` `early*`) | the write is not interlocked with the loop | `rwd %lsr` → `rrd %lsr` is documented as not interlocked (timing.md); the kernel lists `RWD_LSR` for future CPUs; lcc leaves 4 cycles |
| The DAM keeps 32 doubleword entries exactly, but quad entries vary from run to run (33 sometimes kept on the 8C2, up to 44 on the 2C3) | 8C2, 2C3 (`spec.c` `q_cap*`) | the capacity isn't deterministic | replacement may legitimately depend on state we don't control; a lost entry only makes the check reload |
| Only `%aabfN` with bit 1 of N clear stop the APB unit | 4C, 8C2 (`apb.c` `aabf*`) | the encodings are regular (`0x30 \| N << 7`) | the other registers may simply be different ones |
| `aaurrq` of an array descriptor: the high half reads 0 on the 4C, `0x89abcdef` on the 8C2 | `semc.c` | same operation, different results | reading AAU state from user mode may be unspecified |
| Some throughput patterns vary between runs: two stores agreeing in address bits 6–7 and a store plus a load in one 32-byte block (4C), `1 std + 2 ldd` and the store grid (2C3) | `bench.py` `tp` | non-deterministic cost | history-dependent banking is ordinary microarchitecture |
| `fdivd` every 2 cycles with an FP operation in ALC5 in every gap costs 2.76 cycles a pair instead of 2 | 8C2 (`bench.py` `v5_div*`); gone on the 2C3 | a resource conflict lcc's changelog only hints at (1.26: "FP operations in channel 5 need special scheduling") | a documented scheduling constraint rather than an error |

Not hardware, listed elsewhere: MCST ld's unlinkable `R_E2K_ISLOCAL`,
silent ifunc and static-ifunc failures (elf.md); objdump's `staa`/`ldaa`
decoding bug (bundle.md); the emulators' differences (AGENTS.md,
"Gotchas", and each page's "Emulators" section).
