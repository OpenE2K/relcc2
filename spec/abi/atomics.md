# Atomic operations and memory ordering

How lcc compiles the GNU `__atomic_*`/`__sync_*` builtins (and so C11
`<stdatomic.h>` and C++ `<atomic>`), and the one rule that binds every
compiler: 16-byte atomics. Probe: `tools/abi/probes/atomics.c`
(assembly only). Nothing here is checked on hardware yet.

Code from different compilers interoperates as long as each one's
read-modify-write is really atomic in hardware; the lock protocol below
is the only hardware way to do that for 1–8 bytes (and for 16 bytes on
v5+). The exception is a size that one compiler does in hardware and
another through a library lock (16 bytes, see below).

## Read-modify-write: locked load and unlocking store (lcc, qemu, binutils)

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

`wait` waits for the completion of earlier loads (`ld_c`) and stores
(`st_c`). lcc 1.31 also has `__builtin_e2k_read_barrier`,
`_write_barrier`, `_mem_barrier` and `_barrier`, each a compiler barrier
plus a `wait` whose mask is the argument (the mask bits of `wait` for
the target ISA) (**lcc-doc**).

## 16-byte atomics (lcc, lcc-doc)

| Target | Hardware 16-byte atomics | lcc's code | `__atomic_always_lock_free(16)` |
|---|---|---|---|
| v3, v4 | no (only loads and stores of protected-mode descriptors) | calls `__atomic_load_16`, `__atomic_compare_exchange_16`, ... in libatomic, which uses a lock, not lock-free | 0 |
| v5+ | yes | inline `ldqp`/`stqp` loops with the same MAS; seq_cst load = `wait`, `ldqp`, dummy use | 1 |

- Code that mixes the two on **one object** is broken: libatomic's lock
  does not exclude the hardware loop. lcc's `-mno-quad-memory-atomic`
  makes v5+ code call libatomic too.
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
