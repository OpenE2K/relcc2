# Atomic operations and memory ordering

How lcc compiles the GNU `__atomic_*`/`__sync_*` builtins (and so C11
`<stdatomic.h>` and C++ `<atomic>`), and the one rule that binds every
compiler: 16-byte atomics. Probe: `tools/abi/probes/atomics.c`
(assembly only).

On hardware (Elbrus 4C, by the proof-of-concept backend,
`docs/spec-poc-findings.md` A2b): 8 threads, half running a function
built by lcc and half the same function built with these sequences by
another compiler, on shared objects: `fetch_add` on `long`, `int`,
`short`, `char`, strong and weak CAS loops on `long` and `int`, and an
exchange spinlock guarding a plain counter. All counts were exact in
three runs of 1.6 million increments each, while a plain-increment
control lost about half. So the lock protocol below is atomic in
hardware and code from different compilers interoperates. Ordering
and 16-byte atomics on v3 are checked by `tools/abi/probes/litmus.c`
(below).

Code from different compilers interoperates as long as each one's
read-modify-write is really atomic in hardware; the lock protocol below
is the only hardware way to do that for 1–8 bytes (and for 16 bytes on
v5+). The exception is a size that one compiler does in hardware and
another through a library lock (16 bytes, see below).

## Read-modify-write: locked load and unlocking store (lcc, qemu, binutils, hw)

Every exchange, fetch-op, compare-exchange and test-and-set is a retry
loop around a **locked load** and an **unlocking store**. lcc's
`__atomic_fetch_add(&g, v, __ATOMIC_SEQ_CST)` on a `long`:

```
1:  nop 1                                  ! own wide instruction: loop head
    { ldd,0 %g16, 0x0, %g19, mas = 0x7     ! locked load of the old value
      nop 4 }
    { addd,0 %g19, %g17, %g18 }            ! new value
    { std,2 %g16, 0x0, %g18, mas = 0x2     ! store only if the lock still holds
      rbranch 1b }                         ! = ibranch 1b ? %MLOCK
```

- The load has MAS `0x7` and must be in ALC0: "lock-wait" load (qemu
  `target/e2k/mas-format.h`, `e2k_parse_mas_lds1`: mod 7, not speculative,
  channel 0; mod 5 is the variant for channel 0 or for a 16-byte load in
  channel 2). It reads the value and sets a lock on the address.
- The store has MAS `0x2`: "wait-unlock" store
  (`e2k_parse_mas_sts1`, mod 2). It writes only if the lock is still held,
  then releases it. lcc puts it in ALC2.
- `rbranch 1b` in the store's wide instruction is assembled as
  `ibranch 1b ? %MLOCK` (CT condition type 8, bundle.md): taken when the
  lock was lost and the store did not happen, so the loop retries.
- Between the two, lcc's templates contain only ALU operations
  (`addd`; for compare-exchange `cmpesb` and `merges`). Sizes: `ldb`/
  `ldh`/`ldw`/`ldd` with the matching store; 16 bytes `ldqp`/`stqp` (v5+).
- The memory order does not change the loop: relaxed and seq_cst
  read-modify-writes are identical, with no `wait` around them, so the
  locked pair itself orders memory.
- Compare-exchange stores in both outcomes: on a mismatch it writes the
  old value back (`merges` chooses the value to store), so the store
  always ends the lock. Test-and-set stores 1 and returns the old byte.
- These sequences are inline-asm templates in lcc's output (between
  `.accept strict_delay` and `.ignore strict_delay`, with `.ias_line`),
  and they carry their own `nop`s.

## Loads, stores and fences (lcc)

| Operation | lcc's code |
|---|---|
| relaxed load, store | plain `ld`/`st` |
| acquire load | `ld`, then `adds,sm %r, 0x0, %empty` in a later wide instruction: a dummy use that stalls until the value arrives, so nothing after it issues earlier (gas writes `,sm` as `,s`) |
| seq_cst load | `wait ld_c = 1, st_c = 1`, then the acquire load |
| release store | `wait ld_c = 1, st_c = 1` and the `st` (may share a wide instruction) |
| seq_cst store | release store, then `wait st_c = 1` |
| fence of any order, `__sync_synchronize` | `wait ld_c = 1, st_c = 1` |

lcc's code for the 8C2 (v5) adds `sal = 1, sas = 1` (the v5+ `wait`
bits, bundle.md) to the release and seq_cst stores' `wait`; its code for
the 2C3 (v6) is the v3 code above, without them. Read-modify-writes
are the same on all three. v6 has MAS encodings for load-acquire and
store-release (kernel `asm/mas.h`: `MAS_LOAD_ACQUIRE_V6`,
`MAS_STORE_RELEASE_V6`, bit 4 set with `mod` 3) and a "watch for
modification" load, but lcc 1.29.16 doesn't use them.

`wait` waits for the completion of earlier loads (`ld_c`) and stores
(`st_c`). lcc 1.31 also has `__builtin_e2k_read_barrier`,
`_write_barrier`, `_mem_barrier` and `_barrier`, each a compiler barrier
plus a `wait` whose mask is the argument (the mask bits of `wait` for
the target ISA) (**lcc-doc**).

## Memory ordering on hardware (v3) [hw]

