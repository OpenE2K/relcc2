# Spec findings from the proof-of-concept backend

The experimental GCC backend in `toolchains/gcc-claude` (AGENTS.md,
decision 2) is written from `spec/` and `doc/gcc-port-plan.md` to find
where they are wrong, ambiguous or missing data. Every such problem is
recorded here, then fixed in `spec/` or `doc/` with the usual source marks.
No code from the experiment belongs in this file.

## Format

One entry per problem, newest last:

```
### N. Short title

- Spec: file and section (e.g. `spec/abi/calling-convention.md`,
  "Variadic calls")
- Kind: wrong | ambiguous | missing
- Found by: what needed the answer (GCC hook or pattern, compat test,
  probe, assembler error)
- Evidence: what lcc, the hardware or qemu showed (commands, output)
- Status: open | fixed in <commit> | not a spec problem (why)
```

## Findings

None yet.
