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
- **The amount of a `getsp` must be a literal, a window or rotating
  register, or a global from `%g16` up.** The data stack is mapped on
  demand: a `getsp` that moves past the mapped part traps, and up to v6
  (the `array_bounds` exception; also v7 CPUs with the
  `CPU_HWBUG_RRD_USINCR` bug, while other v7 CPUs report the amount in
  `%usincr`) the kernel decodes the instruction to learn the amount
  (`arch/e2k/kernel/getsp.c`, `parse_getsp_operation`, called from
  `traps.c`). It reads literals, window and rotating
  registers (from the procedure stack) and its saved copies of
  `%g16`–`%g31`, but for `%g0`–`%g15` (or a tagged value) it fails and
  the process gets SIGSEGV (`SEGV_BNDERR`). On the 4C
  (`tools/abi/probes/getsp.c`): 16 KiB, 256 KiB and 4 MiB with the
  amount in a window register, `%g16` or `%g24` all work; with `%g8`
  16 KiB works (no growth needed) and 256 KiB and 4 MiB die. So the
  bug shows only for large allocations. lcc puts the amount in `%g24`,
  saving and restoring the old value around it (`adds` + `getsp %g24`;
  the 32-bit form, whose amount the kernel sign-extends). Found by the
  proof-of-concept backend (`docs/spec-poc-findings.md` A16).
  The kernel grows the main stack by exactly the amount asked for and
  then rewrites the stack size in every chain frame, so the first deep
  recursion in the main thread costs time quadratic in its depth (49 s
  for 250,000 levels on the 4C; threads are not affected):
  [kernel-interface.md](kernel-interface.md), "The data stack".
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
`backtrace` takes the return addresses from it. A signal handler's
caller is a frame that returns into the kernel's `[trampolines]` page
(+0x100, the `sigreturn` code), and the chain continues below it into the
interrupted code; the bottom frame's return address is zero (hw:
"Signal frames" below; glibc's comment and MCST's libgcc speak of zero
return addresses for signal frames, which this 6.1 kernel does not show).

The current function's own return address is in the `%cr0.hi` register
(the top chain frame stays in registers until the next call spills it):
lcc compiles `__builtin_return_address (0)` to `rrd %cr0.hi` and `andd`
with `0xffffffffffff` (bits 0–47) [lcc], and rejects levels above 0.
MCST's unwinder uses this to find its own frame.

What a backtrace costs [hw: `tools/abi/probes/ctlcost.cc`, lcc 1.29.16
`l++ -O2`, `%clkr` cycles, median of 7; system calls counted with
strace; at depth d the chain stack is d + 9 frames deep]:

| | 4C | 8C2 | 2C3 | System calls |
|---|---|---|---|---|
| glibc `backtrace (buf, 10)`, d = 10 / 100 | 7,637 / 34,760 | 8,563 / 40,206 | 9,431 / 44,862 | 2 |
| glibc `backtrace (buf, 100000)`, d = 10 / 100 | 7,687 / 35,524 | 8,696 / 41,786 | 9,471 / 45,756 | 2 |
| `get_backtrace (buf, 10, 0, 0)`, d = 10 / 100 | 2,173 / 2,171 | 2,334 / 2,334 | 2,819 / 2,852 | 1 |
| `_Unwind_Backtrace` over 18 / 108 frames | 22,547 / 142,506 | 24,316 / 152,252 | 26,555 / 169,886 | 3 / 6 |

