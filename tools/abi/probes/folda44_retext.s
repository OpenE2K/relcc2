! SPDX-License-Identifier: GPL-3.0-or-later
! Callees that return a narrow integer in the low half of %r0 only, as
! MCST glibc's clone.S, clone2.S and longjmp.S do (adds under a setwd
! without dbl); see folda44_retext.c.  dirty_T (long junk): junk arrives
! in %dr0, and its upper half stays above the 32-bit result.
	.ignore	ld_st_style
	.text

	.global	dirty_int
	.type	dirty_int, #function
	.align	8
dirty_int:			! (int) 0x2130
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  adds,0	0x0, 0x2130, %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	dirty_int, .-dirty_int

	.global	dirty_int_neg
	.type	dirty_int_neg, #function
	.align	8
dirty_int_neg:			! (int) -1, clone's error return
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  adds,0	0x0, -1, %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	dirty_int_neg, .-dirty_int_neg

	.global	dirty_uint
	.type	dirty_uint, #function
	.align	8
dirty_uint:			! (unsigned) 0x80000001
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  adds,0	0x0, 0x80000001, %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	dirty_uint, .-dirty_uint

	.global	dirty_short
	.type	dirty_short, #function
	.align	8
dirty_short:			! (short) -16, with 0x1234 in bits 16-31
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  adds,0	0x0, 0x1234fff0, %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	dirty_short, .-dirty_short

	.global	dirty_uchar
	.type	dirty_uchar, #function
	.align	8
dirty_uchar:			! (unsigned char) 0x81, with 0x12 in bits 8-15
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  adds,0	0x0, 0x1281, %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	dirty_uchar, .-dirty_uchar
