! SPDX-License-Identifier: GPL-3.0-or-later
! getsp with the amount in different kinds of operand; see getsp.c.
!   long gs_KIND (long n): allocate n bytes with getsp (amount -n in a
!   window register, %g8, %g16 or %g24), store 1 at the new stack
!   pointer and at the top of the area, read both back, return their sum.
	.ignore	ld_st_style
	.text

	.macro	gs name reg
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  subd,0	0x0, %dr0, \reg
	  addd,1	0x0, 0x1, %dr3
	}
	{
	  nop 5
	}
	{
	  getsp,0	\reg, %dr1
	}
	{
	  nop 5
	}
	{
	  std,2	%dr1, 0x0, %dr3
	  addd,0	%dr1, %dr0, %dr4
	}
	{
	  nop 1
	}
	{
	  std,2	%dr4, -0x8, %dr3
	}
	{
	  ldd,0	%dr1, 0x0, %dr5
	  ldd,3	%dr4, -0x8, %dr6
	}
	{
	  nop 3
	}
	{
	  addd,0	%dr5, %dr6, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	gs	gs_win, %dr2
	gs	gs_g8, %dg8
	gs	gs_g16, %dg16
	gs	gs_g24, %dg24
