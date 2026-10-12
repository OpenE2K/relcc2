# Kernel interface (Linux 6.1, e2k64)

What compilers and language runtimes need from MCST's Linux 6.1 for
64-bit unprotected processes: system calls, the auxiliary vector,
contexts, signals, the stacks, the address space, threads, profiling and
static PIE. Marks as in [README.md](README.md); **kernel** paths are
relative to `OpenE2K/linux/arch/e2k/` (generic kernel files start with
`linux/`), **glibc** paths to `OpenE2K/glibc/sysdeps/unix/sysv/linux/e2k/`
unless they start with `sysdeps/`, `csu/` or `elf/`; **compiler-rt** is
MCST's compiler-rt 11.1.0 from the same tarball as **libgcc**.

**hw** here means the 4C (ALT Linux, kernel `6.1.158-mcst-e4c`), the 8C2
and the 2C3 (Elbrus Linux 9.5.5, kernels `6.1.0-1.17-e8c2` and
`6.1.0-1.18-e2c3`) agree, unless a row names one of them. The probes are
`tools/abi/probes/sysabi.c` (sections 1–10; `./sysabi maps` for the PIE
and static builds), `sigeh.cc` with `sigeh-c.c` (exceptions and
cancellation), `pgprof.c` (`-pg`), `sanitize.c` (`lcc -S`), `ctlcost.cc`
(what the calls on the hardware stacks cost), `hwscan.c` with
`hwscan.s` (what the copies of the stacks hold), `ctxsig.c` with
`ctxsig.s` (user contexts, edited signal frames, globals across system
calls and in handlers, the chain stack seen from a handler) and
`stackregs.c` with `stackregs.s` (the stack registers, how the data
stack grows, a context's stack overflowing), built by each machine's
own lcc 1.29.16.

## System calls

A system call is a `call` through a CTPR prepared with `sdisp %ctprN, E`
([control-transfer.md](../isa/control-transfer.md)). The kernel enables
these entries for a 64-bit task (`%osem`; kernel
`include/asm/traps.h:137-162`, `include/asm/trap_def.h:236-253`; uapi
`unistd.h:15-20`):

| Entry | Use |
|---|---|
| 1 | 32-bit system call table (enabled for 64-bit tasks too) |
| **3** | **64-bit system call table**: what glibc and the uapi headers use |
| 4 | deprecated: a negative number −N calls N in an old table, others go to entry 3 (kernel `kernel/trap_table.S:554-597`) |
| 5, **6** | fast system calls, 32-bit and **64-bit** (below) |
| 7, 8 | fast and ordinary calls of protected mode: protected tasks only |

32-bit code uses entries 1 and 5, protected code 8 and 7; both are out of
scope here.

```
{ sdisp %ctpr1, 0x3              ; entry 3
  addd  0x0, NR, %b[0]
  addd  0x0, ARG1, %b[1] ... }   ; arguments in %b[1]..%b[6]
call  %ctpr1, wbs = W            ; W = caller's wsz - 4
addd  0x0, %db[0], RESULT
```

(uapi `e2k_api.h:121-311`; glibc `e2k64/sysdep.h:38-86`, `sysdep.h:101-147`.
The uapi macros `__E2K_SYSCALL_0` and `E2K_SYSCALL`, compiled by lcc,
return what `getpid ()` and `write ()` do, and −9 for a bad descriptor:
`sysabi.c` section 9 [hw].)

| Item | Rule | Source |
|---|---|---|
| Window | The caller hands over 1 to 4 pairs (`%b[0]`–`%b[7]` at most); glibc, the uapi macros and lcc hand over 4. glibc's assembly wrappers use `setwd wsz = 9`, `wbs = 5`. The entry starts with `{ vfrpsz rpsz=4; setwd wsz=4; setbp psz=0 }`, so **handing over 5 or more pairs kills the process**: the entry's checks trap at a kernel IP, and the kernel answers that with SIGKILL. `getpid` returns with 1–4 pairs (`wsz − wbs`) and dies with 5–8 [hw: `tools/isa-probes/ctlleft.c` `sc_getpid*`, 4C, 8C2, 2C3]; `getppid` with 6 or 8 pairs and fast call 2 with 6 died too, 2 and 4 pairs worked [hw: `ctxsig.c` section 3]. Which check fires is inferred: in user code, when both fail only the `setwd` one's SIGILL is seen (operations-v3.md, `vfrpsz`), so in entry 3 (`setwd wsz=4`) it is presumably the `setwd`; entry 6's `setwd wsz = 0x8` holds a parameter area of up to 8 pairs (`wsz` counts pairs), so for 5–8 pairs only its `vfrpsz rpsz=4` fails there. Not tried: 0 pairs (the number is in `%b[0]`), and fewer than 4 pairs with arguments (presumably fine while they cover the arguments). A compiler that emits system calls inline must hand over at most the 4 pairs the convention gives | kernel `kernel/trap_table.S:545-546,698-699`, `include/asm/trap_table.S.h:49`, `kernel/traps.c:915-922,1102-1106`; glibc `sysdep.h:105-106` |
| Number | `%b[0]`, low 32 bits, zero-extended; numbers ≥ 446 (`NR_syscalls`) go to `sys_ni_syscall` (−ENOSYS) | kernel `kernel/trap_table.S:405-418`; uapi `unistd.h:466` |
| Arguments | `%db[1]`–`%db[6]`, tags cleared by the kernel. The uapi's 7-argument macro also writes `%b[7]`, but entry 3 uses `%dr7` as scratch | kernel `kernel/trap_table.S:405-460`; uapi `e2k_api.h:278-309` |
| Result, error | `%db[0]`; −errno. glibc's C wrappers test `(unsigned long) r > -4096UL`, its assembly `PSEUDO` any negative value (`cmpldb %db[0], 0`) | uapi `e2k_api.h:129`; glibc `sysdeps/unix/sysv/linux/sysdep.h:26-28`, `sysdep.h:117-122` |
| Clobbers | uapi: `"ctpr1"`–`"ctpr3"`, `"b[0]"`–`"b[7]"`; glibc: `"call"`, `"memory"`. Neither list names a global, but a system call through entry 3 returns with **`%g16`–`%g31` set to 0 with tag 5**: the kernel keeps its own values in `%g16`–`%g25`, and its common exit path empties all sixteen. `%g0`–`%g11`, `%g14` and `%g15` are kept [hw: `ctxsig.c` section 3, `getppid`, `tgkill` and the clock fast calls that continue in entry 3; `%g12`, `%g13` not tried]. (`sigreturn` is the exception: it returns to code interrupted outside a system call with the frame's values, "Signals".) Predicates and the window below `wbs` are kept by the call mechanism, and the `call` empties `%ctpr1`–`%ctpr3` like any call (inferred). glibc's `"call"` clobber makes lcc treat the asm as a call, across which `%g16`–`%g31` are not preserved anyway ([registers.md](registers.md); inferred). Code that uses the uapi macros, or its own asm, must not keep a value in `%g16`–`%g31` across a system call | uapi `e2k_api.h:108-111`; glibc `sysdep.h:172-173`; kernel `kernel/ttable-inline.h:756-783`, `include/asm/gregs.h:146-147`, `include/asm/e2k_api.h:909-922` |

**`%#`** in `call %%ctpr1, wbs = %#` (uapi, glibc) is lcc's operand
modifier for the `wbs` that leaves `%b[0]`–`%b[7]` above it: `wsz − 4`
([source-compat.md](source-compat.md), "`%#`"). A compiler that builds
glibc or code including `<asm/e2k_api.h>` must accept `%#` and these
clobbers.

E2K numbers (uapi `unistd.h`): 119 `sigreturn` (173 `rt_sigreturn` is
`sys_deprecated`, kernel `kernel/systable.c:165,219`), 228 `e2k_syswork`
(users get `FAST_RETURN` 58, an empty call, and `FLUSH_CMD_CACHES` 8 →
−EPERM, "Snooping is done by hardware since V3"; kernel
`kernel/e2k_syswork.c:3083-3145`), 229 `clone_thread`, 230 `e2k_longjmp2`,
252–254 `set_backtrace`, `get_backtrace`, `access_hw_stacks`, 260
`el_binary`, 369–372 `setcontext`, `makecontext`, `swapcontext`,
`freecontext`.

**What a call costs** (`sysabi.c` section 9, `%clkr` cycles per call in a
loop of 100,000):

| Call | 4C | 8C2 | 2C3 |
|---|---|---|---|
| `getppid` (entry 3) | 492 | 447 | 420 |
| `sigprocmask` query (fast call 3) | 96 | 104 | 107 |
| `clock_gettime (CLOCK_MONOTONIC)` (fast call 1) | 1,209 | 1,442 | 234 |
| `clock_gettime (CLOCK_PROCESS_CPUTIME_ID)` (entry 3) | 1,304 | 1,187 | 1,385 |

A fast call itself costs about 100 cycles. The clock read is what is
slow on the 4C and 8C2: their clock source is `lt`, a timer behind an
I/O read, while the 2C3 uses `sclkr` (`/sys/devices/system/clocksource`),
which advances in steps of about 330 ns. `%clkr` is cheap but per core
(below, "Time").

Calls on the hardware stacks [hw: `tools/abi/probes/ctlcost.cc`,
`%clkr` cycles, median of 7. Each call is made from one C function
through glibc's `syscall ()`; `getppid ()` costs 531, 493 and 468
there. At depth 10 the chain stack has 19 frames and the procedure
stack 3.4 KB; at depth 100, 109 frames and 17.8 KB]:

| Call | 4C | 8C2 | 2C3 |
|---|---|---|---|
| `access_hw_stacks` mode 4 or 5 (a stack size) | 585–610 | 555–575 | 550–570 |
| mode 6 `READ_CHAIN_STACK_EX`, the top frame | 1,370 | 1,342 | 1,410–1,440 |
| mode 6, the top 32 frames (of 109) | 10,686 | 12,216 | 13,798 |
| mode 6, the whole chain stack, 19 / 109 frames | 6,598 / 33,509 | 7,426 / 38,980 | 8,320 / 43,671 |
| mode 6, the bottom 32 of 109 frames | 17,875 | 19,679 | 22,993 |
| mode 7 `READ_PROCEDURE_STACK_EX`, the top 1 KiB | 2,390 | 2,250 | 2,630–2,660 |
| mode 7, the bottom 1 KiB, under 19 / 109 frames | 3,411 / 12,223 | 3,449 / 13,465 | 3,916 / 15,562 |
| mode 7, the whole stack, 19 / 109 frames | 3,640 / 14,100 | 3,550 / 14,560 | 4,030 / 16,680 |
| mode 8 `WRITE_PROCEDURE_STACK_EX`, 8 bytes 1 KiB below the top | 2,200 | 2,160 | 2,530–2,560 |
| `get_backtrace`, 10 addresses | 2,170 | 2,330 | 2,820–2,850 |

The cost depends on where the range is: for chain reads and for
procedure-stack reads and writes alike, the kernel walks the chain stack
one frame at a time from the top down to the requested range (kernel
`kernel/e2k_syswork.c`: `____parse_chain_stack` 1413-1436,
`__read_current_chain_stack` 2534-2600, `copy_current_proc_stack`
2712-2866). Each chain frame passed on the way
costs about 93 (4C), 97 (8C2) or 119 (2C3) cycles in a chain read, and
98, 111 or 129 in a procedure-stack read. Each chain frame copied costs
296, 348 or 388 more: the kernel builds every copy field by field and
checks whether its return address is in the trampoline page (inferred).
Procedure-stack bytes cost about 0.1 cycle each or less: at depth 100
the 16.8 KB between the bottom 1 KiB and the whole stack add 1,860
cycles on the 4C and 1,100 on the 8C2 and 2C3. A chain read costs about
1,000 to 1,100 cycles plus these. So the size queries and the reads near
the top cost the same at any depth. Reading a whole stack costs about
300 to 390 (chain) or 100 to 130 (procedure) cycles per frame of the
stack. A read or write deep in the stack pays for every frame above it;
this includes libgcc's mode-8 writes into a landing frame d frames down
(inferred from the source, not timed). What `longjmp` (`e2k_longjmp2`),
exceptions and backtraces cost is in
[stack-and-unwinding.md](stack-and-unwinding.md).

## Fast system calls (entry 6)

Entry 6 masks the number in `%dr0` with 7 and jumps to a table entry with
`%dr1` and `%dr2` (tags cleared) as its only arguments; the handler runs
in the entry's window without saving user state or switching stacks and
returns −errno (kernel `kernel/trap_table.S:691-746`,
`fast_syscalls/fast_syscalls.c:60-69`, `include/asm/unistd.h:14-16`).
The calling sequence is the one above with `sdisp %ctpr1, 0x6` (glibc
`e2k64/sysdep.h:39`). Pointers are masked to 48 bits.

| N | Call | Arguments | Notes (kernel `include/asm/fast_syscalls.h`, `fast_syscalls/`) |
|---|---|---|---|
| 0 | `gettimeofday` | `tv`, `tz` | continues in entry 3 inside the kernel if the clock can't be read fast |
| 1 | `clock_gettime` | `clockid`, `tp` | fast for `CLOCK_REALTIME`, `CLOCK_MONOTONIC` and their `_COARSE` forms; other clocks continue in entry 3, so callers need no fallback (`fast_syscalls.h:294-303`) |
| 2 | `getcpu` | `cpup`, `nodep` | the third argument is not passed |
| 3 | `siggetmask` | `oset`, 8 | glibc's `sigprocmask` with a null new set (`sigprocmask.c:73-80`) |
| 4 | `getcontext` | `ucp`, 8 | no ordinary counterpart (below) |
| 5 | `set_return` | `ip`, 0 | rewrites `cr0_hi` of the nearest user chain frame; checks only that `ip` is a user address (`fast_syscalls.h:415-460`). Not advertised; glibc doesn't use it |
| 6, 7 | — | | −ENOSYS |

A fast call keeps `%g0`–`%g11`, `%g14`, `%g15` and `%g16`–`%g25`
(`%g12`, `%g13` not tried), but **may change `%g26`–`%g31`**: the kernel
is compiled to use those six as scratch registers (`-fglobal-regs`;
`SCRATCH_GREGS_START` 26, kernel `include/asm/glob_regs.h:22-24, 57-68`).
The calls tested changed only `%g26`–`%g28`: fast calls 2 and 3 left
kernel values in all three on every machine, and the clock calls 0 and 1
changed `%g26` on the 2C3. On the 4C and 8C2 the clock calls emptied all
of `%g16`–`%g31`, because their `lt` clock source makes them continue as
ordinary system calls through entry 3 (above; kernel
`fast_syscalls/fast_clock_gettime.c:15-18`) [hw: `ctxsig.c` section 3].
So treat `%g16`–`%g31` as clobbered by every system call, fast or not.

glibc uses a fast call only when its bit is set in `AT_FAST_SYSCALLS`
(glibc `sysdep-vdso.h:26-40`). **There is no vDSO**: `AT_SYSINFO_EHDR`
(33) is defined but never supplied (kernel `include/asm/elf.h:280-293`,
`linux/fs/binfmt_elf.c:253-292`; 0 on all three machines [hw]), glibc's
`libc-vdso.h` is empty, and the time functions go through entry 6.

## Auxiliary vector [hw]

| Entry | Value | Source |
|---|---|---|
| `AT_FAST_SYSCALLS` (32) | `0x1f`: bit N = fast call N available (0–4); glibc's `__libc_e2k_fast_syscalls` | kernel `include/asm/elf.h:280,288`; uapi `auxvec.h:9`; glibc `dl-auxv.h:26-27` |
| `AT_SYSTEM_INFO` (34) | `0x1`: bit 0 = `vfork` implemented (glibc's `__libc_e2k_system_info`; without the bit its `vfork` calls `fork`) | kernel `include/asm/elf.h:281-289`; glibc `vfork.c:18-19` |
| `AT_BINCOMP_INFO` (35) | absent: only the binary translator's loader passes it | kernel `sec_space/rtc_procfs/binfmt_rtc_elf.c:319` |
| `AT_HWCAP` | 0 | kernel `include/asm/elf.h:229` |
| `AT_PLATFORM`, `AT_HWCAP2`, `AT_SYSINFO_EHDR`, `AT_MINSIGSTKSZ` | absent (`ELF_PLATFORM` is NULL) | kernel `include/asm/elf.h:238`; `linux/fs/binfmt_elf.c:279-290` |
| `AT_PAGESZ` | 4096 | kernel `include/asm/elf.h:218` |

**CPU detection.** The auxiliary vector says nothing about the CPU. An
ifunc resolver gets no arguments (glibc `sysdeps/e2k/dl-irel.h`) and runs
before constructors (static: `csu/libc-start.c:307-310` before
`call_init` at :399; dynamic: `elf/rtld.c` relocates before `_dl_init`),
so lcc's `__cpu_info`, filled by the priority-101 constructor `__cpu_init`
([source-compat.md](source-compat.md), "CPU detection"), is still zero
there. Resolvers, `target_clones` and `__builtin_cpu_supports` must call
`__builtin_cpu_init` first or read `%idr`
([semantics.md](../isa/semantics.md)) (inferred).

## User contexts [source; hw: `ctxsig.c` section 1]

Context switching is done by the kernel, which owns the hardware stacks
(glibc `ucontext.h:51-56` and the `*context.c` files; kernel
`kernel/proc_context.c`). There is **no `makecontext`**
(`stdlib/makecontext.c` is the ENOSYS stub; lcc's sysroot exports none).
[hw] in this section means `tools/abi/probes/ctxsig.c` section 1, on
which the 4C, 8C2 and 2C3 agree apart from addresses.

| Function | Implementation |
|---|---|
| `int getcontext (ucontext_t *)` | glibc stores `%cr0`, `%cr1`, `%usd` with `rrd`; fast call 4 stores the signal mask, the caller frame's stack pointers, `sbr`, `%fpcr`/`%fpsr`/`%pfpfr` and the context key (`fast_syscalls.h:370-412`) |
| `int setcontext (const ucontext_t *)` | system call 369 `(ucp, 8)` |
| `int swapcontext (ucontext_t *, const ucontext_t *)` | system call 371 `(oucp, ucp, 8)`: saves and loads the signal mask, `%cr0`/`%cr1`, the stack registers and the three FP control registers (`proc_context.c:1515-1548, 1828`) |
| `int makecontext_e2k (ucontext_t *, void (*)(void), int argc, ...)` | system call 370 `(ucp, makecontext_helper, 8 * (argc + 4), buf, 8)`; allocates a procedure and a chain stack, so it can fail (−1, errno ENOMEM, EINVAL or EFAULT; `EINVAL` for a 0- or 16-byte stack [hw]) |
| `int freecontext_e2k (ucontext_t *)` | system call 372 `(ucp)`: frees them; `ENOENT` for an unknown context, `EBUSY` only for the calling thread's own running context (`proc_context.c:1264-1295`) [hw: a made context freeing itself and the main context freed from main both gave `EBUSY`. The main context freed from inside a made context while it was suspended gave 0, and the later return through `uc_link` to it failed with `ESRCH`. A finished context gave 0 and a second free `ENOENT`. A made context that never ran, freed through a `memcpy` copy, gave 0, and freeing the original then gave `ENOENT`] |

- The context runs on `uc_stack` (base and size aligned to 256,
  `proc_context.c:1132-1138`). Its top is the context's **key**, which
  `makecontext_e2k` writes to `uc_mcontext.nr_TIRs` (kernel
  `include/asm/ucontext.h:38-55`) [hw]. A switch looks the target up by
  the key in the `ucontext_t` it is given (`ESRCH` if no context has it,
  `EBUSY` if that context is running; `proc_context.c:804-822`,
  `311-330`), so every copy of a `ucontext_t` names the same context.
  From that `ucontext_t`, as it is at the time of the switch, the switch
  also loads the signal mask, `%fpcr`/`%fpsr`/`%pfpfr`, `cr0_hi`, `cr1`
  and the chain-stack position (`proc_context.c:1515-1527`).
- **A context's data stack can overflow into the memory below
  `uc_stack`** [hw, `stackregs.c` section 7; 4C, 8C2, 2C3]. At the start
  its `USD` covers exactly `uc_stack`: the bottom is `ss_sp`, and `%sbr`
  is the top rounded down to a page (`stackregs.c` section 5;
  [stack-and-unwinding.md](stack-and-unwinding.md), "Stack registers").
  A `getsp` past the bottom traps as in the main thread ("The data
  stack" below). The kernel then extends the stack, up to a total of
  `RLIMIT_STACK` (`mm/fault.c:1075`), into whatever mapping holds the
  new bottom. It
  checks for holes only between mappings that end below the stack's top
  (`mm/fault.c:1105-1127`), so it does not see a gap directly below the
  mapping that holds `uc_stack` (inferred from the code and the third
  row). The test put a 64 KiB `uc_stack` in the middle of a 1 MiB
  mapping and ran four nested frames that each allocate and fill
  32 KiB:

  | Directly below `uc_stack` | Result |
  |---|---|
  | the rest of the mapping | the stack grows 32 KiB per frame; 65,640 bytes below `uc_stack` are overwritten (65,656 on the 8C2) and no signal is raised |
  | a `PROT_NONE` page | the stack grows over the page; the first store into it gives SIGSEGV `SEGV_ACCERR` at `ss_sp − 16` |
  | one unmapped page | the stack grows over the gap; SIGSEGV `SEGV_MAPERR` at `ss_sp − 16` |
  | 512 KiB unmapped | the `getsp` fails: SIGSEGV at `ss_sp − 1`, `SEGV_BNDERR` on the 8C2 and 2C3 (`kernel/traps.c:1132-1141`); the 4C's ALT kernel reports `SEGV_ACCERR` (its source is not at hand) |

  A fiber library should leave unmapped memory below each context's
  stack, at least one growth step large: the larger of 16 KiB and the
  largest single `getsp`, rounded up to a page. A `getsp` whose new
  bottom falls into that gap fails with SIGSEGV; one that reaches past
  it into another mapping is accepted. A `PROT_NONE` guard page faults
  only when a store lands in it: a frame that the kernel has extended
  past it can write below it unnoticed (inferred). The kernel's comment
  on `expand_user_data_stack` says E2K needs no gap below a stack
  because the hardware bounds catch overflows (`mm/fault.c:1019-1036`).
  The extension into the mapping below is what defeats that.
- glibc passes the arguments as 8-byte slots copied from its `va_list`
  (so no 16-byte arguments) after `ucp`, the function, `argc` and a
  zero. The kernel loads the first 8 slots into `makecontext_helper`'s
  registers and copies the rest to the new stack. The helper calls the
  function with them in `%b[0]`–`%b[7]` (glibc `makecontext.c`,
  `makecontext_helper.S`; `proc_context.c:686-765`). [hw: with 0, 3, 8
  and 12 `long` arguments every one arrived, the 9th to 12th included.]
- **`uc_link` is read twice** [hw, source]. When the function returns,
  glibc's `makecontext_helper` loads `uc_link` from the `ucontext_t` that
  was passed to `makecontext_e2k`, as the field is at that moment, not
  from a copy the context was started through. **It loads only the low
  32 bits** (`ldw` at offset 8, then `cmpedb` with 0; glibc
  `makecontext_helper.S`, where `LDP` is `ldw` for 64-bit code). If they
  are zero, the helper calls glibc's `exit (0)`, which runs atexit
  handlers and ends the process. That also happens for a non-null
  `uc_link` whose low 32 bits are zero, such as an object at a 4 GiB
  boundary. Otherwise the helper returns into `[trampolines]+0x400`. The
  trampoline tests and uses the full 64-bit `uc_link` that the kernel
  **copied at `makecontext_e2k` time** (`proc_context.c:735-753`,
  `proc_context_stacks.c:46-51`) and calls `setcontext` on it. If the
  copied value was null, or that `setcontext` fails, it makes the `exit`
  system call (number 1, not `exit_group`), which **ends only the calling
  thread**: no atexit handlers, no stdio flush. The status is 0 for a
  null value and the error number for a failed `setcontext` (kernel
  `kernel/trap_table.S:1297-1328`). Hardware results:
  - A `uc_link` at 0x7_0000_0000 ran the atexit handler and exited 0.
    The control at 0x7_0000_0100 returned to the main context.
  - Changing `uc_link` from the main context to null after
    `makecontext_e2k` ran the atexit handler and exited 0.
  - Changing it from null to the main context exited 0 without running
    the handler. In a second thread the same change ended only that
    thread: `pthread_join` returned 0 and the process went on.
  - Changing it to another context B, starting from a copy whose
    `uc_link` is B, or starting from a copy whose `uc_link` is null,
    returned to the main context.
  - A `uc_link` naming an unknown key exited with status 3 (`ESRCH`), as
    did one naming a main context freed while it was suspended.

  So set `uc_link` before `makecontext_e2k`, leave it unchanged, and
  never let the context it names lie at an address whose low 32 bits are
  zero.
- **Each context has its own FP control registers and signal mask**
  [hw]. `makecontext_e2k` clears `uc_sigmask` and writes the process
  defaults to `uc_extra` (`fpcr` 0x33f, `fpsr` 0x3f, `pfpfr` 0x1fbf;
  `proc_context.c:954-1035`). A switch loads both from the target
  `ucontext_t` and saves the current ones into `oucp`. The new context
  started with rounding to nearest and all flags set (`%fpcr` read back
  as 0x37f). It set rounding toward zero and overflow, then switched
  back. The main context's upward rounding and inexact flag were
  unchanged, and the context found its own settings again when resumed.
  `getcontext` followed by `setcontext` on one stack also restores the
  FP state saved by `getcontext`. Unlike a signal handler ("Signals"), a
  context switch does keep the FP environment. The signal mask works the
  same way. With USR1 blocked in the main context, a context whose
  `uc_sigmask` got TERM after `makecontext_e2k` started with only TERM
  blocked. It blocked USR2 and switched back, and the main context still
  had only USR1 blocked. The resumed context had USR2 and TERM.

Contexts and their keys [hw]:

| Case | Result |
|---|---|
| Starting a made context through a `memcpy` of its `ucontext_t`; resuming a suspended context through a copy of the state `swapcontext` saved | works |
| `makecontext_e2k` again on a suspended context's stack (same key) | returns 0 and reuses the context; switching to it starts the function again from the top. The state saved at the suspension now names the new context: **switching to that state kills the process (SIGKILL)** |
| `makecontext_e2k` on the running context's own stack, from inside it | returns 0. The new context takes the key (the kernel drops the running one from its table; the comment names gccgo, `proc_context.c:1143-1222`) and runs when switched to. Switching back to the old context's saved state kills the process (SIGKILL) |
| A switch to a context that is running in another thread | −1, `EBUSY` |
| `setcontext` or `swapcontext` to a key that no context has | −1, `ESRCH`. The same for a context freed with `freecontext_e2k`: after the suspended main context was freed, the return through `uc_link` to it ended with the trampoline's exit status 3 (`ESRCH`) |
| `getcontext` before the thread's first switch | the key is the top of the thread's data stack (the end of `[stack]`). `freecontext_e2k` of it gives `ENOENT`: the kernel creates the main context at the first switch (`save_main_coroutine`, `proc_context.c:1736-1746`) |

A switch whose saved chain-stack position or IP is not the target
context's top frame is finished by a kernel `longjmp` on the target's
stacks, which assumes the frame was called with 4 pairs handed over
(`proc_context.c:1832-1852`). If the saved position is not on those
stacks, as for a state saved before the context was made again,
`do_longjmp` does not find the frame and **kills the process instead of
returning an error** (kernel `kernel/signal.c:1504-1545`,
`unwind_stack`, `force_sig (SIGKILL)`). So a program may remake a
context only when it no longer needs any saved state of the context's
earlier run.

A context's hardware stacks, a 16 KiB `[procedure stack]` and a 4 KiB
`[chain stack]`, are mapped by `makecontext_e2k` itself, before the
first switch (kernel `kernel/process.c:714-718`) [hw:
`/proc/self/maps`]. **`freecontext_e2k` does not unmap them**: the
kernel keeps freed hardware stacks in a per-process cache of up to
1 MiB and gives them to the next context or thread (kernel
`kernel/process.c:479-576`; the free itself waits for an RCU grace
period, `proc_context.c:351-405`) [hw: 200 ms after `freecontext_e2k`
the stacks were still mapped, and the next `makecontext_e2k` with a new
key mapped nothing new]. Every switch is a system call.

**What a switch costs** [hw, `ctxsig.c` `cost`; `%clkr` cycles per
iteration over 20,000 iterations (2,000 for the `makecontext_e2k` row),
pinned to the last CPU, min–max of three runs under the shared timing
lock]:

| Operation | 4C | 8C2 | 2C3 |
|---|---|---|---|
| `swapcontext` round trip (two switches between two contexts) | 3,195–3,197 | 3,277–3,308 | 3,293–3,337 |
| `getcontext` (fast call 4) | 146–147 | 146–175 | 158–165 |
| `getcontext` + `setcontext` back to it (same stack) | 2,645–2,649 | 3,209–3,217 | 3,205–3,266 |
| `setjmp` + `longjmp` from one call down | 2,949–2,957 | 3,308–3,314 | 3,349–3,358 |
| `sigsetjmp (b, 0)` + `siglongjmp` | 2,988–2,993 | 3,309–3,312 | 3,347–3,354 |
| `sigsetjmp (b, 1)` + `siglongjmp` | 3,105–3,115 | 3,497–3,502 | 3,474–3,484 |
| `makecontext_e2k` (key reused) + switch + return through `uc_link` | 6,262–6,310 | 6,420–6,433 | 7,025–7,049 |
| `getppid` (for scale) | 513–516 | 454–455 | 431–432 |

A switch costs about 1,600 cycles, three to four plain system calls.
`longjmp` is the `e2k_longjmp2` system call, and a `setjmp` + `longjmp`
costs about as much as a `swapcontext` round trip (glibc's `setjmp` is
the macro `_setjmp` and `_longjmp` an alias of `longjmp`, so `_setjmp` +
`_longjmp` measured the same; more in
[stack-and-unwinding.md](stack-and-unwinding.md), "setjmp/longjmp,
contexts"). Saving and restoring the signal mask adds about 120 cycles
on the 4C and 2C3 and 190 on the 8C2. A coroutine library pays a kernel
entry for every transfer. Runtimes that would use split stacks (gccgo)
must give each goroutine a full-size context:
[stack-and-unwinding.md](stack-and-unwinding.md), "No split stacks".

**The trampoline page.** The kernel maps one read-execute page,
`[trampolines]` (kernel `kernel/trampolines.c`, `include/asm/stacks.h:36-38`;
code `kernel/trap_table.S:1192-1394`; offsets
`include/asm/process.h:36-70`). On the 8C2 and the 2C3 it is the page
below `[cut]` (0xc2ff_ffff_d000, 0xc27f_ffff_d000); the 4C's newer ALT
kernel (6.1.158) maps it at a random address in the `mmap` area
(0x45dc_6594_3000 in one run) [hw: `/proc/self/maps`]. Code must not
assume where it is:

| Offset | Code | Return address of |
|---|---|---|
| +0x000 / **+0x100** / +0x200 | 32-bit / **64-bit** / protected signal return: `sdisp %ctpr2, 3` (64-bit), `sigreturn` (119) with `wsz = 8`, `wbs = 4`, then `exit (1)` | a signal handler (+0x100 [hw]: the handler's caller in `_Unwind_Backtrace` and glibc's `backtrace`, `sigeh.cc` section 3) |
| +0x300 / **+0x400** / +0x500 | context return: `setcontext (uc_link, 8)`, else `exit` | `makecontext_helper` |
| +0x600 | `return` | a frame `set_backtrace` sets to IP −1 ("just return") |

`get_backtrace` and `set_backtrace` skip frames returning into this page
(except +0x600), and chain-stack reads through `access_hw_stacks` set
`cr1.pm` in their copy of such frames, "useful for JITs to distinguish
frames with actual user data" (kernel `kernel/backtrace.c:43,176`,
`kernel/e2k_syswork.c:2581-2584`; hw: "Access" below).

## Signals

**The frame goes directly below the interrupted `USD`** (the stack
pointer), rounded down to 256 bytes: **there is no red zone**, so code
must keep no data below its `USD` (kernel `kernel/signal.c:1137-1197`).
The `rt_sigframe` is **4,272 bytes** [hw]: over 16 stack pointers 16
bytes apart, the frame started 4,272 to 4,512 bytes below `USD` on all
three machines, so it is `round_down (USD − 4272, 256)`. That is the size
with `CONFIG_PROTECTED_MODE`, whose 16-byte-aligned members pad the
4,256-byte frame (kernel `include/asm/ucontext.h:62-83`; computed). It
holds 64 bytes for the handler's incoming arguments, `siginfo_t` (128
bytes) at +64 and `ucontext_t` (0xfd8 bytes) at +192 [hw: the handler's
`uc` is its `si` + 128], then the original return IP. The main stack is
grown if needed.

The handler is entered as if called from `[trampolines]+0x100`, with
`%dr0` = signal, `%dr1` = `&info` with `SA_SIGINFO`, else
`&uc.uc_mcontext`, and `%dr2` = `&uc` or 0 [hw: `sysabi.c` section 3],
and its stack top at the frame (kernel `kernel/signal.c:932-1040`). Its
window and chain frames go on the **thread's own hardware stacks**, also
with `SA_ONSTACK` (`signal.c:1064-1116`).

`%g16`–`%g31` read 0 with tag 5, the kernel's "empty" value [hw: read
with `gettagd` in an assembly handler before any compiled code, for
`tgkill` and for a timer signal, `ctxsig.c` section 3], and `%bgr` is
0xff (`signal.c:1271-1277`, `include/asm/tags.h:48`) [source; the
frame's copy of the interrupted code's `%bgr` also read 0xff]. Reading
the empty registers does not trap. A non-speculative `addd %dg16, 0`
gives 0 with tag 0, and a non-speculative `std` stores 0, which a
fill-MAS load reads back with tag 5. A diagnostic value, such as the
result of a failed speculative load, does trap a non-speculative `addd`
(`ILL_ILLOPN`), in a handler or not. An empty half has bit 30 clear and
a diagnostic one has it set ([speculation.md](../isa/speculation.md),
"Diagnostic values").

`ucontext_t` (glibc `sys/ucontext.h:268-278`, uapi `sigcontext.h`,
`ucontext.h`; offsets printed by `sysabi.c` section 10). The `mcontext_t`,
`gregset_t` and `NGREG` in glibc's `sys/ucontext.h` are SPARC leftovers:

| Offset | Field | Holds |
|---|---|---|
| 0x000, 0x008, 0x010 | `uc_flags`, `uc_link`, `uc_stack` | 0, null (set with `SA_SIGINFO`), `stack_t` |
| 0x028 | `uc_mcontext` (`struct sigcontext`, 0x628 bytes) | starts with `cr0_lo` (predicates) |
| 0x030 | `.cr0_hi` | **interrupted IP**, bits 3–47 (compiler-rt masks `(1 << 48) - 8`; glibc `sigcontextinfo.h`) |
| 0x038, 0x040, 0x048 | `.cr1_lo`, `.cr1_hi`, `.sbr` | window, `ussz`; data stack top |
| 0x050, 0x058 | `.usd_lo`, `.usd_hi` | **SP** = `usd_lo & ((1 << 48) - 1)` [hw] (compiler-rt `sanitizer_linux.cpp:2197-2201`; glibc's `GET_STACK` is 0) |
| 0x060–0x078, 0x080 | `.psp_*`, `.pcsp_*`; `.rpr_*` | hardware stack registers; translator only |
| 0x090, 0x098, 0x130 | `.nr_TIRs`, `.tir_lo[19]`, `.tir_hi[19]` | trap info registers (index of the last) |
| 0x1c8, 0x218, 0x268, 0x278 | `.trap_cell_addr`, `_val`, `_tag`, `_info` [10] | the trapped instruction's pending memory operations |
| 0x2c8, 0x3c8 | `.dam[32]`, `.sbbp[32]` | DAM (0 unless traced); last 32 branches |
| 0x4c8, 0x648 | `.mlt[48]`, `.upsr` | translator only |
| 0x650 | `uc_sigmask` | 128 bytes, the kernel uses 8 |
| 0x6d0 | `uc_extra` (`struct extra_ucontext`, 0x908 bytes) | |
| 0x6d4, 0x6d8 | `.curr_cnt`, `.tc_count` | trap cell being handled (−1 without a trap), count |
| 0x6dc–0x6e4 | `.fpcr`, `.fpsr`, `.pfpfr` | **0 in a signal frame**; only the context calls fill them |
| 0x6e8, 0x700, 0x708 | `.ctpr1`–`3`, `.sc_need_rstrt`, `.ctpr*_hi` (v6) | CTPRs; the interrupted call will restart |
| 0x720, 0x728 | `.chain_stack_offset`, `.proc_stack_offset` | positions for `access_hw_stacks` |
| 0x730–0x848 | `.bgr`, `.g[16]`, `.gtag`, `.gext`, `.gext_tag` | interrupted `%g16`–`%g31`, `%bgr` |
| 0x858–0xfd0 | `.lsr`, `.ilcr`, `.lsr1`, `.ilcr1` (v5), `.aad` … `.aasti` | loop and array prefetch state |

**Filled** (kernel `kernel/signal.c:155-343`): `uc_extra` is cleared;
stack and chain registers, `%g16`–`%g31` and the stack offsets are always
written. A signal from a trap also fills, for every task, the trap cells
(`[0..6]`: `HW_TC_SIZE` 7, `include/asm/ptrace.h:89`), `nr_TIRs`/`tir_*`,
`sbbp`, CTPRs, `%lsr`/`%ilcr` and the prefetch state. Without a trap
(`kill`, delivery after a system call) `nr_TIRs`, `tir_*[0]` and `sbbp`
are 0, `curr_cnt` −1, and the trap cells are stale stack contents.
`upsr`, `rpr_*`, `mlt` are written only for translator tasks (stale
otherwise). [hw, `sysabi.c` section 3: after `raise`, `nr_TIRs` 0,
`curr_cnt` −1, `tc_count` 0; after a load from 0x1000, `si_addr` 0x1000,
`nr_TIRs` 1, `curr_cnt` 0, `tc_count` 1 and `trap_cell_addr[0]` 0x1000.]
MCST's compiler-rt tells a write fault by bit 17 of
`trap_cell_info[curr_cnt]` (`sanitizer_linux.cpp:1942-1947`).

**`sigreturn`** (119) restores the interrupted state from the kernel's
own copy (kernel `kernel/ttable.c:2236-2290`, `signal.c:471-587`). From
the user frame it takes only `uc_stack`, `uc_sigmask`, `uc_extra.g`…
`gext_tag` and `bgr` (→ `%g16`–`%g31`, `%bgr`; not for translator tasks;
privileged descriptors refused) and, **only with `SA_SIGINFO`**,
`uc_mcontext.cr0_hi` as the IP to resume at. If the handler changed the
IP, the trapped instruction's pending memory operations are dropped
unless `uc_flags` has `UC_HANDLE_SIGRETURN_CELLAR` (1, uapi
`ucontext.h:14`) ([x86-support.md](../isa/x86-support.md)). Window
registers, predicates, `cr1`, `usd`, CTPRs, loop, prefetch and FP fields
in the frame are ignored [source]. Checked on hardware with an
`SA_SIGINFO` handler that edits its frame (`ctxsig.c` section 2; the 4C,
8C2 and 2C3 agree; `cr0_hi` not tried):

| Edited in the frame | After the handler returns |
|---|---|
| `uc_sigmask` (+ USR2, − HUP) | applied, after `tgkill` and after a timer signal |
| `uc_extra.g[0..2]` (`%g16`–`%g18`), `gtag[3] = 5` (`%g19`) | applied, tags included, when a timer signal interrupted user code (`%g19` came back with its value and tag 5). When the signal arrived at the end of a system call (`tgkill`, as `raise` sends it), `%g16`–`%g31` came back empty (0, tag 5) whatever the frame held, because the system call empties them ("System calls") |
| `uc_extra.fpcr`, `fpsr`, `pfpfr` (0 at entry) | ignored: the registers stay as the handler left them |
| `uc_mcontext.usd_lo` (−256) | ignored: the stack pointer is unchanged |
| `uc_mcontext.cr1_hi` (`br.rbs` 6 → 7) | ignored: `%b[0]` is the same register |
| nothing; the handler writes `%g16`–`%g19` before returning | after a timer signal the interrupted code gets its own values back; after `tgkill` they are empty (0, tag 5), as above |

The frame's `g[]` holds the values `%g16`–`%g31` had when the code was
interrupted. After `tgkill` these are the values from before the system
call, which the program never sees again.

**The FP control registers are not saved or restored around a handler**
[hw]: a handler starts with the interrupted code's rounding mode and
flags, and whatever it sets stays after it returns, for `raise` and for a
timer signal that interrupts a loop (`sysabi.c` section 1: the
interrupted code had rounding upward and inexact; the handler saw both,
set rounding toward zero and overflow, and the interrupted code then ran
with those). `%pfpfr`, `%fpcr` and `%fpsr` are kept only at context
switch (kernel `include/asm/ptrace.h:294-298`, `kernel/fpu.c:22-43`,
`include/asm/regs_state.h:1029,1129`). x86-64 Linux, by contrast, gives a
handler a fresh FP state and restores the interrupted one. So a handler
that does FP work under its own settings must save and restore them
(`fegetenv`/`fesetenv`), and a compiler may not assume the FP
environment is unchanged across a point where a signal can arrive unless
handlers behave. (Faults in lcc's speculative code arrive as SIGILL:
[source-compat.md](source-compat.md), "Signals".)

**Alternate stacks** [hw, `sysabi.c` section 2]:

- `sigaltstack` refuses less than 4,096 bytes (ENOMEM; the kernel's
  `MINSIGSTKSZ`, uapi `signal.h:88`). glibc's `sysconf (_SC_MINSIGSTKSZ)`
  is 2,048 (no `AT_MINSIGSTKSZ`), and so is the `MINSIGSTKSZ` macro
  without `_GNU_SOURCE`: the size glibc calls the minimum is refused.
  `sysconf (_SC_SIGSTKSZ)` and `SIGSTKSZ` are 8,192; with `_GNU_SOURCE`
  (`_DYNAMIC_STACK_SIZE_SOURCE`) both `SIGSTKSZ` and `MINSIGSTKSZ` become
  `sysconf (_SC_SIGSTKSZ)`, 8,192 (glibc
  `sysdeps/unix/sysv/linux/bits/sigstack.h:27-30`, `bits/sigstksz.h`,
  `sysconf-sigstksz.h`, `dl-parse_auxv.h:36-39`).
- The kernel rounds the base up and the size down to 256 and puts the
  frame **4,352 bytes below the rounded top**; the handler's own frame
  follows. The kernel never grows an alternate stack: if the frame does
  not fit, or the handler runs off the bottom, the process dies with
  SIGSEGV (`expand_user_data_stack(): user data stack overflow`, or
  `setup signal rt frame failed`, in the kernel log). With a base 176
  bytes past a 256-byte boundary, 4,736 bytes ran a small handler and
  4,096, 4,608 and 4,672 died. `SIGSTKSZ` leaves about 3,600 bytes for
  the handler.
- Only the data stack switches: the handler's windows and chain frames
  still go on the thread's hardware stacks.

## Stacks

### Hardware stacks

Each thread and each context has a procedure stack (spilled windows) and
a chain stack (32-byte frames, [stack-and-unwinding.md](stack-and-unwinding.md)),
mapped privileged from `USER_HW_STACKS_BASE` up (kernel
`kernel/process.c:394-470`, `include/asm/stacks.h:118-127`):

| | Procedure stack | Chain stack | Source |
|---|---|---|---|
| Limit (glibc / kernel number) | `RLIMIT_P_STACK` 10001 / 16 | `RLIMIT_PC_STACK` 10002 / 17 | glibc `bits/resource.h:117-120`; kernel `include/asm/rlimits.h:15-16`, `kernel/sys_e2k.c:81-118` |
| Default soft / hard limit | 128 MiB / 128 GiB | 8 MiB / 128 GiB | kernel `include/asm/rlimits.h:9-10`, `kernel/setup.c:503-508`; hw (`/proc/self/limits` lists them as "Max procedure stack size" and "Max chain stack size") |
| Initial size | 16 KiB | 4 KiB | hw (`/proc/self/maps`: `[procedure stack]`, `[chain stack]`, also a 4 KiB `[signal stack]`); also each context's (`makecontext_e2k`, "User contexts") [hw] |
| Per call | 32 bytes × `wbs` (the caller's pairs below the callee) | 32 bytes | hw: with a 1 MiB limit, 8,208 nested calls at `wbs = 4` (128 bytes each) and 32,784 by the chain stack |

`getrlimit`/`setrlimit`/`prlimit64` map 10001/10002 to 16/17.
`RLIMIT_STACK` (the data stack) is 16 MiB on all three machines. A
stack that overflows traps; the kernel grows it by 11/8 (at least a page)
up to the soft limit, moving the procedure stack anywhere above the base
and the chain stack only upwards (old chain areas are kept for `longjmp`)
(kernel `mm/fault.c:1354-1396, 1601-1724`).

**Running out kills the process with SIGKILL** [hw]: recursion until
either stack reaches its limit (1 MiB, or the defaults) ends with
SIGKILL, and a SIGSEGV handler, even on an alternate stack, never runs.
The kernel logs `chain stack overflow (out of memory?)` or `procedure
stack overflow (out of memory?)`: the overflow is found while copying the
user's stacks at a kernel entry, and that path ends in `do_exit
(SIGKILL)` (kernel `include/asm/copy-hw-stacks.h:575-610, 771-774`; the
SIGSEGV of `kernel/traps.c:1220-1240` is for a bounds trap the kernel
cannot handle). With the defaults the chain stack runs out first, at 262,160
nested calls (8 MiB / 32, plus a few). A runtime that must
survive deep recursion has to count depth itself or raise the limits;
it cannot recover from a signal as it can on x86. Both limits are
independent of `ulimit -s`.

**A new thread's hardware stacks start at the creator's current size**
[hw]: after the main thread recursed 100,000 deep (procedure stack
12.4 MiB, chain stack 3.8 MiB), a new thread got stacks of the same sizes
(`sysabi.c` section 4; kernel `kernel/process.c:688-693, 1811`:
`copy_thread` sizes them from a copy of the parent's thread info).

**Access.** User loads and stores can't reach these stacks; the system
calls can (uapi `e2k_syswork.h:30-48`, kernel
`kernel/e2k_syswork.c:2882-3070`). `long access_hw_stacks (unsigned long
mode, unsigned long long *frame_ptr, char *buf, unsigned long buf_size,
void *real_size)` (254):

| Mode | Does |
|---|---|
| 4 `E2K_GET_CHAIN_STACK_SIZE`, 5 `E2K_GET_PROCEDURE_STACK_SIZE` | used size into `*real_size` (chain: + 32, a frame for the current function) |
| 6 `E2K_READ_CHAIN_STACK_EX`, 7 `E2K_READ_PROCEDURE_STACK_EX` | copy `buf_size` bytes from offset `*frame_ptr` (chain frames in the v6 format) |
| 8, 9 `E2K_WRITE_PROCEDURE_STACK_EX`, `E2K_WRITE_CHAIN_STACK_EX` | write at an offset (libgcc stores the landing pad's `%db[0]`/`%db[1]`) |
| 3 `E2K_GET_CHAIN_STACK_OFFSET`, 10 `E2K_READ_CHAIN_STACK_NATIVE` | offset of a chain address (also in a moved area); chain stack in the CPU's format |
| 0, 1, 2 | deprecated reads (EPERM on v7), write from the base |

Chain frames read with mode 6 carry only the predicates (`pf`), the IP,
`ss`, `wbs`, `wpsz`, `ic`, `br`, `wdbl` and `ussz`. `wfx` always reads
0 [source: kernel `kernel/e2k_syswork.c:2560-2590`; hw: frames of
functions with `setwd nfx = 0` and `nfx = 1` both read 0]. A frame that
returns into the trampoline page reads `pm` = 1 and `wbs` = 0 [hw:
`tools/abi/probes/hwscan.c` section 5, the frame below a signal
handler]. Mode 7 copies keep the memory tags [hw: `hwscan.c`, read back
with the fill MAS]. A diagnostic register's slot has tag 5 in the
user's buffer, and on v5+ the second slot (the upper 64 bits) of every
register written by an ordinary 64-bit operation reads 0 with tag 5. An ordinary load returns a
tagged word as a plain number ([speculation.md](../isa/speculation.md),
"Spill and fill"), so C code can scan the copy without trapping.

The kernel spills the stacks itself (no `flushr`/`flushc` needed). A
conservative garbage collector finds callers' register values only
through modes 5 and 7, and Boehm GC does exactly this; the layout
differs between v1–v4 and v5+ [hw:
[stack-and-unwinding.md](stack-and-unwinding.md), "Scanning for
roots"]. `access_hw_stacks` works on the calling thread's or
context's own stacks only: no mode takes a thread or a context
(`kernel/e2k_syswork.c:2887-2911`; [hw, `stackregs.c` section 5]:
inside a `makecontext_e2k` context modes 4 and 5 report that context's
stacks). A collector therefore cannot reach the spilled
windows of a parked context or of another thread; each has to copy its
own, as Boehm GC's stopped threads do in their signal handlers
(inferred). The registers that locate the stacks (`%psp`, `%pshtp`,
`%pcsp`, `%pcshtp`, `%usd`, `%sbr`), what they tell a collector, and
why split stacks are impossible are covered in
[stack-and-unwinding.md](stack-and-unwinding.md), "Stack registers".

`long get_backtrace (unsigned long *buf, size_t count, size_t skip,
unsigned long flags)` (253) returns the return addresses of the caller's
callers, skipping `skip` more and the trampoline frames ("just return"
frames read −1; `flags` must be 0); `set_backtrace` (252) replaces them,
only with addresses in the same file and with the same permissions
(kernel `kernel/backtrace.c:20-100, 160-260`). MCST's unwinder lands in a
catching frame with `set_backtrace`
([stack-and-unwinding.md](stack-and-unwinding.md), "Signal frames").

From a signal handler [hw, `ctxsig.c` section 4; the 4C, 8C2 and 2C3
agree], `E2K_READ_CHAIN_STACK_EX` shows, from the top: the handler and
its callees; one frame whose return address is `[trampolines]+0x100`,
with `cr1.pm` set in the copy and `wbs` 0, `br` 0; then **the
interrupted function's frame, whose return address is the interrupted
instruction itself**. For a timer signal that is an instruction inside
the loop it interrupted. For `raise` it is the instruction after the
`call` of the `tgkill` system call in libc. Then come the callers as
usual. The bottom frame (return address 0) also has `pm` set. In a
context made by `makecontext_e2k`, `makecontext_helper`'s frame is
followed by one returning into `[trampolines]+0x400` (`pm` set, `wbs`
4) and two frames with return address 0.

`get_backtrace (buf, n, 0, 0)` starts with the return address of the
function that calls it, leaves out the `[trampolines]+0x100` frame and
continues with the interrupted frame. `skip` 1 drops the first entry.
Inside a context it ends with a 0 entry (the frame below `+0x400`), so a
reader must stop at 0. glibc's `backtrace` reads the chain stack itself
(glibc `backtrace.c`). It starts one entry earlier, at its caller's own
call site, and keeps the trampoline frames (`[trampolines]+0x100`,
`+0x400`).

### The data stack: growing the main thread's stack costs a walk of the chain stack

On v1–v6, and on v7 CPUs with `CPU_HWBUG_RRD_USINCR`, a `getsp` that
takes the stack pointer below the part of the stack that `USD` covers
traps, even when the memory below is mapped. The kernel then grows
`USD` by **that `getsp`'s amount rounded up to a page, and by at least
16 KiB**, however much was still free, but never beyond a total of
`RLIMIT_STACK` (`kernel/getsp.c:229, 303-306`, `USER_C_STACK_BYTE_INCR` in
`include/asm/stacks.h:129`, `mm/fault.c:1075, 1082`). It then rewrites
the user-stack size (`cr1_hi.ussz`) of **every frame in the chain
stack** (kernel `kernel/traps.c:1148-1186` `parse_and_handle_getsp`,
`mm/fault.c:1037-1172` `expand_user_data_stack`, `:949-967`
`fix_all_chain_stack_sz`). Other v7 CPUs report the amount in
`%usincr`, grow by it with the new bottom rounded down to a page, and
skip the chain walk. [hw, `stackregs.c` section 6; the 4C, 8C2 and 2C3
agree]:

| Main thread | Growth |
|---|---|
| a recursion 20,000 deep with 32-byte locals | 51 growths; the 7 printed were 16,368–16,416 bytes apart and left `0x3fd0`–`0x3ff0` bytes free |
| `alloca (200000)` with about 130 KiB free (`0x209d0` on the 4C) | free size up by 200,704 (49 pages) |
| `alloca` of 1 MiB, of 3 MiB, with less free | by exactly the amount |

So the first time a recursion goes deep, one level per 16 KiB of data
stack traps, and each trap walks all the levels above it: the number of
traps and the length of each walk both grow with the depth, so the cost
is quadratic [hw, `sysabi.c` section 4, a function with a 32-byte
frame]:

| | 4C | 8C2 | 2C3 |
|---|---|---|---|
| First descent to 250,000 levels: seconds per 25,000 levels, first and last block | 0.46 … 9.40 | 0.27 … 4.98 | 0.21 … 4.68 |
| The whole first descent | 49 s | 26 s | 24 s |
| The same descent again | 0.01 s | 0.007 s | under 0.005 s |
| In a thread (its whole stack is mapped up front) | 0.09 s | 0.03 s | 0.03 s |
| In the main thread after one 12 MiB frame was taken and released | 0.08 s | 0.03 s | 0.03 s |
| Recursion without a data-stack frame (assembly), 200,000 levels | 0.07 s | 0.03 s | 0.03 s |

The hardware stacks grow geometrically and cost little; the data stack is
the problem, and only in the main thread, only the first time. A program
or runtime that expects deep recursion can take one large frame early
(an `alloca` of the expected size, touched and released) so that the
kernel hands out the stack in one step. A compiler cannot avoid it: any
function with a data-stack frame `getsp`s.

### Threads [source, except where marked]

| Call | Arguments | Data stack | Source |
|---|---|---|---|
| `clone` (120) | `(flags, stack, ptid, ctid, tls)`; glibc needs a non-null stack | from the stack pointer down to the start of its mapping, both ends aligned to 256, at most 4 GiB − 1 | kernel `kernel/process.c:1603-1660, 1777-1799`; glibc `clone.S` |
| `clone_thread` (229), glibc `clone2 (fn, stack_base, stack_size, flags, arg)` | `(flags, stack_base, stack_size, ptid, ctid, tls)` | the given area | kernel `kernel/process.c:1276-1300`; glibc `clone2.S`, `Versions:8` |
| `clone3` (426), `vfork` (190) | generic | given / the parent's | glibc `vfork.c` |

With `CLONE_VM` the new thread gets its own hardware stacks (kernel
`kernel/process.c:1806-1828`), as large as its creator's at the time
[hw, above]. A thread's data stack is mapped whole: its `USD` covers
the whole mapping above glibc's guard page from the start, so a
thread's recursion never traps [hw, above; `stackregs.c` section 4: the
bottom is the start of the mapping]. On overflow the kernel would grow
`USD` as for a context ("User contexts"): by at least 16 KiB, but never
beyond a total of `RLIMIT_STACK` (`mm/fault.c:1075`). A thread of the
default size is about as large as `RLIMIT_STACK`, so its one step is
what is left, 5,888 bytes for `stackregs.c`'s thread, and a second
overflow fails (computed from the kernel code and the probe's
addresses). Either step reaches past glibc's one-page guard
(`OpenE2K/glibc/nptl/pthread_attr_init.c:46`), so the new bottom lands
below it: a mapping directly below the guard is accepted and written
over unnoticed, while unmapped memory there makes the `getsp` fail with
SIGSEGV (inferred from the context test in "User contexts" and the
kernel code; not run in a thread). `%sbr` in a thread is its top
rounded down to a page: [stack-and-unwinding.md](stack-and-unwinding.md),
"Stack registers".

## Address space [hw, `sysabi.c` sections 6 and `maps`]

| Item | 4C, 8C2 | 2C3 | Source |
|---|---|---|---|
| Layout | upper (`CONFIG_MMU_SEP_VIRT_SPACE` off) | 512 GiB lower (on: every v6+ kernel and every kernel not built for one CPU model) | kernel `Kconfig:584-586`, `include/asm/pv_info.h:21-37` |
| `TASK_SIZE` | 0xd000_0000_0000 | the same | `include/asm/pv_info.h:21-28` |
| `[cut]` (compilation unit table page) | 0xc2ff_ffff_e000 | 0xc27f_ffff_e000 | `include/asm/stacks.h:21-38` |
| Hardware stacks from (`[procedure stack]`, `[chain stack]`, `[signal stack]`) | 0xc2df_ffff_e000 | 0xc25f_ffff_e000 | `include/asm/stacks.h:118-124` |
| Main stack: top below | 0xc2de_ffff_e000, minus up to 8 MiB at random | 0xc25e_ffff_e000, the same | `include/asm/stacks.h:131`, `include/asm/uaccess.h:252`; `linux/mm/util.c:296` |
| `mmap`, shared libraries, `ld.so`, static PIE | from 0x4555_5560_0000 plus up to 1 TiB at random (seen 0x4569…–0x4652…) | the same | `include/asm/processor.h:57-60`, `mm/mmap.c:53-140`, `Kconfig:335-339` |
| PIE executables | 0x8aaa_aaaa_a000 plus the same random offset (seen 0x8ac0…–0x8ba9…) | the same | `include/asm/elf.h:245`; `linux/fs/binfmt_elf.c:1178-1225` |
| Static PIE's `brk` heap | from 0x8aaa_aaaa_a000 plus a little (seen 0x8aaa_aaaf…–0x8aaa_ac43…), while its image is in the `mmap` area | the same | `linux/fs/binfmt_elf.c:1186-1216` |
| Non-PIE executables (lcc's default), static or not | 0x10000 | the same | binutils `ld/emulparams/elf_e2k.sh:3` |
| Pages; ld `MAXPAGESIZE` | 4 KiB, huge 2 MiB and 1 GiB; 0x1000 | the same | `include/asm/page_size.h:14-43`; binutils `bfd/elf64-e2k.c:147` |
| Stack and `brk` heap permissions | `rw-p`, never executable by default. The stacks become executable only through a `PT_GNU_STACK` with `PF_X` (the executable's at `exec`, or a library's when ld.so loads it) or through `mprotect` ([elf.md](elf.md), "Executable stack"). `exec` never sets `READ_IMPLIES_EXEC` | the same | `include/asm/page.h:319-320`; `linux/fs/exec.c:810-820`, `linux/include/linux/elf.h:9-14`, `linux/mm/mmap.c:3137` [hw: `/proc/self/maps` of a program without `PT_GNU_STACK` (`tools/abi/probes/execstack.sh --remote`); saved on the 8C2, checked by hand on the 4C and the 2C3] |

MCST's sanitizers assume an older layout: a fixed maximum user address
0xc2ff_ffff_ffff, the stack below 0xc2df_ffff_f000, the heap from
0x4555_5555_6000, PIE images at 0x8aaa_aaaa_a000 (compiler-rt
`sanitizer_linux.cpp:1057-1059`, `msan/msan.h:219-262`); on the 2C3's
lower layout even the stack is outside their map.

**User pointers use bit 47.** PIE images and the main stack lie above
0x8000_0000_0000; all user addresses are below 2⁴⁸. Schemes that assume
47-bit user pointers (x86-64-style tagging, 47-bit NaN-boxing, MCST LSan's
`p >> 47 == 0`, compiler-rt `lsan_common.cpp:139-141`) break; 48 bits work.

## Time [hw, `sysabi.c` section 7]

`%clkr` (`rrd %clkr`, user mode) counts the core's cycles, and the
counters of different cores are not synchronised on the 4C (four
processors: CPU 1 and CPU 15 read about 1.7 × 10¹² and −1.4 × 10¹²
cycles from CPU 0) or on the 2C3 (CPU 1 reads −7.5 × 10¹³ from CPU 0).
The 8C2's eight cores (one processor) agree within 30 cycles. A thread
that migrates sees `%clkr` jump, so `%clkr` measures intervals only on a
thread pinned to one core; time across cores comes from
`clock_gettime` (costs above).

## Profiling (`-pg`) [hw: `pgprof.c`]

lcc 1.29.16 at `-O3 -pg` starts every function, leaves included, with
[lcc]:

```
{ setwd wsz = 0x8, nfx = 0x1, dbl = 0x0
  setbn rsz = 0x3, rbs = 0x4, rcur = 0x0
  disp  %ctpr1, _mcount; ipd 2
  getsp,0 _f32s,_lts1 0xfffffff0, %r2 }  ; a 16-byte frame
{ nop 3
  rrd,0 %cr0.hi, %b[0] }                  ; frompc, not masked to 48 bits
{ call %ctpr1, wbs = 0x4 }               ; R_E2K_DISP, also with -fPIC
```

glibc's `_mcount (u_long frompc)` is a `noinline` C function that takes
`selfpc` from its own `%cr0.hi`, the address after the call in the
profiled function (glibc `sysdeps/e2k/machine-gmon.h:28-34`). lcc links
`gcrt1.o` for `-pg` and `grcrt1.o` for `-pg -static-pie`; the sysroot
also has `Mcrt1.o`. lcc rejects `-finstrument-functions` [lcc]. On all
three machines a `-pg` program, dynamic and static PIE, writes
`gmon.out`, and the system's `gprof` prints the right call counts and
callers (500 calls of `leaf`, 400 from `twice`, 100 from `main`).

## Sanitizers [lcc: `sanitize.c`]

lcc's valid `-fsanitize=` values are `address`, `leak`, `memory`, `all`
(`thread` is refused; `undefined` is accepted and adds nothing), and it
links `-lcompiler_rt.asan` and the like from its tree:

| | ASan | MSan |
|---|---|---|
| Shadow | `(a >> 3) + 0x7fff8000` (`shrd`, then `ldb …, 0x7fff8000`); compiler-rt `asan/asan_mapping.h:152-158, 234` | `(a & ~0xc00000000000) ^ 0x200000000000`; origin + 0x280000000000 (`msan/msan.h:263-264`) |
| Calls | `__asan_report_load4`, `__asan_report_store8`, …; `__asan_init`, `__asan_register_globals` from a `.ctors.65487` constructor; stack redzones poisoned inline | `__msan_init`, `__msan_warning_noreturn`, `__msan_param_tls`, `__msan_retval_tls`, `__msan_poison_stack` |

The runtime is MCST's compiler-rt 11.1.0 (Apache-2.0 with LLVM
exceptions, `LICENSE.TXT`). Its E2K changes: no fast unwinder (return
addresses are in the chain stack; `sanitizer_stacktrace.h:23-26`), the
instruction before a return address is `pc - 8` (`:88-92`), PC and SP
from `cr0_hi` and `usd_lo` as above, write faults from the trap cell. A
GCC port sharing it needs `TARGET_ASAN_SHADOW_OFFSET` 0x7fff8000. Not
run.

## Static PIE [hw]

lcc links `-static-pie` with `rcrt1.o`, `crtbeginS.o`, `-static -pie
--no-dynamic-linker` and `-lgcc -lgcc_eh -llcc -lmvec -lc` [lcc]:
`ET_DYN`, no `PT_INTERP`, no `DT_RELACOUNT`; `.rela.dyn` holds
`R_E2K_64_RELATIVE`, some `R_E2K_NONE` (112) and some all-zero entries
(type 0 is `R_E2K_32_ABS`), which glibc skips because their offset is 0
(glibc `sysdeps/e2k/dl-machine.h:492-503`). Such programs run on all
three machines, loaded in the `mmap` area (above).

**`-pie` and `-static-pie` do not imply `-fPIE` in lcc** [lcc]: code
compiled without it uses absolute 64-bit literals, so `lcc -pie` links a
PIE with `DT_TEXTREL` (ld warns) and `lcc -static-pie` fails ("read-only
segment has dynamic relocations"). Pass `-fPIE` as well, as GCC's
drivers expect anyway. A static link with an ifunc crashes ld
([elf.md](elf.md)).

## Not checked yet

- Disabled entries and entry 4; fast call 5.
- `sigreturn` with an edited `uc_mcontext.cr0_hi` (resuming elsewhere)
  and `uc_extra.bgr`; all of `uc_extra.g[]` (only `%g16`–`%g19` were
  edited); `%bgr` as a handler finds it (source only).
- Signals delivered while a made context runs; switching to a context
  suspended in another thread; `freecontext_e2k` of a context running in
  another thread (the kernel checks only the calling thread's context,
  so presumably it is freed: source).
- lcc's sanitizers on hardware (MCST's map predates the 6.1 layout,
  above).
- A thread's data stack overflowing past glibc's guard page ("Threads":
  source only); why the 4C's ALT kernel reports a failed growth of a
  context's stack as `SEGV_ACCERR` rather than `SEGV_BNDERR` ("User
  contexts"; its source is not at hand).
