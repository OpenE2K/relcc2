# Control transfer

How E2K changes the flow of control: branches, calls, returns and system
calls. Encodings are in [bundle.md](bundle.md) (CS0, SS, `ctcond`); this
page is about what the operations do, what they cost and how a compiler
should use them.

Status markers, in addition to bundle.md's:

- **[hw]**: run on an Elbrus 4C (elbrus-v3): costs by
  `tools/isa-timing/bench.py` (data in [timing-e2s.yaml](timing-e2s.yaml)),
  behaviour by `tools/abi/probes/ctpr.c`
- **[guide]**: MCST programming guide, chapter 10.9–10.10 and 4.4
- **[lcc]**: observed in lcc 1.29.16 `-O2` output
- **[source]**: kernel (`arch/e2k/include/asm/cpu_regs_types.h`),
  MCST's qemu (`target/e2k/tcg/translate-ct.c`), gas (`tc-e2k.c`)

## Model [guide]

A transfer is either **instant** (one operation) or **prepared** (two):

1. A **prepare** (CTP) operation computes the target and the kind of
   transfer and stores them in one of three **control-transfer
   preparation registers**, `%ctpr1`–`%ctpr3`. The hardware starts
   fetching code at the target in the background.
2. A **transfer** (CT) operation, `ct`, `call` (or `scall`), takes a
   prepared register and transfers, optionally under a condition.

