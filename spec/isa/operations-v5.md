# Operations added in v4 and v5

What `elbrus-v5` (the Elbrus-8C2, `geofront.lan`) adds to
[operations-v3.md](operations-v3.md): 741 operations, 738 of them new
in v5 and three (`fscaled`, `fscales`, `fxscalesx`) in v4, which has
no machine here and was checked on the 8C2. 512 of the 741 are the 256
truth tables of `plog` and `qplog`, so about 230 operations are really
new. **Generated** by `tools/isa-extract/gen_operations.py` from
`operations-v5.yaml`; the columns are those of operations-v3.md, and
"lcc v3 / v5" shows that lcc's v5 sysroot uses 35 of them.

Every v5 ALU operation was run on the 8C2 by `sem5.c` (40 sets of
128-bit inputs) and gives the same 128 bits as the community qemu, so
its helpers (`OpenE2K/qemu-e2k-community/target/e2k/helper_*.c`)
describe what the rows summarise; semantics.md, "v5 operations",
lists where MCST's qemu differs.

## What v5 changes for a compiler

Beyond the new operations (each item has its details in the page
named):

- **128-bit registers.** Every register's extended part is 64 bits,
  so a register holds a 128-bit packed value (`%qprN`), operated on as
  a whole by the `qp*` operations; 64-bit operations read its low half
  (spec/abi/registers.md). A 64-bit FP-side result read by a `qp`
  operation pays the cluster-crossing cost even in the same channel
  (timing.md, "Elbrus-8C2").
- **Six FP operations a cycle**: `fadd`, `fmul`, `fmul_add` and the
  `qpf*` operations also issue in ALC2 and ALC5 (timing.md).
- **Memory**: `ldqp` is served by L2 (11 cycles, one a cycle), so lcc
  loads 128-bit data as two `ldd` + `qppackdl`, or with `movaqp` from
  the array prefetch buffer; `ldq` (a register pair) goes through L1.
  16-byte atomics are inline `ldqp`/`stqp` with the lock MAS
  (atomics.md). A store and a load in one 32-byte block in one
  instruction always take about 2 cycles (timing.md).
- **Array prefetch and loops**: `%aaind`/`%aaincr` are 64-bit, a
  misaligned area gives data instead of a diagnostic value, `movaqp`
  reads 16 bytes and an area delivers 16 bytes a cycle (apb.md);
  `%lsr1` holds the 64-bit loop count (loops.md); the data speculation
  table keeps quad entries (speculation.md).
- **Changed behaviour of v3 operations**: `getf` takes the sign bit
  from the field's top instead of the descriptor's BYTE field, which
  becomes `getfz`'s low-zero count (semantics.md, "Field extract");
  `pshufb` applies mask bits 4–6 (invert, bit-reverse, sign) that v3
  ignores (semantics.md, "Packed operations of v3"); `sdivd` every 2
  cycles no longer conflicts in the divider.
- **Unchanged**: every v3 latency, hazard and control delay
  (timing.md, "Elbrus-8C2"), and the ABI.

## How lcc uses them

In its v5 sysroot libraries (`lcc-usage.yaml`, disassembled):

- **16-byte moves**: `qppackdl` (9,060 times) builds a 128-bit value
  from two `ldd` results and `stqp` (6,490) stores it: structure and
  block copies. `qpswitchd` (3,639) swaps the halves, mostly around
  `__float128` code.
- **String functions** (`strlen`, `strnlen`, `strcat`, `rawmemchr`,
  …) test 16 bytes at a time: `qpcmpeqb` against zero, `qpsgn2mskb`
  to a 16-bit mask tested in a scalar register, `qpminub`/`qpor` to
  combine blocks; `stmqp` writes the tail of `strcpy`-like functions.
- **`memcpy`/`memmove`**: aligned `ldqp` from the source, `qppermb`
  with a byte-shift mask to realign two neighbouring 16-byte chunks,
  aligned `stqp` (some with `mas = 0x60`, a cache hint), and for long
  copies a `loop_mode` loop storing with `staaqp`.
- **`strcasecmp`**: two `qpcmpgtb` range tests and `qpand` find the
  upper-case bytes, `qplog_0xf8` (a | (b & c)) ORs in 0x20 there.
