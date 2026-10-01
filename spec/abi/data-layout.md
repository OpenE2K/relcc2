# Data layout

Little-endian. Checked by `tools/abi/probes/layout.c` (lcc, unipro).

## Scalar types

| Type | Size | Alignment | Register | Notes |
|---|---|---|---|---|
| `_Bool` | 1 | 1 | `%r` | |
| `char` | 1 | 1 | `%r` | **signed** |
| `short` | 2 | 2 | `%r` | |
| `int`, `enum` | 4 | 4 | `%r` | |
| `long`, `long long` | 8 | 8 | `%dr` | |
| `__int128` | 16 | 16 | `%qr` / two `%dr` | |
| pointers | 8 | 8 | `%dr` | |
| `float` | 4 | 4 | `%r` | IEEE single |
| `double` | 8 | 8 | `%dr` | IEEE double |
| `long double`, `__float80` | 16 | 16 | `%xr` | 80-bit x87-style extended in the low 10 bytes |
| `__float128` | 16 | 16 | `%qr` / two `%dr` | IEEE quad; arithmetic is in libgcc (`__multf3`, …) |
| `_Complex float` | 8 | 4 | | |
| `_Complex double` | 16 | 8 | | |
| `size_t` | 8 | 8 | | unsigned long |
| `wchar_t` | 4 | 4 | | signed int |
| `max_align_t` | 32 | 16 | | |

Sources: guide 9.2 for sizes and register formats; gdb sets
`long double` to the i387 extended format in 128 bits and recognises
`__float128`/`_Float128` as IEEE quad.

## Aggregates (guide 9.2.4)

Standard rules: an aggregate is aligned to its most strictly aligned
member, members are placed at the lowest offset that satisfies their
alignment, and the size is padded to a multiple of the alignment.

## Bit-fields (guide 9.2.4.4)

- Allocated from the least significant bit of their container upwards.
- The container has the alignment of the declared type and may also hold
  other members.
- A field that doesn't fit in the remaining bits of its container starts a
  new container at the next suitably aligned offset.
- Unnamed bit-fields don't affect the aggregate's alignment. An unnamed
  zero-width bit-field aligns the next member to its declared type.

This matches the common System V rules (as on x86-64).

## Global and dynamic data (guide 9.1.4)

Recommended alignment of variables, for speed rather than correctness:

| Size | Alignment |
|---|---|
| 1 | 1 |
| 2 | 2 |
| 3–4 | 4 |
| 5–8 | 8 |
| 9 or more | 16 |

`malloc` results and stack allocations are 16-byte aligned.
