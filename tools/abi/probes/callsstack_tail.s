! SPDX-License-Identifier: GPL-3.0-or-later
! Tail calls that give their frame back before the jump, as GCC's
! sibling calls would: see callsstack_tail.c (C) and callsstack_tailx.cc
! (C++ exceptions), which define g10c, gbig and the fin_* targets.
! Every function here takes at least one argument, so that the C
! caller reserves the same argument area as for the direct call it is
! compared with.
	.ignore	ld_st_style
	.text

! long cs_sp (long): the stack pointer at entry (getsp 0 in the first
! instruction).
	.global	cs_sp
	.type	cs_sp, #function
	.align	8
cs_sp:
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
	.size	cs_sp, .-cs_sp

! long cs_rel_K (long x): a 64-byte frame, written, then
!   K = same: given back by a getsp +64 in the ct's own instruction,
!   K = gap:  given back 6 cycles before the ct,
!   K = none: not given back (the control),
! then a jump (ct, no call) to cs_sp (x): returns cs_sp's result.
.macro	rel name, kind
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, cs_sp
	  getsp,0	-0x40, %dr4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr4, 0x0, %dr0
	  std,5	%dr4, 0x38, %dr0
	}
	.ifc	\kind, same
	{
	  getsp,0	0x40, %dr5
	  ct	%ctpr1
	}
	.endif
	.ifc	\kind, gap
	{
	  getsp,0	0x40, %dr5
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr1
	}
	.endif
	.ifc	\kind, none
	{
	  ct	%ctpr1
	}
	.endif
	.size	\name, .-\name
.endm
	rel	cs_rel_same, same
	rel	cs_rel_gap, gap
	rel	cs_rel_none, none

! long cs_rel_vla (long n): a 32-byte frame and a VLA of n bytes rounded
! up to 16 (two getsps, the second by a register), both written at
! their ends, given back by one getsp of their sum (a register) in the
! ct's instruction, then a jump to cs_sp (n).
	.global	cs_rel_vla
	.type	cs_rel_vla, #function
	.align	8
cs_rel_vla:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, cs_sp
	  getsp,0	-0x20, %dr4
	  addd,1	%dr0, 0xf, %dr5
	}
	{
	  andd,0	%dr5, -0x10, %dr5
	}
	{
	  subd,0	0x0, %dr5, %dr6
	  addd,1	%dr5, 0x20, %dr7
	}
	{
	  nop 2
	}
	{
	  getsp,0	%dr6, %dr8
	}
	{
	  nop 5
	}
	{
	  addd,0	%dr8, %dr5, %dr9
	  std,2	%dr4, 0x0, %dr0
	  std,5	%dr4, 0x18, %dr0
	}
	{
	  nop 1
	}
	{
	  std,2	%dr8, 0x0, %dr0
	  std,5	%dr9, -0x8, %dr0
	}
	{
	  getsp,0	%dr7, %dr10
	  ct	%ctpr1
	}
	.size	cs_rel_vla, .-cs_rel_vla

! long cs_round (long a): getsp -a, then getsp +a; returns the stack
! pointer after them minus the one at entry (0 when a is a multiple of
! 16).
	.global	cs_round
	.type	cs_round, #function
	.align	8
cs_round:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  getsp,0	0x0, %dr4
	  subd,1	0x0, %dr0, %dr5
	}
	{
	  nop 2
	}
	{
	  getsp,0	%dr5, %dr6
	}
	{
	  getsp,0	%dr0, %dr7
	}
	{
	  getsp,0	0x0, %dr8
	}
	{
	  nop 5
	}
	{
	  subd,0	%dr8, %dr4, %dr0
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	}
	.size	cs_round, .-cs_round

! long cs_over (long a): getsp +a without any allocation before it (the
! stack pointer goes above its value at entry); returns the stack
! pointer after it minus the one at entry.  Run it in a child.
	.global	cs_over
	.type	cs_over, #function
	.align	8
cs_over:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  getsp,0	0x0, %dr4
	}
	{
	  getsp,0	%dr0, %dr6
	}
	{
	  getsp,0	0x0, %dr8
	}
	{
	  nop 5
	}
	{
	  subd,0	%dr8, %dr4, %dr0
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	}
	.size	cs_over, .-cs_over

! long cs_rel_args10 (a, b, c, d, e, f, g, h, i, j): a 48-byte frame;
! reads slots 8 and 9 (i, j) from the incoming area, gives the frame
! back, writes j and a into slots 8 and 9 of the same incoming area and
! jumps to g10c (b, c, d, e, f, g, h, i, j, a).
	.global	cs_rel_args10
	.type	cs_rel_args10, #function
	.align	8
