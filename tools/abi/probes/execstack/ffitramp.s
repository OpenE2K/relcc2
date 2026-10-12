# SPDX-License-Identifier: GPL-3.0-or-later
# The words MCST's libffi 3.3 writes for a closure trampoline
# (src/e2k/ffi.c:316-342), with sample values in place of the closure
# and the entry point: 0x1122334455667788 for the closure (the static
# chain, into %dg12) and 0x99aabbccddeeff00 for ffi_closure_e2k.
# tools/abi/probes/execstack.sh assembles this and disassembles it; it
# is never run.
	.text
	.global	tramp64
	.align	8
tramp64:			# __ptr64__: two wide instructions, 40 bytes
	.word	0x0c000032, 0x61c0dcd1, 0x11c0deec, 0x00000000
	.word	0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00
	.word	0x00001001, 0x80000420
	.global	tramp32
	.align	8
tramp32:			# __ptr32__: 32 bytes
	.word	0x0c000022, 0x63f0d8d1, 0x11c0d9ec, 0x00000000
	.word	0x55667788, 0xddeeff00
	.word	0x00001001, 0x80000420
