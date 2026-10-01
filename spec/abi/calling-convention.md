# Calling convention

## Call sequence (guide 9.5.3–9.5.4; lcc, unipro)

```
	disp    %ctpr1, callee            ! prepare: also movtd %drN, %ctpr1 for indirect calls
	setwd   wsz = 8, nfx = 1, dbl = 0 ! caller's window, large enough for its outgoing area
	setbn   rbs = 4, rsz = 3, rcur = 0 ! %b[0..7] = %r8..%r15: the outgoing argument registers
	getsp   -48, %dr1                  ! reserve the stack argument area (see below)
	addd    ..., %db[0]                ! arguments go to %db[0], %db[1], ...
	call    %ctpr1, wbs = 4            ! callee's window starts at %r8: its %dr0 = our %db[0]
	...                                ! results are in %db[0], %db[1], ...

callee:
	setwd   wsz = 4, nfx = 1, dbl = 0  ! at least the 4 pairs of the parameter area
	return  %ctpr3                     ! prepare the return; may be done early
	...                                ! arguments in %dr0..%dr7, result to %dr0..
	ct      %ctpr3
```

- The call saves the return address, `WD`, `BR`, the predicate file, `USD`,
  `CUIR`, `TR` and `PSR` in the chain stack, and gives the callee a window
  whose initial size equals the caller's window size minus `wbs`.
- The first argument register must be even (`wbs` counts pairs). Both
  compilers point `%b[0]` at it with `setbn rbs = wbs` and write arguments
  as `%db[i]`.
- The C convention's parameter area is **4 pairs = 8 registers** (kernel
  `C_ABI_PSIZE_UNPROT`).
- Tail calls: `ct` to a function without `call` keeps the current window, so
  arguments are placed in the caller's own `%dr0..` (unipro).

## Argument slots (guide 9.5.1; lcc, unipro)

Arguments form a list of 8-byte **slots**:

| Argument size | Placement | Slots |
|---|---|---|
| 1–8 bytes | next free slot | 1 |
| 9–16 bytes | next **even** slot (an odd slot is skipped) | 2 |
| 17 bytes or more | next even slot | (size + 7) / 8 |

- **Slots 0–7 are passed in registers**: slot *i* in the callee's `%dr<i>`
  (the caller's `%db[i]`). Later slots are passed in memory only.
- An argument that would start in a register slot but not fit entirely in
  slots 0–7 goes entirely to memory. Example: seven `long`s and then a
  16-byte struct: the struct skips slot 7 and goes to slots 8–9 in memory.
- Aggregates are passed by value in slots, packed as in memory: a 3-byte
  struct occupies the low bytes of one slot; `struct { float a, b; }` and
  `_Complex float` share one slot (`a` in the low half); a 72-byte struct
  occupies 9 slots in memory.
- `long double` occupies two slots (mantissa in the first, 16-bit exponent in
  the second), aligned to an even slot. So do `__int128` and `__float128`
  (low half first). `_Complex double` occupies two slots (real first).
- `float` is passed in the low 32 bits of its slot; `double` in the whole slot.

### Unused parts of a slot (lcc, qemu; not verified on hardware)

Bytes of a slot that hold no argument data are undefined, and the callee
must not depend on them. On E2K they may be worse than garbage. When an
aggregate larger than 8 bytes is passed in registers and its last slot
holds only 1–4 bytes (sizes 9–12, 17–20, …, 57–60), lcc writes that slot
with a 32-bit operation while `WD.dbl = 0`. That writes only the low half
of the register and leaves the upper half, including its tag, as it was.
In MCST's qemu-e2k the upper half can then carry a diagnostic tag, and a
non-speculative operation that reads the whole 64-bit register (for
example `std` to spill it) raises `exc_diag_operand`. A callee should
access such a slot only at the width of the data in it, or with
speculative operations.

Found by the compat suite: every such aggregate passed from lcc `-O2` to
Unipro `-O0`, which spills incoming argument registers with `std`. lcc
`-O0` callers and Unipro `-O2` callees are not affected. qemu-e2k loops on
the exception instead of delivering a signal, so the test times out.
Whether hardware raises the exception here is not yet known.

### Integer promotion (guide 9.5.1, 9.5.2; lcc)

Integer arguments narrower than 64 bits are extended to 64 bits by the
caller: sign extension for signed types, zero extension for unsigned ones
(`_Bool`, `char`, `short`, `int`). lcc's callees nevertheless extend
arguments again before using them as 64-bit values, so they don't depend
on it; Unipro's callees depend on it for `_Bool`, `char` and `short` (not
for `int`). A new compiler should extend in both places. (compat: both
compilers extend as callers.)

## Stack argument area (guide 9.4; lcc, unipro)

- The caller reserves an area at the top of its stack, using `getsp` with a
  negative size, before the call. Its size is `8 × slots` rounded up to 16
  bytes, **including the slots passed in registers** (the guide's
  "reserved area"). Both compilers reserve at least this much for every
  call that passes arguments.
- Slot *i* is at offset `8 × i` from the stack pointer (`USD`) at the time
  of the call, which is the callee's stack pointer on entry. The callee
  reads it with `getsp 0`.
- No other stack allocation may happen between filling the area and the
  call.

## Variadic functions and calls without a prototype (guide 9.5.1; lcc, unipro)

- For a call to a **variadic** function, the caller stores **every
  argument, including the fixed ones,** to its memory slot. It may also put
  them in registers; both compilers do.
- lcc's variadic callees read all arguments, fixed ones included, from
  memory. `va_list` is a pointer into the slot area.
- `va_arg` follows the slot rules: a 9–16-byte type (`__int128`,
  `long double`, `__float128`, `_Complex double`, aggregates) starts at an
  even slot. Unipro's `va_arg(ap, __int128)` doesn't skip the odd slot and
  reads one slot early; its callers place the value correctly (compat).
- A call **without a prototype** must place the arguments both in registers
  and in memory, so that it works whether the callee is variadic or not.
  lcc does this; **Unipro's clang does not** (registers only), so such calls
  from Unipro code into lcc-compiled variadic functions are broken.

## Results (guide 9.5.2; gdb, lcc, unipro)

- Results of **up to 64 bytes** are returned in the callee's `%dr0`…`%dr7`
  (the caller's `%db[0]`…), packed into slots as for arguments. Examples:
  `struct { long a[8]; }` uses `%dr0`–`%dr7`; a 3-byte struct, `struct
  { float a, b; }` and `_Complex float` use `%dr0`; `__int128`,
  `__float128` and `_Complex double` use `%dr0`/`%dr1`.
- `long double`: mantissa in `%dr0`, exponent in `%r1`.
- Results **larger than 64 bytes** are written by the callee to memory at its
  entry stack pointer, i.e. into the caller's argument area. The caller
  must reserve an area of at least the result size. There is no hidden
  pointer argument.
- Integer results narrower than 64 bits are extended to 64 bits (guide;
  compat: lcc and Unipro extend `_Bool`, `char`, `short` and `int`
  results).
