# Operation timing and pipeline hazards

Latencies, control-transfer preparation delays and interlock behaviour,
measured on hardware by `tools/isa-timing/bench.py`. The data is in
[timing-e2s.yaml](timing-e2s.yaml) (generated; do not edit).

Status: **[hw]** on an Elbrus 4C (E2S, elbrus-v3, 750 MHz), an
Elbrus-8C2 (E8C2, elbrus-v5, 1.26–1.5 GHz; data in
[timing-e8c2.yaml](timing-e8c2.yaml), section "Elbrus-8C2 (v5)" below)
and an Elbrus-2C3 (E2C3, elbrus-v6, 2 GHz; data in
[timing-e2c3.yaml](timing-e2c3.yaml), section "Elbrus-2C3 (v6)").
Every latency, hazard and control-transfer delay measured on the 4C and
the 8C2 is the same, except that v5 made `pfdivd` and `pfsqrttd` 2
cycles slower. **v6 changes the timing**: an L1 load takes 5
cycles instead of 3, an integer compare reaches a predicated operation
in 1 cycle instead of 2, `ldqp` goes through L1, and a few conflicts
are gone; everything else is as on v3 and v5. The sections below
describe v3 and v5; "Elbrus-2C3 (v6)" lists what differs.

## Method

- Cycles are read from `%clkr` (readable from user code, counts core
  cycles: 750.0 MHz against the wall clock).
- **Back-to-back cost**: a chain of dependent copies of one operation, one
  per wide instruction, so each waits for the previous result. Overhead
  is cancelled by the difference between chains of 64 and 32.
- **Scheduled latency**: the same chain with each operation followed by
  `nop`s so the next one issues `gap` cycles later; the latency is the
  smallest gap at which a step costs exactly `gap` cycles (no stall). This
  is the value a scheduler needs. The loop adds a few tenths of a cycle
  a step, taken as the smallest excess over `gap` among a chain's gaps
  (a stall adds at least 3); on the 4C it is sometimes 0.46–0.5 for a
  whole run.
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
| FP multiply (`fmuld`, `fmuls`, `pfmuls`) with a denormal operand or a tiny result; adds and divides don't change (semantics.md, "Rounding modes, flush to zero and denormals are zero") | — | 5 | — |
| combined FP (`fmul_addd`), through any of its three operands (all are read at issue: the addend too, `pocchk2.c`, 4C, 8C2, 2C3) | 8 | 8 | 9 |
| FP min/max (`fmaxd`, `fmind`, packed `pfmaxd`) | — | **2** (also on the 8C2) | — |
| `fdivs` | 11 | 11 | 13 |
| `fdivd` | 14 | 14 | 17 |
| `fsqrts` | — | 12 | 13 |
| `fsqrtid` → `fsqrttd` or any FP op; `fsqrttd` → FP op (one double `sqrt`: 25) | — | 11; 14 (13; 16 to an integer op) | 13; 17 |
| `frcps`, `frsqrts` (12-bit estimates, semantics.md) | — | 6 (8 to an integer op) | 9 |
| `sdivs`, `sdivd`, `udivs`, `udivd` | — | **11 to 18, by the quotient's size** (below); 13 for small quotients to an integer op | 13 |
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
| conversions (`fdtoid`, `idtofd`, `fstofd`, `fdtofs`, `fstoid`, `fstoidtr`, `fdtoidtr`, `fdtoistr`, `istofd`, `istofs`, `idtofs`) → an op reading the result in its format | — | 4 (5 or 6 in another format, see Formats) | — |
| `fdtoid` → integer op | — | 6 | — |
| integer op → `idtofd` | — | 1 | — |
| FP compare into a register (`fcmpeqd`) | — | 2; 4 to an integer op | — |
| `lzcnts`, `scls`, `getfs` | 1 | 1 | — |
| `sxt`, `merged`, `popcntd`, `lzcntd`, `bitrevd` | 1 | 1 | — |
| `umulhd` (high half of a 128-bit product) | 4 | 4 | — |
| `smulx`, `umulx` (32 × 32 → 64) | — | 4; 6 to an integer op | — |
| `insfs`, `insfd` (field insert) | — | 1; 2 across clusters | — |
| `sdivx`, `udivx`; `smodx`, `umodx` (64 ÷ 32, ALC5) | — | 11; 10 (13; 12 to an integer op) | — |
| `mpsadbh` | — | 2; 4 to an integer op | — |
| `fsubs`, `fsubd`; `fmaxs` | — | 4; 2 | — |
| round to integral `fdtoifd`, `fstoifs` | — | 4 | — |
| `fscaled`, `fscales` (v4+, Elbrus-8C2) | — | 4 | — |
| x86 flag forms (`adds_fw`, `subs_fw`, `ands_fw`, `incs_fw`, `shls_fw`, `addd_fd`, `subd_fd`, `incd_fd`, `shld_fd`, `sard_fd`, in ALC0 or ALC3); `smulx_fw`, `umulx_fw` | — | 2, also to an integer op or `cctop`; 4 | — |
| `cctop{o,b,e,l}` → predicated ALU op or `merge` | — | 2 (as `cmpesb`; 1 on v6) | — |
| `abgd`, `abgi` → a read of `%g24`–`%g31` under the new rotation (v3–v5) | — | the next instruction (`native.c`) | — |
| `rws %bgr` → a read under the new rotation (v3–v5) | — | 10, or the next instruction after `wait all_e` (`native.c`) | — |
| 80-bit `fxaddxx` / `fxmulxx` / `fxdivxx` | — | 5 / 4 / 16 | — |
| `fxaddxx` → integer op | — | 7 | — |
| `movfi` → `movif` (unpack and repack an 80-bit value) | — | 4 | — |
| `movif` → `movfi` (pmu.c) | — | 4 | — |
| `movif` → integer op, `movfi` → integer op (pmu.c) | — | 6 (a consumer 2 cycles after stalls 8, not 4) | — |
| `getsp` → integer op (pmu.c) | — | 2 | — |
| `fdtofx` → `fxtofd` | — | 5 | — |
| 80-bit with a single or double result (`fxadddd`, `fxmuldd`, `fxmulxd`); `fxdivdd` | — | 5; 18 | — |
| `fxtoid`, `idtofx`, `fstofx` | — | 5 | — |
| 80-bit square root steps `fxsqrtixx`, `fxsqrtuxx`, `fxsqrttxx` (ALC5); lcc's chain of the three | — | 11, 16–17, 16 | 53 a chain (double `fsqrtid` + `fsqrttd`: 30) |
| compare → `andp` (reading the `%pred`) | 1 | 1 | — |
| `andp` → `andp` | 0.5 | 0 inside one wide instruction (`@p4` → next CLP), 1 through a `%pred` | — |
| `andp` → predicated ALU op | 2 | 2 | — |
| `andp` → `ct ? pred` | 3 | 3 | — |
| FP compare (`fcmpltdb`) → predicated ALU op | — | 4 | — |
| FP compare → `ct ? pred` | — | 5 | — |

