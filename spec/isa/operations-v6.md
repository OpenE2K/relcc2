# Operations added in v6

What `elbrus-v6` (Elbrus-16C, -2C3) adds to
[operations-v5.md](operations-v5.md); v7 is in
[operations-v7.md](operations-v7.md). **Generated** by
`tools/isa-extract/gen_operations.py` from `operations-v6.yaml`; the
columns are those of [operations-v3.md](operations-v3.md) (the "lcc
v3 / v5" column is empty here: those sysroots predate v6).

Every v6 ALU operation except the register branches and calls was run
on an Elbrus-2C3 by `sem6.c` (46 sets of 128-bit inputs) and timed by
`bench.py` (`v6op_*`): semantics.md, "v6 operations", and timing.md,
"Elbrus-2C3 (v6)". The community qemu matches the hardware on all of
them except the packed fused multiply-adds, which it gets wrong, and
six it doesn't implement.

## What v6 changes for a compiler

- **Loads take 5 cycles** to any consumer in either cluster (3, +1
  across, before), and `ldqp` goes through L1 (5 cycles, four a
  cycle; v5: 11, one a cycle). Schedule load consumers 5 cycles
  out: one at 3 stalls a 4-cycle step. lcc's 2C3 libraries use `ldqp`
  5,747 times (its v5 code loads 128 bits as two `ldd`).
- **Integer predicates are faster**: a compare or predicate logic
  reaches a predicated operation or `merge` in 1 cycle (2 before).
- **Fused multiply-add**: `fma*`, `fms*`, `fnma*`, `fnms*` and their
  `qp` forms round once, take 5 cycles through any operand (the
  combined `fmul_add*` round twice and take 8) and issue six a cycle.
  They are GCC's `fma<mode>4` family and what `-ffp-contract=fast`
  wants. lcc's 2C3 libraries use `fmad` 857 times and `fmul_addd` 27.
- **Vector loop exits**: a packed compare straight into a predicate
  (`qpcmpeqbop`: any lane equal) replaces v5's `qpcmpeqb` +
  `qpsgn2mskb` + compare; lcc's string functions use it (247
  `qpcmpeqbop`, 40 `qpcmpeqbap`). 4 cycles to a predicated operation,
  5 to `ct`.
- **APB**: an area delivers 32 bytes a cycle (16 before), so two
  `movaqp` from one area per iteration run at an iteration a cycle;
  after a trap outside a loop the areas keep their positions
  (apb.md, "v6").
- Smaller changes: a compare one instruction before a taken return now
  reaches the return's instruction; `movtd` and `ct` through one CTPR
  in one instruction don't transfer; `stmqp` takes any address; the
  divider no longer delays ALC5 FP operations; `abg` is gone (gas
  rejects it), replaced by `abgc`/`rbgc`.

## Scalar

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `clmull`, `clmulh` | 6–8 | 0,1,3,4 | Carry-less 64 × 64 product, low or high 64 bits (x86 `pclmulqdq`; checked against the community qemu on the 2C3). 4 cycles, ALC0/1/3/4, four a cycle. **Use:** CRC and GCM (`clmul`). | 4 (v6) |  | sem6 (2C3) |
| `fmas`, `fmad`, `fmss`, `fmsd`, `fnmas`, `fnmad`, `fnmss`, `fnmsd` | 6–8 | 0–5 | Fused a × b + c, a × b − c, −(a × b) + c, −(a × b) − c, rounded once (matches C `fma()` on every input). 5 cycles through any operand, 7 to an integer consumer or the other cluster; six a cycle. **Use:** `fma`, `fms`, `fnma`, `fnms<mode>4`; `fma()`; contraction (the combined `fmul_add*` round twice). | 5 (v6) |  | sem6 (2C3) |
| `ibranchd`, `icalld` | 6–8 | 0 | Branch or call to the address in a register (community qemu; not run). **Use:** Indirect jumps and calls without a CTPR preparation. |  |  | — |
| `mkfsw` | 6–8 | 1,4 | An x87 status word from a and b: C0, C2, C3 = bits 0, 2, 6 (CF, PF, ZF) of a, TOP = b mod 8, other bits 0 (what `fnstsw` returns after an `fcmpud*f`-style compare). **Use:** x87 code on the `%bgr` stack (binary translation); none in C. |  |  | sem6 (2C3) |
| `modbgv`, `vfbgv` | 6–8 | 1,4 | `%bgr` (x87 register stack) helpers: `modbgv` returned 0 for every input; `vfbgv` a, b writes 0 when b's low byte is 0 and raises SIGFPE otherwise (with `%bgr` at its initial 0x0ff). **Use:** Binary translation only. |  |  | sem6 (2C3) |

