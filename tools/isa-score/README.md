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
in one doubleword); a divide 1 cycle after another waits 4 and an
`fsqrts` 5 cycles after another 2 (the fractional rates at distance 2
are not modelled); two stores in one instruction with the same base
whose offsets agree in bits 6–7 take a cycle more (the base is assumed
256-byte aligned). The `stld.c` probe's 60 straight-line cases are
reproduced to within 0.4 cycles a step. A conditional transfer is assumed not taken
unless it is a back edge: not taken, it needs its `disp` only 2 cycles
before it, not 5 (measured with the monitors; on lcc's CoreMark this
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
Preparations (`disp`) are assumed hoistable in the possible schedule.

## Accuracy

`validate/validate.py --remote claude@ringo.lan --remote-dir
relcc2/<dir>` builds nine kernels (sum, dot product, saxpy, byte hash,
branches, polynomial, list walk, copy, an empty function) with lcc at
`-O0`, `-O2` and `-O3`; on the machine it runs each 20,000 times under
`profile.py`'s sampling, and times each with `%clkr`. The prediction is
the static per-block cycles weighted by the sampled counts. On the
Elbrus 4C (about a minute):

| Kernel | -O0 | -O2 | -O3 |
|---|---|---|---|
| sum | 6672 / 6688 / 6678 | 282 / 282 / 284 | 528 / 530 / 532 |
| dot | 9231 / 9247 / 9237 | 2081 / 2076 / 2080 | 2071 / 2085 / 2074 |
| saxpy | 12305 / 12315 / 12310 | 537 / 537 / 544 | 552 / 552 / 554 |
| hash | 34831 / 34864 / 34838 | 5143 / 5150 / 5145 | 4111 / 4120 / 4116 |
| branchy | 17167 / 17194 / 17174 | 1042 / 1046 / 1049 | 1042 / 1057 / 1049 |
| poly | 27150 / 27186 / 27157 | 2097 / 2098 / 2104 | 2097 / 2104 / 2104 |
| list | 6161 / 6171 / 6166 | 1551 / 1558 / 1563 | 1551 / 1564 / 1563 |
| copy | 7183 / 7196 / 7190 | 538 / 541 / 539 | 177 / 183 / 181 |

(predicted / sampled / `%clkr` cycles per call, lcc 1.29.16, data in
L1; `%clkr` includes the call and return, about 6 cycles.) Every kernel
is within 1%, `copy -O3` within 3% (a later run: `copy` within 6%, with a
few hundred samples per kernel).

On the Elbrus-8C2 (`--target v5.8c2 --period 30011`; the default
period makes TICKS sampling exceed perf's limit of about 100,000
samples a second at 1.5 GHz, which throttles it) every kernel is within
3% (`sum -O2` 7%) once the model knows three v5 facts: `movaqp` writes
one 128-bit register (the disassembly names it like a pair), `ldqp`
takes 11 cycles (served by L2), and an array-prefetch area delivers 16
bytes a cycle (lcc's v5 `sum -O3` reads 32 an iteration). Built with
the 4C-tuned lcc (`--target v3.4c`) the 4C results are the same as
with the generic one.

On whole benchmarks (`profile.py`, the 4C, CoreMark with 2,000
iterations, Linpack at size 200 with `--period 10007`; lcc 1.29.16,
Unipro's clang 20, gcc-claude snapshot db34161a6cd):

| Build | Measured cycles | Measured / achieved | Operations per cycle, run | Score regs / flow, run | Fetch bubbles (`IB_NO_COMMAND`) | Taken `ibranch` |
|---|---|---|---|---|---|---|
| lcc `-O3` CoreMark | 13.32 G | 0.98 | 2.77 | 0.88 / 0.87 | 1.3% | 4 M |
| Unipro `-O2` CoreMark | 15.63 G | 1.00 | 1.01 | 0.87 / 0.70 | 6.3% | 0 |
| gcc-claude `-O2` CoreMark | 18.20 G | 1.06 | 0.60 | 0.74 / 0.59 | 11.4% | 377 M |
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
transfers were taken. What is left: in gcc-claude's CoreMark an
interlock step in `matrix_mul_matrix` that the model doesn't predict
(about 2% of the run; not explained by load timing, register kind or
`nop` placement, checked with `pmu.c`'s `mm_*` replicas) and call blocks
that take about 14 cycles more per call than modelled; in Unipro's
Linpack, loops that run faster than modelled. Weighted by the run,
operations per cycle rank the compilers as their measured speed does.

The possible schedule cannot be measured; it is a bound on what a
scheduler could do with the same operations.

## Limits

- Timing is measured on v3 (E2S) and v5 (8C2), which share it apart
  from v5's additions (spec/isa/timing.md). For v6 and v7 they are
  used with one change, loads at 5 cycles instead of 3 (binutils'
  `build_plt_entry`; the owner recalls this as v6's only timing
  change), not measured, and the report says so; the channel tables
  are those of the chosen version. So `--iset 6` scores a program as
  if for a CPU with slower loads, e.g. to see how much a schedule
  tuned for v3 loses there.
- Loop analysis covers loops whose body is one basic block (lcc's
  `loop_mode` loops and other loops compiled into one block). Other
  loops are scored block by block.
- Not modelled: cache misses, the call and return themselves (about 6
  cycles; more in some of gcc-claude's call blocks, see above), which
  way a conditional transfer goes (static: exits not taken, back edges
  taken; `profile.py` measures it), the
  store/load channel pairs gas forbids, branch targets in the bad
  page offsets of `CPU_HWBUG_CODE_PLACEMENT`, store/load hazards and bank
  conflicts between addresses with different base registers, and the
  sharing of L1 bandwidth between a store and a load in one 32-byte block.