**Integer divides take longer for larger quotients** (`pocchk.c`, the
smallest gap from a divide to its use that costs no stall; 4C, 8C2 and
2C3 alike, signed and unsigned alike): 11 cycles for quotients below
2^8, then about one more per 8–10 bits: 12 (2^10–2^17), 13 (2^20), 14
(2^27), 15 (2^37), 16 (2^47), 17 (2^57), 18 (2^63); `sdivs` 11–14 up to
2^31. `bench.py`'s 11 is the small-quotient case (its chain divides 1
by 1). A scheduler can't know the quotient; 13–15 covers quotients up
to 2^37. FP divides don't vary (`fdivd` 14 whatever the operands). The
quotient arrives on the FP side, like an FP result: an integer operation
waits 2 more, in either cluster (`sdivd` → `addd` 13 in ALC0 or ALC5),
an FP-side one doesn't (`sdivd` → `idtofd` 11; "Every v3 operation",
below).

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
- Conversions take 4 to a consumer that reads their result in the
  format they produce (and 6 to integer operations); the 5 (`fdtofs`
  6) recorded earlier came from chaining a conversion into itself,
  which reads its result in another format (Formats, below).
- FP compares into predicates are two cycles slower than integer
  compares (4 to a predicated operation, 5 to `ct`).
- Loads beyond L1 are not scheduled for (see Memory).

## Every v3 operation [hw]

`bench.py --only v3op` chains every v3 computational operation that had
no scheduled latency (807 chains, generated from `binutils-ops.yaml` as
the v5 ones are): the result feeds the same operation, or a conversion,
`merge` or `cctope` that reads it in the format it was produced in, and
each FP-side result also goes to an integer operation in its own
channel. Combined integer operations are chained through each operand.
The 4C, the 8C2 and the 2C3 agree on every chain except where noted
(v6: an integer compare or `cctop` reaches `merge` in 1, as above; v5
and v6: `pfdivd` and `pfsqrttd` 16). The data is in the three YAML
files (`v3op_*`, `v3st_*`).

