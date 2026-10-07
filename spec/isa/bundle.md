# Wide instruction format

This describes how an E2K wide instruction (bundle) is laid out in memory,
as implemented by MCST binutils 2.41-29.023. Main sources:

- `opcodes/e2k-dis.c`: `unpack_instr`, `print_alf`, `print_ss`, `print_cs0`,
  `print_cs1`, `print_pls_syllables`, `print_cds`, `print_ctcond`
- `gas/config/tc-e2k.c`: the `e2k_hs` struct and the packing code that
  follows it

Status markers:

- **[checked]**: verified byte for byte against gas by
  `tools/isa-extract/check_encodings.py`
- **[decoded]**: `tools/isa-decode/e2kdis.py`, written from this document,
  decodes gas output identically to objdump over the corpus in
  `tools/isa-decode/corpus` (compiler output and hand-written bundles, v3–v7).
  This confirms the reading of binutils, not the hardware.
- **[source]**: read from binutils, not yet machine-checked

Every syllable is a little-endian 32-bit word. "Bits a–b" are counted from bit
0, the least significant.

## Overall layout [checked for the parts used by ALU operations]

A wide instruction is 8–64 bytes long, in multiples of 8. It has two parts:

1. **Before the middle point**, in this order, each present only if HS says
   so:

   | Syllable | Present when |
   |---|---|
   | HS | always |
   | SS | HS bit 12 |
   | ALS0 … ALS5 | HS bits 26–31 |
   | CS0 | HS bit 14 |
   | ALES5/ALES2 word | see below |
   | CS1 | HS bit 15 |

   `HS.mdl` gives the position of the middle point.

2. **After the middle point**: 16-bit half-syllables, then any gap, then
   32-bit syllables placed backwards from the end of the instruction.

   - **Half-syllables:** ALES0, 1, 3 and 4 first (those marked in HS), then
     the AAS half-syllables. They are paired into 32-bit words, and **the
     first half of each pair occupies the upper 16 bits** of its word. So
     half-syllable `k` sits at byte offset `mdl + 2*((k & ~1) + 1 - (k & 1))`.
     An odd count leaves an unused half.
   - **From the end, going backwards:** CDS0, CDS1, CDS2 (count in HS), then
     PLS0, PLS1, PLS2 (count in HS), then LTS0, LTS1, … The LTS syllables
     fill whatever space remains down to the half-syllables. Anything between
     them is gap.

### ALES2 and ALES5 [checked]

ALES2 and ALES5 have no half-syllable slots. When needed, they share one
32-bit word just before CS1: ALES5 in bits 0–15 and ALES2 in bits 16–31.

- If HS marks ALES2 or ALES5 but that word is absent, the value is the
  default `0x01c0` (opc2 = EXT, opce = NONE).
- gas omits the word whenever both values are default. Before v4 it never
  emits it at all, so on v1–v3, channels 2 and 5 can only take ALES2/ALES5
  with the default value.
- On v4, an operation with `explicit_ales25_v4` set forces the word to be
  emitted.

The presence test: the word exists if `mdl` equals the current offset plus 4
(no CS1) or plus 8 (with CS1).

## HS: header syllable [decoded]

| Bits | Field | Meaning |
|---|---|---|
| 0–3 | `mdl` | Middle point: number of 32-bit words before it, minus one |
| 4–6 | `lng` | Length in 64-bit units, minus one |
| 7–9 | `nop` | Empty cycles to insert after this instruction |
| 10 | `lm` | Loop mode (`loop_mode`) |
| 11 | — | Reserved |
| 12 | `s` | SS present |
| 13 | `sw` | Set by `set_mark` |
| 14 | `c[0]` | CS0 present |
| 15 | `c[1]` | CS1 present |
| 16–17 | `cd` | Number of CDS syllables |
| 18–19 | `pl` | Number of PLS syllables |
| 20–25 | `ale` | ALES0 … ALES5 present |
| 26–31 | `al` | ALS0 … ALS5 present |

An HS where everything except `lng` is zero is an empty instruction (`nop`).

## ALS: ALU operation syllable [checked]

| Bits | Field |
|---|---|
| 31 | `sm`: speculative mode (`,sm` in assembly) |
| 24–30 | `opc`: opcode |
| 16–23 | `src1`, or `opce` for formats without src1 |
| 8–15 | `src2` |
| 0–7 | `dst`, `src3` for stores, or a predicate destination |