glibc's `backtrace` reads the whole chain stack, whatever its `size`
(`access_hw_stacks` modes 4 and 6; glibc `backtrace.c`: "Always get all
available frames"). So it pays about 300 (4C), 350 (8C2) or 400 (2C3)
cycles for every frame of the stack, even for 10 entries.
`get_backtrace` costs the same at any depth, so it is the cheap way to
get the top few return addresses (profilers, sanitizers).
`_Unwind_Backtrace` reads the chain 32 frames at a time (modes 5, 4,
then 6 per 32 frames) and costs 1,330 to 1,590 cycles per frame. Most
of that is user code (inferred: a chain-stack read costs about 300 to
390 cycles per frame, [kernel-interface.md](kernel-interface.md), "What
a call costs").

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
  libgcc checks the CPU at run time (the layout: "Scanning for roots"
  below).
- glibc's unwinder interface also looks up `_Unwind_GetPCSP`, an E2K
  extension returning the chain stack position of a frame, exported by
  libgcc_s as `_Unwind_GetPCSP@GCC_3.0` (MCST's `libgcc-std.ver.in`).
  glibc dlopens libgcc_s for thread exit and cancellation and asserts
  that the symbol exists (`sysdeps/e2k/unwind-arch.h`): a libgcc_s
  without it aborts every thread that exits through the unwinder.
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

**What exceptions cost** [hw: `tools/abi/probes/ctlcost.cc`, lcc 1.29.16
`l++ -O2`, `%clkr` cycles, median of 7, four runs within 3%; system
calls counted with strace]. `throw 1` from a frame d frames below the
`catch (int)`; "with destructors" puts a local with a destructor in each
of the d frames:

| | 4C | 8C2 | 2C3 | System calls |
|---|---|---|---|---|
| a call 1 frame deep inside a `try`, returning normally | 30 | 29 | 35 | 0 |
| throw, d = 1 | 26,437 | 28,039 | 29,526 | 7 |
| d = 10 | 52,280 | 55,956 | 59,280 | 7 |
| d = 100 | 359,802 | 384,623 | 427,832 | 13 |
| throw with destructors, d = 1 | 39,186 | 41,606 | 43,645 | 13 |
| d = 10 | 195,734 | 210,877 | 221,815 | 67 |
| d = 100 | 2,211,900 | 2,420,284 | 2,574,946 | 610 |
| `catch (...) { throw; }` one frame below the final catch, d = 1 | 64,609 | 68,737 | 72,544 | 20 |
| control: the same throw caught one frame further up | 28,585 | 30,550 | 32,290 | 7 |

A throw costs as much as 50 (d = 1) to 680 (d = 100) `getppid` calls on
the 4C, and 60 to 910 on the 2C3 (`getppid` costs 531 and 468 cycles in
the same harness). Each frame it passes costs about 2,900 (4C), 3,100
(8C2) or 3,300 (2C3) cycles between d = 1 and 10, and 3,400, 3,650 or
4,100 between d = 10 and 100. That is about 2.6 times what
`_Unwind_Backtrace` pays per frame ("Chain stack frames" above), since
both phases walk the stack. Deeper frames cost more because every refill
of the unwinder's chain-stack cache and both landing writes make the
kernel walk down from the top of the stack
([kernel-interface.md](kernel-interface.md), "What a call costs";
inferred). Each frame with a destructor costs 12,700 to 14,100 cycles
more at d = 1, about half a throw, and 18,500 to 21,500 at d = 100. A
rethrow costs about a second throw with one cleanup on the way: 36,000
to 40,000 cycles and 13 system calls more than the same throw caught
directly. Entering a `try` costs nothing measurable: 100 nested calls
inside one cost what they cost after a `setjmp`, less the `setjmp`
itself, within 12 cycles.

The system calls follow from MCST's unwinder (`unwind-dw2-e2k.c`; the
strace counts are the same on all three machines):

| Step | Calls |
|---|---|
| `uw_init_context` | `access_hw_stacks` modes 5 and 4 (the stack sizes), and mode 6 for the top min(32, depth) chain frames into a cache (`CHAIN_STACK_CACHE_LEN` 32) |
| each phase's walk | mode 6 for the next 32 frames whenever the walk leaves the cache (`fill_in_chain_stack_fragm`); phase 2 starts by reading the top again (`reinit_chain_stack_cache`) |
| `uw_install_context` | mode 8 twice, 8 bytes each: the exception pointer and the selector into the landing frame's `%db[0]` and `%db[1]` (at 32 × ⌊i / 2⌋ + 8 × (i mod 2) on v1–v4, 16 × i on v5+, for `%db[i]`). Then `set_backtrace` once per 128 frames, setting the frames between to −1 ("just return") and the last one to the landing pad |

So a throw caught within about 30 frames makes 7 calls (modes 5, 4, 6,
6, 8, 8 and `set_backtrace`). One caught 100 frames up makes 13, with
eight mode-6 reads. Every landing is a full install. A frame with a
cleanup gets one, and `_Unwind_Resume` then starts again with
`uw_init_context`, so each destructor adds 6 calls (modes 5, 4, 6, 8, 8
and `set_backtrace`). With destructors a throw makes 7 + 6d calls plus
the phase-1 refills: 13, 67 and 610. The rethrow case makes 20: the
first throw (7); the rethrown exception, which first lands in the
cleanup that calls `__cxa_end_catch` (7); and its resume (6). From the
per-call costs ([kernel-interface.md](kernel-interface.md), "What a call
costs"), the calls other than `set_backtrace` take about 13,600 of the
26,400 cycles of a d = 1 throw on the 4C (inferred). The rest is
`set_backtrace` (not timed alone) and user code: the FDE lookup, the
personality routine and the exception object. A refill below the top of
the stack and a mode-8 write into a deep landing frame also pay the
kernel's walk down from the top, about 100 cycles a frame (inferred; at
d = 100 both writes go about 100 frames down). A destructor costs more
deeper in the stack (12,700 cycles at d = 1, 18,500 per frame at
d = 100 on the 4C) because each resume reads up to 32 chain frames, at
about 300 cycles each (inferred).

### Signal frames, cancellation (hw)

`tools/abi/probes/sigeh.cc` (with `sigeh-c.c`), built by lcc 1.29.16's
`l++ -O2`, the same with `-fnon-call-exceptions`, on the 4C, 8C2 and 2C3
(all agree, except where noted):

| Case | Result |
|---|---|
| `_Unwind_Backtrace` and glibc's `backtrace` in a handler of a `raise`d signal | both walk through the signal frame to `main` and `_start`: the handler, `[trampolines]+0x100`, a libc frame, `raise`, the caller, …; `_Unwind_Backtrace` ends with `_URC_END_OF_STACK` |
| `throw` in a handler of a `raise`d signal, caught in the code that called `raise` | **the process dies with SIGSEGV** |
| `throw` caught inside the handler itself | works, and the program goes on |
| `throw` in a SIGSEGV handler for a load fault, in C++ code or in an assembly leaf, with or without `-fnon-call-exceptions` | `terminate` (no handler found; SIGABRT) |
| `pthread_cancel` of a C thread blocked in `pause ()`, with a `pthread_cleanup_push` handler | works: the cleanup handler runs, `PTHREAD_CANCELED` |
| `pthread_cancel` of a C++ thread in a `pthread_testcancel` loop | works: destructors run, `catch (abi::__forced_unwind &)` sees it and rethrows |
| `pthread_cancel` of a C++ thread blocked in `pause ()` | **the process dies**: SIGILL on the 4C, SIGSEGV on the 8C2 and 2C3 |
| `pthread_exit` from a nested C++ function | works: destructors run, `catch (...)` sees it and rethrows |
| `siglongjmp` out of a handler | works; the signal mask is restored |

So with lcc's runtime an exception (or a forced unwind) cannot cross a
kernel signal frame. MCST's unwinder lands in the catching frame by
rewriting the return addresses of every frame between with
`set_backtrace` and then returning through them (above); the kernel's
`set_backtrace` skips trampoline frames (kernel `kernel/backtrace.c`),
and returning through the handler's caller runs `sigreturn`, which
restores the interrupted state (inferred). Cancellation of a thread
blocked in a system call goes through glibc's SIGCANCEL handler, which
unwinds from inside the handler, hence the death of the blocked C++
thread. In C, `pthread_cleanup_push` registers a jump buffer, and glibc's
forced-unwind stop function leaves for it with a `longjmp`, which can
cross the signal frame as `siglongjmp` does, so the C thread survives
(glibc `nptl/unwind.c`; inferred). A port that uses MCST's unwinder inherits all of
this; it needs no `MD_FALLBACK_FRAME_STATE_FOR` (signal frames have no
FDE and are skipped like any other frame without one).

## Scanning for roots [hw: `tools/abi/probes/hwscan.c` with `hwscan.s`, lcc 1.29.16 `-O2`; 4C, 8C2 and 2C3 unless marked]

On E2K the registers of every caller are in the procedure stack, not on
the data stack. So a conservative collector that scans only the data
stack, as on x86, misses pointers that live only in registers. In
`hwscan` a `malloc`ed pointer is kept in a register of a function,
where an `asm` barrier holds it, with 4 and with 41 frames between it
and the scanner. It is in none of the data stack's words between the
scanner and the top of the main stack. In the procedure stack it is in
that function's `%r2` and `%r3`. There are also stale copies in unused
registers of the frames between: in one frame on the 8C2 and 2C3, and
in three on the 4C, two of them tagged. The collector must read the
procedure stack:

1. Call `access_hw_stacks (E2K_GET_PROCEDURE_STACK_SIZE, 0, 0, 0,
   &size)`, then `E2K_READ_PROCEDURE_STACK_EX` from offset 0 for `size`
   bytes. Make both calls from one function, since the size changes with
   every call ([kernel-interface.md](kernel-interface.md), "Access").
   The read costs about 100 to 130 cycles per chain frame of the stack
   (kernel-interface.md, "What a call costs"). The kernel has spilled
   everything. The copy ends with the registers below the call (`wbs`
   pairs) of the function that made the system call. Through glibc
   these are `syscall ()`'s 5 pairs. For an assembly function that calls
   the kernel itself, they are its `%r0`–`%r15` (`wbs` = 8), in the last
   256 bytes. That function's registers above `wbs` (the system call's
   arguments) are not in the copy. No part of any window stays only in
   registers.
2. To tell the frames apart, also read the chain stack (modes 4 and 6).
   Walk down from the top chain frame; mode 4 counts one frame for the
   current function. Each frame's area is its `wbs` × 32 bytes, just
   below the previous one's, as in MCST's unwinder. A collector that
   scans every word needs no frame map.
3. The layout of a frame's area, for register N (an assembly frame with
   known values in `%r0`–`%r15`; `setwd nfx = 0` and `nfx = 1` alike):

   | | v1–v4 (4C) | v5+ (8C2, 2C3) |
   |---|---|---|
   | `%rN`, the low 64 bits | 32 × ⌊N / 2⌋ + 8 × (N mod 2) | 16 × N |
   | the rest of the register: on v1–v4 the 16-bit extension (an 80-bit value's high part); on v5+ the upper 64 bits (also a 128-bit value's high half, `hwscan.c` section 7) | 32 × ⌊N / 2⌋ + 16 + 8 × (N mod 2) | 16 × N + 8 |
   | that slot after an ordinary 64-bit write | stale bits, tag 0 (seen: up to 16 bits) | 0, tag 5 |

   On v1–v4 only the value slots, half of the words, can hold a pointer.
   On v5+ the second slot can hold one too. lcc's v5 code moves 16-byte
   data, such as a structure with two pointers, through 128-bit
   registers (`qppackdl` + `stqp`, [operations-v5.md](../isa/operations-v5.md)),
   so a collector there must scan every word.
