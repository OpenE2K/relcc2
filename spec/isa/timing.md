# Operation timing and pipeline hazards

Latencies, control-transfer preparation delays and interlock behaviour,
measured on hardware by `tools/isa-timing/bench.py`. The data is in
[timing-e2s.yaml](timing-e2s.yaml) (generated; do not edit).

Status: **[hw]** on an Elbrus 4C (E2S, elbrus-v3, 750 MHz) and an
Elbrus-8C2 (E8C2, elbrus-v5, 1.26–1.5 GHz; data in
[timing-e8c2.yaml](timing-e8c2.yaml), section "Elbrus-8C2 (v5)" below).
Every latency, hazard and control-transfer delay measured on both is
the same; v6 differs (load→use 5 instead of 3, binutils
`bfd/elfxx-e2k.c` `build_plt_entry`; the owner recalls this as v6's only
timing change, and isa-score uses it for v6+) and is not measured.

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
| FP min/max (`fmaxd`, `fmind`, packed `pfmaxd`) | — | **2** (also on the 8C2) | — |
| `fdivs` | 11 | 11 | 13 |
| `fdivd` | 14 | 14 | 17 |
| `fsqrts` | — | 12 | 13 |
| `sdivs`, `sdivd` | — | 11 | 13 |
| `ldd`, L1 hit, any of ALC0/2/3/5 → next load's address | 3 | 3 | 5 |
| `ldd` → integer op | 3 | 3 | — |
| `ldb`, `ldh`, `ldw` → integer op | 3 | 3 | — |
| integer op → load address | — | 1 | — |
| FP add → integer op | 4 + 2 | 6 | — |
| integer multiply (`muld`, `muls`) → integer op | 4 + 2 | 6 (7 with a write-back conflict, below) | — |
| integer op → FP add | 1 + 1 | 1 in the same cluster, 2 across (see Clusters) | — |
| FP add → FP multiply | 4 | 4 | — |
| compare → predicated ALU op | 2 | 2 | — |
| compare → `ct ? pred` | 3 | 3 | — |
| `disp` → `ct` | 5 | 5 | — |
| `return` → `ct` | 6 | 6 | — |
| `movtd` → `ct` | 9 | 9 | — |
| taken `ibranch` (instant branch), taken prepared `ct` | — | +6, 0 (see [control-transfer.md](control-transfer.md)) | — |
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
| `sxt`, `merged`, `popcntd`, `lzcntd`, `bitrevd` | 1 | 1 | — |
| `umulhd` (high half of a 128-bit product) | 4 | 4 | — |
| 80-bit `fxaddxx` / `fxmulxx` / `fxdivxx` | — | 5 / 4 / 16 | — |
| `fxaddxx` → integer op | — | 7 | — |
| `movfi` → `movif` (unpack and repack an 80-bit value) | — | 4 | — |
| `movif` → `movfi` (pmu.c) | — | 4 | — |
| `movif` → integer op, `movfi` → integer op (pmu.c) | — | 6 (a consumer 2 cycles after stalls 8, not 4) | — |
| `getsp` → integer op (pmu.c) | — | 2 | — |
| `fdtofx` → `fxtofd` | — | 5 | — |
| compare → `andp` (reading the `%pred`) | 1 | 1 | — |
| `andp` → `andp` | 0.5 | 0 inside one wide instruction (`@p4` → next CLP), 1 through a `%pred` | — |
| `andp` → predicated ALU op | 2 | 2 | — |
| `andp` → `ct ? pred` | 3 | 3 | — |
| FP compare (`fcmpltdb`) → predicated ALU op | — | 4 | — |
| FP compare → `ct ? pred` | — | 5 | — |

Rows marked (pmu.c) come from `tools/isa-probes/pmu.c` (producer, D
cycles of `nop`, consumer, counted with the TICKS monitor;
[monitors.md](monitors.md)), not from `bench.py`, so they are not in
`timing-e2s.yaml`.

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
- Loads beyond L1 are not scheduled for (see Memory).

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

