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
- `wbs` lies between the pairs the caller was itself handed (4 for a
  C function) and its `wsz` − 1: a smaller `wbs`, a larger one, or one
  equal to `wsz` (no pair handed over) is SIGSEGV `SEGV_BNDERR`, at the
  `call` or at the callee's first instruction (guide 9.5.3;
  [registers.md](registers.md), window size limits [hw]).
- The C convention's parameter area is **4 pairs = 8 registers** (kernel
  `C_ABI_PSIZE_UNPROT`).
- Tail calls: `ct` to a function without `call` keeps the current window, so
  arguments are placed in the caller's own `%dr0..` (unipro). lcc does
  the same at `-O2` and `-O3`: `return g (x + 1)` is `disp %ctpr1, g`,
  `addd %r0, 0x1, %r0` and `ct %ctpr1`, also from a function with a
  frame or with stack arguments [lcc 1.29.16 v3.4c, v5.8c2, v6.2c3 and
  1.31.05; `tools/abi/probes/pocplan.c` `tail1`, `tail_frame`,
  `tail10`]. It passes the stack slots in an area of its own frame, even
  where the incoming area would do (a `getsp` of 16 bytes for one
  argument in `tail1`, 0x50 for ten in `tail10`, slots 8 and 9 stored
  there; `tail_frame` reuses the outgoing area of its earlier call), and
  releases neither that area nor the rest of its frame before the `ct`.
  So the data stack grows at each tail call until the last callee
  returns (its return restores `USD` from the chain stack): against a
  direct call, the callee's locals sit 16 bytes lower after `tail1`, 48
  after `tail_frame` (its whole frame), 80 after `tail10`, and 16 bytes
  lower for each of 10 or 20 mutually recursive tail calls [hw:
  `tools/abi/probes/pocplan_tail.c`, 4C, 8C2, 2C3, and MCST's qemu].
  GCC's `gcc.dg/sibcall-3.c` and `sibcall-9.c`, which check that a
  local's address stays the same across sibling calls of a one-argument
  function, therefore abort with lcc's code; `sibcall-4.c` and `-10.c`
  (no arguments, no area) pass [hw: lcc 1.29.16 `e2k-v3` `-O2
  -fno-inline`, 4C, 8C2, 2C3]. Releasing the frame before the jump, as
  GCC's sibling calls do, is compatible with this: a callee sees only
  the `USD` it is entered with (inferred).

## Argument slots (guide 9.5.1; lcc, unipro, libffi)

Arguments form a list of 8-byte **slots**:

| Argument size | Placement | Slots |
|---|---|---|
| 0 (empty GNU C `struct {}`) | none: the next argument takes the slot (lcc) | 0 |
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
- `long double` occupies two slots (mantissa in the first, 16-bit sign and
  exponent in the second), aligned to an even slot. So do `__int128` and
  `__float128` (low half first). `_Complex double` occupies two slots
  (real first). For the exponent slot of a `long double`, lcc callers
  zero-extend the word from 32 bits (`sxt 0x6`) and lcc returns it
  zero-extended from 16 bits (`sxt 0x5`; `sxt` codes 4–6 are the unsigned
  forms); lcc callees use only bits 0–15 (hw: garbage above them is
  ignored).
- `float` is passed in the low 32 bits of its slot; `double` in the whole slot.
- GNU vector types (`__attribute__ ((vector_size (N)))`) are passed like
  an integer or aggregate of their size: an 8-byte vector takes one slot
  (`f (int, v4hi, double, v4hi)` gets `%r0`…`%r3`), a 16-byte one an
  even pair (`g (int, v4si)` passes it in `%r2`/`%r3`); results come
  back in `%r0` or `%r0`/`%r1`, and a struct holding a vector follows the
  aggregate rules [lcc: 1.29.16 `-O2`; the proof-of-concept back end
  agrees]. `va_arg` of a vector and v5's 16-byte vectors are not in the
  compat suite yet.

### Unused parts of a slot (lcc, qemu, compat, hw)

Bytes of a slot that hold no argument data are undefined, and the callee
must not depend on them. On E2K they may be worse than garbage. When an
aggregate larger than 8 bytes is passed in registers and its last slot
holds only 1–4 bytes (sizes 9–12, 17–20, …, 57–60), lcc writes that slot
with a 32-bit operation while `WD.dbl = 0`. That writes only the low half
of the register and leaves the upper half, including its tag, as it was.
Each 32-bit half of a register has a tag, and a half tagged 1 is
**diagnostic** when its bit 30 is set and **empty** when it is clear
([speculation.md](../isa/speculation.md), "Diagnostic values"). If the
upper half is diagnostic, a non-speculative operation that reads the
whole 64-bit register (for example `std` to spill it) raises
`exc_diag_operand`, which Linux delivers as `SIGILL`. An empty upper
half does not trap: a plain `std` stores the register, and 64-bit
operations read the half as a number [hw: `tools/abi/probes/tagempty.c`
variants g, h, l; 4C, 8C2, 2C3]. A callee should access such a slot only
at the width of the data in it, or with speculative operations: a
speculative store `std,sm` of such a register does not trap and stores
the low half intact (the upper half becomes an arbitrary diagnostic
pattern, `4afafafa` on the 4C, 8C2 and 2C3; hw,
`tools/abi/probes/tagtrap.c` variant d). MCST's libffi spills incoming
argument registers this way (`std,sm` throughout `src/e2k/call.S`).

The mechanism is confirmed on hardware by `tools/abi/probes/tagtrap.c`
(the 4C, 8C2 and 2C3 print the same): a speculative load from a bad
address leaves a diagnostic value; a 32-bit `adds` into that register
under `dbl = 0` keeps the old upper half, and a following `std` dies
with `SIGILL`. Under `dbl = 1` the same `adds` zero-extends and the
`std` succeeds.

Whether a real call hits it depends on what the upper half last held.
A register that a window gains, the part of a callee's window beyond the
parameter area or what a later `setwd` adds, holds an **empty** value on
all three machines: 0 with tag 5 on the 8C2 and the 2C3, and on the 4C
whatever an earlier frame left there with bit 30 of each half cleared,
also with tag 5 (`5555555555555555` comes back as `1555555515555555`)
[hw: `tagempty.c` variants n, o, p]. So when lcc fills the last slot
of such an aggregate with a 32-bit write into a register the window has
just gained, the upper half is empty, and a callee that spills it with
`std` works. That is why lcc `-O2` callers and Unipro `-O0` callees,
which spill incoming argument registers with `std`, pass every
`structs`, `floats` and `layout` test of the compat suite on the 4C, the
8C2 and the 2C3 (`run.py --pairs lcc:unipro-O0 --remote`; its other
failures are Unipro's known ones). A diagnostic upper half needs a
register the caller itself filled from a failed speculative operation,
such as a `%b[N]` reused after one; nothing empties that, on any
machine (inferred).

MCST's qemu-e2k also fills every register that `setwd` adds with 0,
tag 5 (`e2k_greg_make_empty`, `target/e2k/cpu.h:806-817`, written by
`helper_e2k_clear_rf_regs_on_setwd`,
`target/e2k/tcg/translate-misc.c:647-657, 1206-1212`), as the 8C2 and
the 2C3 do. It treats the cleaning as a feature that the
`CU_HW0.rf_clean_dsbl` bit turns off (MCST Linux declares that bit right
after a field marked "since iset v5", kernel
`include/asm/cpu_regs_types.h:886-887`), and `-e2k-opt no-rf-clean`
turns it off too. But qemu cannot store a register that has a tagged
half that is not diagnostic: its non-speculative store tries an
unchecked path that leaves for a slower translation when the data has a
tag, and the slower translation takes the same path again, so the
store's instruction runs forever and no exception is raised
(`target/e2k/tcg/translate-mem.c:262-271, 1443-1458`) [qemu:
`tagempty.c` variants g–k never finish; `-d exec` shows one translation
block repeating]. So under qemu every such aggregate passed from lcc
`-O2` to Unipro `-O0` times out (54 `structs`, 2 `floats` and 2 `layout`
tests on v3, `tools/abi/compat/known.txt`); with `-e2k-opt no-rf-clean`
in `run.py --runner` they all pass. lcc `-O0` callers and Unipro `-O2`
callees are not affected. A diagnostic value in the stored register, by
contrast, stops qemu with "exception handling is unimplemented"
(`tagtrap.c` variants a, b, f). qemu's `gettagd` of an emptied register
returns 1, not the hardware's 5 (`tagempty.c` variants n–p).

Which operations trap on such a register when its upper half is
diagnostic (a speculative `ldd` from a bad address leaves the whole
register diagnostic, then a 32-bit `adds` writes the low half under
`dbl = 0`), Elbrus 4C. The operation rows come from the
proof-of-concept backend's probe (`docs/spec-poc-findings.md`, A1), the
copy rows from `tagtrap.c` variants e and f (hw; the 8C2 and 2C3 give
the same):

| Operation reading the register | Result |
|---|---|
| any non-speculative 64-bit read: `addd`, `ord`, `shld`, `cmpedb`, `merged`, `faddd`, `std` | **SIGILL** |
| 32-bit reads: `adds`, `sxt 0x2`, `sxt 0x6`, `cmpesb`, `fadds`, `stw` | no trap, correct result (`adds 0, r` gives `0000000012345678`) |
| `movtd %r, %r` | no trap; the copy carries the tag (a later plain `std` of it traps) |
| `movtd %r, %ctprN` | no trap |
| speculative `addd,sm 0x0, %r, %r'` (lcc's form for 64-bit copies) | no trap, but the **whole result is a diagnostic value**: even the low half is lost (`std,sm` of it stores `4afafafa52345678`, not `…12345678`) |
| speculative `std,sm` of the register itself | no trap; the low half is stored intact (variant d) |

So a value written by 32-bit operations under `dbl = 0` must be read,
copied and spilled at 32 bits (`adds`, `sxt`, `stw`), as lcc does
(`adds 0x0, %r1, %b[1]` for 32-bit copies, `addd,sm` only for 64-bit
values): no 64-bit operation, speculative or not, gives a usable result.
For a GCC port this rules out paradoxical subregs of such values; the
proof-of-concept backend promotes narrow integers to 64 bits with `sxt`
and spills 32-bit values with `stw`.

### Integer promotion (guide 9.5.1, 9.5.2; lcc)

Integer arguments narrower than 64 bits are extended to 64 bits by the
caller: sign extension for signed types, zero extension for unsigned ones
(`_Bool`, `char`, `short`, `int`). lcc's callees nevertheless extend
arguments again before using them as 64-bit values, so they don't depend
on it; Unipro's callees depend on it for `_Bool`, `char` and `short` (not
for `int`). A new compiler should extend in both places: the compat
suite's `ext` unit passes garbage above the value and fails a callee that
trusts the caller. (compat: both compilers extend as callers.) The rule
covers memory slots too: lcc writes a narrow argument in slot 8 and up as
`sxt` and a full 64-bit `std`, not `stb`/`sth`/`stw`.

## Stack argument area (guide 9.4; lcc, unipro)

- The caller reserves an area at the top of its stack, using `getsp` with a
  negative size, before the call. Its size is `8 × slots` rounded up to 16
  bytes, **including the slots passed in registers** (the guide's
  "reserved area"). Both compilers reserve at least this much for every
  call that passes arguments. A call without arguments reserves nothing
  (lcc: no `getsp`).
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
- **The last fixed argument is the last declared parameter.** A hidden
  result pointer (C++ classes that are not trivially copyable, below)
  takes slot 0 and moves every argument up a slot, but it is not a
  declared parameter. For `S f (long a, long b, ...)` lcc's caller stores
  slot 2 (`b`) and the variadic slots, but not slots 0 and 1, and its
  callee reads `b` from slot 2 of memory; for `S g (int n, ...)` the
  caller stores from slot 1. A member function's `this` follows the
  pointer, so `S X::m (int n, ...)` stores from slot 2. With no declared
  parameter (C++ `S h (...)`, `long k (...)`) the caller stores every
  slot, the hidden pointer's slot 0 included [lcc: l++ 1.29.16 and
  1.31.05 `-O3`, v3; compat-style run: `tools/abi/probes/pocabi1_va.cc`,
  every pair of lcc `-O3`, lcc `-O0` and Unipro callers and callees,
  under MCST's qemu and on the 4C: all right]. A caller that leaves the
  last fixed argument's slot unwritten makes lcc's callees (`-O3` and
  `-O0`) read whatever the slot held, while Unipro's callees read the
  register (`pocabi1_va.cc -DGARBAGE`, its control line). Found by the
  proof-of-concept back end, whose callers counted the declared
  parameters without the pointer (`docs/spec-poc-findings.md` A24).
- `va_list` is `char *`, a plain pointer to the next slot (lcc's
  `__builtin_va_list`: `__builtin_types_compatible_p (va_list, char *)` is
  1, with `void *` or `signed char *` 0); `va_start(ap, last)` sets it to
  the slot after `last`. It can be passed to a function built by another
  compiler (compat: `valist` unit). In C++ the type is part of the mangled
  name: lcc's `long sum (int, va_list)` is `_Z3sumiPc`, `va_list *` is
  `PPc`, and `std::va_list` and `const va_list` are `Pc` too. Unipro's
  clang uses `void *` (`_Z3sumiPv`; in C `va_list` is compatible with
  `void *`), so a C++ function with a `va_list` parameter doesn't link
  between the two (`undefined reference to sum(int, char*)`) [lcc
  1.29.16 v3 and v6.2c3, 1.31.05 v3; Unipro 20.1.8 `-march=elbrus-v3`:
  `tools/abi/probes/pocabi1_valist.cc`, linked both ways and run under
  MCST's qemu; found by the proof-of-concept back end,
  `docs/spec-poc-findings.md` A27].
- `va_arg` of any type larger than 8 bytes (`__int128`, `long double`,
  `__float128`, `_Complex double`, aggregates of any size; checked with a
  24-byte struct) first rounds the pointer up to a 16-byte **address**
  (lcc: `ap + (ap & 15)`). For a normal call the slot area is
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
  pointer argument (except for the C++ classes below). The result area overlaps the arguments passed in
  memory, so the callee must finish reading them before it writes the
  result (libffi's closures build the result in a separate buffer and
  copy it last, `src/e2k/ffi.c`).
- Integer results narrower than 64 bits are extended to 64 bits (guide;
  compat: lcc and Unipro extend `_Bool`, `char`, `short` and `int`
  results).

## C++ classes that are not trivially copyable (lcc: l++ 1.29.16 `-O1`)

A class that is "non-trivial for the purposes of calls" in the Itanium
C++ ABI (a non-trivial copy or move constructor, or a non-trivial
destructor; GCC's `TREE_ADDRESSABLE`) can't be passed or returned in
registers, because it must stay at one address:

- **Result**: returned through a **hidden pointer in argument slot 0**,
  whatever its size (24 bytes and 128 bytes probed). The caller passes
  the address of the object to build in `%db[0]` and the real arguments
  move up one slot (`S mk3 (long, long)`: the callee gets the address in
  `%r0` and the arguments in `%r1`, `%r2`); the callee constructs the
  object there. The caller uses its own copy of the address afterwards,
  not `%dr0`. A class with only a non-trivial destructor counts too.
  In a member function the pointer comes first and `this` second: for
  `S X::m (long a)` the callee gets the address in `%r0`, `this` in `%r1`
  and `a` in `%r2` [lcc: l++ 1.29.16 `-O3`;
  `tools/abi/probes/pocabi1_va.cc` for the variadic form]. GCC orders
  them the same way: it puts `.result_ptr` before `this` in the parameter
  list (function.cc:2474-2496). In a variadic call the pointer counts as
  a slot but not as the last fixed argument ("Variadic functions",
  above).
- **Argument**: passed by invisible reference: the caller makes a
  temporary and passes its address in the argument's own slot
  (`take (long, S, long)`: `%r1` points to the copy). This part comes
  from GCC's C++ front end.

Trivially copyable classes follow the C rules above. The C rule applied
to these classes breaks objects that point into themselves
(`std::string`'s short-string buffer): the proof-of-concept backend's
`to_string` returned a string with a null `c_str ()` until it used the
hidden pointer (`docs/spec-poc-findings.md` A9).

The compat suite's `cxxclass` unit checks both rules across compilers
(lcc and Unipro, under qemu and on the 4C): classes with a user copy
constructor (8–128 bytes), only a non-trivial destructor, a pointer into
themselves, move-only, polymorphic, and `std::string`, as results (also
with stack arguments and from member functions, the callee building the
object at the caller's address) and as arguments (in a register slot,
on the stack, two at once, each destroyed once by the caller), with
trivially copyable controls.
