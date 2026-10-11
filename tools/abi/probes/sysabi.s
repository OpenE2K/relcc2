! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for sysabi.c.
	.ignore	ld_st_style
	.text

! long sa_clkr (void): %clkr.
	.global	sa_clkr
	.type	sa_clkr, #function
	.align	8
sa_clkr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrd,0	%clkr, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sa_clkr, .-sa_clkr

! long sa_sp (void): the caller's stack pointer (getsp 0 in the caller's
! frame is what lcc's __builtin_frame_address (0) gives; here our own).
	.global	sa_sp
	.type	sa_sp, #function
	.align	8
sa_sp:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  getsp,0	0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sa_sp, .-sa_sp

! long sa_rd_upsr (void), void sa_wr_upsr (long).
	.global	sa_rd_upsr
	.type	sa_rd_upsr, #function
	.align	8
sa_rd_upsr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrs,0	%upsr, %r0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sa_rd_upsr, .-sa_rd_upsr

	.global	sa_wr_upsr
	.type	sa_wr_upsr, #function
	.align	8
sa_wr_upsr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rws,0	%r0, %upsr
	}
	{
	  wait	all_c = 1
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sa_wr_upsr, .-sa_wr_upsr

! long sa_ldw (char *p), sa_ldd (char *p): a 4- or 8-byte load from p
! (which may be misaligned).
	.global	sa_ldw
	.type	sa_ldw, #function
	.align	8
sa_ldw:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldw,0	%dr0, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sa_ldw, .-sa_ldw

	.global	sa_ldd
	.type	sa_ldd, #function
	.align	8
sa_ldd:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sa_ldd, .-sa_ldd

! void sa_down (long n): recursion n deep with no data-stack frame (each
! level puts 4 pairs on the procedure stack and a frame on the chain
! stack).
	.global	sa_down
	.type	sa_down, #function
	.align	8
sa_down:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	  disp	%ctpr1, sa_down
	  cmpedb,0	%dr0, 0x0, %pred0
	}
	{
	  return	%ctpr3
	  subd,1	%dr0, 0x1, %db[0]	! not with setbn: old base there
	}
	{
	  ct	%ctpr3 ? %pred0
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  return	%ctpr3
	}
	{
	  ct	%ctpr3
	}
	.size	sa_down, .-sa_down
