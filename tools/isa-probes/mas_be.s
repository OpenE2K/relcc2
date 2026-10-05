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

! The same chase through lcc's swap: ldd, then pshufb with the mask
! 0x0001020304050607 (what lcc emits for __builtin_bswap64 (*p)).
	fn	chase_shuf
	{
	  addd,0	0x0, _f64,_lts0 0x1020304050607, %dr6
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  ldd,0	%dr0, 0x0, %dr0
	}
	{
	  pshufb,0	%dr0, %dr0, %dr6, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	end	chase_shuf

! Store side, 32 steps of x = ldw mas=0x8 (store of bswap32 (x)) through
! buf (%dr1): with lcc's htonl (sxt, pshufb with the mask
! 0x8080808000010203, stw) or with stw mas=0x8.  Returns cycles.
	fn	chase_st_lcc
	{
	  addd,0	0x0, _f64,_lts0 0x8080808000010203, %dr6
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  sxt,0	0x2, %r0, %r7
	}
	{
	  pshufb,0	%dr7, %dr7, %dr6, %dr7
	}
	{
	  stw,2	%dr1, 0x0, %r7
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	end	chase_st_lcc
	fn	chase_st_mas
	{
	  addd,0	0x0, _f64,_lts0 0x8080808000010203, %dr6
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  stw,2	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  ldw,0	%dr1, 0x0, %r0, mas=0x8
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	end	chase_st_mas


	fn	mis_ldh1
	{
	  ldh,0	%dr1, 0x1, %dr4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldh1

	fn	mis_ldh1_be
	{
	  ldh,0	%dr1, 0x1, %dr4, mas=0x8
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldh1_be

	fn	mis_ldw1
	{
	  ldw,0	%dr1, 0x1, %dr4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldw1

	fn	mis_ldw1_be
	{
	  ldw,0	%dr1, 0x1, %dr4, mas=0x8
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldw1_be

	fn	mis_ldw2
	{
	  ldw,0	%dr1, 0x2, %dr4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldw2

	fn	mis_ldw2_be
	{
	  ldw,0	%dr1, 0x2, %dr4, mas=0x8
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldw2_be

	fn	mis_ldd3
	{
	  ldd,0	%dr1, 0x3, %dr4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldd3

	fn	mis_ldd3_be
	{
	  ldd,0	%dr1, 0x3, %dr4, mas=0x8
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldd3_be

	fn	mis_ldd4
	{
	  ldd,0	%dr1, 0x4, %dr4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldd4

	fn	mis_ldd4_be
	{
	  ldd,0	%dr1, 0x4, %dr4, mas=0x8
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	end	mis_ldd4_be

	fn	mis_sth1
	{
	  sth,2	%dr1, 0x1, %dr2
	}

	end	mis_sth1

	fn	mis_sth1_be
	{
	  sth,2	%dr1, 0x1, %dr2, mas=0x8
	}

	end	mis_sth1_be

	fn	mis_stw2
	{
	  stw,2	%dr1, 0x2, %dr2
	}

	end	mis_stw2

	fn	mis_stw2_be
	{
	  stw,2	%dr1, 0x2, %dr2, mas=0x8
	}

	end	mis_stw2_be

	fn	mis_std4
	{
	  std,2	%dr1, 0x4, %dr2
	}

	end	mis_std4

	fn	mis_std4_be
	{
	  std,2	%dr1, 0x4, %dr2, mas=0x8
	}

	end	mis_std4_be