The rule behind every measured gap: an operand that is `e = L − g`
cycles early stalls the consumer's whole wide instruction for
`4 × ceil(e / 4)` cycles, so a step costs `g + 4 × ceil((L − g) / 4)`.
The pipeline retries in 4-cycle steps: one cycle early costs 4, five
cycles early 8. This fits all the chains in `timing-e2s.yaml` (`addd`,
`add_addd`, `muld`, `faddd`, `fmuld`, `fmul_addd`, `fdivs`, `fdivd`,
`fsqrts`, `sdivd`, `ldd`; checked by `tools/isa-score/selftest.py`),
including the dividers' apparent period of 4 and the combined integer
operations' 5 back-to-back (latency 2, one cycle early: 1 + 4). A
`mova` result used early stalls the same way (apb.md). Consequences for
a compiler: model latencies exactly, and fill a known gap with `nop N`
(or independent work) rather than relying on the interlock. This is why
lcc pads with `nop`s.

Each 4-cycle step is one repeat of the pipeline's E2 phase, which the
`BUB_E2` monitor counts ([monitors.md](monitors.md)). Also measured
there (`tools/isa-probes/pmu.c`): a stalled instruction's own `nop N`
cycles come after the stall, not during it; a load in a stalled
instruction delivers its result at its normal latency from the
instruction's real issue; two loads in one instruction, 0 to 256 bytes
apart, don't delay each other. Transfers wait exactly, not in steps
([control-transfer.md](control-transfer.md)).

## Write-back conflicts

An FP-side result (FP add or multiply, integer multiply) on its way to
an integer operation arrives one cycle late when an integer-side
operation (an ALU operation or a load, not a store) issues in the
**same channel exactly 2 cycles after** the producer. With the consumer
placed at the producer's latency, that is a stall step (4 cycles).
Measured (`bench.py`, `*_p*` kernels; the reported latency counts from
the second operation, so the producer → consumer distance is 2 more):

| Producer (ALC0) → `addd,1` | Operation 2 cycles later | Producer → consumer latency |
|---|---|---|
| `muld` | `addd,0` | 7 |
| `muld` | `ldd,0` | 7 |
| `muld` | `addd,1` (other channel) | 6 |
| `muld` | `faddd,0` (FP side) | 6 |
| `faddd` | `addd,0` | 7 |

The same conflict delays an FP-side result going to an **FP consumer in
the other cluster** (`fmuld,0` → `faddd,3`: 6 cycles, 7 with an
`addd,0` 2 cycles after the `fmuld`), but not one in the producer's
cluster (`fmuld,0` → `faddd,1`: 4 either way); 4C and Elbrus-8C2 alike
(`fmuld_to_faddd_*` kernels; the proof-of-concept back end's finding I19:
a pipelined daxpy kernel at 6.0 cycles an iteration instead of 4.6 from
this). Not affected: FP consumers in the producer's cluster, stores, and
operations in any other channel. lcc's `-O0` code hits it (`muls,0`
then `ldb,0` two cycles later in a hash loop: 17 cycles per iteration
instead of 13). A scheduler should keep the producer's channel free of
integer operations 2 cycles after an FP-side result it routes to the
integer side, or place the consumer a cycle later.

## Throughput

Measured by `bench.py`'s `tp` kernels: 32 or 64 wide instructions, each
a group of independent operations with constant inputs and fresh
destinations, back to back; the cost per instruction is the difference
divided by 32 (`throughput` in the YAML). Every channel takes a new
operation every cycle, at full width:

| Group per instruction | Cycles |
|---|---|
| 6 `addd`; 4 `muld`; 4 `faddd`, `fadds`, `fmuld`, `fmul_addd` or `fdtoid`; 4 `faddd` + 2 `addd` | 1 |
| 2 `paddw` (ALC0, 3 only on v3); 2 `popcntd` (ALC1, 4) | 1 |
| 2 `add_addd` (ALC1, 4); 2 `add_addd` or `shl_addd` + 4 `addd`: **8 simple integer operations** (4C and 8C2) | 1 |
| 4 `ldd` or `ldw`; 4 `ldd` + 2 `addd` | 1 |
| 1 `std` (ALC2 or 5); a store with loads in any allowed channels, far apart | 1 |

The exceptions are the divider and the memory banks.

**Divider.** `fdivs`, `fdivd`, `sdivs`, `sdivd` and `fsqrts` share one
unit in ALC5 (a `fdivd`/`sdivd` pair alternating behaves like either
alone). It is pipelined, but some issue distances conflict and stall
the later instruction. Cycles per divide when one issues every `g`
cycles:

