! SPDX-License-Identifier: GPL-3.0-or-later
! What a call under a false predicate costs: see callsstack_pcall.c
! (spec/isa/predicates.md, control-transfer.md, timing.md).
	.ignore	ld_st_style
	.ignore	strict_delay
	.text

! long pc_clkr (long): %clkr.
	.global	pc_clkr
	.type	pc_clkr, #function
	.align	8
pc_clkr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrd,0	%clkr, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	pc_clkr, .-pc_clkr

! long pc_leaf (long): returns at once (the target of the false calls,
! never reached).
	.global	pc_leaf
	.type	pc_leaf, #function
	.align	8
pc_leaf:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pc_leaf, .-pc_leaf

! long pl_KIND_dD (long n): cycles (%clkr) for n iterations of an
! 8-instruction loop (one cycle each, the back edge a prepared ct).  Its
! first instruction holds, by KIND:
!   none   nothing (the baseline),
!   call   call %ctpr2, wbs = 4 ? %pred1, %pred1 false,
!   tcall  the same with %pred1 true, to pc_leaf: a real call (the loop
!          then prepares %ctpr1 again, so it is 2 instructions longer:
!          compare with none2),
!   none2  none with those 2 instructions,
!   ct     ct %ctpr2 ? %pred1, %pred1 false (a false branch),
! and D is how many instructions before it a compare writes %pred1
! (cmpesb,1 0, 1: false; 0: once before the loop).
.macro	ploop kind, d
	.global	pl_\kind\()_d\d
	.type	pl_\kind\()_d\d, #function
	.align	8
pl_\kind\()_d\d:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  .ifc	\kind, tcall
	  cmpesb,1	0x0, 0x0, %pred1
	  .else
	  cmpesb,1	0x0, 0x1, %pred1
	  .endif
	}
	{
	  disp	%ctpr2, pc_leaf
	}
	{
	  disp	%ctpr1, .Lpl_\kind\()_d\d
	  rrd,0	%clkr, %dr4
	}
	{
	  nop 7
	}
.Lpl_\kind\()_d\d:
	{
	  subd,0	%dr0, 0x1, %dr0
	  .ifc	\kind, call
	  call	%ctpr2, wbs = 0x4 ? %pred1
	  .endif
	  .ifc	\kind, tcall
	  call	%ctpr2, wbs = 0x4 ? %pred1
	  .endif
	  .ifc	\kind, ct
	  ct	%ctpr2 ? %pred1
	  .endif
	}
	.ifc	\kind, tcall
	{
	  disp	%ctpr1, .Lpl_\kind\()_d\d
	}
	{
	  disp	%ctpr2, pc_leaf
	}
	.endif
	.ifc	\kind, none2
	{
	  nop
	}
	{
	  nop
	}
	.endif
	.irp	k, 7, 6, 5, 4, 3, 2
	{
	  .if	\k == 6
	  cmpedb,0	%dr0, 0x0, %pred0
	  .endif
	  .if	\d == \k
	  .ifc	\kind, tcall
	  cmpesb,1	0x0, 0x0, %pred1
	  .else
	  cmpesb,1	0x0, 0x1, %pred1
	  .endif
	  .endif
	  .if	\k - 6
	  .if	\d - \k
	  nop
	  .endif
	  .endif
	}
	.endr
	{
	  ct	%ctpr1 ? ~%pred0
	  .if	\d == 1
	  .ifc	\kind, tcall
	  cmpesb,1	0x0, 0x0, %pred1
	  .else
	  cmpesb,1	0x0, 0x1, %pred1
	  .endif
	  .endif
	}
	{
	  rrd,0	%clkr, %dr5
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	  ct	%ctpr3
	}
	.size	pl_\kind\()_d\d, .-pl_\kind\()_d\d
.endm
	.irp	d, 0, 1, 2, 3, 4
	ploop	none, \d
	ploop	call, \d
	ploop	ct, \d
	.endr
	ploop	tcall, 0
	ploop	none2, 0