4. The copy keeps the tags, but an ordinary load reads a tagged word as
   a plain number without trapping ([speculation.md](../isa/speculation.md),
   "Spill and fill"). A C mark loop over the copy ran over a diagnostic
   value without trapping, and so did Boehm GC (below). A scanner must
   not read the copy with the fill MAS.
5. A thread stopped by a signal (Boehm GC stops threads with SIGPWR)
   reads its stacks inside the handler. Its copy has the interrupted
   function's whole window as an ordinary frame (`wbs` = its `wsz`: all
   4 pairs of the probe's assembly loop). Then comes the trampoline
   frame (`[trampolines]+0x100`, `wbs` 0, `cr1.pm` 1), then the
   handler's frames. `uc_extra.proc_stack_offset` is where the
   interrupted thread's part ends, and `uc_extra.chain_stack_offset` is
   the trampoline frame's offset (480 and 128 in the probe).
   `%g16`–`%g31` are in the signal frame on the data stack
   (`uc_extra.g`; `%g20` at `ucontext` + 1,880), so a data-stack scan
   from the handler's stack pointer covers them. `%g0`–`%g15` are in
   neither stack: they stay in the registers, and the handler read the
   interrupted `%g4` at entry. Code that follows the register convention
   keeps no pointer there ([registers.md](registers.md): temporaries
   only in `%g16`–`%g31`). No global survives a call, so the collecting
   thread itself holds nothing in globals.
