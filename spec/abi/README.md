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
| **qemu** | `OpenE2K/qemu-e2k-mcst` and `OpenE2K/qemu-e2k-community` (source or behaviour) |
| **compat** | Checked by the compatibility suite `tools/abi/compat`: caller and callee built by different compilers, run under MCST's qemu-e2k |

Where lcc and Unipro disagree, lcc is the reference: it built the system.

Not yet verified on hardware. The compatibility suite runs mixed lcc and
Unipro programs under qemu-e2k; neither emulator can be fully trusted, so
its results still need confirming on real machines.

## Compatibility suite

```
python3 tools/abi/compat/gen.py toolchains/compat/gen
python3 tools/abi/compat/run.py toolchains/compat/gen
```

`gen.py` writes caller/callee pairs for the rules in
calling-convention.md (scalars, aggregates of every size, layout
attributes, large results, long argument lists, variadic and unprototyped
calls, integer extension) plus seeded random signatures. `run.py` builds
every ordered pair of compilers (`--cc NAME=COMMAND` adds one, e.g. a GCC
port), links with lcc and runs each program under `--runner` (qemu-e2k by
default). Known differences of the reference compilers are listed in
`tools/abi/compat/known.txt`; any other failure fails the run.

## Probes

```
toolchains/lcc-1.29.16-e2k-v5/lcc -O1 -fno-optimize-sibling-calls -S tools/abi/probes/callconv.c
toolchains/lcc-1.29.16-e2k-v5/lcc -c tools/abi/probes/layout.c    # must compile
```

`layout.c` encodes every rule in data-layout.md as `_Static_assert`s; both
lcc and Unipro's clang compile it.
