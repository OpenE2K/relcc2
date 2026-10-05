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
	fn	mbe_loads
	{
	  ldb,0	%dr1, 0x0, %dr2, mas=0x8
	  ldh,2	%dr1, 0x0, %dr3, mas=0x8
	  ldw,3	%dr1, 0x0, %dr4, mas=0x8
	  ldd,5	%dr1, 0x0, %dr5, mas=0x8
	}
	{
	  ldd,0,sm	%dr1, 0x0, %dr6, mas=0xb
	  ldd,2	%dr1, 0x0, %dr7
	  ldd,3	%dr1, 0x8, %dr8, mas=0x8
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr2
	  std,5	%dr0, 0x8, %dr3
	}
	{
	  std,2	%dr0, 0x10, %dr4
	  std,5	%dr0, 0x18, %dr5
	}
	{
	  std,2	%dr0, 0x20, %dr6
	  std,5	%dr0, 0x28, %dr7
	}
	{
	  std,2	%dr0, 0x30, %dr8
	}
	end	mbe_loads

! Stores of %dr1 to buf (%dr0): sth at 0, stw at 8, std at 16, all
! mas=0x8, and stb mas=0x8 at 24.
	fn	mbe_stores
	{
	  sth,2	%dr0, 0x0, %dr1, mas=0x8
	  stw,5	%dr0, 0x8, %dr1, mas=0x8
	}
	{
	  std,2	%dr0, 0x10, %dr1, mas=0x8
	  stb,5	%dr0, 0x18, %dr1, mas=0x8
	}
	end	mbe_stores
	fn	chase_plain
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	end	chase_plain
	fn	chase_be
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  ldd,0	%dr0, 0x0, %dr0, mas=0x8
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	end	chase_be
