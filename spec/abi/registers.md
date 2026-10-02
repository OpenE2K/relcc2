# Registers

## Register file (guide 9.3.1)

The register file holds 256 64-bit registers (each also has an 80-bit
extended part used by `%xr` operations).

| Part | Registers | Addressed as |
|---|---|---|
| Global | 32 | `%g0`–`%g31` (`%dg`, `%qg`), absolute |
| Stack (windows) | 224 | `%r0`–`%r63` relative to the current window base; `%b[0]`–`%b[127]` relative to the rotating base inside the window |

- `%rN` / `%drN` is the 64-bit register N of the window; `%qrN` (even N) is
  the pair N, N+1, low half first.
- A window can be larger than 64 registers, but `%r` addressing reaches only
  the first 64. `%b[]` reaches up to 128 registers from the rotating base.
- `%g24`–`%g31` can rotate (`abg`/`abgc`).

### Window and rotating-base units

The hardware counts these in **pairs of 64-bit registers** ("quad"
units). (kernel `e2k_wd_t`, `e2k_br_t`; unipro, lcc)

| Operation | Field | Meaning |
|---|---|---|
| `setwd wsz=N` | `wsz` | Window size: N pairs (2N 64-bit registers) |
| `setbn rbs=B, rsz=S, rcur=C` | `rbs` | Rotating area starts at pair B (`%b[0]` = `%r(2B)`) |
| | `rsz` | Rotating area size: S + 1 pairs |
| | `rcur` | Current rotation offset |
| `setbp psz=P` | `psz` | Rotating predicate area size |
| `call ..., wbs=W` | `wbs` | The callee's window starts at pair W of the caller's window |

`setwd` also sets two window flags (qemu, kernel):

- **`nfx`**: the kernel's `WD.fx` is its inverse. With `nfx = 0` the
  hardware saves and restores the 80-bit extended parts of the window's
  registers when spilling. Both compilers clear `nfx` only in functions
  that use `long double` (lcc, unipro).
- **`dbl`** (v3+): with `dbl = 1`, 32-bit integer ALU operations
  zero-extend their result into the whole 64-bit register; with
  `dbl = 0`, they write only the low half and leave the upper half
  (including its tag) unchanged. The flag is saved and restored per frame
  (`cr1_hi.wdbl`). lcc uses `dbl = 0`, Unipro `dbl = 1`. Code must not rely
  on the upper half of a 32-bit result either way. Measured on the 4C
  (`tools/abi/probes/dbl.c`, hw; MCST's qemu agrees):

  | Operation | `dbl = 0` | `dbl = 1` |
  |---|---|---|
  | `adds`, `shls`, `muls` (32-bit integer) | upper half kept | zero-extended |
  | `ldw` | zero-extended | zero-extended |
  | `sxt` | 64-bit result | 64-bit result |
  | `fadds` (32-bit float) | upper half kept | upper half kept |
  | `movts` | upper half kept | upper half kept |

  MCST's qemu limits `dbl` to the integer operation group plus `gettags`,
  `puttags` and `rrs` (`operations-descr.h`, `e2k_get_eff_dst_type`). The
  community qemu applies it to every 32-bit ALU result except `movt*s`
  (`translate.c`, `is_alop_affected_by_dbl`), which disagrees with the
  hardware for `fadds`.

## Conventions

| Register | Convention | Source |
|---|---|---|
| Window registers | Saved automatically by the call mechanism, except the parameter area, which a call may change | guide 9.3.1.2 |
| `%g13` | Thread pointer (TLS) | guide 9.3.1.4 |
| `%g12` | Reserved | guide 9.3.1.4 |
| Other globals | Scratch: not preserved across calls | guide 9.3.1.4 |
| Predicates `%pred0`–`%pred31` | Saved and restored automatically; each call gets a fresh predicate file | guide 9.3.2, 9.5.3 |
| `%ctpr1`–`%ctpr3` | Scratch: not preserved across calls. Returns go through `%ctpr3` | guide 9.3.3 |
| `%lsr`, `%ilcr` (loop status, counter) | Scratch | guide 9.3.4 |
| `%fpfr`, `%pfpfr`, `%upsr` | Special: a function that changes them must restore them | guide 9.3.4 |
| `WD`, `BR`, `USD`, `CUD`, `GD`, `CUIR`, `TSD`, `IP` | Saved and restored automatically | guide 9.3.4 |

The kernel saves `%g12` and `%g13` only on context switch. All other
globals are additionally saved around user signal handlers (guide 9.3.1.4).
Keeping global variables in global registers is only safe in
single-threaded programs without signal handlers.

## DWARF register numbers

| DWARF number | Register | Source |
|---|---|---|
| 0–63 | `%r0`–`%r63` (64-bit `%dr`) | gdb `e2k_dwarf_reg_to_regnum`, lcc, unipro |
| 64–127 | `%b[0]`–`%b[63]` | gdb, libgcc |

No other registers have DWARF numbers: lcc assigns none to `%b[64]`–`%b[127]`,
globals, predicates or control registers. Debug info from lcc and Unipro
uses `DW_OP_regN` with these numbers; arguments appear as `DW_OP_reg0`, … in
the callee.

**Exception handling** uses DWARF 64 and 65 (`%b[0]`, `%b[1]`) as
`__builtin_eh_return_data_regno (0)` and `(1)` (libgcc). A compiler that
numbers them differently produces libraries that are incompatible with
lcc's libgcc and libstdc++ at run time; the libgcc source notes this
happened with an old `e2k-linux-gcc`.
