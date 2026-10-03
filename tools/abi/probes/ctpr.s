! SPDX-License-Identifier: GPL-3.0-or-later
! Edge cases of control-transfer preparation registers; see ctpr.c.
! Each t_* returns 1 if the transfer through the prepared %ctpr2 happened,
! 0 if execution fell through; a trap is caught by ctpr.c.
	.ignore	ld_st_style
	.text

	.type	ctpr_leaf, #function
	.align	8
ctpr_leaf:
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
	.size	ctpr_leaf, .-ctpr_leaf

! 1. Prepare %ctpr2, call a leaf, then ct %ctpr2 after the return.
	.global	t_keep_across_call
	.type	t_keep_across_call, #function
	.align	8
t_keep_across_call:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr2, .Lkac_ok
	}
	{
	  disp	%ctpr1, ctpr_leaf
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr2
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x0, %dr0
	}
.Lkac_ok:
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	t_keep_across_call, .-t_keep_across_call

! 2. The caller prepares %ctpr2 to a label in the callee; the callee's
!    first transfer is ct %ctpr2.
	.type	ctpr_callee, #function
	.align	8
ctpr_callee:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr2
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x0, %dr0
	}
ctpr_callee_lbl:
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	ctpr_callee, .-ctpr_callee

	.global	t_callee_sees
	.type	t_callee_sees, #function
	.align	8
t_callee_sees:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	  disp	%ctpr2, ctpr_callee_lbl
	}
	{
	  disp	%ctpr1, ctpr_callee
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, %db[0], %dr0
	}
	.size	t_callee_sees, .-t_callee_sees

! 3. After a call, ct %ctpr2 under a false predicate.
!    Returns 1 if execution reaches the end without a trap.
	.global	t_false_on_emptied
	.type	t_false_on_emptied, #function
	.align	8
t_false_on_emptied:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr2, .Lfoe_taken
	  addd,0 0x0, 0x0, %dr5
	}
	{
	  disp	%ctpr1, ctpr_leaf
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  cmpedb,0 %dr5, 0x1, %pred0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr2 ? %pred0
	}
.Lfoe_taken:
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	t_false_on_emptied, .-t_false_on_emptied

! 4. A taken ct through %ctpr1 between preparing %ctpr2 and using it.
	.global	t_branch_keeps
	.type	t_branch_keeps, #function
	.align	8
t_branch_keeps:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr2, .Lbk_ok
	}
	{
	  disp	%ctpr1, .Lbk_mid
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr1
	}
.Lbk_mid:
	{
	  ct	%ctpr2
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x0, %dr0
	}
.Lbk_ok:
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	t_branch_keeps, .-t_branch_keeps

! 5. movtd of a value from a failed speculative load (a diagnostic tag)
!    into %ctpr2, then ct %ctpr2 under a true (t_diag_true) or false
!    (t_diag_false) predicate. The argument is a bad address. Returns 1
!    if the end is reached, 2 if ct transferred (it cannot: the target is
!    invalid).
.macro	diag name, cond
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  ldd,0,sm	%dr0, 0x0, %dr2
	  addd,1 0x0, 0x0, %dr5
	}
	{
	  nop 5
	  cmpedb,0 %dr5, \cond, %pred0
	}
	{
	  movtd,0	%dr2, %ctpr2
	}
	{
	  nop 7
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr2 ? %pred0
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	\name, .-\name
.endm
	diag	t_diag_true, 0x0
	diag	t_diag_false, 0x1
