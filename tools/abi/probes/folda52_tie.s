! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for folda52_tie.c (see there for how to build and run): a
! window filled with a pattern for the next callee to find, and two
! writes of one register in one wide instruction, a 64-bit one and a
! 32-bit one, as lcc writes an asm input tied to a wider output
! (addd,1 0x0, 0x0, %r3 with adds,2 0x1, 0x0, %r3).
	.ignore	ld_st_style
	.ignore	strict_delay
	.text

! void fill (long pattern): %dr0-%dr7 (the caller's %b[0]-%b[7], which
! the next callee from the same frame and wbs inherits) all hold PATTERN.
	.global	fill
	.type	fill, #function
	.align	8
fill:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  addd,0	0x0, %dr0, %dr1
	  addd,1	0x0, %dr0, %dr2
	  addd,3	0x0, %dr0, %dr3
	  addd,4	0x0, %dr0, %dr4
	}
	{
	  addd,0	0x0, %dr0, %dr5
	  addd,1	0x0, %dr0, %dr6
	  addd,3	0x0, %dr0, %dr7
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fill, .- fill

! long ww_NAME (long x): one wide instruction writes %r1 twice, then
! %dr1 is returned.  dbl = 0 unless the name ends in _dbl1.
!   ww_d0_s1      addd,0 x (64-bit)   adds,1 1 (32-bit)  lcc's order, one cluster
!   ww_d1_s3      addd,1 x            adds,3 1           lcc's order, across clusters
!   ww_s0_d1      adds,0 1            addd,1 x           the 64-bit write higher
!   ww_d0_s1_dbl1 addd,0 x            adds,1 1           under dbl = 1
	.macro	WW name, dbl, op0, op1
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = \dbl
	  return	%ctpr3; ipd 2
	}
	{
	  \op0
	  \op1
	}
	{
	  addd,0	0x0, %dr1, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .- \name
	.endm

	WW	ww_d0_s1, 0x0, "addd,0 0x0, %dr0, %dr1", "adds,1 0x1, 0x0, %r1"
	WW	ww_d1_s3, 0x0, "addd,1 0x0, %dr0, %dr1", "adds,3 0x1, 0x0, %r1"
	WW	ww_s0_d1, 0x0, "adds,0 0x1, 0x0, %r1", "addd,1 0x0, %dr0, %dr1"
	WW	ww_d0_s1_dbl1, 0x1, "addd,0 0x0, %dr0, %dr1", "adds,1 0x1, 0x0, %r1"