- **Multi-precision arithmetic** (`__mpn_add_n`, `__mpn_mul_1`,
  `strtod`): `addcd`/`addcd_c` and `subcd`/`subcd_c` chain the carry.
- **libm**: `fscaled`, `fscales`, `fxscalesx` implement `ldexp`,
  `scalbn` and friends.

Its code generator is more conservative: at `-O3` the loops tried
(`tools/isa-probes` aside: integer, float and double array arithmetic,
saturating byte adds, `short` min and dot products, rotates, selects)
mostly keep the 64-bit packed operations of v3 with 64-bit `mova`,
adding `pmullw` for 32-bit multiplies and `psrcw` for rotates; 128-bit
operations appear in some (`qpaddusb` with `movaqp`/`staaqp`,
`qpfmuld` + `qpfaddd` for `a = b × c + a` on doubles). `__int128`
addition stays scalar (no `addcd`), and a 16-byte `memcpy` of unknown
alignment is `qppackdl`, `qpswitchd` and 16 byte stores.

## What v3 code does instead

| v5 | v3 equivalent |
|---|---|
| a `qp` lane operation | the 64-bit `p` operation twice |
| `qppackdl`, `qpswitchd` | nothing (a register pair `%qrN` is two registers) |
| `getfz` | `getf` then `andd` (or a shift) |
| `addcd`/`subcd` with `_c` | add, an unsigned compare for the carry, a predicated add |
| `plog`/`qplog` | up to three logic operations (`andn`, `orn`, `xorn` help) |
| `qppermb` | per 8-byte half of the result, two `pshufb` and a `pmerge` |
| `qpsgn2mskb` | `pmovmskb hi, lo` on the two halves (one operation) |
| `pmullw` | two `muls` and repacking |
| `psrcw`, `psrcd` | `psll*` + `psrl*` + `por` |
| `stmqp` | byte stores, or load, `pmerge` and store (when the bytes outside may be rewritten) |
| `ldqp`/`stqp` | two `ldd`/`std` (`ldq`/`stq` pair forms are v5 too) |
| `fscale*` (v4) | a library call (`ldexp`) |

