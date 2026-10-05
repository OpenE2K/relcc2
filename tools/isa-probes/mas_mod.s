! SPDX-License-Identifier: GPL-3.0-or-later
! One MAS kind each for mas_mod.c: f (out, buf, value); loads put their
! result in out[0], stores write buf[1].
	.ignore	ld_st_style
	.text

	.global	mm_ld_m4_c0
	.type	mm_ld_m4_c0, #function
	.align	8
mm_ld_m4_c0:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  ldd,0	%dr1, 0x0, %dr4, mas=0x4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_ld_m4_c0, .-mm_ld_m4_c0

	.global	mm_ld_m6_c2
	.type	mm_ld_m6_c2, #function
	.align	8
mm_ld_m6_c2:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  ldd,2	%dr1, 0x0, %dr4, mas=0x6
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_ld_m6_c2, .-mm_ld_m6_c2

	.global	mm_ld_m6_c0
	.type	mm_ld_m6_c0, #function
	.align	8
mm_ld_m6_c0:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  ldd,0	%dr1, 0x0, %dr4, mas=0x6
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_ld_m6_c0, .-mm_ld_m6_c0

	.global	mm_ld_m2_c0
	.type	mm_ld_m2_c0, #function
	.align	8
mm_ld_m2_c0:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  ldd,0	%dr1, 0x0, %dr4, mas=0x2
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_ld_m2_c0, .-mm_ld_m2_c0

	.global	mm_ld_m3_c0
	.type	mm_ld_m3_c0, #function
	.align	8
mm_ld_m3_c0:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  ldd,0	%dr1, 0x0, %dr4, mas=0x3
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_ld_m3_c0, .-mm_ld_m3_c0

	.global	mm_ld_m1bit
	.type	mm_ld_m1bit, #function
	.align	8
mm_ld_m1bit:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  ldd,0	%dr1, 0x0, %dr4, mas=0x10
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_ld_m1bit, .-mm_ld_m1bit

	.global	mm_ld_m1bit_c2
	.type	mm_ld_m1bit_c2, #function
	.align	8
mm_ld_m1bit_c2:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  ldd,2	%dr1, 0x0, %dr4, mas=0x10
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_ld_m1bit_c2, .-mm_ld_m1bit_c2

	.global	mm_st_m1bit
	.type	mm_st_m1bit, #function
	.align	8
mm_st_m1bit:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  std,2	%dr1, 0x8, %dr2, mas=0x10
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_st_m1bit, .-mm_st_m1bit

	.global	mm_st_m4
	.type	mm_st_m4, #function
	.align	8
mm_st_m4:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr4
	}
	{
	  std,2	%dr1, 0x8, %dr2, mas=0x4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	mm_st_m4, .-mm_st_m4
