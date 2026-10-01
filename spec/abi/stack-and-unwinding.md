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

## setjmp/longjmp, contexts

glibc implements `setjmp`/`longjmp`, `getcontext`/`setcontext` and
`makecontext` in assembly and C with kernel support for the hardware
stacks (`OpenE2K/glibc/sysdeps/unix/sysv/linux/e2k`). A compiler only needs
to treat `setjmp` as returning twice. Not yet studied in detail.