cs_rel_args10:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, g10c
	  getsp,0	-0x30, %dr8
	  addd,1	0x0, %dr0, %dr13
	}
	{
	  nop 5
	}
	{
	  addd,0	%dr8, 0x30, %dr10
	  std,2	%dr8, 0x0, %dr0
	  std,5	%dr8, 0x28, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr10, 0x40, %dr11
	  ldd,3	%dr10, 0x48, %dr12
	}
	{
	  addd,0	0x0, %dr1, %dr0
	  addd,1	0x0, %dr2, %dr1
	  addd,2	0x0, %dr3, %dr2
	  addd,3	0x0, %dr4, %dr3
	  addd,4	0x0, %dr5, %dr4
	  addd,5	0x0, %dr6, %dr5
	}
	{
	  nop 2
	}
	{
	  addd,3	0x0, %dr7, %dr6
	  addd,1	0x0, %dr11, %dr7
	  getsp,0	0x30, %dr9
	}
	{
	  std,2	%dr10, 0x40, %dr12
	  std,5	%dr10, 0x48, %dr13
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr1
	}
	.size	cs_rel_args10, .-cs_rel_args10

! struct big cs_rel_big (long x), struct big = 12 longs: a 64-byte
! frame given back, then a jump to gbig (x), which writes its 96-byte
! result at its entry stack pointer: our caller's argument area.
	.global	cs_rel_big
	.type	cs_rel_big, #function
	.align	8
cs_rel_big:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, gbig
	  getsp,0	-0x40, %dr4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr4, 0x0, %dr0
	  std,5	%dr4, 0x38, %dr0
	}
	{
	  getsp,0	0x40, %dr5
	  ct	%ctpr1
	}
	.size	cs_rel_big, .-cs_rel_big

! long cs_chain_K (long n, long (*fin) (long, long, long), long acc):
! a 64-byte frame holding n and acc (stored and loaded back); for n > 0
! gives the frame back and jumps to the other function of the pair with
! (n - 1, fin, acc + n), for n = 0 gives it back and jumps to
! fin (acc, fin, acc).  So fin gets acc + n (n + 1) / 2 and runs on the
! caller's stack pointer and chain stack frame, whatever n.  fin is
! prepared in %ctpr2 by movtd (a movtd under a predicate is SIGILL on
! the 4C, 8C2 and 2C3: cs_movtdp below), the other function in %ctpr1.
!   cs_chain_a/b: the release 6 cycles before the two cts;
!   cs_chain_c/d: the release in each ct's own instruction, the first
!   getsp under the ct's predicate.
.macro	chain name, other, same
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, \other
	  getsp,0	-0x40, %dr4
	  cmpedb,1	%dr0, 0x0, %pred0
	}
	{
	  nop 5
	}
	{
	  movtd,0	%dr1, %ctpr2
	  std,2	%dr4, 0x0, %dr2
	  std,5	%dr4, 0x38, %dr0
	}
	{
	  ldd,0	%dr4, 0x0, %dr5
	  ldd,3	%dr4, 0x38, %dr6
	}
	{
	  nop 3
	}
	{
	  addd,0	%dr5, %dr6, %dr2
	  subd,1	%dr6, 0x1, %dr0
	}
	{
	  nop 1
	}
	{
	  addd,0	0x0, %dr2, %dr0 ? %pred0
	}
	{
	  nop 5
	}
	.if	\same
	{
	  getsp,0	0x40, %dr7 ? %pred0
	  ct	%ctpr2 ? %pred0
	}
	{
	  getsp,0	0x40, %dr7
	  ct	%ctpr1
	}
	.else
	{
	  getsp,0	0x40, %dr7
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr2 ? %pred0
	}
	{
	  ct	%ctpr1
	}
	.endif
	.size	\name, .-\name
.endm
	chain	cs_chain_a, cs_chain_b, 0
	chain	cs_chain_b, cs_chain_a, 0
	chain	cs_chain_c, cs_chain_d, 1
	chain	cs_chain_d, cs_chain_c, 1

! long cs_movtdp (long v, long (*f) (long)): movtd f into %ctpr1, which
! holds cs_sp from a disp, under a predicate that is true when v is 0,
! then ct %ctpr1 (f's or cs_sp's result).  Run it in a child.
	.global	cs_movtdp
	.type	cs_movtdp, #function
	.align	8
cs_movtdp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, cs_sp
	  cmpedb,1	%dr0, 0x0, %pred0
	}
	{
	  nop 5
	}
	{
	  movtd,0	%dr1, %ctpr1 ? %pred0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr1
	}
	.size	cs_movtdp, .-cs_movtdp

! long cs_movtdu (long v, long (*f) (long)): the same with an
! unconditional movtd over the disp'd %ctpr1 (control: f's result).
	.global	cs_movtdu
	.type	cs_movtdu, #function
	.align	8
cs_movtdu:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, cs_sp
	}
	{
	  nop 5
	}
	{
	  movtd,0	%dr1, %ctpr1
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr1
	}
	.size	cs_movtdu, .-cs_movtdu

! long cs_movtdq (long v, long (*f) (long)): movtd f into %ctpr2 (no
! earlier prepare of it) under a predicate true when v is 0, then
! ct %ctpr2 under the same predicate; 777 when not taken.
	.global	cs_movtdq
	.type	cs_movtdq, #function
	.align	8
cs_movtdq:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  cmpedb,1	%dr0, 0x0, %pred0
	  addd,2	0x0, 0x309, %dr2
	}
	{
	  nop 5
	}
	{
	  movtd,0	%dr1, %ctpr2 ? %pred0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr2 ? %pred0
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	cs_movtdq, .-cs_movtdq