| Latency to its own kind | To an integer op | Operations |
|---|---|---|
| 1 | — | scalar logic, shifts and rotates (`scl`, `scr`), `getfd`, `merges`, `popcnts`, `bitrevs`, `sub` |
| 1 | 4 | every packed integer operation except the multiplies: add and subtract (also saturating), compares, min/max, averages, shifts (`psllqh` …), packs and unpacks, `pshufh`, `pshufw`, `pmerge`, `pinsh`, `pextrh`, `pmovmsk*`, `psign*`, horizontal add and subtract, logic |
| 2 | 2 | the x86 flag forms (`*_fb`, `_fh`, `_fw`, `_fd`); combined integer operations through any operand (`and_addd`, `add_xord`, `merge_addd` …); an integer compare, `cmpand*` or `cctop*` → `merge` (v6: 1) |
| 2 | 4 | FP compares into a register (`fcmp*s`, `fcmp*d`, `pfcmp*`), FP min/max (`fmins`, `pfmax*`, `pfmin*`), packed multiplies (`pmulh*`, `pmadd*`, `pmulubhh`), `phminposuh` |
| 4 | 4 | the FP flags compares `fcmp{ud,od}{s,d}f` (to `cctop` as to any integer op) |
| 4 | — | FP compares into a predicate (`fcmp*b`, `fxcmp*b`) → predicated op or `merge` |
| 4 | 6 | FP add, subtract, multiply, scalar and packed (`pfhadds`, `pfaddsubs` …); conversions, also packed (`pfdtois`, `pistofs` …) and round to integral, to a consumer of their format; `smulhd`, `umulhd`, `umulx`; the multiply flag forms (`smulx_fw` …); 80-bit `movx`, `movxa`, `movxc` and `fxmul*` with an 80-bit result |
| 5 | 7 | 80-bit add, subtract and `rsub`, `fxmul*` with a single or double result, conversions to and from 80-bit (`fxto*`, `*tofx`) |
| 6 | 6 | the 80-bit flags compares `fxcmp{ud,od}*f` (to `cctop` as to any integer op) |
| 6 | 8 | `frcps`, `frsqrts` |
| 8 | 10 | combined FP, scalar and packed, every first and second stage (`fsub_rsubs`, `pfhadd_addsubs` …) |
| 10 | 12 | `smodx`, `umodx` |
| 11 | 13 | integer divides with small quotients (`sdivs`, `sdivd`, `udivs`, `udivd`, `sdivx`, `udivx`; 11–18 by the quotient, above), `fsqrtid`, `pfdivs`; the 80-bit `fxsqrti*` (to an FP op only) |
| 12 | 14 | `fsqrts`, `pfsqrts` |
| 14 | 16 | `fdivd`, `fsqrttd`; `pfdivd` and `pfsqrttd` on v3 (16 and 18 on v5 and v6) |
| 16 | 18 | 80-bit divides `fxdiv*`; the square root steps `fxsqrtu*` (`fxsqrtuxx` 16–17, 18 on v6) and `fxsqrtt*` (to an FP op only) |
| 19 | 20 | `fxdivt*`, timed with ordinary operands, for which they return invalid (semantics.md) |

What this adds to the table above:

- **Every FP-side result reaches an integer operation 2 cycles later**,
  in any channel and either cluster, and that includes the divider's
  integer results (13, not 11) and the 80-bit unit's (7). Packed integer
  results and FP compares into a register are the exceptions with +3,
  as before.
- **A double square root is 25 cycles**: `fsqrtid` 11, `fsqrttd` 14
  (`fsqrttd` reads `fsqrtid`'s result as its second operand, which it
  needs: semantics.md). The 12, "about 17" and 8 for `fsqrtid`,
  `fsqrttd` and `frcps` recorded before came from `sqrt.c`'s
  back-to-back chains, which include the stall (13, 17, 9).
- **`pfdivd` and `pfsqrttd`** compute what `fdivd` and `fsqrttd` do
  (semantics.md) and take as long on v3, but 2 cycles more on v5 and
  v6.
- `pextrh`'s own chain reads its 32-bit result as 64 bits; its 4 to
  `adds` is the cleaner figure. `pmovmsk*` were chained as packed
  integers (1), their result being an integer mask: 4 to an integer op.

**The divider.** Every ALC5 operation that divides, takes a root or
estimates a reciprocal shares one unit with `fdivd` (`v3div_*` kernels):
alternating with `fdivd` it costs what `fdivd` alone does (5 an
instruction), and two a cycle apart take 13 cycles a pair, as two
`fdivd`. Cycles per operation when one issues every `g` cycles:

| Operations | `g` = 1 | 2 | 3 | 4 | 5 | 6+ |
|---|---|---|---|---|---|---|
| `fsqrtid`, `udivs`, `sdivx`, `udivx`, `pfdivs`, `frcps`, `frsqrts`, `fxsqrtixx` (as `fdivs`) | 5 | 2 | 3 | 4 | 5 | `g` |
| `udivd` (as `sdivd`) | 5 | 3.4 (v5, v6: 2) | 3 | 4 | 5 | `g` |
| `pfdivd` | 5 | 2 (v5: 2.5) | 3 | 4 | 5 | `g` |
| `fxdivxx`, `fxdivdd`, `fxdivss` | 5 | 3.4 | 3 | 4 | 5 | `g` |
| `fxsqrtuxx`, `fxsqrttxx` | 5 | 3.4 | 3 (v5, v6: 4.4) | 4 | 5 | `g` |
| `fsqrttd` | 5 | 4 (v6: 2) | 5 (v6: 3) | 4 | 5 | `g` |
| `pfsqrts` (as `fsqrts`) | 7 | 2.8 | 3 | 4 | 7 | `g` |
| `pfsqrttd` | 5 (v5: 6) | 4 (v5: 3.6, v6: 2) | 5 (v5: 3.9, v6: 3) | 4 | 5 (v5: 6) | `g` |
| `smodx`, `umodx` | 9 | 10 | 11 | 12 | 9 | 10, 11, 12, 9 … |

`smodx` and `umodx` hold the unit for 9 cycles: anything issued sooner
waits in 4-cycle steps (alternating with `fdivd`, 7 an instruction; two
a cycle apart, 21 a pair, or 17 with `fdivd` either way round).
isa-score models all of these as divider operations, and the 9 cycles.

