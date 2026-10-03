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
without judging the algorithm. The score is static (every block counts
once) and needs no hardware and no emulator; how fast a program really
runs is measured by running it on the hardware.

```
tools/isa-score/score.py PROGRAM
tools/isa-score/score.py PROGRAM --function f --explain 0x10450
                                                  # one block, cycle by cycle
```

Options: `--iset N` (default: the ISA version in the ELF header),
`--function NAME` (repeatable), `--top N`, `--json FILE` (every block's
numbers).

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
an integer operation is a cycle late when an integer operation or load
issues in the same channel 2 cycles after its producer
(`spec/isa/timing.md`). A loop's achieved cost is its initiation
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
`-O0`, `-O2` and `-O3`; on the machine it single-steps each kernel once
under gdb (`validate/count.py`) to learn how often each wide instruction
runs, and times each kernel with `%clkr`; the prediction is the static
per-block cycles weighted by those counts. The table below was produced
with execution counts from an earlier qemu-based counter (since
removed); the gdb counter has not completed a run yet. On the Elbrus 4C
every kernel was within 2% (a call and return adds 6 cycles the model
doesn't count):

| Kernel | -O0 | -O2 | -O3 |
|---|---|---|---|
| sum | 6672 / 6678 | 283 / 284 | 529 / 532 |
| dot | 9231 / 9237 | 2089 / 2080 | 2075 / 2074 |
| saxpy | 12304 / 12310 | 538 / 545 | 554 / 554 |
| hash | 34832 / 34838 | 5148 / 5145 | 4113 / 4116 |
| branchy | 17168 / 17174 | 1044 / 1049 | 1044 / 1049 |
| poly | 27151 / 27157 | 2101 / 2104 | 2101 / 2105 |
| list | 6160 / 6166 | 1551 / 1563 | 1551 / 1563 |
| copy | 7184 / 7190 | 540 / 539 | 178 / 181 |

(predicted / measured cycles per call, lcc 1.29.16, data in L1.)

The possible schedule cannot be measured; it is a bound on what a
scheduler could do with the same operations.

## Limits

- Timing is measured on v3 (E2S) only; the guide says v1-v5 share it.
  For v6 and v7 the v3 latencies are used and the report says so; the
  channel tables are those of the chosen version.
- Loop analysis covers loops whose body is one basic block (lcc's
  `loop_mode` loops and other loops compiled into one block). Other
  loops are scored block by block.
- Not modelled: cache misses, the call and return themselves (about 6
  cycles), taken `ibranch` costs other than on a loop's back edge, the
  store/load channel pairs gas forbids, and branch targets in the bad
  page offsets of `CPU_HWBUG_CODE_PLACEMENT`.