`tools/abi/probes/litmus.c` with hand-written sides (`litmus.s`) runs
the classic two-thread tests on the 4C, two threads on one chip (CPUs
0, 1) and on different chips (1, 9), 2 million iterations per variant,
each access in its own wide instruction unless noted:

| Test | Forbidden under SC | Seen |
|---|---|---|
| SB: `x = 1; r1 = y` ‖ `y = 1; r2 = x` | `r1 = r2 = 0` | 23 and 41 times |
| SB, store and load in one wide instruction | same | 5,540 and 15 |
| SB with `wait ld_c = 1, st_c = 1` or `wait st_c = 1` between | same | never |
| MP: `x = 1; y = 1` ‖ `r1 = y; r2 = x` | `r1 = 1, r2 = 0` | never (also with the loads in one instruction, `wait`, or lcc's release/acquire) |
| MP, both stores in one wide instruction | same | 2 in 20 million (one per channel order) |
| LB: `r1 = x; y = 1` ‖ `r2 = y; x = 1` | `r1 = r2 = 1` | never (also in one instruction) |

So across wide instructions v3 behaves like TSO (x86): the only
reordering is a later load passing an earlier store (a store buffer),
and `wait st_c = 1` before the load prevents it. Loads are not
reordered with loads, stores not with stores, and a load not with a
later store. Within one wide instruction a load may see memory before
the instruction's store, and two stores may become visible in either
order (rare, since they need another core reading between them). The
counts are low because the two threads rarely overlap closely, so
"never" means not seen in 4 million tries per variant. The Elbrus-8C2
(v5, 300,000 tries per variant, built for v3 and for v5) gives the same
picture with far more SB outcomes (870 to 5,052, against 5 on the 4C),
none for MP or LB, and once more an MP outcome with both stores in one
instruction. The Elbrus-2C3 (v6, its two cores, 200,000 tries, built for
the 2C3) too: SB 326 and 263 (in one instruction), none with a `wait`,
none for MP (including release/acquire) or LB; its 16-byte atomics
(inline, lock-free) count exactly with no torn read.

For a compiler on v3 that means: seq_cst stores and fences need the
`wait` (or a locked read-modify-write); acquire loads and release
stores need only that the compiler keeps their order and does not put
the release store in one instruction with an earlier store (lcc still
emits `wait` before a release store and a dummy use after an acquire
load, which is safe).

16-byte atomics on v3 (same probe): two threads doing
`__atomic_fetch_add` and CAS loops on an `__int128` through libatomic
gave exact counts (4 million each), a third thread's `__atomic_load_n`
never saw the two halves differ, and `__atomic_is_lock_free(16)` is 0.
The same v3 binary on the Elbrus-8C2 also counts exactly but reports
lock-free 1, because that system's libatomic is built for the 8C2
(below).

## 16-byte atomics (lcc, lcc-doc)

| Target | Hardware 16-byte atomics | lcc's code | `__atomic_always_lock_free(16)` |
|---|---|---|---|
| v3, v4 | no (only loads and stores of protected-mode descriptors) | calls `__atomic_load_16`, `__atomic_compare_exchange_16`, ... in libatomic, which uses a lock, not lock-free | 0 |
| v5+ | yes | inline `ldqp`/`stqp` loops with the same MAS; seq_cst load = `wait`, `ldqp`, dummy use | 1 |

- Whether code that mixes the two on **one object** works depends on
  the system's libatomic. A libatomic built for v3/v4 (ALT Linux's on
  the 4C) uses a lock, which does not exclude the hardware loop, so
  mixing breaks. Elbrus Linux 9.5 on the 8C2 ships a libatomic built for
  `elbrus-8c2` whose `__atomic_*_16` are the same `ldqp`/`stqp` loops
  (147 quad operations in it), so there v3 code (calling libatomic) and
  v5 code (inline) agree: two threads adding to one `__int128`, one
  through each, counted exactly (4 million, three runs; hw). lcc's
  `-mno-quad-memory-atomic` makes v5+ code call libatomic too.
- v3 and v4 do not define `__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16`; v5 does.
  Both define `__GCC_ATOMIC_*_LOCK_FREE` as 2 for every type up to
  `long long` and pointers.
- MCST's libatomic is GCC's generic one (`libatomic/config/posix`, lock
  tables) with only protected-mode changes (`internal-lcc-libs`,
  `lib/GNU/gcc-11/libatomic`).

For a GCC port: under the default `-march=elbrus-v3` it must call
libatomic for 16 bytes, as lcc does, and a v5+ mode should offer the same
switch as `-mno-quad-memory-atomic`. Mixing v3 and v5 code that shares
16-byte atomic objects is unsafe whichever compiler built it.

## Other differences from GCC (lcc-doc)

- A `__sync_*` builtin with no hardware support becomes a call into
  libatomic (the equivalent `__atomic_*`), not a call to a user-provided
  `__sync_*` function as in GCC; link with `-latomic`.
- Generic `__atomic_*` on a type of size 2, 4, 8 or 16 that is less
  aligned than its size are built as if aligned, as GCC does;
  `-mno-unaligned-type-atomic` makes lcc honour the alignment (all code
  touching the object must then agree).
