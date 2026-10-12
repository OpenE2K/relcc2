# isa-score: how much of E2K's parallelism a compiled program uses

E2K is statically scheduled: a wide instruction's operations issue
together, latencies are fixed, and apart from memory nothing in the
hardware reorders or predicts. So the cycles a block of code takes are a
property of the binary. isa-score reads an E2K ELF file and, for every
basic block, compares

- **achieved**: the cycles of the compiler's schedule as written, and
- **possible**: the cycles of the best schedule of the *same operations*
  on the chosen ISA version.

The ratio says how far a compiler is from what the machine allows,
without judging the algorithm. `score.py` is static (every block counts
once) and needs no hardware and no emulator. `profile.py` weights the
same blocks by a run on the hardware, using the performance monitors
(`spec/isa/monitors.md`), and compares the model with the measured
cycles block by block.

```
tools/isa-score/score.py PROGRAM
tools/isa-score/score.py PROGRAM --function f --explain 0x10450
                                                  # one block, cycle by cycle
```

Options: `--iset N` (default: the ISA version in the ELF header),
`--function NAME` (repeatable), `--top N`, `--json FILE` (every block's
numbers).

```
tools/isa-score/profile.py PROGRAM [ARGS...] --remote claude@ringo.lan \
    --remote-dir relcc2/<dir>       # run on the 4C, weight, compare
tools/isa-score/profile.py PROGRAM --samples FILE [--stat FILE]
                                    # again from a saved run
```

