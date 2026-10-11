# Operations added in v6

What `elbrus-v6` (Elbrus-16C, -2C3) adds to
[operations-v5.md](operations-v5.md); v7 is in
[operations-v7.md](operations-v7.md). **Generated** by
`tools/isa-extract/gen_operations.py` from `operations-v6.yaml`; the
columns are those of [operations-v3.md](operations-v3.md) (the "lcc
v3 / v5" column is empty here: those sysroots predate v6).

Every v6 ALU operation was run on an Elbrus-2C3, by `sem6.c` (46 sets
of 128-bit inputs) and timed by `bench.py` (`v6op_*`), the register
branch and call and the user-mode control operations by `ctl6.c`:
semantics.md, "v6 operations", timing.md, "Elbrus-2C3 (v6)", and
control-transfer.md, "v6". The community qemu matches the hardware on all of
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
  lcc itself emits `fmad` for `__builtin_fma` always; under
  `-ffp-contract=fast` also in recurrences (Horner) and `restrict`
  loops (`fmsd`, `fnmad`), dropping `fmul_addd` there and in other
  loops; under `-ffast-math` also for `fma()` and in loops without
  `restrict`; never for a returned `a * b + c` or in reductions. At
  `-O3` it keeps `fmul_addd` and calls libm for `fma()`.
- **lcc's v6 code** otherwise differs little from its v5 code: in
  CoreMark 4% fewer wide instructions (fewer `ibranch`es and
  predicate operations), in Linpack 6% fewer (fewer `nop`s), with the
  same operations.
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
| `fmas`, `fmad`, `fmss`, `fmsd`, `fnmas`, `fnmad`, `fnmss`, `fnmsd` | 6–8 | 0–5 | Fused a × b + c, a × b − c, −(a × b) + c, −(a × b) − c, rounded once (matches C `fma()` on every input). 5 cycles through any operand, 7 to an integer consumer or the other cluster; six a cycle. **Use:** `fma`, `fms`, `fnma`, `fnms<mode>4`; `fma()`; contraction (the combined `fmul_add*` round twice). | 5 (v6) |  | fpenv, sem6 (2C3) |
| `ibranchd`, `icalld` | 6–8 | 0 | Branch (`ibranchd,0 %drN, %empty [? cond]`) or call (`icalld,0 wbs = N, %drN, %empty`) to the address in a register, without a CTPR. A taken `ibranchd` costs 12 cycles beyond its instruction however early the address is ready; `icalld` + return 23 a call (`disp` + `call` 14). **Use:** Code size only: `movtd` 9 cycles ahead + `ct`/`call` is faster for indirect jumps and calls (control-transfer.md, "v6"). binutils uses `ibranchd` in v7 PLT entries, and lcc 1.31.05 for v7 jump tables, indirect tail calls and calls (not for v6). | 12 (taken) |  | ctl6 (2C3) |
| `mkfsw` | 6–8 | 1,4 | An x87 status word from a and b: C0, C2, C3 = bits 0, 2, 6 (CF, PF, ZF) of a, TOP = b mod 8, other bits 0 (what `fnstsw` returns after an `fcmpud*f`-style compare). **Use:** x87 code on the `%bgr` stack (binary translation); none in C. | 2 (v6) |  | sem6 (2C3) |
| `modbgv`, `vfbgv` | 6–8 | 1,4 | `%bgr` (x87 register stack) helpers, masks rotated by the stack top: `modbgv a` sets `val` = (`val` & ~a[7:0]) \| a[15:8] (marks registers empty or full); `vfbgv a, b` raises SIGFPE `FPE_FLTINV` if a register in b[7:0] is full or one in b[15:8] is empty. Both write 0. **Use:** Binary translation only (loops.md, "v6"). | 2 (v6) |  | ctl6, sem6 (2C3) |

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
| `abgc`, `rbgc` | 6–7 |  | Replace `abg` (gas rejects it for v6): `abgc N [? %predN]` adds N (1-7) to `%bgr.cur`, `rbgc` sets it to 0; neither checks or changes `val`, neither traps. **Use:** Binary translation only (loops.md, "v6"). |  |  | ctl6, native (2C3) |
| `fillr`, `fillc` | 6–7 |  | Fill the register window stack or the chain stack back from memory (the counterparts of `flushr`, `flushc`); they run in user mode and return normally. |  |  | ctl6 (2C3) |
| `hcall`, `icall` | 6–7 |  | Hypercall and its kernel-entry sibling (binutils `parse_hicall_args`: a trap number and `wbs`; the kernel enables user hypercall numbers through `%hcem`). |  |  | v6; not run (enters the hypervisor) |
| `iret`, `hret`, `glaunch` | 6–7 |  | Privileged returns from traps and the hypervisor, guest launch. |  |  | v6; not run (privileged) |