### Operand encodings [checked for window registers and src3 literals; other literals decoded]

Applies to `src1`, `src2`, `src3` and `dst` bytes:

| Value | src1 | src2 | src3 | dst |
|---|---|---|---|---|
| `0xxxxxxx` | `%b[x]` rotating-base register | same | same | same |
| `10xxxxxx` | `%rx` window register | same | same | same |
| `110xxxxx` | 5-bit literal `x` | see next rows | literal forms (v7 `_lt3`) | special registers |
| `1100xxxx` | — | 4-bit literal `x` | — | — |
| `11010hll` | — | 16-bit literal: LTS`ll` (0 or 1), half `h` | — | — |
| `110110ll` | — | 32-bit literal: LTS`ll` | 32-bit literal (v7 `_lt3`) | — |
| `110111ll` | — | 64-bit literal: LTS`ll` and LTS`ll+1` | — | — |
| `111xxxxx` | `%gx` global register | same | same | same |

Special destinations (`110xxxxx`): 13 `%tst`, 14 `%tc`, 15 `%tcd`, 17–19
`%ctpr1`–`%ctpr3`, 30 `%empty.lo`, 31 `%empty.hi`. Other values are reserved.

The register's width (`%r`, `%dr`, `%qr`, `%qpr`) comes from the operation,
not from the encoding.

### Operand roles per format [checked except where marked]

`args` in `binutils-ops.yaml` lists the operand formats in this order:

| Format | Operands | Extra fields |
|---|---|---|
| ALOPF1, ALOPF11 | src1, src2, dst | ALOPF11: ALES = `opc2 << 8 \| opce` |
| ALOPF21 | src1, src2, src3, dst | src3 in ALES bits 0–7; ALES bits 8–15 = `opc2` |
| ALOPF2, ALOPF12, ALOPF22 | src2, dst | `opce` in the src1 field; ALOPF12 also has ALES; ALOPF22 has ALES `0x01c0` |
| ALOPF7 | src1, src2 → predicate | `cmpopce` in dst bits 5–7, predicate number in bits 0–4 |
| ALOPF17 | src1, src2 → predicate | ALES = `0x0200 \| cmpopce`; dst bits 5–7 are zero |
| ALOPF8 | src2 → predicate | src1 field = `0xc0` (none); `cmpopce` in dst bits 5–7 |
| ALOPF3, ALOPF13 | src1 (address), src2 (offset), src3 (data, in the dst byte) | stores; ALOPF13 also has ALES = `opc2 << 8 \| opce` |
| ALOPF11_LIT8 | src1, src2, 8-bit literal, dst | literal in ALES bits 0–7 |
| ALOPF12_PSHUFH | src2, 8-bit literal, dst | `opce` in the src1 field; literal in ALES bits 0–7 |
| ALOPF12_IBRANCHD, ALOPF12_ICALLD [decoded] | src2 (target), dst | v6+. `icalld` keeps `wbs` in the src1 field; the condition comes from `SS.ctcond` |
| ALOPF15 | src2 → state register | register number in the dst byte, `opce` in the src1 field, ALES `0x01c0` (`rws`/`rwd`) |
| ALOPF16 | state register → dst | register number in the src1 field, the table's `src2` value in the src2 field, ALES `0x01c0` (`rrs`/`rrd`) |
| ALOPF10, ALOPF19 | AAU store/load (`staa*`/`ldaa*`): `%aadD [ %aastiI + literal ]`; the data (store) is in the dst byte | ALS bits 8–23 form a 16-bit field: `lt` bits 0–1 (LTS number + 1, or 0 for none), `am` bit 2, `incr` bits 4–6, `ind` bits 7–10, `d` bits 11–15. ALES `0x01c0` (`0x02c0` for `staaqp`/`ldaaqp`) |
| AAURR, AAURW | AAU register read/write | Same opcodes as `ldaa*`/`staa*` with MAS `0x3f` in CS1; ALS bits 8–23 select the register (`registers.yaml`) |

When the table lists fewer formats than the struct's array holds, the
remaining operands are 32-bit (`s`), as C zero-initialises them. The
extractor pads `args` accordingly.

On v7, the `_lt3` forms (ALOPF21_LT_PACK_CODE) take src3 as a literal; gas
always encodes it as a 32-bit literal in an LTS (`110110ll`).