`profile.py` runs the program under `perf record` sampling two monitors
every `--period` events (default 100,003): EXEC (wide instructions) and
TICKS (cycles); a second run samples EXEC and `NO_CT_TAKEN`, whose
difference at a transfer counts the times it was taken. The monitors record the exact instruction (EXEC samples
spread evenly over a block's instructions), so a block's EXEC samples ÷
its length × the period estimate how often it ran, and its TICKS
samples × the period the cycles spent in it. It reports, for the
program's own blocks weighted by those counts: measured cycles, the
model's achieved and possible cycles, operations per cycle of each, and
the possible/achieved score weighted by the run; per function and per
block, measured against achieved (where they differ, the model is wrong
or the block waited for memory); and a `perf stat` breakdown of the run
(instructions per cycle, interlock stall steps × 4, `nop` cycles,
cycles without an instruction to issue, spills). Counts are estimates:
a block with n EXEC samples has about 1/√n relative error. It uses
`perf record -m 32`, since every `perf record` of one account shares a
locked-memory quota and a second one with the default buffer fails.

To compare compilers' schedulers, compare **operations per cycle** of
the achieved schedule on the same program: on CoreMark and Linpack it
ranks lcc, Unipro's clang and gcc-claude as the 4C does. The score
(possible / achieved) only says how much each compiler leaves on the
table with its own operations.

## What is measured

Memory is assumed to hit L1, as compilers schedule it; cache misses are
the one thing the score leaves out.

**Achieved** simulates the instructions in order: each issues one cycle
after the previous one plus its `nop`, and waits when an operand isn't
ready. A wait costs whole 4-cycle steps (`ceil(early / 4) × 4`), which
matches every gap measured on the 4C (`selftest.py`). It also models the
write-back conflict found while validating: an FP-side result going to
an integer operation, or to an FP operation in the other cluster, is a
cycle late when an integer operation or load issues in the same channel
2 cycles after its producer (`spec/isa/timing.md`). Also from
`timing.md`: a load of the aligned doubleword stored in the instruction
just before gets its value 8 cycles late (same base register, offsets
in one doubleword); a divide 1 cycle after another waits 4 (every
operation of the ALC5 divider: divides, square roots and reciprocal
estimates, integer, FP, packed and 80-bit), an `fsqrts` or `pfsqrts` 5
cycles after another 2, and anything within 9 cycles of an `smodx` or
`umodx` until those 9 have passed (the fractional rates at distance 2
are not modelled); two stores in one instruction with the same base
whose offsets agree in bits 6–7 take a cycle more (the base is assumed
256-byte aligned); an `aaurw` or `aaurwd` (an array-prefetch register
write) takes its register like a store's data, an integer result from
either cluster the next cycle (`tools/isa-probes/aaurwct.c` on the 4C
and the 8C2). Under `setwd dbl = 0`, which the model reads from each
function's `setwd`, a 32-bit integer result (`adds`, `ands`, `shls`,
`muls`, `merges`, `gettagd`, `rrs`, the combined 32-bit operations;
not loads, `sxt` or FP-unit results such as `fadds` and `movts`) read
as 64 bits (`%dr`, `%db[]`, `%dg`: `addd`, `movtd`, an address, store
data) reaches its reader 4 cycles after the producer issues, from
either cluster, instead of 1 (a slower producer is not delayed
further: `muls` → `addd` stays 6); a reader of the 32-bit operand gets
it in 1 (`timing.md`, "Formats"; `tools/isa-probes/tagx.c`). lcc's code
reads such results through `sxt` (no block of its CoreMark or Linpack
pays it); gcc-claude's doesn't (below). On v5 an FP result reaches the single or double operand of
an 80-bit operation (`fxadddd`'s src2, `fxdivtdd`'s src1) as if from
the other cluster, 6 cycles after `faddd` instead of 4, and a load
from its own cluster in 4 instead of 3 (`timing.md`, "Elbrus-8C2";
`sregs.c`). AAU loads (`ldaa*`) are served by L2 even when the line is
in L1: 9 cycles to any consumer on v3, 11 on v5 and 13 on v6, with no
cluster-crossing cost (`spec/isa/apb.md`, "AAU loads"; `memleft.c`,
measured with the line in L1 and to an integer consumer; lcc emits
none). `popcnt*` is an integer operation (1 cycle to an integer
consumer), not a packed one. `selftest.py` checks these edges against
the measured ones.
The `stld.c` probe's 60 straight-line cases are
reproduced to within 0.4 cycles a step. A conditional transfer is assumed not taken
unless it is a back edge: not taken, it needs its `disp` or `return`
only 2 cycles before it, not 5 or 6 (measured with the monitors, and
for `return` with `aaurwct.c` on the 4C and the 8C2; on lcc's CoreMark this
assumption took the model from 10% too slow to within 1%). A taken
`ibranch` costs 6 cycles: unconditional ones and back edges are charged
it. For a block ending in a conditional transfer `score.py` also
computes both outcomes (`achieved_taken`, `achieved_fall` in `--json`),
which `profile.py` weights by measured taken fractions. A loop's
achieved cost is its initiation
interval, from simulating three iterations, plus a one-time entry cost.

**Possible** list-schedules the block's operations: the ALU channels each
operation may use on the ISA version (`spec/isa/binutils-ops.yaml`),
literal syllables (four 32-bit), one CS0, CS1 and control transfer per
instruction, four `mova` channels and the predicate logic slots, with
the measured latencies including cluster crossings. The transfer stays
last. Two dependence models:

| Model | Registers | Memory |
|---|---|---|
| `regs` | the compiler's: true, anti and output dependences (except inside one wide instruction) | within a block, ordered unless two accesses provably don't overlap (same base register, constant offsets); across loop iterations, only where they provably overlap (a compiler that pipelined the accesses knew they don't alias) |
| `flow` | true dependences only (unlimited registers) | ordered only where accesses provably overlap |

The gap between `regs` and `flow` is what register allocation and
unknown aliasing cost. For a single-block loop the possible cost is
max(ResMII, RecMII): the channel bound and the longest dependence cycle
per iteration (rotating `%b[]` registers under `abn` count as
iteration distances).

ALU channels are assigned per wide instruction by bipartite matching,
so an operation that can use only one channel is not crowded out. The
list scheduler is greedy, so the possible cost of a block is the shorter
of its schedule and the compiler's own (which is also a valid schedule
of the same operations). Over 6,278 functions compiled by lcc the
possible total exceeds the achieved one in a single function, by one
cycle.

