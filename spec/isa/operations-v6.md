# Operations added in v6 and v7

What `elbrus-v6` (Elbrus-16C, -2C3) and `elbrus-v7` (in development)
add to [operations-v5.md](operations-v5.md). **Generated** by
`tools/isa-extract/gen_operations.py` from `operations-v6.yaml`.

No v6 or v7 machine is available here: every row comes from the
community qemu (`OpenE2K/qemu-e2k-community`, which matches the
hardware on every v5 operation) or, where qemu doesn't implement an
operation, only from binutils (name, channels, operand formats). v6
results could come from targeted self-contained tests run by someone
with a v6 machine. The columns are those of
[operations-v3.md](operations-v3.md).

## Scalar

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `clmull`, `clmulh` | 6–8 | 0,1,3,4 | Carry-less 64 × 64 product, low or high 64 bits (community qemu). **Use:** CRC and GCM (`clmul`). |  |  | v6+: no hardware here |
| `fmas`, `fmad`, `fmss`, `fmsd`, `fnmas`, `fnmad`, `fnmss`, `fnmsd` | 6–8 | 0–5 | Fused a × b + c, a × b − c, −(a × b) + c, −(a × b) − c, one rounding (community qemu). **Use:** `fma`, `fms`, `fnma`, `fnms<mode>4`; `fma()` (the combined operations round twice). |  |  | v6+: no hardware here |
| `ibranchd`, `icalld` | 6–8 | 0 | Branch or call to the address in a register (community qemu). **Use:** Indirect jumps and calls without a CTPR preparation. |  |  | v6+: no hardware here |
| `mkfsw`, `modbgv`, `vfbgv` | 6–8 | 1,4 | Operations on the global register rotation (`%bgr`) and FP state words; not implemented in qemu. |  |  | v6+; no open source |

## Packed

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `pcmpeqbop`, `pcmpeqbap`, `pcmpeqhop`, `pcmpeqhap`, `pcmpeqwop`, `pcmpeqwap`, … (32 operations) | 6–8 | 0,3 | Into a predicate: true if the compare holds in any lane (`op`) or in all lanes (`ap`) (community qemu). **Use:** Vector loop exits (`any`/`all`) without `qpsgn2mskb` and a scalar compare. |  |  | v6+: no hardware here |
| `pmrgp`, `qpmrgp` | 6–8 | 0,1,3,4 | Packed merge by a predicate, as `merged` (community qemu). |  |  | v6+: no hardware here |
| `qpsrad` | 6–8 | 0,1,3,4 | Arithmetic right shift of 64-bit lanes by the count b (community qemu). |  |  | v6+: no hardware here |
| `qpcext_0x00`, `qpcext_0x7f`, `qpcext_0x80`, `qpcext_0xff` | 6–8 | 0,1,3,4 | 128 bits with b in the low half and the high half filled with the byte in the name (community qemu). |  |  | v6+: no hardware here |
| `qpackhbss`, `qpackhbus`, `qpackwhss`, `qpackwhus` | 7–8 | 0,1,3,4 | Narrowing packs; the community qemu implements them with the v5 `qpack*` helpers (a's lanes in the high 64 bits). |  |  | v6+: no hardware here |
| `qpfmas`, `qpfmad`, `qpfmss`, `qpfmsd`, `qpfnmas`, `qpfnmad`, `qpfnmss`, `qpfnmsd`, `qpfmass`, `qpfmasd`, `qpfmsas`, `qpfmsad` | 6–8 | 0–5 | Fused multiply-add per lane (`mas`: add in even lanes, subtract in odd; `msa` the reverse) (community qemu). |  |  | v6+: no hardware here |
| `qppermb_lt3`, `qpinss_lt3`, `qpinsd_lt3`, `qplog_0x00_lt3`, `qplog_0x01_lt3`, `qplog_0x02_lt3`, … (259 operations) | 7 | 0–5 | v7 encodings with the table or selector in a literal (binutils); not implemented in qemu. |  |  | v7; no open source |
| `qpchecks`, `qpcheckd`, `qpcheckq`, `qpcm2b`, `qpcm2h`, `qpcm2w`, … (41 operations) | 6–8 | 0–5 | Operations no open source implements: dot products (`qpidot*`, `qpbfdots`), bfloat16 conversion (`qpfstobf`), AES (`qpaes*`) and Kuznyechik/Streebog (`qpkuz*`, `qpsbgltr*`, `qpmgme`) rounds, table lookups (`qptbl_*`), masks (`qpcm2*`, `qps2*`), checks. Only names and operand formats (binutils) are known. |  |  | v6+; no open source |

## Memory, protected mode and state

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `ldosrrd`, `ldosrrqp`, `stosrrd`, `stosrrqp` | 7–8 | 0,2,3,5 | v7 accesses, no open implementation. |  |  | v7; no open source |
| `apincr`, `apincrc`, `cudincr`, `gdincr`, `getind`, `getmi`, `getptr`, `getptrc`, `getspd`, `subarr`, `subarrc`, `vfsid` | 7–8 | 0,1,3,4 | v7 pointer and array operations of protected mode; no open implementation. |  |  | v7; no open source |
| `rrshd`, `rwshd` | 7–8 | 0 | v7 state accesses (binutils); not implemented in qemu. |  |  | v7; no open source |

## Control

| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |
|---|---|---|---|---|---|---|
| `abgc`, `rbgc` | 6–7 |  | Global-register rotation forms (binutils; gas doesn't accept them). |  |  | v6+; source only |
| `fillr`, `fillc` | 6–7 |  | Fill the register window stack or the chain stack back from memory (the counterparts of `flushr`, `flushc`). |  |  | v6+: no hardware here |
| `hcall`, `icall`, `ctp_call`, `dtbp` | 6–7 |  | Hypercall, indirect call, call through a prepared CTPR, the v7 `dtbp` (binutils). |  |  | v6+; source only |
| `iret`, `hret`, `glaunch` | 6–7 |  | Privileged returns from traps and the hypervisor, guest launch. |  |  | v6+: no hardware here |

