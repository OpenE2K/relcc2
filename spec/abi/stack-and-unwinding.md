# Stacks and unwinding

Each thread has three stacks (guide 9.1.1):

| Stack | Holds | Accessible to the program |
|---|---|---|
| User stack | Local variables, argument areas, `alloca` | Yes, through `USD` |
| Procedure stack | Spilled register windows | Only through kernel calls |
| Chain stack | Return information for each call | Only through kernel calls |

## User stack (guide 9.4)

- Grows downwards. `USD` points to the lowest used address.
- `getsp N` moves the stack pointer by N bytes (negative N allocates) and
  returns the new pointer, aligned to 16 bytes. `getsp 0` reads it.
- The previous `USD` is restored automatically on return, so functions
  don't deallocate their stack.
- Areas in a frame, from higher to lower addresses: local variables,
  dynamic area (`alloca`), outgoing argument area. Each is a multiple of 16
  bytes.
- Incoming stack arguments are at positive offsets from the entry `USD`;
  locals at negative offsets from it.

## Chain stack frames (kernel `e2k_cr0_t`, `e2k_cr1_t`; glibc `backtrace`)

Each call pushes 32 bytes:

| Offset | Register | Contents |
|---|---|---|
| 0 | `cr0_lo` | Saved predicate file |
| 8 | `cr0_hi` | Return address (bits 3–47) |
| 16 | `cr1_lo` | `wbs`, `wpsz`, `wfx`, `cuir`, saved `PSR` bits, high part of the user-stack size (v7) |
| 24 | `cr1_hi` | Saved `BR` (`rbs`, `rsz`, `rcur`, `psz`, `pcur`), `wdbl`, user-stack size `ussz` (in 16-byte units) |

User code reads the chain stack with the `access_hw_stacks` system call
(`E2K_GET_CHAIN_STACK_SIZE`, `E2K_READ_CHAIN_STACK_EX`). glibc's
`backtrace` takes the return addresses from it. Frames with a zero return
address belong to kernel signal handling.

The current function's own return address is in the `%cr0.hi` register
(the top chain frame stays in registers until the next call spills it):
lcc compiles `__builtin_return_address (0)` to `rrd %cr0.hi` and `andd`
with `0xffffffffffff` (bits 0–47) [lcc], and rejects levels above 0.
MCST's unwinder uses this to find its own frame.

## Exceptions (libgcc `unwind-dw2-e2k.c`)

C++ exceptions use MCST's E2K `libgcc_s`:

- Each frame is found by walking the chain stack, not by interpreting CFI.
  The CFA is the caller's stack pointer, computed from `ussz` in the chain
  frame.
- `.eh_frame` is still needed: its FDE locates the function and its
  augmentation gives the personality routine and LSDA. **CFI instructions
  are ignored**, so a compiler only needs to emit the FDE with its
  augmentation data (lcc and Unipro emit empty CFI programs).
- A frame without an FDE is skipped rather than ending the search, so C code
  compiled without `-fexceptions` doesn't stop exceptions.
- On reaching a landing pad, libgcc writes the exception object and
  selector into the target frame's `%db[0]` and `%db[1]` (DWARF 64 and 65)
  in the procedure stack, using `E2K_WRITE_PROCEDURE_STACK_EX`, and redirects
  the intermediate returns with the `set_backtrace` system call.
- The layout of saved registers in the procedure stack differs between
  v1–v4 and v5+ (32 bytes per register pair vs. 16 bytes per register), so
  libgcc checks the CPU at run time.
- glibc's unwinder interface also looks up `_Unwind_GetPCSP`, an E2K
  extension returning the chain stack position of a frame.
