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

## Argument slots (guide 9.5.1; lcc, unipro, libffi)

Arguments form a list of 8-byte **slots**:

| Argument size | Placement | Slots |
|---|---|---|
| 1–8 bytes | next free slot | 1 |
| 9–16 bytes | next **even** slot (an odd slot is skipped) | 2 |
| 17 bytes or more | next even slot | (size + 7) / 8 |

Placement depends on the size only, not on the type's alignment: a
9-byte `struct { char c[9]; }` also starts at an even slot (libffi
`calc_arg_alignment`, checked against lcc).

- **Slots 0–7 are passed in registers**: slot *i* in the callee's `%dr<i>`
  (the caller's `%db[i]`). Later slots are passed in memory only.
- An argument that would start in a register slot but not fit entirely in
  slots 0–7 goes entirely to memory. Example: seven `long`s and then a
  16-byte struct: the struct skips slot 7 and goes to slots 8–9 in memory.
- Slots are assigned in order and never backfilled: a later small argument
  does not go into a skipped slot (slot 7 above stays unused).
- Aggregates are passed by value in slots, packed as in memory: a 3-byte
  struct occupies the low bytes of one slot; `struct { float a, b; }` and
  `_Complex float` share one slot (`a` in the low half); a 72-byte struct
  occupies 9 slots in memory.
- `long double` occupies two slots (mantissa in the first, 16-bit exponent in
  the second), aligned to an even slot. So do `__int128` and `__float128`
  (low half first). `_Complex double` occupies two slots (real first).
- `float` is passed in the low 32 bits of its slot; `double` in the whole slot.

### Unused parts of a slot (lcc, qemu, compat, hw)

Bytes of a slot that hold no argument data are undefined, and the callee
must not depend on them. On E2K they may be worse than garbage. When an
aggregate larger than 8 bytes is passed in registers and its last slot
holds only 1–4 bytes (sizes 9–12, 17–20, …, 57–60), lcc writes that slot
with a 32-bit operation while `WD.dbl = 0`. That writes only the low half
of the register and leaves the upper half, including its tag, as it was.
If the upper half carries a diagnostic tag, a non-speculative operation
that reads the whole 64-bit register (for example `std` to spill it)
raises `exc_diag_operand`, which Linux delivers as `SIGILL`. A callee
should access such a slot only at the width of the data in it, or with
speculative operations: a speculative store `std,sm` of such a register
does not trap and stores the low half intact (the upper half becomes an
arbitrary diagnostic pattern, `4afafafa` on the 4C; hw,
`tools/abi/probes/tagtrap.c` variant d). MCST's libffi spills incoming
argument registers this way (`std,sm` throughout `src/e2k/call.S`).

The mechanism is confirmed on hardware by `tools/abi/probes/tagtrap.c`:
on an Elbrus 4C (v3), a speculative load from a bad address leaves a
diagnostic tag; a 32-bit `adds` into that register under `dbl = 0` keeps
the old upper half, and a following `std` dies with `SIGILL`. Under
`dbl = 1` the same `adds` zero-extends and the `std` succeeds.

Whether a real call hits it depends on what the upper half last held.
MCST's qemu-e2k fills every register that `setwd` newly allocates with a
diagnostic "empty" value, so the compat suite fails for every such
aggregate passed from lcc `-O2` to Unipro `-O0` (which spills incoming
argument registers with `std`); qemu loops on the exception instead of
delivering a signal, so these tests time out. lcc `-O0` callers and
Unipro `-O2` callees are not affected. On the 4C the same programs pass:
there the registers still hold whatever an earlier frame left, which in
these tests was never tagged. qemu models the cleaning as a hardware
feature that the `CU_HW0.rf_clean_dsbl` bit turns off, and MCST Linux
declares that bit between a field marked "since iset v5" and the v6
fields, so v5 and later machines may clean registers as qemu does
(inferred; not yet run on v5). Either way, a stale tag from an earlier
speculative load can reach such a slot on any machine.

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
- The area belongs to the callee for the duration of the call: it may
  store its register arguments into their slots (Unipro's variadic
  callees and libffi's closures do) and use the area as scratch. A caller
  must not expect its contents to survive the call.

## Variadic functions and calls without a prototype (guide 9.5.1; lcc, unipro, compat, hw)

- For a call to a **variadic** function, the caller stores the **last
  fixed argument and every variadic argument** to its memory slot, and
  also puts the arguments in registers as usual. Fixed arguments before the
  last one need only be in their registers: lcc stores exactly the last
  fixed argument onward; Unipro stores every argument.
- A variadic callee therefore takes fixed arguments other than the last
  from registers (or from the stack for slots 8 and above), and the last
  fixed argument and the variadic ones from memory, as lcc does. A callee
  that read the earlier fixed arguments from memory would read garbage
  when called from lcc. (Unipro's variadic callees store their incoming
  registers to the slots themselves, so they work either way.)
- `va_list` is a plain pointer to the next slot; `va_start(ap, last)` sets
  it to the slot after `last`. It can be passed to a function built by
  another compiler (compat: `valist` unit).
- `va_arg` of a 9–16-byte type (`__int128`, `long double`, `__float128`,
  `_Complex double`, aggregates) first rounds the pointer up to a 16-byte
  **address** (lcc: `ap + (ap & 15)`). For a normal call the slot area is
  16-byte aligned, so this is the same as "start at an even slot", and
  callers place such values there. It differs for `static` variadic
  functions: at `-O1` and above lcc gives them a private calling sequence
  whose slot area is only 8-byte aligned, and puts a 16-byte value at the
  next 16-byte address, which may be an odd slot. A `va_list` from such a
  function that reaches code built by another compiler (for example
  `vprintf`) is read correctly only if `va_arg` aligns by address.
- Unipro's `va_arg(ap, __int128)` doesn't align at all, so it reads one
  slot early at odd slots, and for a `va_list` from an lcc `static`
  variadic function also at even slots; its callers place the value
  correctly (compat). `long double`, `__float128` and `_Complex double`
  are read correctly.
- A call **without a prototype** must place the arguments both in registers
  and in memory, so that it works whether the callee is variadic or not.
  lcc does this; **Unipro's clang does not** (registers only), so such calls
  from Unipro code into lcc-compiled variadic functions are broken.

## Results (guide 9.5.2; gdb, lcc, unipro, libffi)

- Results of **up to 64 bytes** are returned in the callee's `%dr0`…`%dr7`
  (the caller's `%db[0]`…), packed into slots as for arguments. Examples:
  `struct { long a[8]; }` uses `%dr0`–`%dr7`; a 3-byte struct, `struct
  { float a, b; }` and `_Complex float` use `%dr0`; `__int128`,
  `__float128` and `_Complex double` use `%dr0`/`%dr1`; `_Complex long
  double` (32 bytes) uses `%dr0`–`%dr3` (libffi, checked against lcc).
- `long double`: mantissa in `%dr0`, exponent in `%r1`.
- Results **larger than 64 bytes** are written by the callee to memory at its
  entry stack pointer, i.e. into the caller's argument area. The caller
  must reserve an area of at least the result size. There is no hidden
  pointer argument. The result area overlaps the arguments passed in
  memory, so the callee must finish reading them before it writes the
  result (libffi's closures build the result in a separate buffer and
  copy it last, `src/e2k/ffi.c`).
- Integer results narrower than 64 bits are extended to 64 bits (guide;
  compat: lcc and Unipro extend `_Bool`, `char`, `short` and `int`
  results).
