# SPDX-License-Identifier: GPL-3.0-or-later
# MCST libffi's 64-bit closure trampoline (src/e2k/ffi.c:316-342) written
# as gas text, with the same sample literals as ffitramp.s:
# tools/abi/probes/execstack.sh checks that gas assembles it to the same
# 40 bytes. A GCC port's trampoline template can be this text with the
# two literals filled in at run time. It is never run.
	.ignore	ld_st_style
	.ignore	strict_delay
	.text
	.align	8
	.global	tramp_text
tramp_text:
	{
	  movtd,0	_f64,_lts0 0x99aabbccddeeff00, %ctpr1
	  addd,1	0x0, _f64,_lts2 0x1122334455667788, %dg12
	}
	{
	  ct	%ctpr1
	  ipd	2
	}