## Scalar operations

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `addcd`, `subcd`, `addcd_c`, `subcd_c` | 5–8 | 1,4 | 64-bit add or subtract with carry or borrow in from c bit 0: `addcd` gives the sum, `addcd_c` the carry out (0 or 1), `subcd` and `subcd_c` likewise (community qemu `tcg_gen_add2`). ALC1/4. **Use:** Multi-word arithmetic: `__int128`, `addti3`/`subti3`, `uaddv`, bignums (lcc: `__mpn_*`). (semantics.md, "v5 operations") | 1 (v5) | 0 / 193 | sem5 (8C2, 2C3) |
| `getfzs`, `getfzd` | 5–8 | 0–5 | `getf` (v5 sign rule), then clear the result's low BYTE bits (descriptor bits 13–15): a contiguous mask with up to 7 low zeros in one operation. **Use:** `(x >> k) & m` and `x & m` for masks like 0xff0 (lcc: `des_crypt`, `strcasecmp_nonascii`). (semantics.md, "Field extract") | 1 (v5) | 0 / 1.2k | sem5 (8C2, 2C3) |
| `fscales`, `fscaled` | 4–8 | 1,4 | a × 2^b, b a signed 32-bit integer, rounded once with IEEE flags. v4+; ALC1/4. **Use:** `ldexp<mode>3` with `-fno-math-errno` (lcc's libm: `ldexpf`, `scalbnf`, `__kernel_rem_pio2`). (semantics.md, "Other scalar operations") | 4 (v5) | 0 / 103 | semx (8C2, 2C3) |
| `fxscalesx` | 4–8 | 1,4 | 80-bit a × 2^b (b a 32-bit integer), **rounded to the precision control** unlike x87 `fscale`. v4+. **Use:** `ldexpl`, `scalbnl`. (semantics.md, "80-bit operations") |  | 0 / 32 | semfx (8C2, 2C3) |

## 64-bit packed additions

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `pmullw` | 5–8 | 1,4 | Two 32-bit lanes, low 32 bits of each product (x86 `pmulld` on 64 bits). **Use:** `mulv2si3`; lcc's v5 vectorizer uses it for `int` multiplies. | 2 (v5) |  | sem5 (8C2, 2C3) |
| `psrcw`, `psrcd` | 5–8 | 0,1,3,4 | Rotate every 32-bit (`w`) or 64-bit (`d`) lane right by the count b. **Use:** `rotrv2si3`; lcc vectorizes `(x >> k) \| (x << (32 − k))` with it. | 1 (v5) |  | sem5 (8C2, 2C3) |
| `plog_0x00`, `plog_0x01`, `plog_0x02`, `plog_0x03`, `plog_0x04`, `plog_0x05`, … (256 operations) | 5–7 | 0,1,3,4 | Any three-input bitwise function of a, b, c: result bit = bit ((a << 2) \| (b << 1) \| c) of the table in the name (`0x80` a & b & c, `0x96` a ^ b ^ c, `0xf8` a \| (b & c), `0xca` a ? b : c, `0xe8` majority). 1 cycle. **Use:** Any expression of three operands' bits in one operation (`ternlog`): selects, majority, a \| (b & c). | 1 (v5) | 0 / 8 | sem5 (8C2, 2C3) |

## 128-bit integer operations

Lane-wise unless noted, with twice the lanes of the 64-bit operation of the same name without `q` (operations-v3.md). 1 cycle (multiplies and SAD 2), fully pipelined in every channel the ALC column gives.

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `qpaddb`, `qpaddh`, `qpaddw`, `qpaddd`, `qpsubb`, `qpsubh`, … (16 operations) | 5–8 | 0,3 | Add or subtract, wrapping or saturating. **Use:** `add`, `sub`, `ssadd`, `usadd`, … on `V16QI`, `V8HI`, `V4SI`, `V2DI`. | 1 (v5) | 0 / 158 | memx, sem5 (8C2, 2C3) |
| `qpmaxsb`, `qpmaxsh`, `qpmaxsw`, `qpmaxub`, `qpmaxuh`, `qpmaxuw`, … (14 operations) | 5–8 | 0,3 | Maximum, minimum, rounding average (x86 `pmax*`, `pmin*`, `pavg*`). **Use:** `smax`, `umin`, `uavg*_ceil`; lcc's `strlen` merges blocks with `qpminub`. | 1 (v5) | 0 / 78 | sem5 (8C2, 2C3) |
| `qpcmpeqb`, `qpcmpeqh`, `qpcmpeqw`, `qpcmpeqd`, `qpcmpgtb`, `qpcmpgth`, `qpcmpgtw`, `qpcmpgtd` | 5–8 | 0,3 | All-ones lanes where a = b, or a > b signed. **Use:** `vec_cmp`; byte search (`qpcmpeqb` then `qpsgn2mskb`). | 1 (v5) | 0 / 416 | sem5 (8C2, 2C3) |
| `qpmullh`, `qpmulhh`, `qpmulhuh`, `qpmulhrsh`, `qpmullw`, `qpmaddh`, `qpmaddubsh`, `qpsignb`, `qpsignh`, `qpsignw` | 5–8 | 0,1,3,4 | Multiplies (low, high signed or unsigned, rounded high, 32-bit low), `pmaddwd`-style and signed × unsigned byte dot steps, `psign`. | 1 (v5), 2 (v5) | 0 / 95 | sem5 (8C2, 2C3) |
| `qphaddh`, `qphaddw`, `qphaddsh`, `qphsubh`, `qphsubw`, `qphsubsh` | 5–8 | 0,3 | Horizontal pairs over 128 bits, a's pairs into the low half, b's into the high half. | 1 (v5) |  | sem5 (8C2, 2C3) |
| `qpsadbw`, `qpmpsadbh`, `qpmulubhh` | 5–8 | 1,4 | `psadbw` per 64-bit half; `mpsadbh` per half with the same 32-bit b; `qpmulubhh`: `pmulubhh` per half, bytes 0–3 of the 64-bit a for the low half and 4–7 for the high half. | 2 (v5) |  | sem5 (8C2, 2C3) |
| `qpand`, `qpandn`, `qpor`, `qpxor` | 5–8 | 0,1,3,4 | 128-bit and, a & ~b, or, xor. | 1 (v5) | 0 / 470 | abg, sem5, tagx (8C2, 2C3) |
| `qplog_0x00`, `qplog_0x01`, `qplog_0x02`, `qplog_0x03`, `qplog_0x04`, `qplog_0x05`, … (256 operations) | 5–7 | 0–5 | `plog` on 128 bits (the table in the name). lcc's `strcasecmp` folds case with `qplog_0xf8` = a \| (b & c). | 1 (v5) | 0 / 64 | sem5 (8C2, 2C3) |
| `qpsllh`, `qpsllw`, `qpslld`, `qpsrlh`, `qpsrlw`, `qpsrld`, `qpsrah`, `qpsraw`, `qpsrcw`, `qpsrcd` | 5–8 | 0,1,3,4 | Every lane shifted (or rotated: `qpsrc`) by the 64-bit count b; counts ≥ the lane width give 0 (the sign for `qpsra*`). MCST's qemu takes `qpslld`/`qpsrld` counts modulo 64; the hardware doesn't. (semantics.md, "v5 operations") | 1 (v5) | 0 / 48 | sem5 (8C2, 2C3) |
| `qpacksshb`, `qpackushb`, `qpacksswh`, `qpackuswh` | 5–8 | 1,4 | Narrow with saturation: a's lanes into the high 64 bits, b's into the low 64. **Use:** `vec_pack_ssat`, `vec_pack_usat` (operands swapped). | 1 (v5) |  | sem5 (8C2, 2C3) |
| `qpshufb` | 5–7 | 0,1,3,4 | Low 64 bits of the result = `pshufb` (b.high, b.low, c.low), high = `pshufb` (a.high, a.low, c.high), with v5's byte rules. | 1 (v5) |  | sem5 (8C2, 2C3) |
| `qppermb` | 5–7 | 0,1,3,4 | Byte i: c's byte selects byte (bits 0–3) of a (bit 4 set) or b, then optionally inverts it (bit 5), reverses its bits (bit 6) or replicates its sign (bit 7). **Use:** `vec_perm` on 128 bits; realigning two 16-byte chunks (lcc's `memcpy`). | 1 (v5) | 0 / 228 | sem5 (8C2, 2C3) |
| `qpmerge` | 5–7 | 0,1,3,4 | Byte i = c's byte negative ? b's : a's. **Use:** `vcond`, `vec_merge` with a byte mask. | 1 (v5) | 0 / 38 | sem5 (8C2, 2C3) |
| `qppackdl`, `qpswitchd`, `qpswitchw` | 5–8 | 0,1,3,4 | `qppackdl a, b` makes a 128-bit value with a high, b low; `qpswitchd` swaps the halves, `qpswitchw` the 32-bit words within each half. **Use:** Building 128-bit values from 64-bit ones (lcc: two `ldd` + `qppackdl` instead of `ldqp`). | 1 (v5) | 0 / 12k | loops, memx, sem5, sem6… (8C2, 2C3) |
| `qpsgn2mskb`, `qpmsk2sgnb` | 5–8 | 0,1,3,4 | `qpsgn2mskb b`: the 16 byte sign bits as a 16-bit mask; `qpmsk2sgnb a, b`: a with each byte's sign bit set from mask bit i. **Use:** `vec_extract_msb`-style mask tests (lcc's string functions). | 1 (v5), 4 (v5) | 0 / 327 | sem5 (8C2, 2C3) |
| `qphminposuh` | 5–8 | 1,4 | Minimum unsigned halfword of 16 lanes (a's lanes first) and its index (bits 16 up), into a 64-bit register. 4 cycles. | 4 (v5) |  | sem5 (8C2, 2C3) |

## 128-bit FP and conversions

4 singles or 2 doubles per register, IEEE with flags as the scalar operations (packed compares quiet for eq, neq, od, uod); 4 cycles, in all six channels for `qpfmul_add*` and ALC0, 1, 3, 4 otherwise.

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `qpfadds`, `qpfaddd`, `qpfsubs`, `qpfsubd`, `qpfmuls`, `qpfmuld`, … (14 operations) | 5–8 | 0–5 | Lane-wise arithmetic; x86-style max/min (2 cycles); `addsub` subtracts in even lanes and adds in odd. **Use:** `V4SF`, `V2DF` patterns (lcc: `qpfmuld` + `qpfaddd`). | 2 (v5), 4 (v5) |  | sem5 (8C2, 2C3) |
| `qpfcmpeqs`, `qpfcmpeqd`, `qpfcmplts`, `qpfcmpltd`, `qpfcmples`, `qpfcmpled`, … (16 operations) | 5–8 | 0,1,3,4 | All-ones lanes where the compare holds; eq, neq, od, uod quiet (MCST's and the community qemu both raise invalid wrongly). | 2 (v5) |  | sem5 (8C2, 2C3) |
| `qpfadd_adds`, `qpfadd_addd`, `qpfsub_adds`, `qpfsub_addd`, `qpfhadd_adds`, `qpfhsub_adds`, … (52 operations) | 5–7 | 0–5 | Combined: Y(c, X(a, b)) lane by lane, two roundings, 8 cycles; `qpfmul_add*` in all six channels. **Use:** Multiply-add under `-ffp-contract=fast` (not `fma()`). (semantics.md, "FP min, max and combined operations") | 8 (v5) |  | sem5 (8C2, 2C3) |
| `qpidtofs`, `qpidtofd`, `qpistofs`, `qpistofd`, `qpfdtoid`, `qpfdtois`, … (14 operations) | 5–8 | 0,1,3,4 | Conversions between 32/64-bit integers, singles and doubles (widening ones read 64 bits, narrowing ones write 64). 4 cycles to a consumer of their format. (timing.md, "Formats") | 4 (v5) |  | sem5 (8C2, 2C3) |
| `qpfdtoifd`, `qpfstoifs` | 5–8 | 0,1,3,4 | Round every lane to integral, a giving the mode as x86 `roundpd`'s immediate (bit 3 suppresses inexact; MCST's qemu ignores it). | 4 (v5) |  | sem5 (8C2, 2C3) |

## Memory

New 16-byte accesses. `ldqp` goes to L2 (11 cycles); `ldq`, a register pair, through L1 (3).

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `ldq`, `stq` | 5–8 | 0,2,3,5 | Load or store a register pair (16 bytes at a + b, the low register at the lower address). | 3 (v5), 5 (v6) |  | abi/lockloss-v5.s, memx, semc (8C2, 2C3) |
| `ldqp`, `stqp` | 5–8 | 0,2,3,5 | 128-bit load/store of a `%qpr` register; with the lock and unlock MAS the inline 16-byte atomics (v3 code calls libatomic). **Use:** Block copies and 16-byte atomics; lcc's `memcpy` combines aligned `ldqp` with `qppermb`. (atomics.md; timing.md, "Elbrus-8C2") | 5 (v6), 11 (v5) | 0 / 8.0k | abg, abi/lockloss-v5.s, apb, loops… (8C2, 2C3) |
| `stmqp` | 5–8 | 2,5 | Masked 128-bit store: byte i written when bit i of b's low 16 bits is set; address a alone; SIGBUS unless 16-byte aligned. **Use:** String and vector tails at aligned addresses (lcc: `strcpy`, `strncat`, `memccpy`). (semantics.md, "Memory, tag and descriptor operations") |  | 0 / 199 | semc, stmqp (8C2, 2C3) |
| `ldaaqp`, `staaqp` | 5–8 | 2,5 | 128-bit AAU access through `%aadN` + `%aastiN`; `staaqp` counts as one store for loop-mode store suppression. **Use:** Pipelined loops (lcc's `memmove`, vectorized loops). (apb.md, loops.md) |  | 0 / 544 | loops, semc (8C2, 2C3) |
| `ldcsqp`, `lddsqp`, `ldesqp`, `ldfsqp`, `ldgsqp`, `ldssqp`, … (27 operations) | 5–8 | 0,2,3,5 | 128-bit forms of the segment, protected-mode and privileged accesses: they fault in ordinary user mode (SIGILL, SIGSEGV or SIGBUS) as the v3 forms do. (semantics.md, "Memory, tag and descriptor operations") |  |  | semc (8C2, 2C3) |
| `puttagqp` | 5–8 | 0,1,3,4 | Set the tags of a 128-bit value, 2 bits per 32-bit word (the low 8 bits of b): tag 1 as asked (diagnostic where the word's bit 30 is set, empty where it is clear), 2 and 3 give 0; never traps, `,sm` or not (semantics.md, "Tag operations"); lcc gives speculatively loaded blocks tag 0 with it before testing them in string functions. |  | 0 / 130 | semc, tagx (8C2, 2C3) |

