! SPDX-License-Identifier: GPL-3.0-or-later
! A load and a store, or two stores, to one address in one wide
! instruction; see samewi.c.
!   long ldst_L_S (long *p): *p is 5; one instruction loads *p in ALC L
!     and stores 7 to *p in ALC S; returns the value loaded.
!   void stst_A_B (long *p): one instruction stores 7 in ALC A and 9 in
!     ALC B to *p.
!   long ldstw_L_S (long *p): as ldst, but the store is a 4-byte stw of 7
!     to p + 4 (the high half of the doubleword loaded).
	.ignore	ld_st_style
	.text

	.macro	ldst name ld st op=std off=0x0 reg=%dr2
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x7, %dr2
	}
	{
	  nop 5
	}
	{
	  ldd,\ld	%dr0, 0x0, %dr1
	  \op,\st	%dr0, \off, \reg
	}
	{
	  nop 5
	  addd,0	0x0, %dr1, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	ldst	ldst_0_2, 0, 2
	ldst	ldst_3_5, 3, 5
	ldst	ldst_2_5, 2, 5
	ldst	ldst_5_2, 5, 2
	ldst	ldstw_5_2, 5, 2, stw, 0x4, %r2
	ldst	ldstw_0_2, 0, 2, stw, 0x4, %r2

	.macro	stst name a b
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x7, %dr2
	  addd,1	0x0, 0x9, %dr3
	}
	{
	  nop 5
	}
	{
	  std,\a	%dr0, 0x0, %dr2
	  std,\b	%dr0, 0x0, %dr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	stst	stst_2_5, 2, 5
	stst	stst_5_2, 5, 2