**State registers** (`v3st_*`). `rrs` and `rws` of `%pfpfr`, `%fpcr`
and `%upsr`, and `rrd %ip`, issue one a cycle. A read of the FP state
(`%pfpfr`, `%fpcr`, `%fpsr`) delivers its value 11 cycles later; `%upsr`
and `%ip` 2. Writes need no distance: an operation in the next
instruction sees a new `%pfpfr` or `%fpcr`, and an `rrs` of the flags
issued in the instruction after an FP operation waits nothing (its
result is late anyway). Two patterns serialise: a write after a read of
`%pfpfr` waits for the read (`rws`, `rrs` alternating: 10 cycles a
pair), and a write of `%pfpfr` does not overlap a divide in flight
(`fdivd`, `rws` alternating: 22 a pair; with `faddd` 2). So `fesetenv`
and friends cost a dozen cycles, and FP code should not read the flags
on a hot path.

**`wait`** (`v3wait_*`) costs 6–7 cycles even with nothing outstanding
(`all_c`, `ld_c`, `ld_c`+`st_c` 7; `st_c`, `all_e` 6). After the
operation it waits for, a pair costs: `std` then `wait st_c` (or
`ld_c`+`st_c`, `all_c`) 15 (v6 16), so a store completes about 14
cycles after issue; `ldd` then `wait ld_c`+`st_c` 8 (v6 11); `faddd`
then `wait all_c` 10, `all_e` 10 (v6 16); `fdivd` then `wait all_c`
20. A C11 fence or seq_cst store (atomics.md) therefore costs about 15
cycles after a store.

## Formats

An FP-side result read by an FP-side operation in another format than
the one it was produced in arrives later (`bench.py` chains
`<producer>_to_<consumer>`, the 4C and the Elbrus-8C2 alike):

| Result read as | Scheduled latency | Examples |
|---|---|---|
| its own format | 4 | `faddd` → `fdtoid`, `fdtoid` → `idtofd`, `fdtofs` → `fadds`, `fadds` → `fadds`, `qpfdtoid` → `qpidtofd` or `qpaddd` |
| a double or 64-bit integer read as a single (also packed singles) | 5 | `faddd` → `fadds`, `fstofd` → `fadds`, `fmuld` → `fmuls`, `fmuld` → `pfmuls` |
| a 64-bit integer read as a double | 5 | `fdtoid` → `faddd`, `fdtoid` → `fdtoid` |
| a single read as a double | 6 | `fadds` → `faddd`, `fadds` → `fdtoid`, `fdtofs` → `faddd`, `fdtofs` → `fdtofs` |

C code meets the slow cases rarely (a conversion reads its own input
format, and promotions are explicit `fstofd`), but self-chained
measurements do: the v5 conversions' 5 and 6 in the table of v5
latencies below are of this kind (with a consumer of their format,
`qpfdtoid` and `qpidtofd` take 4). The 64-bit → 128-bit penalty of v5
("Crossings with 128-bit values", below) may be the same effect.
`tools/isa-score` uses 4 for every conversion and does not model the
penalty.

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

**A store's data** waits like an FP operand, not an integer one
(`tools/isa-probes/pocchk2.c`, `t_fpst_*` and `t_ist_*`; a chain of
steps, each a producer of the next value and a store of the last one in
one instruction; 4C, 8C2 and 2C3 alike; found by the proof-of-concept
back end, I44):

| Producer → store data | Same cluster | Other cluster |
|---|---|---|
| `addd` | 1 | 1 (no crossing cost) |
| `faddd`, `fmuld` | 4 (not the 6 an integer op waits) | 6 |
| `fdivd` (ALC5) | — | 16 (14 + 2) |

A store's address operands pay the integer crossing (+1, as a load's
address does): two stores a cycle after the `addd`s that advance their
bases take 4.66 cycles an instruction when one base comes from the other
cluster (`t_st2_std_inc_*`).

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
alone), and so does every other ALC5 operation that divides, takes a
root or estimates a reciprocal ("Every v3 operation", above, which has
their patterns; `smodx` and `umodx` hold it 9 cycles). It is
pipelined, but some issue distances conflict and stall the later
instruction. Cycles per divide when one issues every `g` cycles:

| `g` | 1 | 2 | 3 | 4 | 5 | 6+ |
|---|---|---|---|---|---|---|
| `fdivs`, `fdivd`, `sdivs` | 5 | 2 | 3 | 4 | 5 | `g` |
| `sdivd` | 5 | 3.4 | 3 | 4 | 5 | `g` |
| `fsqrts` | 7 | 2.8 | 3 | 4 | **7** | `g` |

So a scheduler should keep divides 2 cycles apart (3 for `sdivd` and
`udivd` on v3, and for the 80-bit divides and square root steps) and
not issue `fsqrts` exactly 5 cycles after another. Fractions are
averages of a pattern of stalled and unstalled issues; the structure
behind them (and behind `fsqrts` at 5) is not known.

