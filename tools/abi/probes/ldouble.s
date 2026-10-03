! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for ldouble.c: access to the FP state registers, and the
! extended-register spill test. See ldouble.c.
	.ignore	ld_st_style
	.text

! unsigned rd_pfpfr(void), rd_fpcr(void), rd_fpsr(void)
! void wr_pfpfr(unsigned), wr_fpcr(unsigned), wr_fpsr(unsigned)
.macro	rdreg name, reg
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrs,0	\reg, %r0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
.endm
.macro	wrreg name, reg
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rws,0	%r0, \reg
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
.endm
	rdreg	rd_pfpfr, %pfpfr
	rdreg	rd_fpcr, %fpcr
	rdreg	rd_fpsr, %fpsr
	wrreg	wr_pfpfr, %pfpfr
	wrreg	wr_fpcr, %fpcr
	wrreg	wr_fpsr, %fpsr

! unsigned long keep_ext_nfxN(unsigned long mant, unsigned long exp,
!                             void (*deep)(int), int depth)
! Packs (mant, exp) into the extended register %r6 with movif, calls
! deep(depth) (which recurses far enough to spill this window to the
! procedure stack), then returns the 16-bit part of %r6 read back with
! movfi. The window is set up with nfx = 1 or nfx = 0.
.macro	keep name, nfx
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x8, nfx = \nfx, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	  movtd,0	%dr2, %ctpr1
	  movif,1	%dr0, %r1, %r6
	}
	{
	  nop 7
	  addd,3	0x0, %dr3, %db[0]	! not with setbn: %b uses the old base there
	}
	{
	  nop 2
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
.endm
	keep	keep_ext_nfx1, 0x1
	keep	keep_ext_nfx0, 0x0
