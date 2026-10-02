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
| **hw** | Run on hardware: Elbrus 4C (v3), by the compat suite (`run.py --remote`) or a probe in `tools/abi/probes` |

Where lcc and Unipro disagree, lcc is the reference: it built the system.

The compatibility suite runs mixed lcc and Unipro programs under qemu-e2k
and on an Elbrus 4C (v3). On the 4C every pair gives the same results as
under qemu except the tag trap in "Unused parts of a slot", which needs a
diagnostic tag that qemu puts in fresh registers and the 4C does not. Other
rules are marked **compat** rather than **hw** until a hardware run is
recorded against them; v5 and v6 hardware runs are still to do.

## Compatibility suite

```
python3 tools/abi/compat/gen.py toolchains/compat/gen
python3 tools/abi/compat/run.py toolchains/compat/gen
```

`gen.py` writes caller/callee pairs for the rules in
calling-convention.md (scalars, aggregates of every size, layout
attributes, large results, long argument lists, variadic and unprototyped
calls, integer extension, `va_list` passed between compilers, `longjmp`
across compilers) plus seeded random signatures. `run.py` builds every
ordered pair of compilers (`--cc NAME=COMMAND` adds one, e.g. a GCC
port, and `--cxx NAME=COMMAND` its C++ compiler for C++ units), links
with lcc (`l++` for C++) and runs each program under `--runner` (qemu-e2k
by default) or on hardware over SSH (`--remote`). Known differences of the reference compilers are listed in
`tools/abi/compat/known.txt`; any other failure fails the run.

## Probes

```
LCC=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc    # or toolchains/lcc-1.29.16-e2k-v5/lcc
$LCC -O1 -fno-optimize-sibling-calls -S tools/abi/probes/callconv.c
$LCC -c tools/abi/probes/layout.c    # must compile
```

`layout.c` encodes every rule in data-layout.md as `_Static_assert`s; both
lcc and Unipro's clang compile it.