| `g` | 1 | 2 | 3 | 4 | 5 | 6+ |
|---|---|---|---|---|---|---|
| `fdivs`, `fdivd`, `sdivs` | 5 | 2 | 3 | 4 | 5 | `g` |
| `sdivd` | 5 | 3.4 | 3 | 4 | 5 | `g` |
| `fsqrts` | 7 | 2.8 | 3 | 4 | **7** | `g` |

So a scheduler should keep divides 2 cycles apart (3 for `sdivd`) and
not issue `fsqrts` exactly 5 cycles after another. Fractions are
averages of a pattern of stalled and unstalled issues; the structure
behind them (and behind `fsqrts` at 5) is not known.

**Two stores in one instruction** take 2 cycles when their addresses
agree in bits 6–7 (the same 64-byte bank of four: 0, 8, 16, 32, 48, 256
or 512 bytes apart), and 1 cycle otherwise (64, 80, 96, 128, 192, 320,
384, 448 apart). `stw` pairs behave the same way. Alternating stores to
one bank in successive instructions cost nothing.

**A store and a load in one instruction** to the same 8 bytes take
about 1.9 cycles. To other bytes of the same 32-byte block they take
either 2 cycles or 1, and which one is not a property of the code or
the address: identical kernels (same instructions, same data
addresses) differ within one run and between runs, also pinned to one
CPU, in contiguous stretches of the run, so some state left by earlier
code decides it. Different 32-byte blocks never conflict, nor do two
loads (0 to 512 bytes apart), nor a store followed by a load of the
same bytes in the next instruction.

A compiler cannot see most addresses, but it can for one base register
(stack slots, structure fields): don't put two stores whose offsets
agree modulo 256 within 64 bytes, or a store and a load in the same 32
bytes, in one instruction.

**Combined operations widen the instruction.** A combined operation is
two ordinary ones chained in one channel (semantics.md) with the
latency of both, so it saves no time on a dependent path, but it takes
one channel slot: with two integer combined operations in ALC1/4 an
instruction does 8 simple integer operations a cycle instead of 6, and
4 `fmul_addd` give 8 FP operations a cycle on v3's four FP channels. It
also leaves the intermediate result unwritten. Where channels limit a
schedule (pipelined loops), combining is worth up to a third more
integer work per cycle, and twice the FP work.

## Memory

Compilers schedule every load for an L1 hit (3 cycles); lcc does too.
Longer latencies cannot be scheduled for and are left to the interlock,
prefetching and APB, so `bench.py` measures them only with `--memory`
(pinned with `--numa CPU:MEM`). For reference, on the 4C (64 KiB L1D and
2 MiB L2 per core, no L3, four NUMA nodes), a random pointer chase gave:
L2 hit 9 cycles (the guide says 11), memory on the local node about
105–110 (+15 with 4 KiB pages, from TLB misses), on another node about
190–235.
This 4C runs its DDR3 memory in two channels per processor instead of
the three the E2S has (owner): latency should hardly change, but any
bandwidth-bound result on it (memory-bound benchmarks, streaming loops)
understates a fully populated machine. The 8C2 has DDR4 in all four
of its memory channels, so 4C/8C2 bandwidth comparisons favour the
8C2 by more than the hardware does.

On the Elbrus-8C2 (64 KiB L1D and 512 KiB L2 per core, 16 MiB L3
shared, one node; `--memory` at 1,500 MHz): L2 hit 11 cycles (scheduled
latency, a 256 KiB chase; the same as `ldqp`, which L2 serves), L3
about 32 (1–8 MiB chases cost 32–33 cycles a load whatever the gap),
memory about 180 (120 ns; a 128 MiB chase) and about 20 more with
4 KiB pages. In nanoseconds the 8C2's L2 is faster than the 4C's (7 ns
against 12) and so is its memory (120 against 145), but in cycles both
are slower, which a schedule tuned for the 4C feels: Unipro's Linpack,
whose 320 KB matrix leaves L1 and which uses no array prefetch, runs
12% slower than isa-score's L1 model on the 8C2 and 6% faster on the
4C.


