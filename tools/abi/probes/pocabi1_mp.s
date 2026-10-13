! SPDX-License-Identifier: GPL-3.0-or-later
! Message-passing sides for pocabi1_mp.c (build line there).
!   void w_SS (long *x, long *y, long v): x = v in the first instruction,
!        then y = v in the next; S = 2 or 5 is the store's channel (lcc
!        1.29.16 puts the wait-unlock store of relaxed and acquire
!        exchanges, fetch-ops and test-and-sets in ALC5 on v5 and v6, and
!        in ALC2 for every other order and always on v3/v4).
!   void w_2L5 / w_2L2 (long *x, long *y, long v): x = v (ALC2), then
!        y = v by a lock-wait load and a wait-unlock store in ALC5 / ALC2
!        (lcc's relaxed exchange; the ALC5 form is SIGILL on v3).
!   void w_22w: a wait st_c = 1 between the two stores.
!   void w_22r: y = v by lcc's release store (v3 and v6 form):
!        { wait ld_c = 1, st_c = 1; std y }.
!   void r_yx (long *x, long *y, long *out): out[0] = y, then out[1] = x;
!        r_yxw with a wait ld_c = 1 between, r_yx1 both loads in one
!        instruction, r_yxu with lcc's acquire (a dummy use of y) between.
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
	  addd,0	0x0, %dr2, %dr4
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

	head	w_22
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr4
	}
	tail	w_22

	head	w_25
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  std,5	%dr1, 0x0, %dr4
	}
	tail	w_25

	head	w_52
	{
	  std,5	%dr0, 0x0, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr4
	}
	tail	w_52

	head	w_55
	{
	  std,5	%dr0, 0x0, %dr4
	}
	{
	  std,5	%dr1, 0x0, %dr4
	}
	tail	w_55

	head	w_2L5
	{
	  std,2	%dr0, 0x0, %dr4
	}
1:
	{
	  ldd,0	%dr1, 0x0, %dr5, mas = 0x7
	  nop 1
	}
	{
	  std,5	%dr1, 0x0, %dr4, mas = 0x2
	  rbranch	1b
	}
	tail	w_2L5

	head	w_2L2
	{
	  std,2	%dr0, 0x0, %dr4
	}
1:
	{
	  ldd,0	%dr1, 0x0, %dr5, mas = 0x7
	  nop 1
	}
	{
	  std,2	%dr1, 0x0, %dr4, mas = 0x2
	  rbranch	1b
	}
	tail	w_2L2

	head	r_yx
	{
	  ldd,0	%dr1, 0x0, %dr5
	}
	{
	  ldd,0	%dr0, 0x0, %dr6
	}
	{
	  nop 2
	}
	{
	  std,2	%dr2, 0x0, %dr5
	  std,5	%dr2, 0x8, %dr6
	}
	tail	r_yx

	! A wait between the stores, a wait between the loads.
	head	w_22w
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  wait	st_c = 1
	}
	{
	  std,2	%dr1, 0x0, %dr4
	}
	tail	w_22w

	! lcc's release store of y (v3 and v6; v5 adds sal, sas and a { nop }).
	head	w_22r
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  wait	ld_c = 1, st_c = 1
	  std,2	%dr1, 0x0, %dr4
	}
	tail	w_22r

	head	r_yxw
	{
	  ldd,0	%dr1, 0x0, %dr5
	}
	{
	  wait	ld_c = 1
	}
	{
	  ldd,0	%dr0, 0x0, %dr6
	}
	{
	  nop 2
	}
	{
	  std,2	%dr2, 0x0, %dr5
	  std,5	%dr2, 0x8, %dr6
	}
	tail	r_yxw

	! The loads in one wide instruction.
	head	r_yx1
	{
	  ldd,0	%dr1, 0x0, %dr5
	  ldd,3	%dr0, 0x0, %dr6
	}
	{
	  nop 2
	}
	{
	  std,2	%dr2, 0x0, %dr5
	  std,5	%dr2, 0x8, %dr6
	}
	tail	r_yx1

	! lcc's acquire load of y: a dummy use of the value (it stalls until
	! the value arrives) before the load of x.
	head	r_yxu
	{
	  ldd,0	%dr1, 0x0, %dr5
	}
	{
	  adds,0,sm	%r5, 0x0, %empty
	}
	{
	  ldd,0	%dr0, 0x0, %dr6
	}
	{
	  nop 2
	}
	{
	  std,2	%dr2, 0x0, %dr5
	  std,5	%dr2, 0x8, %dr6
	}
	tail	r_yxu