! long pr_KIND (long n): recursion n deep; each level (n > 0) calls
! pr_KIND (n - 1), under a predicate (call ? ~%pred0, %pred0 = n == 0).
! KIND:
!   none   nothing else,
!   pre    a call ? %pred1 (false) to pc_leaf before the recursive call,
!   post   the same after it returns,
!   both   one before and one after.
! The kernels are not aligned against gas's CODE_PLACEMENT padding: in
! the build that was timed, gas put an empty wide instruction before
! pr_both's recursive call (callsstack_pcall.c, rec).
.macro	prec kind
	.global	pr_\kind
	.type	pr_\kind, #function
	.align	8
pr_\kind:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, pr_\kind
	  cmpedb,0	%dr0, 0x0, %pred0
	  cmpesb,1	0x0, 0x1, %pred1
	  subd,2	%dr0, 0x1, %dr8
	}
	{
	  disp	%ctpr2, pc_leaf
	}
	{
	  nop 4
	}
	{
	  .ifc	\kind, pre
	  call	%ctpr2, wbs = 0x4 ? %pred1
	  .endif
	  .ifc	\kind, both
	  call	%ctpr2, wbs = 0x4 ? %pred1
	  .endif
	  .ifc	\kind, none
	  nop
	  .endif
	  .ifc	\kind, post
	  nop
	  .endif
	}
	{
	  call	%ctpr1, wbs = 0x4 ? ~%pred0
	}
	{
	  return	%ctpr3
	}
	{
	  disp	%ctpr2, pc_leaf
	}
	{
	  nop 4
	}
	{
	  .ifc	\kind, post
	  call	%ctpr2, wbs = 0x4 ? %pred1
	  .endif
	  .ifc	\kind, both
	  call	%ctpr2, wbs = 0x4 ? %pred1
	  .endif
	  .ifc	\kind, none
	  nop
	  .endif
	  .ifc	\kind, pre
	  nop
	  .endif
	}
	{
	  ct	%ctpr3
	}
	.size	pr_\kind, .-pr_\kind
.endm
	prec	none
	prec	pre
	prec	post
	prec	both

! void pc_shtp (long *out): out[0..1] = %pcshtp, %pcsp.hi at entry;
! then three times a call ? %pred1 (false), 8 cycles later a read of
! the two into out[2], out[3], then out[4], out[5], then out[6], out[7].
	.global	pc_shtp
	.type	pc_shtp, #function
	.align	8
pc_shtp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  cmpesb,1	0x0, 0x1, %pred1
	}
	{
	  disp	%ctpr2, pc_leaf
	}
	{
	  nop 7
	}
	{
	  rrd,0	%pcshtp, %dr4
	}
	{
	  rrd,0	%pcsp.hi, %dr5
	}
	{
	  nop 7
	}
	{
	  call	%ctpr2, wbs = 0x4 ? %pred1
	}
	{
	  nop 7
	}
	{
	  rrd,0	%pcshtp, %dr6
	}
	{
	  rrd,0	%pcsp.hi, %dr7
	}
	{
	  nop 7
	}
	{
	  call	%ctpr2, wbs = 0x4 ? %pred1
	}
	{
	  nop 7
	}
	{
	  rrd,0	%pcshtp, %dr8
	}
	{
	  rrd,0	%pcsp.hi, %dr9
	}
	{
	  nop 7
	}
	{
	  call	%ctpr2, wbs = 0x4 ? %pred1
	}
	{
	  nop 7
	}
	{
	  rrd,0	%pcshtp, %dr10
	}
	{
	  rrd,0	%pcsp.hi, %dr11
	}
	{
	  nop 7
	}
	{
	  std,2	%dr0, 0x0, %dr4
	  std,5	%dr0, 0x8, %dr5
	}
	{
	  std,2	%dr0, 0x10, %dr6
	  std,5	%dr0, 0x18, %dr7
	}
	{
	  std,2	%dr0, 0x20, %dr8
	  std,5	%dr0, 0x28, %dr9
	}
	{
	  std,2	%dr0, 0x30, %dr10
	  std,5	%dr0, 0x38, %dr11
	}
	{
	  ct	%ctpr3
	}
	.size	pc_shtp, .-pc_shtp
