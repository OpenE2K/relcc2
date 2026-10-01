# E2K Linux ABI (64-bit mode)

This directory describes the application binary interface used by MCST's lcc
and the system libraries on Elbrus Linux, for 64-bit unprotected mode
(`e2k64`, `-mptr64`). A new compiler must follow it to link with existing
code.

| Document | Contents |
|---|---|
| [data-layout.md](data-layout.md) | Sizes and alignment of types, aggregates, bit-fields |
| [registers.md](registers.md) | Register files, windows, conventions, DWARF numbering |
| [calling-convention.md](calling-convention.md) | Calls, argument passing, results, varargs |
| [stack-and-unwinding.md](stack-and-unwinding.md) | User stack, hardware stacks, exceptions, backtraces |

## Sources

Each rule is marked with where it comes from:

| Mark | Source |
|---|---|
| **guide** | MCST, "Руководство по эффективному программированию на платформе «Эльбрус»", chapter 9 (CC BY 4.0) |
| **lcc** | Observed in the output of lcc 1.29.16 for `tools/abi/probes/` |
| **unipro** | Observed in the output of Unipro's clang 20.1.8 for the same probes |
| **gdb**, **glibc**, **kernel**, **libgcc** | MCST's GPL sources: `OpenE2K/gdb`, `OpenE2K/glibc`, `OpenE2K/linux`, and the E2K libgcc in lcc's `internal-lcc-libs-open-source` tarball |
| **qemu** | `OpenE2K/qemu-e2k-community` |

Where lcc and Unipro disagree, lcc is the reference: it built the system.

Not yet verified on hardware. The probes only show what the compilers emit;
running mixed-compiler tests on real machines is the next step.

## Probes

```
toolchains/lcc-1.29.16-e2k-v5/lcc -O1 -fno-optimize-sibling-calls -S tools/abi/probes/callconv.c
toolchains/lcc-1.29.16-e2k-v5/lcc -c tools/abi/probes/layout.c    # must compile
```

`layout.c` encodes every rule in data-layout.md as `_Static_assert`s; both
lcc and Unipro's clang compile it.
