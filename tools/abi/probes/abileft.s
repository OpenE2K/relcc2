! SPDX-License-Identifier: GPL-3.0-or-later
! Register reads for abileft.c (build and run instructions there).  Only
! reads: nothing here writes a state register.
!
!   long al_NAME (void): one rrd of one stack register, so that a
!   handler, a context or a thread can tell which data stack (%usd, %sbr)
!   and which hardware stacks (%psp.lo, %pcsp.lo: their bases) it runs on.
	.ignore	ld_st_style
	.text

	.macro	rdd name reg
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	\reg, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	rdd	al_usd_lo, %usd.lo
	rdd	al_usd_hi, %usd.hi
	rdd	al_sbr, %sbr
	rdd	al_psp_lo, %psp.lo
	rdd	al_pcsp_lo, %pcsp.lo
