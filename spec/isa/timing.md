# Operation timing and pipeline hazards

Latencies, control-transfer preparation delays and interlock behaviour,
measured on hardware by `tools/isa-timing/bench.py`. The data is in
[timing-e2s.yaml](timing-e2s.yaml) (generated; do not edit).

Status: **[hw]** on an Elbrus 4C (E2S, elbrus-v3, 750 MHz). v1–v5 are
said to share timings (AGENTS.md); v6 differs (e.g. load→use 5, binutils
`bfd/elfxx-e2k.c` `build_plt_entry`) and is not measured yet.

## Method

- Cycles are read from `%clkr` (readable from user code, counts core
  cycles: 750.0 MHz against the wall clock).
- **Back-to-back cost**: a chain of dependent copies of one operation, one
  per wide instruction, so each waits for the previous result. Overhead
  is cancelled by the difference between chains of 64 and 32.
- **Scheduled latency**: the same chain with each operation followed by
  `nop`s so the next one issues `gap` cycles later; the latency is the
  smallest gap at which a step costs exactly `gap` cycles (no stall). This
  is the value a scheduler needs.
- **Memory** latencies come from a pointer chase pinned with `--numa
  CPU:MEM` (`numactl`), see Memory.
- **Hazards**: a producer and a consumer `D` cycles apart (`D = 0`: same
  wide instruction), checking whether the consumer sees the new value.
  `.ignore strict_delay` keeps gas from inserting delays.

The guide's numbers (chapter 6.1) are in the second column for comparison.

## Latencies

| Producer → consumer | Guide | Scheduled latency | Back-to-back |
|---|---|---|---|
| integer add/sub, logic, shift, `sxt` (32 or 64-bit) | 1 | 1 | 1 |
| combined integer (`add_addd`, `shl_addd`; ALC1/4) | 2 | 2 | 5 |
| integer multiply (`muls`, `muld`) | 4 | 4 | 5 |
| FP add, multiply (single and double) | 4 | 4 | 5 |
| combined FP (`fmul_addd`) | 8 | 8 | 9 |
| `fdivs` | 11 | 11 | 13 |
| `fdivd` | 14 | 14 | 17 |
| `fsqrts` | — | 12 | 13 |
| `sdivs`, `sdivd` | — | 11 | 13 |
| `ldd`, L1 hit, any of ALC0/2/3/5 → next load's address | 3 | 3 | 5 |
| `ldd` → integer op | 3 | 3 | — |
| integer op → load address | — | 1 | — |
| FP add → integer op | 4 + 2 | 6 | — |
| integer multiply → integer op | 4 + 2 | 6 | — |
| integer op → FP add | 1 + 1 | 1 in the same cluster, 2 across (see Clusters) | — |
| FP add → FP multiply | 4 | 4 | — |
| compare → predicated ALU op | 2 | 2 | — |
| compare → `ct ? pred` | 3 | 3 | — |
| `disp` → `ct` | 5 | 5 | — |
| `return` → `ct` | 6 | 6 | — |
| `movtd` → `ct` | 9 | 9 | — |
| packed integer (`paddw`, `pandd`, `pcmpeqw`, `pmaxsh`, `psllw`, `pshufb`) | 4 (FP class) | 1 | — |
| packed multiply, `psadbw` (`pmullh`; ALC1/4) | 4 | 2 | — |
| packed integer → integer op | 4 + 2 | 4 | — |
| packed integer → FP add | 4 | 1 | — |
| conversions `fdtoid`, `idtofd`, `fstofd` | — | 5 | — |
| conversion `fdtofs` | — | 6 | — |
| `fdtoid` → integer op | — | 6 | — |
| integer op → `idtofd` | — | 1 | — |
| FP compare into a register (`fcmpeqd`) | — | 2; 4 to an integer op | — |
| `lzcnts`, `scls`, `getfs` | 1 | 1 | — |
| compare → `andp` (reading the `%pred`) | 1 | 1 | — |
| `andp` → `andp` | 0.5 | 0 inside one wide instruction (`@p4` → next CLP), 1 through a `%pred` | — |
| `andp` → predicated ALU op | 2 | 2 | — |
| `andp` → `ct ? pred` | 3 | 3 | — |
| FP compare (`fcmpltdb`) → predicated ALU op | — | 4 | — |
| FP compare → `ct ? pred` | — | 5 | — |
| load, L2 hit | 11 | 9 | 9 |
| load from memory, own NUMA node | ~100 | about 105–110 | 108–115 |