The operand formats in the table are not always exact: `plog`/`qplog` are
listed with 64-bit operands (`ARGS_DDDD`), although `qplog` works on 128-bit
packed registers (the older generator output has `ARGS_PPPP`). gas accepts
either register width.

### Operations that take two channels [checked]

Operations with a 128-bit (`q`) operand in certain positions occupy a pair of
channels. Which pair follows from the format and operand formats alone:

| Operation | Pairs |
|---|---|
| 128-bit result: ALOPF11 with dst `q`, or ALOPF2/12/22 with dst `q` | (0, 1) and (3, 4) if the operation is allowed in ALC1; otherwise (0, 2) and (3, 5) |
| 128-bit store data: ALOPF13 with src3 `q`; `staaq`, `aaurwq`, `ldaaq`, `aaurrq` | (2, 5) |

The operation may be written with either channel of its pair.

- **Before v7:** both channels are encoded with the same opcode. In the upper
  one, every `q` operand names the odd half (N+1) of the register pair.
  The exception is a `q` src1 of a load (ALOPF11 with MAS) or of a pair-(2, 5)
  operation, which is the same in both channels.
- **From v7:** only the lower channel is encoded; the upper one is reserved
  implicitly (offset 1, 2 or 3; see `implicit_paired_alc_delta` in
  `include/opcode/e2k.h`).

objdump hides the upper half only for a hard-coded list of names, so for
other paired operations (`movt{c,r,rc}q`, `cudtoap`, `ldod{w,r,p}q`,
`getsod`) it prints both channels.

## SS: stubs syllable [decoded for type 0, except `dtbp`]

Bits 0–9, 30 and 31 are common to both types. `type` is bit 20.

| Bits | Field | Meaning |
|---|---|---|
| 0–8 | `ctcond` | Control-transfer condition (see below) |
| 9 | `dtbp` | v7 |
| 10–11 | `ctop` | `ct %ctprN` with N = ctop; 0 means `ibranch` or `call` via CS0/CS1 |
| 12–15 | `aa0`–`aa3` | AAS half-syllables present (type 0 only) |
| 16 / 17 | `alct` / `alcf` | `alc`: advance loop counters |
| 18 / 19 | `abpt` / `abpf` | `abp`: advance the predicate rotation base |
| 20 | `type` | |
| 21 / 22 | `abnt` / `abnf` | `abn`: advance the register rotation base |
| 23–24 | `abg` | `abgi`, `abgd` |
| 25 | `crp` | |
| 26 | `vfdi` | |
| 27 | `srp` | `crp` and `srp` together encode `slrp` |
| 28 | `bap` | Begin array prefetch |
| 29 | `eap` | End array prefetch |
| 30–31 | `ipd` | Instruction prefetch depth |

