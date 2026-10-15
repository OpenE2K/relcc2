! SPDX-License-Identifier: GPL-3.0-or-later
! Stack-clash probing of the data stack: see callsstack_clash.c.
! Each function first moves the stack pointer down by PHASE bytes (a
! multiple of 16; shifts the frame against the guard page), records its
! progress in ST[]:
!   st[0] the frame's bottom B once allocated (incremental: updated at
!         each step), st[1] probes done, st[2] 1 once probing is done,
!   st[3] 1 once the frame is filled, st[4] the last probed address,
! and returns B after giving everything back.  Probes and the fill
! store the 64-bit value 0x7e7e7e7e.
	.ignore	ld_st_style
	.text

! long cl_one (long size, long step, long phase, long *st): one getsp of
! SIZE, then a store at top - STEP * k for k = 1, 2, ... while it is not
! below B (top: the stack pointer before the getsp), then a fill of the
! frame from B upward, 8 bytes a store (as an array filled from its
! start).
	.global	cl_one
	.type	cl_one, #function
	.align	8
cl_one:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  subd,0	0x0, %dr2, %dr10
	  subd,1	0x0, %dr0, %dr11
	  addd,2	0x0, 0x0, %dr8
	  addd,3	0x0, 0x7e7e7e7e, %dr7
	  addd,4	0x0, 0x1, %dr9
	}
	{
	  disp	%ctpr1, .Lone_probe
	}
	{
	  getsp,0	%dr10, %dr12
	}
	{
	  getsp,0	%dr11, %dr4
	}
	{
	  disp	%ctpr2, .Lone_fill
	}
	{
	  nop 4
	}
	{
	  addd,0	%dr4, %dr0, %dr5
	  std,2	%dr3, 0x0, %dr4
	}
	{
	  subd,0	%dr5, %dr1, %dr6
	}
.Lone_probe:
	{
	  cmpbdb,0	%dr6, %dr4, %pred0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr2 ? %pred0
	}
	{
	  std,2	%dr6, 0x0, %dr7
	  addd,0	%dr8, 0x1, %dr8
	}
	{
	  std,2	%dr3, 0x8, %dr8
	  std,5	%dr3, 0x20, %dr6
	  subd,0	%dr6, %dr1, %dr6
	}
	{
	  ct	%ctpr1
	}
.Lone_fill:
	{
	  disp	%ctpr1, .Lone_fl
	  addd,0	0x0, %dr4, %dr6
	}
	{
	  std,2	%dr3, 0x10, %dr9
	}
	{
	  nop 4
	}
.Lone_fl:
	{
	  std,2	%dr6, 0x0, %dr7
	  addd,0	%dr6, 0x8, %dr6
	}
	{
	  nop 1
	}
	{
	  cmpbdb,0	%dr6, %dr5, %pred0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr1 ? %pred0
	}
	{
	  std,2	%dr3, 0x18, %dr9
	  addd,0	%dr0, %dr2, %dr13
	}
	{
	  nop 1
	}
	{
	  getsp,0	%dr13, %dr14
	  addd,1	0x0, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	cl_one, .-cl_one

! long cl_inc (long size, long step, long phase, long *st): SIZE bytes
! in steps of STEP, each a getsp -STEP and a store at the new stack
! pointer (as GCC's x86 prologue probes: the stack pointer is never more
! than one step below a probed word), then the same fill.
	.global	cl_inc
	.type	cl_inc, #function
	.align	8
cl_inc:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  subd,0	0x0, %dr2, %dr10
	  subd,1	0x0, %dr1, %dr11
	  addd,2	0x0, 0x0, %dr8
	  addd,3	0x0, 0x7e7e7e7e, %dr7
	  addd,4	0x0, 0x1, %dr9
	  addd,5	0x0, 0x0, %dr12
	}
	{
	  disp	%ctpr1, .Linc_step
	}
	{
	  getsp,0	%dr10, %dr13
	}
	{
	  getsp,0	0x0, %dr5
	}
	{
	  nop 5
	}
.Linc_step:
	{
	  getsp,0	%dr11, %dr4
	}
	{
	  nop 2
	}
	{
	  std,2	%dr4, 0x0, %dr7
	  addd,0	%dr12, %dr1, %dr12
	  addd,1	%dr8, 0x1, %dr8
	}
	{
	  std,2	%dr3, 0x8, %dr8
	  std,5	%dr3, 0x20, %dr4
	}
	{
	  std,2	%dr3, 0x0, %dr4
	  cmpbdb,0	%dr12, %dr0, %pred0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr1 ? %pred0
	}
	{
	  disp	%ctpr1, .Linc_fl
	  addd,0	0x0, %dr4, %dr6
	}
	{
	  std,2	%dr3, 0x10, %dr9
	}
	{
	  nop 4
	}
.Linc_fl:
	{
	  std,2	%dr6, 0x0, %dr7
	  addd,0	%dr6, 0x8, %dr6
	}
	{
	  nop 1
	}
	{
	  cmpbdb,0	%dr6, %dr5, %pred0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr1 ? %pred0
	}
	{
	  std,2	%dr3, 0x18, %dr9
	  addd,0	%dr12, %dr2, %dr13
	}
	{
	  nop 1
	}
	{
	  getsp,0	%dr13, %dr14
	  addd,1	0x0, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	cl_inc, .-cl_inc

! long cl_spin (long size, long iters, long phase, long *st): one getsp
! of SIZE, nothing stored in the frame, then ITERS iterations of a loop
! of register operations (signals arrive meanwhile), then the release.
	.global	cl_spin
	.type	cl_spin, #function
	.align	8
cl_spin:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  subd,0	0x0, %dr2, %dr10
	  subd,1	0x0, %dr0, %dr11
	  addd,2	0x0, 0x1, %dr9
	}
	{
	  disp	%ctpr1, .Lspin
	}
	{
	  getsp,0	%dr10, %dr12
	}
	{
	  getsp,0	%dr11, %dr4
	}
	{
	  nop 5
	}
	{
	  std,2	%dr3, 0x0, %dr4
	}
.Lspin:
	{
	  subd,0	%dr1, 0x1, %dr1
	}
	{
	  nop 1
	}
	{
	  cmpledb,0	%dr1, 0x0, %pred0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr1 ? ~%pred0
	}
	{
	  std,2	%dr3, 0x18, %dr9
	  addd,0	%dr0, %dr2, %dr13
	}
	{
	  nop 1
	}
	{
	  getsp,0	%dr13, %dr14
	  addd,1	0x0, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	cl_spin, .-cl_spin