The scheduled latencies of scalar integer, FP, control and predicate
operations match the guide. Differences:

- The guide's integer → FP "+1" appears only when the consumer is in the
  other cluster, and integer → integer pays it too: it is a cluster
  crossing, not a domain crossing (see Clusters).
- Packed integer operations are not 4-cycle FP-class operations on the
  4C: most take 1 cycle (multiply and `psadbw` 2) to another packed or FP
  operation. Their results reach integer operations at 4, like an FP
  compare into a register: the FP-side → integer penalty is +3 for the
  1-cycle ones and +2 otherwise.
- Conversions take 5 (`fdtofs` 6), and their results reach integer
  operations at 6.
- FP compares into predicates are two cycles slower than integer
  compares (4 to a predicated operation, 5 to `ct`).
- An L2 hit is 9, not 11. The 4C (E2S) has no L3.

## Clusters

The six ALU channels form two clusters, ALC0–2 and ALC3–5. A result
reaches the other cluster later (scheduled latency, producer in ALC0 or
ALC3, consumer in the channel shown):

| Producer | Consumer in the same cluster | Consumer in the other cluster |
|---|---|---|
| integer ALU op (`addd`) | 1 to an integer op, FP op or load address (ALC1, ALC2: 1) | 2 (ALC3, ALC4; ALC3 → ALC0 also 2) |
| `ldd`, L1 hit | 3 (to `addd` or the next load's address) | 4 |
| FP add/multiply, integer `muld` | 4 to an FP op or `muld`; 6 to an integer op | 6 to any op |

So integer and load results cost one extra cycle across clusters, and
FP-class results two, which an FP → integer use pays anyway. All of these
paths are interlocked (hazard tests `int_fp_use`, `fp_int_use` and their
`_x` cross-cluster forms see the new value at distance 1). For a
scheduler: give each result two latencies, own cluster and other cluster,
and prefer placing a consumer in the producer's cluster.

## Stalls cost more than the latency

The pipeline interlocks, but an early consumer is expensive. For a
4-cycle producer, a consumer placed `g` cycles later costs:

| g | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| cycles | 5 | 6 | 7 | 4 | 5 |

So for `g` below the latency the cost is about `g + latency`, not
`latency`: issuing one cycle early loses almost a whole latency. The
dividers and square root behave periodically (period 4: `fdivs` costs 11,
12, 13, 14, 11, ... for `g` = 3, 4, 5, 6, 7). The combined integer
operations cost 5 back-to-back despite a latency of 2. Consequences for a
compiler: model latencies exactly, and fill a known gap with `nop N` (or
independent work) rather than relying on the interlock. This is why lcc
pads with `nop`s.

## Memory

The 4C has 64 KiB of L1 data cache and 2 MiB of L2 per core, no L3, and
four NUMA nodes (one per E2S chip, four cores each). A random pointer
chase (one pointer per 64-byte line) with a gap after each load gives
the scheduled latency; each run continues the chase where the last one
stopped, so the lines are not cached again. Run with `--numa 1:1`
(CPUs and memory of node 1) and `--numa 1:3` (memory of node 3), buffers
in 2 MiB transparent huge pages unless noted:

| Working set | Latency, local memory | Remote memory (other chip) |
|---|---|---|
| 256 KiB (L2) | 9 | 9 |
| 8 MiB, 64 MiB (memory) | about 105–110 | about 190–235 |
| 64 MiB in 4 KiB pages | about 125 (+15: TLB miss) | about 250 |

The memory latency is not fixed: it varies by about ±5 cycles between
gaps and runs, and a consumer issued earlier stalls until the data
arrives (the cost stays flat at the latency until the gap exceeds it).
Without pinning, the results mix local and remote memory, which is why
earlier runs showed 108–212 cycles. A compiler should schedule for an
L1 hit (3) and leave longer latencies to prefetching, as lcc does.

## Hazards (interlocking)

| Producer → consumer | Interlocked? | Detail |
|---|---|---|
| ALU, multiply, divide, load → register use | yes | values correct at any distance ≥ 1, also integer ↔ FP and across clusters |
| compare → predicated ALU op, `ct ? pred`, `ibranch ? pred` | yes | correct at distance ≥ 1 |
| compare → `andp`, `andp` → predicated ALU op | yes | correct at distance ≥ 1 |
| `andp` → `andp` in the same wide instruction (a CLP reading `@p4` written by an earlier CLP) | — | sees the new value: predicate logic chains inside one instruction |
| compare → `andp` through `%spred0` in the same instruction | ? | read the old (false) value both on the 4C and under qemu; how `%spred` is meant to be used is not known yet |
| `disp`, `movtd` → `ct` | yes | correct at distance ≥ 1 |
| `rws %fpcr` → `rrs %fpcr` | yes | |
| `rws %pfpfr` (rounding mode) → FP operation | yes | |
| FP operation → `rrs %pfpfr` (exception flags) | yes | |
| **`rwd %lsr` → `rrd %lsr`** | **no** | old value at distance 1–3, new from 4 |
| anything in the same wide instruction | — | every operation reads the state from before the wide instruction (distance 0 sees the old value) |

gas, for operations written outside braces (its `strict_delay` mode,
`gas/config/tc-e2k.c`), inserts `nop 2` after compares and `ccto*`,
`nop 5` or `wait all_e=1` after writes to the system registers flagged
in `include/opcode/e2k/state-regs.def` (`%psr`, `%cwd`, `%psp`, `%pcsp`,
`%usd`, segment registers and others), and `nop 4` after reading `%cud`
and `%gd`. On the 4C the compare padding is not needed for correctness
(it saves the stall cost above); the system-register cases are
privileged and not tested here.

Where the floating-point state lives (glibc `sysdeps/e2k/fpu`): the
rounding mode and exception flags of ordinary (scalar and packed)
operations are in `%pfpfr` (rounding in bits 13–14:
`FE_TONEAREST` 0, `FE_DOWNWARD` 0x2000, `FE_UPWARD` 0x4000,
`FE_TOWARDZERO` 0x6000; flags in bits 0–5, `FE_INEXACT` 0x20); `%fpcr` and
`%fpsr` are the x87-style control and status words that 80-bit operations
use instead (hw; MCST's qemu wrongly uses `%pfpfr` for them; see
`spec/abi/registers.md`, "Extended parts and `long double`").

## Differences between generations (lcc-doc, not measured)

lcc's changelogs name scheduling changes it made for later CPUs, which
`bench.py` should check when it runs there:

- v5: floating-point operations in channel 5 need special scheduling
  (lcc 1.26).
- v6: the bypass from a store to a following load was removed (lcc 1.26),
  so a load that reads what a store just wrote waits longer.
- Elbrus-48C (v7): prepare-to-transfer delays differ;
  `-mset-prepare-ct-delays-e48c` applies them and is on with
  `-mtune=elbrus-48c` (lcc 1.31).

## Not measured yet

- v4, v5, v6 (the 8CB is v5).
- Loop mode registers other than `%lsr` (`%ilcr`), APB.
- 128-bit operations (v5+), `%spred`.
- Throughput (operations per cycle per channel) as opposed to latency.