**Cache hints in the MAS** [source: kernel `asm/mas.h`; lcc]: for
ordinary accesses (MAS `mod` 0) bits 5–6 (`dc_ch`) choose the caches
the access may fill: 0 all, 1 not L1, 2 not L1 or L2, 3 none
(`CACHE_BYPASS_*`). lcc's builtins (`e2kintrin.h`) use them: `ld_*_t1`
is `mas=0x20`, `ld_*_t2` `mas=0x40`, `ld_*_nta` `mas=0x60` (v3 and v5);
`st_*_nt` is a plain store on v3 and `mas=0x40` on v5; the stores of a
loop under `#pragma vector nontemporal` ("no caching at any level, write
combining") get `mas=0x60`, and lcc's software-pipelined streaming
loops at `-O3` load with `ldd,sm …, mas=0x20` (a second copy of the
loop with `0x40`), after `ldb,sm …, %empty, mas=0x20` prefetches.
Measured with `tools/isa-probes/mas_hint.c` (hand-written loops,
`mas_hint.s` from `gen_mas_hint.py`): 32 MiB from memory, one access a
64-byte line, then rereading a hot working set (ns per line; the 4C on
one node):

| Access | 4C stream | 4C hot set after it | 8C2 stream | 8C2 hot set after it |
|---|---|---|---|---|
| `ldd` | 4.8 | evicted (256 KiB: 16 → 121) | 2.22 | evicted, from L1, L2 and L3 |
| `ldd mas=0x20` | 4.4 | evicted (48 KiB partly kept) | 2.25 | evicted |
| `ldd mas=0x40` | 7.6 | **kept** | 2.15 | evicted |
| `ldd mas=0x60` | 7.6 | **kept** | **1.77** | **kept** (48 KiB to 4 MiB) |
| `std` | 12.5 | evicted | 4.0 | evicted |
| `std mas=0x20` | 12.3 | evicted | 4.0 | evicted |
| `std mas=0x40` | 11.4 | **kept** | 3.8 | evicted |
| `std mas=0x60` | 12.9 | **kept** | **3.0** | **kept** |

So the hints behave as APB's `dcd` does (apb.md): on the 4C, bypassing
L2 (`0x40`) or everything (`0x60`) keeps other data cached at the price
of about 1.6× slower streamed loads (stores cost nothing extra); on the
8C2 only `0x60` keeps it, and makes the stream faster too (loads 1.77
against 2.22 ns a line, stores 3.0 against 4.0), while `0x20` and
`0x40` act as plain accesses. For data read or written once, `0x60` is
the hint to use on v5 and `0x40` on v3; lcc's automatic `0x20` changes
little on either.

**Store to load** (`tools/isa-probes/stld.c`, 4C and 8C2 alike): a load
of a doubleword just stored, issued in the **very next** instruction,
returns its value about 7.6 cycles late (two 4-cycle stall steps: a
store, reload and use take 14.5 cycles a step instead of 6.9). The load
itself issues at once; only its result is delayed, and only when it is
used. Any gap of one cycle or more between the store and the load
removes the penalty completely. The match is by aligned doubleword: a
narrower or wider reload, and `stw` at +0 followed by `ldw` at +4 (no
overlapping bytes), all pay it; the next doubleword, the next line or
any other address doesn't. So keep a reload of a just-stored location
(a spill and its fill, a stack argument) at least one instruction away
from the store.

## Array prefetch

Measured by `tools/isa-probes/apb.c`, not `bench.py`; details in
[apb.md](apb.md#timing-hw). A `mova` result is ready 5 cycles after
issue, in any channel and for a consumer in either cluster; a consumer
issued earlier stalls a fixed 4 cycles, as above. The first `mova` after
`bap` issues without a stall 22 cycles after it when the data is in the
caches.

**One area delivers 16 bytes a cycle.** lcc's `loop_mode` loops over
arrays in L1 (`-O3 -faligned`, timed with `%clkr`), on the 4C and the
8C2 alike: a sum of `long`s reading one area runs at 0.53–0.54 cycles
per element, also when lcc's v5 code reads 32 bytes an iteration from
that area with two `movaqp` (each iteration then takes 2 cycles, where
the v3 code's two `movad`, 16 bytes, take 1); two arrays (two areas)
keep 0.54 cycles per element, 16 bytes a cycle from each; three arrays
reach 23 (v3 code) to 30 bytes a cycle (v5 code). So reading more than
16 bytes from one area in one instruction costs a cycle per extra 16
bytes (isa-score models this).

## Hazards (interlocking)

| Producer → consumer | Interlocked? | Detail |
|---|---|---|
| ALU, multiply, divide, load → register use | yes | values correct at any distance ≥ 1, also integer ↔ FP and across clusters |
| compare → predicated ALU op, `ct ? pred`, `ibranch ? pred` | yes | correct at distance ≥ 1 |
| compare → predicated op or `merge` in the wide instruction of a **taken return** (`ct %ctpr3` after `return`) | **no** | at distance 1 the op reads the predicate as it was before the compare, with no stall; at distance 2 it reads the new value. A taken branch (`ct %ctpr1`, near or far), a `call`, a `ct` that is not taken and no transfer all wait (`tools/isa-probes/preds.c`, `i17_*`; 4C and Elbrus-8C2; the proof-of-concept back end's finding I17, where a `merge` in a return's instruction broke expat). Keep a compare 2 instructions before any predicate reader in a return's instruction. |
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

## Elbrus-8C2 (v5) [hw]

`bench.py --target v5.8c2` on the 8C2 (kernels as on the 4C plus v5
ones, driver built by the 8C2-tuned lcc). The table was taken under the
`schedutil` governor; rerun under `performance` (1500 MHz fixed,
`%clkr` counting 1500 MHz against the wall clock, busy or idle) 368 of
370 latency and throughput kernels gave the same numbers, the other two
being store + load pairs in one 32-byte block (1.8–2 cycles either
way), so the governor doesn't affect cycle counts. All 80 scheduled latencies,
28 back-to-back costs, 21 hazards, 11 control-transfer delays and 10
branch costs equal the 4C's, and so do 164 of the 166 kernels of the
monitor probe (`pmu.c`, cycles from `TICKS`). So v3 latencies hold for
v5, as lcc's documentation says for v1–v5. What v5 adds or changes:

- **Six FP operations per cycle.** v4 added `faddd`, `fmuld`,
  `fmul_add*` (and the single forms) in ALC2 and ALC5, so 6 `faddd`,
  `fmuld` or `fmul_addd` issue in one instruction (v3: 4). In ALC2/5
  they have the usual latencies (4; `fmul_addd` 8) and crossing costs
  (ALC5 → ALC0 FP 6, ALC5 FP → integer 6).
- **128-bit packed operations** (`qp*`, v5) work on whole registers
  (`%qprN` is register N at its full 128 bits): `qpfaddd`, `qpfmuld` 4
  cycles, `qpfmul_addd` 8, `qpaddd` and `qpand` 1; 6 `qpfaddd` or
  `qpfmuld` per instruction.
- **`ldqp` bypasses L1:** the single-register 128-bit load is served
  by L2. Its latency is **11** to any consumer (integer or `qp`; `ldd`
  stays 3), only one issues per cycle (2, 3 or 4 in one instruction
  take 2, 3, 3–4 cycles), and the monitors show why: 10 million `ldqp`
  of one cached address make almost no L1 requests and 10 million L2
  queries, all hits, where `ldd` hits L1 (the v5 event list says
  `L1_REQUEST` counts "synchronous requests except qpword";
  monitors.md). **`ldq`** (a register pair `%qrN`, two load channels:
  `ldq,0` takes ALC0 and ALC2, `ldq,3` ALC3 and ALC5) goes through L1:
  latency 3, two per instruction (32 bytes a cycle). lcc's v5 code
  never uses `ldqp` for data: a 128-bit vector is two `ldd` and a
  `qppackdl` (`ldqp`/`stqp` appear only in 16-byte atomics). Stores
  go through L1 and on to L2 alike (L1 is write-through: `std` and
  `stqp` make as many L2 store queries as L1 ones); two `stqp` in one
  instruction, 64 bytes apart, take 1 cycle.
- **Latencies of every v5 operation** (`bench.py`'s `v5op_*` chains,
  generated from `binutils-ops.yaml`: each result feeds the next
  copy's first source; 195 operations, `plog`/`qplog` represented by
  two tables each):

  | Latency | Operations |
  |---|---|
  | 1 | packed integer add/sub (also saturating), logic, shifts (`qpsll*`, `qpsrl*`, `qpsra*`, `qpsrc*`), compares (`qpcmp*`), min/max, averages, signs, horizontal adds (`qphadd*`, `qphsub*`), packs (`qpack*`), shuffles and permutes (`qpshufb`, `qppermb`, `qpmerge`, `qpswitch*`), `qpmsk2sgnb`, `qppackdl`, `plog`/`qplog`, `addcd`/`subcd` (with carry), `getfz*`, `psrc*` |
  | 2 | packed integer multiplies (`qpmul*`, `qpmadd*`, `qpsadbw`, `qpmpsadbh`, `pmullw`), packed FP compares (`qpfcmp*`), `qpfmax*`/`qpfmin*` |
  | 4 | packed FP add, subtract, multiply, horizontal and add-subtract forms, round to integral (`qpfdtoifd`, `qpfstoifs`), `qphminposuh`, `qpsgn2mskb` (to an integer register and back) |
  | 5 | packed conversions (`qpfdtoid`, `qpfstoid`, `qpfstofd`, `qpfstois`, `qpidtofd`, `qpistofd`, `qpistofs`, and the `tr` forms) |
  | 6 | `qpfdtofs`, `qpfdtois`, `qpfdtoistr`, `qpidtofs` (narrowing, as scalar `fdtofs`) |
  | 8 | combined packed FP (`qpfadd_*`, `qpfsub_*`, `qpfmul_*`, `qpfhadd_*`, `qpfhsub_*`, `qpfaddsub_*`) |

  So 128-bit operations take the latency of their 64-bit counterparts,
  and FP min/max is 2, not 4, on v3 too (table above).
- **128-bit operations are fully pipelined** in every channel they are
  allowed (`v5tp_*` kernels: one per channel per cycle): packed integer
  add `qpaddd` in ALC0/3; `qpmullh`, `qpsadbw`, `addcd` in ALC1/4;
  logic, shifts, shuffles (`qpshufb`, `qppermb`, `qpmerge`), FP compares,
  `qpfmaxd`, conversions, `plog`/`qplog` in ALC0/1/3/4; combined FP
  (`qpfmul_addd`) in all six.
- **Crossings with 128-bit values**: a `qpfaddd` result costs 6 in the
  other cluster (as scalar FP) and 6 to `addd` (reading the low half), 4
  to `faddd`; `qpaddd` costs 4 in the other cluster and 4 to `addd`;
  `addd` → `qpaddd` 1. But a **64-bit FP-side result read by a 128-bit
  operation pays as if crossing, in the same channel**: `faddd` →
  `qpfaddd` 6, `fmuld` → `qpfmuld` 6, `faddd` → `qpaddd` 6, `paddw` →
  `qpaddd` 4 (instead of 4, 4, 4, 1). And packed integer results in the
  other cluster cost 4 on both machines (`paddw` → `paddw,3`, `qpaddd` →
  `qpaddd,3`), not latency + 2: an FP-side result in the other cluster
  costs at least what it costs an integer consumer.
- **Divider:** `sdivd` every 2 cycles no longer conflicts (2 cycles; v3
  3.4); the other divider distances are as on v3 (`fsqrts` still
  conflicts at 5). An ALC5 FP operation never conflicts with a divide
  in flight (distances 1–16 for `faddd` and `fmuld`), except in one
  pattern: `fdivd` every 2 cycles with an ALC5 FP operation in every
  gap costs 2.76 cycles per pair instead of 2 (probably what lcc 1.26's
  "FP operations in channel 5 need special scheduling" is about).
- **A store and a load in one 32-byte block** in one instruction take
  1.8–2 cycles in every run (the 4C sometimes runs them at 1); two
  stores agreeing in address bits 6–7 take 2, as on the 4C.

## Differences between generations (lcc-doc, not measured)

lcc's changelogs name scheduling changes it made for later CPUs, which
`bench.py` should check when it runs there:

- v5: floating-point operations in channel 5 need special scheduling
  (lcc 1.26); measured above: one conflicting pattern with the divider.
- v6: the bypass from a store to a following load was removed (lcc 1.26),
  so a load that reads what a store just wrote waits longer.
- Elbrus-48C (v7): prepare-to-transfer delays differ;
  `-mset-prepare-ct-delays-e48c` applies them and is on with
  `-mtune=elbrus-48c` (lcc 1.31).

## Not measured yet

- v4, v6, v7.
- Loop mode registers other than `%lsr` (`%ilcr`); APB throughput.
- 128-bit operations (v5+), `%spred`.
- Throughput of 128-bit and packed FP operations, `mova`, AAU stores,
  and loads and stores of other sizes; whether the store bank rule
  involves physical address bits.
