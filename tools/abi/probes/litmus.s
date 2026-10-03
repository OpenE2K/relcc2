! SPDX-License-Identifier: GPL-3.0-or-later
! Litmus-test sides for litmus.c, each access in program order in its own
! wide instruction unless the name ends in _same (both in one).
!   long st_ld (long *a, long *b):        *a = 1; return *b
!   long ld_st (long *a, long *b):        r = *a; *b = 1; return r
!   void st_st (long *a, long *b, long *): *a = 1; *b = 1
!   void ld_ld (long *a, long *b, long *out): out[0] = *a; out[1] = *b
! _wait: lcc's fence (wait ld_c = 1, st_c = 1) between the two.
	.ignore	ld_st_style
	.text

	.macro	head name
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x1, %dr4
	}
	{
	  nop 5
	}
	.endm

	.macro	tail name
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	head	st_ld
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	tail	st_ld

	head	st_ld_same
	{
	  std,2	%dr0, 0x0, %dr4
	  ldd,0	%dr1, 0x0, %dr0
	}
	tail	st_ld_same

	head	st_ld_wait
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  wait	ld_c = 1, st_c = 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	tail	st_ld_wait

	head	st_ld_waitst
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  wait	st_c = 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	tail	st_ld_waitst

	head	ld_st
	{
	  ldd,0	%dr0, 0x0, %dr5
	}
	{
	  std,2	%dr1, 0x0, %dr4
	}
	{
	  addd,0	0x0, %dr5, %dr0
	}
	tail	ld_st

	head	ld_st_same
	{
	  ldd,0	%dr0, 0x0, %dr5
	  std,2	%dr1, 0x0, %dr4
	}
	{
	  addd,0	0x0, %dr5, %dr0
	}
	tail	ld_st_same

	head	st_st
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr4
	}
	tail	st_st

	head	st_st_same
	{
	  std,2	%dr0, 0x0, %dr4
	  std,5	%dr1, 0x0, %dr4
	}
	tail	st_st_same

	head	st_st_wait
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  wait	ld_c = 1, st_c = 1
	}
	{
	  std,2	%dr1, 0x0, %dr4
	}
	tail	st_st_wait

	head	ld_ld
	{
	  ldd,0	%dr0, 0x0, %dr5
	}
	{
	  ldd,0	%dr1, 0x0, %dr6
	}
	{
	  nop 2
	}
	{
	  std,2	%dr2, 0x0, %dr5
	  std,5	%dr2, 0x8, %dr6
	}
	tail	ld_ld

	head	ld_ld_same
	{
	  ldd,0	%dr0, 0x0, %dr5
	  ldd,3	%dr1, 0x0, %dr6
	}
	{
	  nop 2
	}
	{
	  std,2	%dr2, 0x0, %dr5
	  std,5	%dr2, 0x8, %dr6
	}
	tail	ld_ld_same

	head	ld_ld_wait
	{
	  ldd,0	%dr0, 0x0, %dr5
	}
	{
	  wait	ld_c = 1, st_c = 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr6
	}
	{
	  nop 2
	}
	{
	  std,2	%dr2, 0x0, %dr5
	  std,5	%dr2, 0x8, %dr6
	}
	tail	ld_ld_wait

	head	st_st_same_rev
	{
	  std,5	%dr0, 0x0, %dr4
	  std,2	%dr1, 0x0, %dr4
	}
	tail	st_st_same_rev