Values still in flight cross block boundaries: a block entered from a
single predecessor starts with that predecessor's results in flight,
under the same model. So padding with `nop` at the end of one block and
a stall at the start of the next cost the same, in both measures.
After a single-block loop whose `abn` has `abnf=1` the values carried
out are renamed as the hardware renames them: the last iteration's
`%b[X]` is `%b[X + 2]` (`spec/isa/loops.md`). Until 2026-10-12 the
model left the names unrotated, which charged false stalls to the
blocks after some of lcc's pipelined loops (four in lcc's v3 CoreMark
`matrix_test`, 9 cycles instead of 5) and missed a real one (Linpack's
`matgen`, whose `fmaxd` result is read right after its loop).
Preparations (`disp`) are assumed hoistable in the possible schedule.

## Accuracy

`validate/validate.py --remote claude@ringo.lan --remote-dir
relcc2/<dir>` builds nine kernels (sum, dot product, saxpy, byte hash,
branches, polynomial, list walk, copy, an empty function) with lcc at
`-O0`, `-O2` and `-O3`; on the machine it times each with `%clkr`, then
runs each under `profile.py`'s sampling, called often enough for about
20,000 TICKS samples (`--samples`; `k_empty` at most 5,000,000 calls)
with a pseudo-random delay of 0–31 loop iterations between the calls.
The prediction is the static per-block cycles weighted by the sampled
counts. On the Elbrus 4C (under two minutes):

| Kernel | -O0 | -O2 | -O3 |
|---|---|---|---|
| sum | 6693 / 6682 / 6678 | 279 / 279 / 284 | 527 / 528 / 532 |
| dot | 9230 / 9256 / 9238 | 2082 / 2078 / 2080 | 2071 / 2084 / 2074 |
| saxpy | 12341 / 12340 / 12310 | 537 / 540 / 544 | 550 / 553 / 554 |
| hash | 34818 / 34817 / 34838 | 5142 / 5156 / 5146 | 4112 / 4125 / 4116 |
| branchy | 17233 / 17197 / 17174 | 1039 / 1050 / 1049 | 1041 / 1042 / 1049 |
| poly | 27146 / 27203 / 27157 | 2095 / 2107 / 2104 | 2095 / 2104 / 2105 |
| list | 6132 / 6176 / 6166 | 1550 / 1560 / 1563 | 1550 / 1568 / 1563 |
| copy | 7166 / 7189 / 7190 | 537 / 535 / 539 | 175 / 175 / 181 |

(predicted / sampled / `%clkr` cycles per call, lcc 1.29.16, data in
L1; `%clkr` includes the call and return, about 6 cycles.) Every kernel
is within 1%; `k_empty` (7 cycles, a few thousand samples) within 4%.
Rerun on 2026-10-12 with the `dbl`, 80-bit operand and AAU-load rules
and the loop-exit renaming (above), every kernel is again within 1%
(`k_empty` 1.03): none of them changes a block of these kernels, whose
lcc code reads 32-bit results through `sxt`.

