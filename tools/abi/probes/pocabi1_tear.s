! SPDX-License-Identifier: GPL-3.0-or-later
! Readers for pocabi1_a16run.c's "tear" test: a 16-byte object read as two
! ldd with the second N cycles after the first (what a compiler that
! splits a 16-byte atomic load into two 8-byte loads might emit), to see
! whether a concurrent 16-byte store can land between the halves.
! unsigned __int128 rd_gapN (unsigned __int128 *p): N = 0, the halves in
! one wide instruction; 1, consecutive instructions; 8 and 32 cycles.
! Built with -DGAPS: lcc -O3 -pthread -DGAPS pocabi1_a16run.c pocabi1_tear.s
	.ignore	ld_st_style
	.text

	.macro	head name
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	.endm

	.macro	tail name
	{
	  nop 3
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	! Both halves in one wide instruction (lcc's v5 relaxed load).
	head	rd_gap0
	{
	  ldd,0	%dr0, 0x0, %dr2
	  ldd,2	%dr0, 0x8, %dr1
	}
	{
	  nop 2
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	tail	rd_gap0

	! In consecutive instructions.
	head	rd_gap1
	{
	  ldd,0	%dr0, 0x0, %dr2
	}
	{
	  ldd,0	%dr0, 0x8, %dr1
	}
	{
	  nop 2
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	tail	rd_gap1

	! 8 cycles apart.
	head	rd_gap8
	{
	  ldd,0	%dr0, 0x0, %dr2
	}
	{
	  nop 6
	}
	{
	  ldd,0	%dr0, 0x8, %dr1
	}
	{
	  nop 2
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	tail	rd_gap8

	! 32 cycles apart.
	head	rd_gap32
	{
	  ldd,0	%dr0, 0x0, %dr2
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  nop 6
	}
	{
	  ldd,0	%dr0, 0x8, %dr1
	}
	{
	  nop 2
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	tail	rd_gap32
