! SPDX-License-Identifier: GPL-3.0-or-later
! Byte-swapped memory accesses (MAS bit 3) for mas_be.c.
	.ignore	ld_st_style
	.text

	.macro	fn name
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	.endm

	.macro	end name
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

! v5: ldqp mas=0x8 from buf (%dr1) into out[0..1]; stqp mas=0x8 of buf's
! first 16 bytes to out + 16.
	fn	mbe_quad
	{
	  ldqp,0	%dr1, 0x0, %qpr2, mas=0x8
	  ldqp,2	%dr1, 0x0, %qpr3
	}
	{
	  nop 5
	}
	{
	  stqp,2	%dr0, 0x0, %qpr2
	}
	{
	  stqp,5	%dr0, 0x10, %qpr3, mas=0x8
	}
	end	mbe_quad