The prepare and the transfer may be any distance apart, as long as the
CTPR is not overwritten in between; one prepare can serve many transfers
(a loop's back edge). The only instant transfers available to user code
are `ibranch` (and `rbranch`, its `%MLOCK` form) and, from v6, `ibranchd`
/`icalld` with a register target.

After the wide instruction containing a transfer, the next one executed
is the target if the condition holds and the next in memory otherwise;
nothing from the other path changes the state. The other operations of
the wide instruction containing the transfer execute normally.

A wide instruction holds at most one prepare (in CS0) and one transfer
(in SS) [guide 4.4, bundle.md].

## Prepare operations

| Operation | CTPR | Target | Kind | Use |
|---|---|---|---|---|
| `disp %ctprN, label` | 1–3 | IP-relative, signed 28 bits × 8 bytes (±1 GiB) [bundle.md] | branch or call | direct jumps and calls (`R_E2K_DISP`, elf.md) |
| `movtd %drN, %ctprN` | 1–3 | 64-bit address in a register | branch or call | indirect calls, `switch` jump tables, PLT |
| `getpl %rN, %ctprN` | 1–3 | offset from the compilation-unit descriptor | branch or call | 32-bit mode only [guide] |
| `return %ctpr3` | 3 only | the caller's IP from the chain stack | return | function return |
| `sdisp %ctprN, n` | 1–3 | system call entry `n` (5 bits) | system call (`call` becomes `scall`) | glibc: `sdisp %ctpr1, __SYSCALL_TRAPNUM` then `call %ctpr1, wbs = N` |
| `ldisp %ctpr2, label` | 2 only | an array-prefetch (APB) program | not transferable | APB setup; any later write to `%ctpr2`, a call or a return stops the program ([apb.md](apb.md)) |
| `pref %iprN, label` | `%ipr0–7` | code to prefetch | none | prefetch into the instruction cache; lcc does not emit it |

`disp`, `movtd`, `return` and `pref` take an optional `; ipd N` (prefetch
depth in instruction-cache lines: 0, 1, 2; lcc always writes `ipd 2`).
`gettsd`, `puttsd`, `done`, `iret`, `hret`, `glaunch` are for protected
mode or the kernel.

## Transfer operations

| Operation | Notes |
|---|---|
| `ct %ctprN [? cond]` | branch or return (whatever the CTPR was prepared for) |
| `call %ctprN, wbs = W [? cond]` | call: the callee's window starts at pair W of the caller's window (registers.md); conditional calls exist |
| `ibranch label [? cond]` | instant branch, IP-relative like `disp`; no CTPR |
| `rbranch label` | `ibranch label ? %MLOCK` (the atomic retry loop, atomics.md) |
| `ibranchd`, `icalld` | v6+: instant branch or call to a register target |

Conditions (`ctcond`, bundle.md): a predicate or its negation, loop
conditions (`%LOOP_END`, `%NOT_LOOP_END`, used with loop mode, `alc`,
`abn`, `abp` in the same instruction), `%MLOCK`, and compare/CLP results
of the same instruction.

## The CTPR register [source]

64 bits on v1–v6 (v6 adds a second word with the compilation-unit index;
v7 widens the register to 128 bits and moves the fields):

| Bits | Field | Values |
|---|---|---|
| 0–47 | `ta_base` | target address |
| 54–56 | `ta_tag` | 0 empty, 1 diagnostic, 2 procedure label, 3 local label, 4 numeric label, 5 system label |
| 57–58 | `opc` | 0 `disp`, 1 `ldisp`, 3 `return` |
| 59–60 | `ipd` | 0 no prefetch, 1 the target instruction, 2 the target and the next |

## Lifetime and validity [hw, source]

Checked on the 4C by `tools/abi/probes/ctpr.c`:

| Situation | Result |
|---|---|
| A CTPR prepared before a `call`, used after the callee returns | **SIGILL** (`ILL_ILLOPC`): the CTPR is empty |
| A CTPR prepared by the caller, used by the callee | **SIGILL** (`ILL_ILLOPC`): `call` leaves the CTPRs empty too |
| `ct %ctprN ? cond` with a false condition on an empty CTPR | no trap |
| A CTPR used after a taken `ct` through another CTPR | still valid; a taken branch does not clear the CTPRs |
| `movtd` of a value with a diagnostic tag (failed speculative load) | no trap: the CTPR holds a diagnostic label |
| `ct` taken through that CTPR | **SIGILL** (`ILL_ILLADR`) |
| `ct ? false` through it | no trap |

So:

- **Every call and return empties all three CTPRs.** A function starts
  with no prepared transfer, and after each call it must prepare again
  everything it still needs, its own return included. MCST's qemu clears
  them on return (`translate-ct.c`); its `ct` on an empty CTPR is not
  implemented (it aborts), while the community qemu traps with SIGILL.
- A `ct` traps only when taken, so a conditional transfer may name a CTPR
  that is not valid when the condition is false, and a prepare may be
  speculative: lcc issues `movtd,0,sm` for indirect calls and `switch`.
- `return` is valid only in `%ctpr3`; a CTPR prepared by `ldisp` cannot be
  transferred through (MCST's qemu raises an illegal-address exception for
  both).

## Restrictions [source: gas]

- One prepare and one transfer per wide instruction; `disp`, `ldisp`,
  `return`, `sdisp` all live in CS0, so only one of them.
- On v1–v5 a wide instruction may not prepare and transfer through the
  **same** CTPR (gas: "not supported on elbrus-vN", MCST bug 154538);
  v6+ allows it. Different CTPRs are fine: lcc puts `call %ctpr2` and
  `disp %ctpr1, loop_head` next to each other. [hw] On v3 and v5 a
  `movtd` and a `ct` through the same CTPR in one instruction (gas
  accepts `movtd`) transfer to the CTPR's old target; on v6 (the
  Elbrus-2C3) the `ct` doesn't transfer at all (`bench.py` hazard
  `movtd_ct`, distance 0). Neither is useful: prepare at least 9
  cycles ahead.
- `movtd` and `getpl` into a CTPR go only in ALC0
  ([bundle.md](bundle.md), "Bundling restrictions").

## Costs [hw]

Delays between a prepare (or the condition) and the transfer, from
[timing.md](timing.md):

| From | To | Cycles |
|---|---|---|
| `disp` | `ct`, `call` | 5 |
| `return` | `ct` | 6 |
| `movtd` | `ct`, `call` | 9 |
| integer compare, `andp` | `ct ? pred` | 3 |
| FP compare | `ct ? pred` | 5 |