**Two stores in one instruction** take 2 cycles when their addresses
agree in bits 6–7 (the same 64-byte bank of four: 0, 8, 16, 32, 48, 256
or 512 bytes apart), and 1 cycle otherwise (64, 80, 96, 128, 192, 320,
384, 448 apart). `stw` pairs behave the same way. Alternating stores to
one bank in successive instructions cost nothing. **This holds only
when instructions store to the same addresses again** (`bench.py`'s
kernels store to two fixed places every instruction): in a run of 32
instructions storing to the same two places 8 or 256 bytes apart, a
pair costs 1.41 cycles on the 4C and 8C2 and 1.53 on the 2C3 (64 apart:
1); when the addresses move on every instruction, as in a stream loop,
two stores cost 1 cycle at any distance (8, 64, 256), and so do a store
and a load. AAU stores (`staad`) behave exactly as `std` in all of these
(`pocchk2.c` `t_st2_*`, `t_stld_*`; the proof-of-concept back end's
finding I28).

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

Compilers schedule loads for an L1 hit (3 cycles; 5 on v6), and so does
lcc outside pipelined loops. Inside them lcc issues the loads array
prefetch doesn't serve far ahead of their use: 40–90 cycles in 31 of the
155 v3 PolyBench kernels with such loads (mvt's column walk: 22
iterations at II 4), 40–76 on v5 and v6, 22–47 in CoreMark's
`matrix_test` (lcc 1.29.16 `-O3`; the proof-of-concept back end's P44).
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

| Access | 4C stream | 4C hot set after it | 8C2 stream | 8C2 hot set after it | 2C3 stream | 2C3 hot set after it |
|---|---|---|---|---|---|---|
| `ldd` | 4.8 | evicted (256 KiB: 16 → 121) | 2.22 | evicted, from L1, L2 and L3 | 1.42 | evicted (48 KiB too) |
| `ldd mas=0x20` | 4.4 | evicted (48 KiB partly kept) | 2.25 | evicted | 1.41 | 48 KiB kept, larger evicted |
| `ldd mas=0x40` | 7.6 | **kept** | 2.15 | evicted | 1.40 | **kept** (to 1 MiB) |
| `ldd mas=0x60` | 7.6 | **kept** | **1.77** | **kept** (48 KiB to 4 MiB) | 1.40 | **kept** |
| `std` | 12.5 | evicted | 4.0 | evicted | 4.3 | evicted |
| `std mas=0x20` | 12.3 | evicted | 4.0 | evicted | 4.3 | evicted |
| `std mas=0x40` | 11.4 | **kept** | 3.8 | evicted | **3.4** | **kept** |
| `std mas=0x60` | 12.9 | **kept** | **3.0** | **kept** | **2.9** | **kept** |

So the hints behave as APB's `dcd` does (apb.md): on the 4C, bypassing
L2 (`0x40`) or everything (`0x60`) keeps other data cached at the price
of about 1.6× slower streamed loads (stores cost nothing extra); on the
8C2 only `0x60` keeps it, and makes the stream faster too (loads 1.77
against 2.22 ns a line, stores 3.0 against 4.0), while `0x20` and
`0x40` act as plain accesses. For data read or written once, `0x60` is
the hint to use on v5 and `0x40` on v3; lcc's automatic `0x20` changes
little on either. On the 2C3 (v6; 2 MiB L2, no L3) every hint streams
loads at the same speed (1.4 ns a line), `0x40` and `0x60` keep the hot
set for loads and stores alike, and they make streamed stores faster
(3.4 and 2.9 against 4.3 ns): there either is free.

**Misaligned loads and stores** (`pocchk.c`, cycles per wide
instruction; any misaligned offset costs the same):

| | 4C (v3) | 8C2 (v5) | 2C3 (v6) |
|---|---|---|---|
| 4 `ldd` in one instruction, aligned / misaligned | 1.0 / 5.7 | 1.0 / 6.0–6.8 | 1.0 / 1.0 |
| 2 `std`, aligned / misaligned | 1.0 / 8.0 | 1.0 / 8.0 | 1.0 / 1.0 |
| load → address of the next load, aligned / misaligned / crossing a 64-byte line | 4.7 / 8.3 / 11.9 | 4.7 / 11.9 / 15.5 | 4.7 / 4.7 / 4.7 |

So on v3 and v5 a misaligned access costs several cycles (a misaligned
load about 4 more latency, 7–11 across a line), and peeling or
versioning for alignment pays; v6 handles misaligned accesses at full
speed. (lcc keeps `-mstrict-align` up to v4, source-compat.md; the
8C2's misaligned accesses work but are no faster than the 4C's.)
16-byte accesses (`memx.c`; semantics.md, "Memory, tag and descriptor
operations"): 32 `ldq` take 33 cycles aligned or at +8 and 133 at +4 or
+1 on the 8C2, 32 `stq` 41 and 297; misaligned `ldqp`/`stqp` are SIGBUS
there. On the 2C3 all four take the aligned time at any offset.

**Store to load** (`tools/isa-probes/stld.c`, 4C and 8C2 alike): a load
of a doubleword just stored, issued in the **very next** instruction,
returns its value about 7.6 cycles late (two 4-cycle stall steps: a
store, reload and use take 14.5 cycles a step instead of 6.9). The load
itself issues at once; only its result is delayed, and only when it is
used. Any gap of one cycle or more between the store and the load
removes the penalty completely. (A store's base or data register
written by the instruction before costs nothing: `pocchk2.c`
`t_stbase_k*`, `t_stdata_k*`, all three machines.) The match is by aligned doubleword: a
narrower or wider reload, and `stw` at +0 followed by `ldw` at +4 (no
overlapping bytes), all pay it; the next doubleword, the next line or
any other address doesn't. So keep a reload of a just-stored location
(a spill and its fill, a stack argument) at least one instruction away
from the store.

**Loads whose value is never read** (`pocchk2.c` `t_dead_*`, the
proof-of-concept back end's finding I37): a step of four independent
`ldd` to lines that miss the caches, ten empty cycles, then the four
registers (A) overwritten, (B) left alone, or (D) read; (C) is the step
without the loads. Cycles a step, the best of three sweeps of 256 MiB:

| Case | 4C, each load on a new page | 4C, 576 bytes apart | 8C2, new pages | 8C2, 576 apart | 2C3, new pages | 2C3, 576 apart |
|---|---|---|---|---|---|---|
| A: loads, registers overwritten | 102 | 43 | 12 | 13 | 17 | 14 |
| B: loads, registers left | 98 | 43 | 11 | 12 | 17 | 14 |
| C: no loads | 11 | 11 | 11 | 11 | 11 | 11 |
| D: loads, values read | 202 | 136 | 40 | 172 | 219 | 184 |

So a write of a pending load's register doesn't wait for the load (A ≈
B), the 8C2 and 2C3 overlap independent misses (four dead misses cost
them 0–6 cycles), and the 4C doesn't: four misses nobody reads cost it
about 32 cycles, 87 with TLB misses. On v3 a speculative load that
misses is not free even when its value turns out dead.

## Array prefetch

Measured by `tools/isa-probes/apb.c`, not `bench.py`; details in
[apb.md](apb.md#timing-hw). A `mova` result is ready 5 cycles after
issue, in any channel and for a consumer in either cluster; a consumer
issued earlier stalls a fixed 4 cycles, as above. The first `mova` after
`bap` issues without a stall 22 cycles after it when the data is in the
caches.

**One area delivers 16 bytes a cycle** (32 on v6, Elbrus-2C3 below). lcc's `loop_mode` loops over
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
| an operation **under a false predicate** → a reader of its destination | **no wait** | a reader one cycle after an `ldd` or `muld` whose predicate is false runs as if that operation wrote another register (2.0 cycles a step, against 5.9 and 9.8 when it executes; `pocchk.c`, 4C, 8C2, 2C3): the interlock follows the predicate, so the other arm of an if-converted if-else costs nothing |
| compare → predicated ALU op, `ct ? pred`, `ibranch ? pred` | yes | correct at distance ≥ 1 |
| compare or predicate logic (`pass`/`landp`/`pass`) → predicated op or `merge` in the wide instruction of a **taken return** (`ct %ctpr3` after `return`) | **no** | at distance 1 the op reads the predicate as it was before the compare or logic, with no stall (in the 2026-10-10 runs an `addd` reader after a compare got the new value instead in 4 of 4 runs on the 4C and 1 of 4 on the 8C2, so the outcome depends on timing; `merge` readers, and readers after predicate logic, always got the old one); at distance 2 it reads the new value (`i17l_*` for predicate logic: 4C, 8C2, 2C3; the proof-of-concept back end's finding I38). A taken branch (`ct %ctpr1`, near or far), a `call`, a `ct` that is not taken and no transfer all wait (`tools/isa-probes/preds.c`, `i17_*`; 4C and Elbrus-8C2; the proof-of-concept back end's finding I17, where a `merge` in a return's instruction broke expat). Keep a compare 2 instructions before any predicate reader in a return's instruction. On v6 (the 2C3) distance 1 reads the new value. |
| compare → `andp`, `andp` → predicated ALU op | yes | correct at distance ≥ 1 |
| `andp` → `andp` in the same wide instruction (a CLP reading `@p4` written by an earlier CLP) | — | sees the new value: predicate logic chains inside one instruction |
| an operation in ALC K whose operand isn't ready, in an instruction with `pass %spredK` | **no wait, by design** | the instruction issues without waiting for ALC K: the operation sees a diagnostic operand (SIGILL without `,sm`, a diagnostic result with it) and `%spredK` is true; 1,000 iterations of a load and such a consumer one instruction later take 6,002 cycles against 10,001 without `%spred` (`preds.c` `spt_*`, 4C, 8C2, 2C3; [predicates.md](predicates.md)) |
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
monitor probe (`pmu.c`, cycles from `TICKS`), and of the v3 sweep's 807
chains all but `pfdivd` and `pfsqrttd` ("Every v3 operation", above).
So v3 latencies hold for v5, as lcc's documentation says for v1–v5.
What v5 adds or changes:

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
  | 5 | packed conversions (`qpfdtoid`, `qpfstoid`, `qpfstofd`, `qpfstois`, `qpidtofd`, `qpistofd`, `qpistofs`, and the `tr` forms), chained into themselves; 4 to a consumer of their result's format (Formats) |
  | 6 | `qpfdtofs`, `qpfdtois`, `qpfdtoistr`, `qpidtofs` (narrowing), chained into themselves, as scalar `fdtofs` |
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
- **Divider:** `sdivd` and `udivd` every 2 cycles no longer conflict (2
  cycles; v3 3.4); `fxsqrtuxx` and `fxsqrttxx` every 3 cycles now do
  (4.4; v3 3), `pfdivd` every 2 cycles takes 2.5, and `pfsqrttd` has a
  pattern of its own (6, 3.6, 3.9, 4, 6 at `g` = 1–5); the other
  divider distances are as on v3 (`fsqrts` still conflicts at 5).
  `pfdivd` and `pfsqrttd` take 16 cycles to an FP op, 18 to an integer
  op (v3 14, 16; `fdivd`, `fsqrttd` stay 14). An ALC5 FP operation
  never conflicts with a divide in flight (distances 1–16 for `faddd`
  and `fmuld`), except in one pattern: `fdivd` every 2 cycles with an
  ALC5 FP operation in every gap costs 2.76 cycles per pair instead of
  2 (probably what lcc 1.26's "FP operations in channel 5 need special
  scheduling" is about).
- **A store and a load in one 32-byte block** in one instruction take
  1.8–2 cycles in every run (the 4C sometimes runs them at 1); two
  stores agreeing in address bits 6–7 take 2, as on the 4C.

## Elbrus-2C3 (v6) [hw]

`bench.py --target v6.2c3` on an Elbrus-2C3 (E2C3, 2 cores, 2000 MHz,
`performance` governor; the kernels of the 4C and the 8C2 plus v6 ones,
`add_v6`), and the probes in `tools/isa-probes` run on it and on the
8C2 side by side. These stay as on v3 and v5: every integer, multiply,
divide, FP, conversion, packed and 128-bit latency (the `v5op_*`
chains: all 195 equal; the v3 sweep's 807 chains: all equal the 8C2's
but the integer compares into `merge`, below), the format penalties,
the crossing costs between clusters and between the integer and FP sides, the stall rule
(an early consumer waits in 4-cycle steps), write-back conflicts,
branch costs (a taken `ibranch` 6, a prepared `ct` 0), CTPR delays (`disp` 5, `movtd` 9, `return` 6), the divider's own
conflicts, the store bank rule (two stores agreeing in address bits
6–7 take 2 cycles) and the store-to-load penalty. What v6 changes:

- **An L1 load takes 5 cycles** (3 before), in every load channel
  (ALC0, 2, 3, 5), for every size (`ldb` … `ldd`, `ldq`), to integer,
  FP and 128-bit consumers, and **in either cluster** (v3 and v5 add 1
  across). A consumer scheduled 3 cycles after a load, as for v3, stalls
  a whole 4-cycle step: the load–use pair takes 7 cycles, not 5. A
  dependent pointer chase costs 5 a step on all three machines (on v3
  and v5 it is 3 + a stall). Loads still issue four per cycle.
- **`ldqp` goes through L1**: latency 5, four per instruction (v5:
  served by L2, 11, one per cycle). lcc's v6 code uses it for data
  (with `movaqp` in APB loops).
- **Integer predicates are faster**: an integer compare (`cmp*`,
  `cctop*`) or predicate logic (`andp` …) reaches a predicated ALU
  operation or a `merge` in **1** cycle (2 before); to a `ct` it is still
  3, and FP compares are unchanged (4 to an operation, 5 to `ct`).
  The v6 packed compares into a predicate (`pcmp*op`, `pcmp*ap`,
  `qpcmp*op`, `qpcmp*ap`) behave like FP compares: 4 to a predicated
  operation or `merge`, 5 to `ct`.
- **Operations new in v6** (`bench.py`'s `v6op_*` chains and the
  kernels below):

  | Latency | Operations |
  |---|---|
  | 1 | `qpcext_*`, `qpsrad`, `pmrgp`, `qpmrgp` |
  | 2 | `mkfsw`, `modbgv`, `vfbgv` (the x87-stack helpers) |
  | 4 | `clmull`, `clmulh` (ALC0, 1, 3, 4; four per instruction) |
  | 4 | packed compares into a predicate, to a predicated op or `merge` (5 to `ct`) |
  | 5 | `fma*`, `fms*`, `fnma*`, `fnms*` (single and double) and every `qpfma*` form, through any source, the addend included; 7 to an integer consumer and to the other cluster (FP side, + 2); six per instruction |

  A fused multiply-add (5) is thus cheaper than `fmul_addd` (8) and
  rounds once (semantics.md).
- **A compare before a taken return**: the predicate written by a
  compare, or by predicate logic (`landp`), one instruction before a
  `ct %ctpr3` that returns now reaches a predicated operation or `merge`
  in the return's instruction (v3 and v5 read the old value; Hazards,
  below).
- **`movtd` and `ct` through the same CTPR in one instruction**: the
  `ct` does not transfer (falls through); v3 and v5 transfer to the old
  target (`bench.py` hazard `movtd_ct`, distance 0).
- **Conflicts that are gone**: an FP operation in ALC5 next to a divide
  (`fdivd` every 2 cycles with `faddd,5` between: 1 cycle each, v5
  1.38), and divider distances that conflicted: `fsqrttd` and
  `pfsqrttd` every 2 or 3 cycles take 2 and 3 (v3 4 and 5). The store
  bank rule stays. `qpcheckd` with equal operands issues two a cycle
  (ALC0 and ALC3).
- **A store and a load in one 32-byte block** in one instruction still
  take 1 or 2 cycles, by run, as on the 4C: in four runs on 2026-10-12
  a pair in the cell's first block always took 2, one in the block at
  +0x100 took 2 in two runs and 1 in two (the 2026-10-06 run, 1 there,
  had looked like the conflict was gone).
- **`fxsqrtuxx`** takes 18 cycles (16–17; from a single or double,
  `fxsqrtusx` and `fxsqrtudx`, 16 as before).
- **`wait`** after a load or an FP operation costs more: `ldd` then
  `wait ld_c`+`st_c` 11 cycles a pair (v3, v5 8: the load's 5), `faddd`
  then `wait all_e` 16 (10); after a store 16 (15).
- **Store to load**: a reload of the same aligned doubleword in the next
  instruction still costs 7.6 cycles (lcc 1.26's note that v6 removed
  the store-to-load bypass shows no extra cost), but a load of *other*
  bytes of that doubleword no longer pays (`stw` at +0 then `ldw` at +4,
  `stb` at +0 then `ldb` at +1: 6.9, as for a load of another
  doubleword; `stld.c`).
- **Caches** (`--memory`, `/sys/devices/system/cpu`): 64 KiB L1D, 2 MiB
  L2, no L3; L2 hit 13 cycles (6.5 ns), memory about 220 cycles (110 ns).
- **Array prefetch** (apb.md): one area delivers **32 bytes a cycle**
  (16 on v3 and v5), so lcc's v6 loop reading two `movaqp` from one
  area runs an iteration per cycle (a sum of `long`s: 0.28 cycles per
  element, against 0.53 on the 4C and the 8C2); a `mova` result is
  ready after 5 cycles and an earlier consumer stalls 4; the first
  `mova` may issue 2 cycles sooner after `bap`; from L1 an area
  fetches a new 64-byte line every cycle (every 2 before), about one
  line a cycle per half of the buffer; from memory a line every 2.7
  cycles (3.2).

isa-score uses these numbers for v6 (and v7); `validate.py --target
v6.2c3 --cpu 0` puts every kernel within 1% of the 2C3 except two APB
loops (0.85 and 1.19), and `profile.py` CoreMark within 3% and Linpack
(size 200) within 1% (lcc `-O3` for the 2C3).

`pmu.c` on the 2C3 (monitors.md) agrees: of its 166 kernels, the 32
whose cycles differ from the 8C2's are the ones that use a load 3–4
cycles after it (+4 each), a `disp` and a false `ct` in one instruction
(2 cycles, 3 before) and 64-deep recursion (+20, chain-stack refill).

**What tuning for v6 is worth**: lcc's generic v3 code and its
2C3-tuned code (`-O3`), both run on the 2C3: CoreMark 4,411 against
5,266 iterations a second (1.19×), Linpack (size 200, 4,096 fixed
repetitions) 4.59 s against 3.39 s (1.35×). In the v3 CoreMark 30% of
the cycles are interlock stalls (`BUB_E2`), 2% in the tuned one: v3
code consumes loads 3 cycles out, and each such use stalls a 4-cycle
step on v6. isa-score with the v6 model predicts the v3 binary there
within 8% (measured / achieved 1.08). So a compiler that runs on v6
should schedule loads for 5 cycles even when it emits v3 code
(`-mtune`).

**v6 control transfers** (`ctl6.c`; control-transfer.md): a taken
`ibranchd` costs 12 cycles beyond its instruction however early its
target register is ready (13 a block, against 7 for `ibranch` and for a
`ct` prepared 5 cycles ahead); with the target loaded, a load-to-use
stall on top. `icalld` + return costs 23 cycles a call against 14 for
`disp` + `call`. `movtd` → `ct` still needs 9.

The 2C3's `CLOCK_MONOTONIC` (clock source `sclkr`) advances in steps
of about 330 ns between calls, so wall-clock timing of short loops
there reads 0; use `%clkr`, which counts the 2 GHz core clock.

## Differences between generations (lcc-doc)

lcc's changelogs name scheduling changes it made for later CPUs, which
`bench.py` should check when it runs there:

- v5: floating-point operations in channel 5 need special scheduling
  (lcc 1.26); measured above: one conflicting pattern with the divider.
- v6: the bypass from a store to a following load was removed (lcc 1.26),
  so a load that reads what a store just wrote waits longer. Not seen
  on the 2C3: the reload penalty is the 8C2's (Elbrus-2C3, above).
- Elbrus-48C (v7): prepare-to-transfer delays differ;
  `-mset-prepare-ct-delays-e48c` applies them and is on with
  `-mtune=elbrus-48c` (lcc 1.31).

## Not measured yet

- v4, v7; v6 on the Elbrus-16C (the 2C3 is the v6 machine measured).
- Loop mode registers other than `%lsr` (`%ilcr`); APB throughput.
- Throughput of 128-bit and packed FP operations, `mova`, AAU stores,
  and loads and stores of other sizes; whether the store bank rule
  involves physical address bits.
