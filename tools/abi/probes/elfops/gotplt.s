! SPDX-License-Identifier: GPL-3.0-or-later
! Calls through @GOTPLT ("inlined PLT entries", elfxx-e2k.c): load the
! function's .got.plt slot, movtd it to %ctpr1 and call, as -fno-plt
! code would.  long call_gotplt_ext (long x) calls g_ext (x) in libext.so,
! call_gotplt_loc (long x) calls g_loc (x) defined here; both return the
! callee's result.
	.ignore	ld_st_style
	.text
	.global	g_loc
	.type	g_loc, #function
g_loc:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	%dr0, 0x2, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	g_loc, .-g_loc

	.macro	caller name sym
	.global	\name
	.type	\name, #function
\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	}
	{
	  nop 1
	  rrd,0	%ip, %dr4
	  addd,1	0x0, [ _f64,_lts0 _GLOBAL_OFFSET_TABLE_ ], %dr5
	}
	{
	  addd,0	%dr4, %dr5, %dr6
	  addd,1	0x0, %dr0, %dr8
	}
	{
	  nop 3
	  ldd,0	%dr6, [ _f32s,_lts0 \sym@GOTPLT ], %dr7
	}
	{
	  movtd,0	%dr7, %ctpr1
	}
	{
	  nop 7
	}
	{
	  nop 2
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  return	%ctpr3
	  addd,0	0x0, %dr8, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	caller	call_gotplt_ext, g_ext
	caller	call_gotplt_loc, g_loc