A transfer placed earlier waits until the CTPR is ready, then proceeds:
with the prepare D cycles before the `ct`, the pair costs `max(D,
delay) + 1` cycles. Unlike an early data consumer (timing.md, "Stalls
cost more than the latency"), there is no extra penalty. The same holds
for the condition: a `ct ? pred` 1 or 2 cycles after its integer compare
waits 2 or 1 cycles, taken or not.

A conditional `ct` that is **not taken** waits for its CTPR only until
2 cycles after the `disp` (1 cycle early: 1 cycle lost; later: none),
not 5. Measured with the performance monitors (`tools/isa-probes/pmu.c`,
`ct_*_dK` and `cmpct_*_dK`; [monitors.md](monitors.md)):

| `disp` → `ct ? pred` distance | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| extra cycles, condition false | 1 | 0 | 0 | 0 | 0 |
| extra cycles, condition true | 4 | 3 | 2 | 1 | 0 |

So a compiler may put the `disp` of a rarely taken exit late, without
cost on the fall-through path.

Cost of the transfer itself (cycles per block in a loop of 16 identical
blocks, compared with a block of `nop`s of the same shape; the loop adds
about 0.4 per block):

| Block | Cycles | Cost of the transfer |
|---|---|---|
| `nop` | 1.38 | — |
| `ibranch` not taken | 1.38 | 0 |
| `ibranch` taken, with or without a true condition | 7.38 | **+6** |
| `disp`, `nop 5`, `nop` | 8.44 | — |
| `disp`, `nop 5`, `ct` taken / `ct ? true` / `ct ? false` | 8.38–8.44 | 0 |
| the same with `disp …; ipd 0` | 8.38 | 0 (the target is cached) |
| `disp`, `nop 5`, `call` to a leaf that prepares `return` on entry and returns 6 cycles later | 16.38 | 0 beyond the 8 + 8 cycles of the two instruction sequences |

So a prepared transfer issued at least its delay after the prepare costs
nothing, calls and returns included, while a taken `ibranch` always
costs 6 cycles, forward or backward (monitors: `ibranch_fwd` 1
instruction in 7 cycles, a forward and a backward one 2 in 14).

A taken `ibranch` **hides an interlock stall in its own instruction**:
a load used one instruction later stalls 4 cycles (2 instructions, 6
cycles); with a taken `ibranch` in the consumer's instruction the pair
takes 8 cycles, not 12, while a prepared `ct` there adds nothing and the
stall is paid (monitors, `stall_*`). So padding with `nop` before an
`ibranch` back edge doesn't pay, as the proof-of-concept backend found
(`docs/spec-poc-findings.md` I4); with a `ct` back edge it does. A compiler should prepare every likely-taken transfer
early (5 cycles before for `disp`, 9 for `movtd`) and keep `ibranch` for
rarely taken branches or when no CTPR is free.

### Instruction fetch [hw: monitors]

The `IB_NO_COMMAND` monitor counts cycles with no instruction ready to
issue ([monitors.md](monitors.md)). In loops that fit the instruction
buffer (`tools/isa-probes/pmu.c`, `fetch_*`, `cmpib_*`, `wide*`,
`ctipd_*`) only two things produce them:

- a **taken `ibranch`: 6 cycles**, forward or backward, wherever its
  condition comes from;
- a **taken `ct` whose CTPR is not ready yet**: the wait (5 cycles after
  `disp`, 9 after `movtd`, minus the distance) is counted here too.

Free: a not-taken `ibranch` (it only waits, exactly, for its condition:
compare → `ibranch` 3), a not-taken `ct`, any prepare (`disp` with `ipd`
0, 1 or 2, `return`), the `ipd` of the transfer's own instruction, and
straight-line code of wide instructions (up to 40 bytes each, one per
cycle, also right after a taken branch).

Whole programs agree (the 4C, CoreMark, `tools/isa-score/profile.py`):
gcc-claude's code branches with conditional `ibranch`; 6 cycles per
taken `ibranch` (sampled: 377 M of them) predicts 2.14 G cycles, and
the monitor counts 2.07 G (11% of the run). Unipro's clang has no taken
`ibranch`; its 0.98 G bubble cycles (6%) are `switch` dispatches that
`ct` through a CTPR written by `movtd` one instruction earlier (8 cycles
early) and conditional `ct`s 2–3 cycles after their `disp`. lcc prepares
early and loses 1.3%. So: prepare every transfer that may be taken at
least its delay ahead, `movtd` 9 cycles ahead, and keep `ibranch` for
transfers that are rarely taken.

Not measured: targets that are not in the instruction cache (the effect
of `ipd` and `pref` there), v4 and v7, the 48C's different delays (lcc's
`-mset-prepare-ct-delays-e48c`, timing.md).