Without an SS, `ipd` takes a default value other than 0 (binutils MCSTBug
#163454).

**Type 1 (SF2) [source; run]:** used from v6 for `abgc`/`rbgc`, which replace
`abg`. `abgc NUM` (1–7) puts NUM in bits 13–15 (the `aa1`–`aa3` positions of
type 0) together with a condition. objdump prints no type-1 fields, so this
can't be checked by decoding, but gas's encoding does what it should on
the Elbrus-2C3 (`ctl6.c`: `abgc N` adds N to `%bgr.cur`, also under a
predicate; loops.md, "v6").

### ctcond [decoded for types 0–9, 14, 15; type 11 source]

Bits 0–4 are `psrc`; bits 5–8 are the condition type:

| Type | Condition |
|---|---|
| 0 | never |
| 1 | always |
| 2 / 3 | `%predN` / `~%predN` |
| 4 | `%LOOP_END` |
| 5 | `%NOT_LOOP_END` |
| 6 | `%predN \|\| %LOOP_END` |
| 7 | `~%predN && %NOT_LOOP_END` |
| 8 | `%MLOCK`, optionally `\|\| %dt_al…` (psrc bits 0–3) |
| 9 | `%MLOCK \|\| ` a compare or CLP result (psrc encodes which) |
| 11 | Same as 9 without `%MLOCK` (v7 only) |
| 14 | `~%predN \|\| %LOOP_END` |
| 15 | `%predN && %NOT_LOOP_END` |
| 10, 12, 13 | Reserved (and 11 before v7) |

gas accepts conditions involving `%MLOCK` only on `ibranch`. In types 8, 9
and 11, `%dt_alN`, `%cmpN` and `%clpN` refer to operations in the same
bundle: a speculative operation in ALC N (true when it has a diagnostic
operand; [speculation.md](speculation.md)), a compare in ALC N, and the
CLP that produces `@p(4 + N)`.

## CS0: control syllable 0 [decoded, except `puttsd`, which gas does not accept]

Bits 30–31 are `ctpr` and bits 28–29 are `ctp_opc`. Together they select the
operation:

| ctpr \ ctp_opc | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| 0 | `ibranch` | `pref` | `puttsd` | `done` (also `iret`/`hret`/`glaunch`, chosen by `param` bits 0–2 = 2/3/4) |
| 1 | `disp` | — | `sdisp` | `gettsd` |
| 2 | `disp` | `ldisp` | `sdisp` | `gettsd` |
| 3 | `disp` | — | `sdisp` | `return` (`gettsd` if `param` bits 0–2 = 1) |

- **`disp`, `ldisp`, `ibranch`, `puttsd`:** bits 0–27 hold a signed
  displacement in 8-byte units, relative to the start of the instruction.
- **`sdisp`:** bits 0–4.
- **`pref`:** `%iprN` in bits 0–2, `ipd` in bit 3, displacement in bits 4–27.
- **`ibranch`, `done`, `iret`, `hret`, `glaunch`** need an SS with `ctop` = 0;
  the condition comes from `SS.ctcond`.

## CS1: control syllable 1 [decoded, except `vfbg`, which gas does not accept]

Bits 28–31 are the opcode:

| opc | Operation | Fields |
|---|---|---|
| 0 | `setwd` (+ `setbn`/`setbp`) | `setwd` takes `wsz` (bits 5–11), `nfx` (bit 4) and `dbl` (bit 3, v3+) from **LTS0** |
| 1 | `vfrpsz` + `setwd` (+ `setbn`/`setbp`) | `rpsz` in LTS0 bits 12–16 |
| 2 | `setei` / `setsft` | `setei`: value in bits 0–7. Bit 27 set means `setsft` |
| 3 | `wait` | `all_c` bit 0, `all_e` 1, `st_c` 2, `ld_c` 3, `fl_c` 4, `ma_c` 5, `trap` 6 (v2+), `sas` 7, `sal` 8 (v5+) |
| 4 | `setbn` / `setbp` | Applies to opcodes 0, 1 and 4: `setbn` (bit 26) uses `rbs` bits 0–5, `rsz` bits 6–11, `rcur` bits 12–17; `setbp` (bit 27) uses `psz` bits 18–22 |
| 5 | `call` | `wbs` in bits 0–6, target from `SS.ctop`. With `ctop` = 0, bits 7–9 = 1/2 select `icall`/`hcall`, and CS0 holds the displacement |
| 6 | MAS | 7-bit memory access specifiers for ALC0 (bits 21–27), ALC2 (14–20), ALC3 (7–13) and ALC5 (0–6) |
| 7 | `flushr` / `flushc` / `fillr` / `fillc` | Bits 0–3; `fill*` is v6+ |
| 8 | `vfbg` | `umask` bits 0–7, `dmask` 8–15, `chkm4` 16 |

## PLS: predicate logic syllables [decoded, except `%bgrpred`, which gas does not accept as a `pass` source]

PLS0 and PLS1 each contain two ELPs and one CLP. PLS2 contains only a CLP.

- **ELP** (7 bits; at bits 24–30 and 16–22) loads a primary predicate
  `@pK`:

  | ELP value | Source |
  |---|---|
  | `0x00` | `%lcntex` |
  | `0bbbbb`, nonzero | `%spred`, with bits selecting ALC compare results 0–4 |
  | `0x40` | `%bgrpred` |
  | `0x41`–`0x4f` | `%rndpred1`–`%rndpred15` |
  | `11nnnnn` | `%predN` |

- **CLP/MLP** (bits 0–15) combines two primary predicates:

  | Bits | Field |
  |---|---|
  | 14–15 | opcode: 0 `andp`, 1 `landp`, 3 `movep` |
  | 13 | neg0 |
  | 10–12 | p0 |
  | 9 | neg1 |
  | 6–8 | p1 |
  | 5 | vdst: also write the result to a register predicate |
  | 0–4 | pdst: the destination `%predN` |

  The result goes to `@p(4 + i)`. A CLP may read the result of another
  CLP of the same instruction, **two levels at most**: gas rejects a
  third ("maximal supported cascading of 2 exceeded for {C/M}LP2",
  `tc-e2k.c`, `verify_pls_insns`, citing MCST's iset description), so
  four conditions in one instruction are a tree, `landp (landp (c1, c2),
  landp (c3, c4))`; lcc never chains three. Run on the 4C and the
  Elbrus-8C2 for all 16 inputs (`preds.c`, `tree_*`).

## CDS: condition syllables [decoded for RLP and MRGC on ALC0–5]

Each CDS holds two 16-bit RLP/MRGC entries. The second entry is printed
first.

| Bits | Field |
|---|---|
| 14–15 | Opcode: 0 RLP for ALC0–2, 1 RLP for ALC3–5, 2 MRGC for ALC0–2, 3 MRGC for ALC3–5 |
| 10–13 | Mask: which of the three channels the entry applies to. Bit 3 is AM2/AM5 (the AAU increment of `staa`) |
| 7–9 | Per-channel negation |
| 0–6 | Predicate: `10nnnnn` `%pcntN`, `11nnnnn` `%predN` |

## AAS: array access syllables [decoded]

Present only with an SS of type 0, selected by `SS.aa0`–`aa3`.

- **AAS0 and AAS1** each hold two 8-bit destinations. The order is swapped:
  movement j=0 is in the upper byte.
- **AAS2–AAS5** each describe one movement:

  | Bits | Field |
  |---|---|
  | 15 | `be` |
  | 12–14 | opc: 1 `movab`, 2 `movah`, 3 `movaw`, 4 `movad`, 5 `movaq`, 7 `movaqp` |
  | 6–10 | `area` |
  | 1–5 | `ind` |
  | 0 | `am` |

## APB instructions [hw]

An array prefetch program is a sequence of 16-byte APB instructions, not
wide instructions: a left half (APS0, APLS0) and a right half (APS1,
APLS1), each a 32-bit field word and a 32-bit `disp`. objdump prints the
halves as `apb` (API_L, API_R) and recognises them only when the bytes
don't parse as a wide instruction (`print_apb_instr`). Field layout,
semantics and generations: [apb.md](apb.md). Programs written by gas
from `fapb` lines ran as described there on the 4C.

## Bundling restrictions [source: gas]

gas rejects these combinations within one wide instruction:

- a store in ALC2 together with a load in ALC3;
- a store in ALC5 together with a load in ALC0;
- `disp` and `ldisp` together (both need CS0);
- two operations in the same channel;
- a load or store with a MAS together with `setwd`, `wait` or a `call`
  ("cannot encode MAS: CS1 syllable is busy"): the MAS goes in CS1,
  which those also use [checked].

### Two writes of one register [hw]

Two operations of one wide instruction may write the same register when
predicates enable at most one of them, as lcc does in if-converted code
(`addd,3 … %dr0 ? ~%pred0` with `addd,4 … %dr0 ? %pred0`); the enabled
one's value lands, for two ALU operations in one cluster or across
clusters and for a load against an ALU operation. With both enabled the
higher channel's result is left: ALC1 over ALC0 in either source order,
ALC3 over ALC0, an ALC1 `addd` over an ALC0 `ldd` although the load
completes later (`tools/isa-probes/pocchk.c`; 4C, 8C2 and 2C3 alike).
gas accepts all of these without a warning; don't rely on the
both-enabled order.

### Memory accesses in one wide instruction [hw]

Memory accesses to one address in one wide instruction are **not**
simultaneous: on the 4C (`tools/isa-probes/samewi.c`, 1,000 runs each,
also found by the proof-of-concept backend, `docs/spec-poc-findings.md`
I9 and I12) they take effect in the order ALC2 before ALC5, and a load
before a store in the same cluster:

| Load + store of one doubleword | The load returns |
|---|---|
| load ALC0, store ALC2 | the old value |
| load ALC3, store ALC5 | the old value |
| load ALC2, store ALC5 | the old value |
| load ALC5, store ALC2 | **the new value** (also when the store writes only part of it, `stw` into the upper half) |
| load ALC3, store ALC2; load ALC0, store ALC5 | gas rejects these |

Two stores to one address: the **ALC5 store wins**, whichever value
it writes. So a compiler may put a load and a store that may alias in
one instruction only when the load is meant to see the old value and
is not in ALC5 with the store in ALC2 (gas chooses channels for
operations written without `,N`, so in practice: keep them apart), may
never put a load that must see a store in the same instruction as the
store, and should not put two stores that may alias in one instruction
unless it places them in channels. Other cores may see two stores of
one instruction in either order (atomics.md). A failed check load in
ALC5 also sees an ALC2 store of its own instruction (I12).

Channel lists in `binutils-ops.yaml` don't depend on the operands, but
one rule does: `movtd` and `getpl` **into `%ctpr1–3` go only in ALC0**
(gas: "may be encoded in the 0-th channel only", citing every
`iset-vX.single`; checked for v3 and v6), while into an ordinary
register they take ALC0, 1, 3 or 4.

## Assembler syntax notes [decoded]

- **Store operand order.** By default gas reads stores as *data, address,
  offset*. The directive `.ignore ld_st_style` switches to *address, offset,
  data*, which is the order objdump prints and the order Unipro's clang emits
  (it puts the directive at the top of every file).
- **`%empty`** is gas's name for the `%empty.hi` destination (`0xdf`). Unipro's
  clang writes `%empty.hi`, which gas rejects.

What a compiler can rely on (MCST gas 2.41-29, checked by assembling and
disassembling; the first four also by the proof-of-concept backend,
`docs/spec-poc-findings.md` I3):

- **Literals.** A bare constant, symbol or relocation operator is
  accepted as src2 without the `[ _f64,_lts0 ... ]` decoration
  (`0x12345678`, `-1`, `sym`, `sym+8`, `sym@GOT`, `sym@GOTOFF`,
  `x@TLS_LE`, `x@TLS_IE`, `x@TLS_GDMOD`, `x@TLS_GDREL`); gas picks the
  literal syllables and the smallest form (`-1` becomes `_f16s 0xffff`,
  a symbol `_f64`).
- **Register widths.** The width prefix is ignored for operands whose
  width the operation fixes: `addd %r0, %r1, %r2` assembles as `addd %dr0,
  %dr1, %dr2`.
- **Channels and braces.** Without braces each operation is its own wide
  instruction; inside braces gas still assigns channels to operations
  written without `,N`. `nop N` inside braces sets the header's `nop`.
- **Operands.** `merged a, b, dst, ~%predN` takes a negated predicate;
  `getsp %rN, %drM` and `getsp 0, %drM` (read the stack pointer) work; a
  constant address works as `ldd 0x0, sym, %dr4` / `std 0x0, sym, %dr4`.
- **What lcc emits** (1.29.16): `.ignore ld_st_style` and `.ignore
  strict_delay` at the top of every file; `.type sym, #function` /
  `#object` (SPARC style); `.align` in bytes; 64-bit data as `.uadword`
  (constants) or `.dword` (relocated values such as pointers), 32-bit as
  `.uaword`; strings as `.ascii`. gas also accepts `.quad` and `.8byte`.
  lcc emits no `.machine` or other target directive: the target reaches
  gas only on its command line (`e2k-linux-as -mcpu=elbrus-v3 -mptr64`),
  which also sets the ELF `e_flags` ([elf.md](../abi/elf.md)).
  Every object gets a weak absolute marker symbol naming the compiler
  (`elbrus_optimizing_compiler_v1.29.16_Jan_22_2026 = 0x0`); nothing
  depends on it.

## Known gas bugs [checked: gas 2.41-29.018, ours and lcc's; 2.41-29.023 native on the 4C and the 8C2]

- **Internal error on a label after a short label difference.** A label
  defined in `.text` after a `.rodata` `.short` or `.byte` holding the
  difference of two `.text` labels makes gas abort ("Internal error in
  check_current_insn_or_label_alignment at tc-e2k.c:9164"), even after
  an `.align 8` and a following function's label; a 4-byte `.long`
  difference, or no later label, is fine. GCC emits such tables for
  narrowed computed-goto offset tables and with `-g` its end-of-text
  label follows them (gcc.c-torture/compile/labels-3.c; found by the
  proof-of-concept backend, I8); a port should keep case-vector and
  label-difference tables at 4 bytes (`CASE_VECTOR_SHORTEN_MODE`).
  lcc never emits such tables.

## Known objdump bugs

- `staa*`/`ldaa*` without a memory specifier are printed as `aaurw*`/`aaurr*`
  with `%unknown_aau_reg`, and the `incr` that may accompany a `staa` is
  dropped. So objdump can never show `staa` or `incr` correctly, and their
  encodings are marked [source] here. `print_alf` accepts the AAU register forms when
  ALES is `0x01c0` *or* MAS is `0x3f`; it should require MAS `0x3f`.