6. Boehm GC 8.2.2 (`libgc.so.1` of Elbrus Linux on the 8C2 and 2C3; ALT
   Linux on the 4C ships none). Its `gc.h` gives E2K a `reg_base` ("the
   bottom of the register stack") in `struct GC_stack_base`, as for
   IA-64 [source: the installed header; the library's source is not
   installed]. It reads the procedure stack through glibc's `syscall
   ()`: mode 5 twice, then mode 7 from offset 0 for the size returned,
   into a buffer on the data stack. It does this in the collecting
   thread and in each stopped thread's SIGPWR handler, and it never
   reads the chain stack [hw, 8C2: `strace -f` and an `LD_PRELOAD`
   logger of `syscall ()`; every one of its 43 reads in a run was the
   whole stack from offset 0, and all mode 4 and 6 calls were the
   probe's own]. So each collection copies every thread's whole
   procedure stack onto that thread's data stack. Results (`hwscan.c`
   built with `-DHWSCAN_LIBGC`), at least two runs on each machine, all
   the same:

   | Object referenced only from | Result |
   |---|---|
   | a register of a function 6 or 43 calls above `GC_gcollect` | kept |
   | nothing (control) | finalized |
   | `%r1`, `%r4` or `%r7` of a thread stopped in an assembly loop | kept |
   | a register of the frame below the stopped one, or a local on its data stack | kept |
   | `%g20` of the stopped thread, also as an interior pointer | kept |
   | **`%g4` of the stopped thread** | **finalized while still referenced** |
   | (`GC_gcollect` called below a frame that holds a diagnostic value) | no trap |

   The first row's object also survived one more collection after its
   function had returned. A leftover copy somewhere kept it; the probe
   did not find where. Stale stack slots do the same on other targets.
   So code compiled under the register convention is scanned correctly.
   A program that keeps a heap pointer only in `%g0`–`%g15` (a global
   register variable) must also keep it in a root the collector sees.

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

glibc implements `setjmp`/`longjmp` in assembly and C with kernel
support for the hardware stacks (`OpenE2K/glibc/sysdeps/unix/sysv/linux/e2k`).
The user contexts are kernel calls: `getcontext` is fast system call 4,
`setcontext` and `swapcontext` are system calls 369 and 371, and there
is **no `makecontext`**: E2K's glibc exports `int makecontext_e2k (ucp,
fn, argc, ...)` (system call 370, which allocates hardware stacks for
the context and can fail) and `freecontext_e2k` (372, which frees them)
instead (glibc `ucontext.h`, `makecontext.c`, `Versions`; lcc
1.29.16's sysroot has no `makecontext`). Code written for
`makecontext` (coroutine libraries, gccgo's goroutines) needs porting,
and every switch is a system call [source]. A compiler only needs to
treat `setjmp` as returning twice. The compat suite (`longjmp` unit)
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

The `jmp_buf` layout (`setjmp_tmpl.S`): byte 0 the signal mask, 8 the
return IP (`%cr0.hi`, **not mangled**), 16 `%cr1.lo`, 24/32
`%pcsp.lo`/`.hi`, 40 `%pcshtp` (32 bits), 44 `%br` (32 bits), 48
`%usd.lo`, 56 `%wd.psz` (32 bits), 64 and 72 reserved. `e2k_longjmp2`
copies the IP into the target frame as it is, so a compiler can
**re-enter a frame at a label**, which GCC's nonlocal `goto` and
`__builtin_longjmp` need: the receiving function calls `_setjmp` on a
`jmp_buf` in its frame and writes the label's address at byte 8; a
`longjmp` from any depth then resumes the frame at the label, with the
window registers as they were at the frame's latest call and `%br`
from the `_setjmp` call (so not inside a rotating loop). On the 4C
(`tools/abi/probes/reenter.c` with `reenter.s`, non-PIC): a local set
before the second call is intact at the label, from depths 0 to 256
(spilled windows included). The proof-of-concept backend uses this for
nested functions and `__builtin_setjmp`, PIC and non-PIC
(`docs/spec-poc-findings.md` A15).

**What it costs** [hw: `tools/abi/probes/ctlcost.cc`, lcc 1.29.16 `l++
-O2`; `%clkr` cycles, pinned to one CPU, median of 7 runs; four runs
agree within 8%, the last three within 1%]. The function that calls
`setjmp` calls a function that recurses d frames deep, and the innermost
frame jumps back:

| | 4C | 8C2 | 2C3 |
|---|---|---|---|
| `setjmp` (glibc's macro for `_setjmp`) | 74 | 61 | 66 |
| `sigsetjmp (env, 1)` | 175 | 170 | 179 |
| `setjmp` + `longjmp`, d = 1 | 2,976 | 3,331 | 3,385 |
| d = 10 | 4,525 | 4,976 | 5,235 |
| d = 100 | 18,741 | 20,697 | 23,862 |
| `sigsetjmp (env, 1)` + `siglongjmp`, d = 1 / 100 | 3,136 / 18,900 | 3,512 / 20,876 | 3,514 / 24,049 |
| `setjmp` and the same calls returning normally, d = 1 / 100 | 94 / 2,860 | 91 / 2,838 | 89 / 3,217 |
| `sigsetjmp (env, 1)` and the same calls returning normally, d = 1 / 100 | 196 / 2,959 | 201 / 2,950 | 201 / 3,330 |
| `getppid` in the same harness, for scale | 531 | 493 | 468 |

`_setjmp`/`_longjmp` cost the same as `setjmp`/`longjmp`. Every
`longjmp` is one system call, `e2k_longjmp2`, and nothing else (strace,
at d = 1 and 100 and for `siglongjmp`). Out of one frame it costs about
2,900 (4C), 3,250 (8C2) or 3,300 (2C3) cycles more than returning
normally, and each further frame adds 131, 148 or 175. The kernel walks
the chain stack one frame at a time down to the target (kernel
`kernel/signal.c` `do_longjmp` → `unwind_stack` → `parse_chain_stack`).
`siglongjmp` restores the signal mask inside the same call.
`__sigjmp_save` stores the mask with the SIGKILL and SIGSTOP bits set as
a marker, and `do_longjmp` sets the blocked mask only when the SIGKILL
bit is there (glibc `sigjmp.c`; kernel `kernel/signal.c:1892-1895`).
Saving the mask costs `sigsetjmp` one fast `siggetmask` call (fast call
3, about 100 cycles; [kernel-interface.md](kernel-interface.md)).
Restoring it costs `siglongjmp` less than 100 cycles more than
`longjmp`: 50 to 85 on the 4C and 8C2, 15 to 75 on the 2C3. A `longjmp`
out of one frame costs as much as 6 or 7 `getppid` calls, or about as
much as returning normally through 100 frames.