## v6 [hw: Elbrus-2C3]

Every cost above is the same on the 2C3 (timing.md, "Elbrus-2C3 (v6)").
v6 adds transfers through a register without a CTPR, run by
`tools/isa-probes/ctl6.c` (`gen_ctl6.py` writes `ctl6.s`):

| Operation | What it does | Cost |
|---|---|---|
| `ibranchd,0 %drN, %empty [? cond]` | jump to the address in `%drN` (ALC0; the condition is a ctcond, as for `ct`, not an ALU predicate) | 13 cycles for a one-instruction block: 12 beyond the instruction, however early the address is ready (`ibranch` 6, a `ct` prepared 5 ahead 0); a loaded address adds the load-to-use stall |
| `icalld,0 wbs = N, %drN, %empty` | call the address in `%drN`, `wbs` as for `call`; the callee returns normally and the caller's registers below `wbs` survive | 23 cycles a call with a trivial leaf, against 14 for `disp` + `call` |
| `disp %ctprN, L` and `ct %ctprN` in one instruction (gas allows it from v6) | **no transfer**: execution falls through; so does `movtd` + `ct` (timing.md) | 1 cycle |
| `fillr`, `fillc` | run in user mode (after a `flushr`) and return normally | |

So for indirect jumps and calls `movtd` 9 cycles ahead (or `disp` 5
ahead) plus `ct`/`call` stays the fast way; `ibranchd`/`icalld` only
save code where nothing else can fill the delay (MCST's binutils uses
`ibranchd` in v7 PLT entries). Remember the window rule: a callee's
`setwd` must not be smaller than what `wbs` passes it (SIGILL
`ILL_ILLOPC` at the callee's first instruction).

## How lcc uses it [lcc]

From `-O2` code for an if/else with calls, a counted loop, a loop with a
call, a `switch`, an early exit and an indirect call:

- Every prepare carries `; ipd 2`.
- Functions prepare their return in the first wide instructions
  (`return %ctpr3`) and return with `ct %ctpr3`, often under a condition
  for early exits (`ct %ctpr3 ? %pred0`). `%ctpr3` is also used for other
  transfers when the return is not yet prepared (`disp %ctpr3, g` for a
  tail call in a `switch`).
- Calls: `disp %ctprN, f` well ahead, then `call %ctprN, wbs = 4`;
  conditional calls (`call %ctpr1, wbs = 4 ? %pred0`) next to the opposite
  `ct` for if/else.
- After every call it prepares again whatever the rest needs (the loop's
  back edge, the next call, the return), as the hardware requires.
- Counted loops use loop mode: `disp %ctpr1, head` before the loop and
  `ct %ctpr1 ? %NOT_LOOP_END` with `loop_mode`, `alc`, `abn`
  ([loops.md](loops.md)).
- `switch`: the target is loaded from a jump table and moved with
  `movtd,0,sm` into `%ctpr1`; indirect calls the same way, 9 cycles before
  the `call`.
- `ibranch` is rare: one in the sample against 20 `ct`s.