## Packed

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `pcmpeqbop`, `pcmpeqbap`, `pcmpeqhop`, `pcmpeqhap`, `pcmpeqwop`, `pcmpeqwap`, … (32 operations) | 6–8 | 0,3 | Into a predicate: true if the compare (signed for `gt`) holds in any lane (`op`) or in all lanes (`ap`). 4 cycles to a predicated operation or `merge`, 5 to `ct` (as FP compares); ALC0/3. **Use:** Vector loop exits (`any`/`all`) without `qpsgn2mskb` and a scalar compare; lcc's string functions. | 4 (v6) |  | sem6 (2C3) |
| `qpchecks`, `qpcheckd`, `qpcheckq` | 6–8 | 0,3 | Nothing when a = b (32, 64 or 128 bits); SIGBUS `BUS_OBJERR` at the operation when they differ. The destination keeps its old value. **Use:** A one-operation "trap unless equal" (assertions); lcc doesn't use it. |  |  | sem6 (2C3) |
| `pmrgp`, `qpmrgp` | 6–8 | 0,1,3,4 | Merge by a predicate, as `merged`: a when it is false, b when true (64 or 128 bits). 1 cycle. **Use:** `vcond`/`vec_select` on a predicate (lcc: `qpmrgp` 30 times). | 1 (v6) |  | sem6 (2C3) |
| `qpsrad` | 6–8 | 0,1,3,4 | Arithmetic right shift of both 64-bit lanes by the count b. 1 cycle. **Use:** `ashr` of `V2DI`, which v5 lacks (`qpsraw` stops at 32-bit lanes). | 1 (v6) |  | sem6 (2C3) |
| `qpcext_0x00`, `qpcext_0x7f`, `qpcext_0x80`, `qpcext_0xff` | 6–8 | 0,1,3,4 | 128 bits with b in the low half and the high half filled with the byte in the name. 1 cycle. **Use:** Widening a 64-bit value to 128 bits (zero, sign-style fills). | 1 (v6) |  | sem6 (2C3) |
| `qpfmas`, `qpfmad`, `qpfmss`, `qpfmsd`, `qpfnmas`, `qpfnmad`, `qpfnmss`, `qpfnmsd`, `qpfmass`, `qpfmasd`, `qpfmsas`, `qpfmsad` | 6–8 | 0–5 | Fused multiply-add per lane, rounded once; `mas`: add c in even lanes (lane 0 is the low one) and subtract in odd, `msa` the reverse (so x86 `vfmaddsub` is `qpfmsa*`). 5 cycles, six a cycle. The community qemu gets these wrong. **Use:** Vector `fma<mode>4` and friends; `vec_fmaddsub`. | 5 (v6) |  | sem6 (2C3) |

## Control

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `abgc`, `rbgc` | 6–7 |  | Global-register rotation forms that replace `abg` (gas rejects `abg` for v6; binutils; not run). |  |  | v6; source only |
| `fillr`, `fillc` | 6–7 |  | Fill the register window stack or the chain stack back from memory (the counterparts of `flushr`, `flushc`; not run). |  |  | — |
| `hcall`, `icall` | 6–7 |  | Hypercall and indirect call (binutils). |  |  | v6; source only |
| `iret`, `hret`, `glaunch` | 6–7 |  | Privileged returns from traps and the hypervisor, guest launch. |  |  | — |