On the Elbrus-8C2 (`--target v5.8c2 --period 30011`; the default
period makes TICKS sampling exceed perf's limit of about 100,000
samples a second at 1.5 GHz, which throttles it; about 80 seconds)
every kernel is within 2% in four runs (all but two results within
1%; `k_empty` 0.94–1.14 with about 1,250 samples), and in a fifth with
the rules of 2026-10-12 (`sum -O2` 1.02, `k_empty` up to 1.11), once the model
knows three v5 facts: `movaqp` writes
one 128-bit register (the disassembly names it like a pair), `ldqp`
takes 11 cycles (served by L2), and an array-prefetch area delivers 16
bytes a cycle (lcc's v5 `sum -O3` reads 32 an iteration). Built with
the 4C-tuned lcc (`--target v3.4c`) the 4C results are the same as
with the generic one.

The delay matters. Until 2026-10-12 every kernel was called 20,000
times back to back, so each kernel's run was strictly periodic (a few
hundred cycles a call for the short ones), and with a fixed sampling
period the TICKS samples lock to it and land unevenly in the call: on
the 8C2, `sum -O2` (an APB loop of one instruction, one cycle an
iteration, 256 iterations) measured 0.92–0.93 in every run, its loop
0.88 cycles an iteration, which is impossible. Called back to back on
its own it measured 0.35, 0.40, 0.98, 1.06, 1.08, 1.09 and 1.25 cycles an
iteration at periods 50021, 29989, 30089, 20011, 30059, 10007 and
30011; built without APB (`-fno-loop-apb`) it locked too (0.34 at
30059), while the EXEC samples gave 256 iterations a call at every
period and `%clkr` the modelled cycles. More samples alone don't
help: called back to back for about 20,000 samples each, `sum -O2`
read 1.02 (its blocks still far off) but `saxpy -O3` 0.71, `sum -O3`
0.90 and `copy -O3` 0.94, while the EXEC samples gave every block
exactly once a call. A plausible mechanism: the kernel
(arch/e2k/kernel/perf_event/perf_event.c) restarts the counter at
−period, so each sampling interval is the period plus the cycles
counted between the overflow and the trap, which depend on where the
overflow falls (EXEC overflows at an instruction and doesn't lock);
before v7 it also takes the sample's IP from the trap, not from
`%dimar`. `branchy -O2` (1,050 cycles a call) locked in one run of four
(0.90) and at none of eight periods on its own; the `-O0` kernels
(6,000 cycles and more) never did. With the
delay the samples fall at random, so each column of a kernel is off
by about 1/√samples (0.7% for 20,000). Whole programs are rarely that
periodic, but a run dominated by one short call repeated many times
can lock the same way: compare two `--period` values.

On whole benchmarks (`profile.py`, the 4C, CoreMark with 2,000
iterations, Linpack at size 200 with `--period 10007`; lcc 1.29.16,
Unipro's clang 20, gcc-claude snapshot db34161a6cd):

| Build | Measured cycles | Measured / achieved | Operations per cycle, run | Score regs / flow, run | Fetch bubbles (`IB_NO_COMMAND`) | Taken `ibranch` |
|---|---|---|---|---|---|---|
| lcc `-O3` CoreMark | 13.32 G | 0.98 | 2.77 | 0.88 / 0.87 | 1.3% | 4 M |
| Unipro `-O2` CoreMark | 15.63 G | 1.00 | 1.01 | 0.87 / 0.70 | 6.3% | 0 |
| gcc-claude `-O2` CoreMark | 18.20 G | 1.06 (now 0.99, below) | 0.60 | 0.74 / 0.59 | 11.4% | 377 M |
| lcc `-O3` Linpack | 8.34 M | 1.02 | 3.76 | 0.71 / 0.70 | 0.8% | 0 |
| Unipro `-O2` Linpack | 37.8 M | 0.94 | 0.56 | 0.84 / 0.11 | 0.4% | 0 |
| gcc-claude `-O2` Linpack | 47.7 M | 1.03 | 0.47 | 0.57 / 0.15 | 1.8% | 0.2 M |

On the Elbrus-8C2 with the 8C2-tuned lcc and Unipro's clang
(`-march=elbrus-v5`), measured / achieved is 1.00 for CoreMark (lcc
`-O3` and Unipro `-O2`), 0.98 for lcc's Linpack and 1.12 for Unipro's
(Linpack's 320 KB matrix leaves L1, which the model assumes). The
4C-tuned lcc (`e2k-v3.4c`) gives the same cycles on the 4C as the
generic one (CoreMark 13.30 G against 13.32 G). The same v3 CoreMark
binary takes 2% more cycles on the 8C2 than on the 4C for the same
iterations (fix the iteration count: CoreMark's fourth argument 0 makes
it calibrate, so runs differ).

Fetch bubbles are taken `ibranch`es (6 cycles each; gcc-claude's) and
taken `ct`s waiting for a late prepare (Unipro's `switch` dispatch and
early conditional exits); the model charges both once it knows which
transfers were taken. An interlock step in gcc-claude's
`matrix_mul_matrix` (about 2% of the run), which load timing, register
kind and `nop` placement did not explain (`pmu.c`'s `mm_*` replicas),
is the `dbl = 0` rule: its loops (`dbl = 0`) end with `adds %g0, %g5,
%g0` and then `addd %dg0, 0x0, %dg5`. With the rule, a run of the same
binary with 10,000 iterations (5.85 G cycles, 2026-10-12) gives
measured / achieved 0.99 for the whole run (1.04 under the old model
with the same samples) and 0.98 and 1.00 for the loops of
`matrix_mul_matrix` and `matrix_mul_matrix_bitextract` (1.21 and 1.16
before: 20.6 and 27.9 cycles an iteration measured, 21 and 28
modelled, 17 and 24 before). What is left: in gcc-claude's CoreMark,
call blocks that take about 14 cycles more per call than modelled; in
Unipro's Linpack, loops that run faster than modelled. Weighted by the
run, operations per cycle rank the compilers as their measured speed
does.

On the Elbrus-2C3 (v6; `--target v6.2c3 --cpu 0 --period 40009
--ssh-port ...`) the model with the 2C3's latencies puts every kernel
within 1% except the two APB loops (`copy -O3` 0.85, `sum -O3` 1.19, a
few hundred samples each), and on the benchmarks built by the 2C3-tuned
lcc at `-O3`: CoreMark 27.5 G cycles, measured / achieved 1.03;
Linpack at size 200 (320 KB, in the 2 MiB L2) 0.99, at size 1000 (8 MB,
from memory) 1.58. Unipro's clang (`-march=elbrus-v6 -O2`): CoreMark
1.00, Linpack 1.08. On the 2C3 Unipro's CoreMark runs at 0.70× lcc's
speed (3,592 against 5,167 iterations a second) and its Linpack 5× slower
(16.9 s against 3.4 s for 4,096 repetitions), as operations per cycle
rank them (0.91 and 0.62 against 2.31 and 4.31).

The possible schedule cannot be measured; it is a bound on what a
scheduler could do with the same operations.

## Limits

- Timing is measured on v3 (E2S), v5 (8C2) and v6 (2C3)
  (spec/isa/timing.md). v5 differs from v3 in `ldqp` (11), `pfdivd`
  and `pfsqrttd` (16), AAU loads (11) and the narrow operand of 80-bit
  operations; v6 differs in loads (5 cycles in either
  cluster, `ldqp` 5, AAU loads 13), integer predicates (1 to a predicated operation),
  APB bandwidth (32 bytes a cycle per area) and the new operations; v7
  uses v6's numbers, unmeasured, and the report says so. The channel
  tables are those of the chosen version. So `--iset 6` on a v3 binary
  shows how much a schedule tuned for v3 loses on v6.
- Loop analysis covers loops whose body is one basic block (lcc's
  `loop_mode` loops and other loops compiled into one block). Other
  loops are scored block by block.
- Not modelled: an FP-side result read in another format than it was
  produced in (a double as a single, a single as a double: 5 or 6
  cycles instead of 4; `timing.md`, "Formats"), the v5 narrow-operand
  delay from integer multiplies, packed results or into the `fx`
  compares and `fdtofx` (not measured), AAU loads to consumers other
  than integer ones (assumed the same), cache misses, the call and
  return themselves (about 6
  cycles; more in some of gcc-claude's call blocks, see above), which
  way a conditional transfer goes (static: exits not taken, back edges
  taken; `profile.py` measures it), the
  store/load channel pairs gas forbids, branch targets in the bad
  page offsets of `CPU_HWBUG_CODE_PLACEMENT`, store/load hazards and bank
  conflicts between addresses with different base registers, and the
  sharing of L1 bandwidth between a store and a load in one 32-byte block.