- The landing pad is reached by an ordinary return: `uw_install_context`
  is a `noreturn` function that rewrites the chain with `set_backtrace`
  and returns, where other targets end it with `__builtin_eh_return`
  (MCST's comment in `unwind-dw2-e2k.c`). So a compiler needs no
  `eh_return` pattern for it.
- MCST changed the generic `unwind.inc` too, under `#if defined
  __e2k__`: the unwind context carries a chain-stack cache
  (`e2k_mem_crs_t chain_stack[CHAIN_STACK_CACHE_LEN]`) that
  `uw_init_context` fills, and `reinit_chain_stack_cache` resets it
  before phase 2. Using MCST's unwinder with another libgcc means
  carrying these hunks.
- Unipro's clang compiles C++ against lcc's libstdc++ headers (it finds
  them through the `lcc` driver on `PATH`), and both link with `l++`, so
  they share one C++ runtime and the Itanium C++ ABI. The compat suite
  (`eh` unit) throws and catches across compilers, also through and
  out of frames of the other compiler with cleanups to run, and with
  rethrow: scalars, `long double`, `__int128`, structs of every kind,
  class hierarchies caught by base reference (including a second base,
  which needs a pointer adjustment), types whose vtable and `type_info`
  come from only one compiler, and `std::exception` and `std::string`.
  lcc and Unipro interoperate (compat, hw).
- The runtime aligns exception objects to 16 bytes (the `__aligned__` of
  `_Unwind_Exception`), not to a larger `alignas`: an `alignas(32)` object
  lands at an address that is 16 mod 32 (compat, hw). A compiler must not
  assume more than 16 for a caught object.

## Debug information (gdb, lcc)

MCST's gdb 15.2 (`OpenE2K/gdb`, and ALT's gdb on the 4C) walks the
chain stack and has the DWARF CFI unwinder compiled out
(`e2k-linux-tdep.c`, `#if 0` around `dwarf2_append_unwinders`). lcc
emits no `.debug_frame` and, for C, no `.eh_frame` [lcc].

For `DW_OP_call_frame_cfa` gdb returns its frame base, which for E2K
frames is an offset into the procedure stack (`this_psp - psp_base`,
`e2k_frame_this_id`), not a data-stack address. A `DW_AT_frame_base` of
`DW_OP_call_frame_cfa` (GCC's default) therefore makes every argument
and local unreadable, while backtraces still work (proof-of-concept
backend, `docs/spec-poc-findings.md` A13). lcc's frame base is a window
register holding a data-stack address (`DW_OP_reg2` at `-O0`, set from
`getsp` plus the frame size; `DW_OP_reg1` in an `-O2` function), and
`-O0` arguments live in window registers (`DW_OP_reg0`, `DW_OP_reg1`)
[lcc]. A compiler should describe the frame base with a register
expression (`DW_OP_bregN` of the register holding the stack pointer
after `getsp`, or of a frame pointer).

lcc emits no DIE for a global register variable (`register long gv asm
("%g20")`) [lcc], so it never needs DWARF numbers for `%g0–31`, which
[registers.md](registers.md) leaves unnumbered; MCST's gdb maps nothing
above 127.

## setjmp/longjmp, contexts

glibc implements `setjmp`/`longjmp`, `getcontext`/`setcontext` and
`makecontext` in assembly and C with kernel support for the hardware
stacks (`OpenE2K/glibc/sysdeps/unix/sysv/linux/e2k`). A compiler only needs
to treat `setjmp` as returning twice. The compat suite (`longjmp` unit)
checks `longjmp` across frames built by another compiler, from recursion
depths up to 200 (enough to spill register windows to the procedure
stack), both from the callee itself and through a callback into the
caller's compiler: lcc and Unipro interoperate (compat, hw).

`jmp_buf` is 10 doublewords (`bits/setjmp.h`); `setjmp` stores `%cr0.hi`
(the return address), `%cr1.lo` (`wbs`, `wpsz`), `%pcsp.lo`/`.hi`,
`%pcshtp`, the low 28 bits of `%br`, `%usd.lo` and `%wd.psz`
(`setjmp_tmpl.S`), and `longjmp` is the `e2k_longjmp2` system call
(`longjmp.S`): unwinding the chain stack takes the kernel. So GCC's
`__builtin_setjmp` (5 words, no system call) has no E2K counterpart;
lcc doesn't offer it ([source-compat.md](source-compat.md)).
