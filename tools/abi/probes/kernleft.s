! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for kernleft.c (system-call entries, edited signal frames and
! %bgr, context switch costs).  Build and run lines are in kernleft.c:
!   $L -O2 kernleft.c kernleft.s -o kernleft -lpthread         (v3)
!   $L -O2 -Wa,--defsym,KL_V5=1 kernleft.c kernleft.s ...       (v5, v6)
! Nothing here writes a privileged register; the only state registers
! written are %bgr (user-writable, restored before every return).
	.ignore	ld_st_style
	.text

! long kl_clkr (void): %clkr.
	.global	kl_clkr
	.type	kl_clkr, #function
	.align	8
kl_clkr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrd,0	%clkr, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	kl_clkr, .-kl_clkr

! long kl_prep_N (void): sdisp %ctpr1, N, then (%ctpr1 >> 54) & 0x7f,
! the ta_tag (bits 0-2 of the result) and opc (bits 3-4).  The target
! address is shifted out before anything is stored.  No transfer.
	.macro	klprep entry
	.global	kl_prep_\entry
	.type	kl_prep_\entry, #function
	.align	8
kl_prep_\entry:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  sdisp	%ctpr1, \entry
	}
	{
	  nop 4
	}
	{
	  rrd,0	%ctpr1, %dr1
	}
	{
	  nop 2
	}
	{
	  shrd,0	%dr1, 0x36, %dr1
	}
	{
	  return	%ctpr3
	  andd,0	%dr1, 0x7f, %dr0
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr3
	}
	.size	kl_prep_\entry, .-kl_prep_\entry
	.endm

	.irp	e, 0,3,4,7,8,10,11,12,16,17,18
	klprep	\e
	.endr

! long kl_call_N (long nr, long a1, long a2, long a3, long *out):
! %g16 = 0x1616, then a system call through entry N with nr in %b[0]
! and a1..a3 in %b[1]..%b[3], 4 pairs handed over (wsz 0xc, wbs 0x8).
! Returns %db[0]; out[0] = %g16 afterwards (tag cleared), out[1] its tag.
	.macro	klcall entry
	.global	kl_call_\entry
	.type	kl_call_\entry, #function
	.align	8
kl_call_\entry:
	{
	  setwd	wsz = 0xc, nfx = 0x1, dbl = 0x0
	  sdisp	%ctpr1, \entry
	  addd,0	0x0, %dr0, %dr16
	  addd,1	0x0, %dr1, %dr17
	  addd,3	0x0, %dr2, %dr18
	  addd,4	0x0, %dr3, %dr19
	}
	{
	  addd,0	0x0, 0x1616, %dg16
	}
	{
	  nop 3
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	.global	kl_call_\entry\()_ret
kl_call_\entry\()_ret:
	{
	  gettagd,2	%dg16, %r6
	  puttagd,5	%dg16, 0x0, %dr7
	}
	{
	  nop 2
	}
	{
	  return	%ctpr3
	  std,2	%dr4, 0x0, %dr7
	  std,5	%dr4, 0x8, %dr6
	  addd,0	0x0, %dr16, %dr0
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr3
	}
	.size	kl_call_\entry, .-kl_call_\entry
	.endm

	.irp	e, 0,3,4,7,8,10,11,12,16,17,18
	klcall	\e
	.endr

! void kl_gspin (long *io): io[0] = the %bgr to run under (bits 0-10),
! io[1..16] = values for %dg16..%dg31 (logical names, loaded after the
! %bgr write took effect).  Spins until io[20] != 0 (a timer signal's
! handler sets it), then io[32 + k] = %dg(16+k) with its tag cleared,
! io[48 + k] = its tag, io[66 + k] = movfi of it (the 16-bit extension),
! io[64] = %bgr, io[65] = %bgr read right after the write; v5+ builds
! (KL_V5) also store %qpg(16+k) at io[96 + 2k] (low, high).  %bgr is
! put back as it was at entry before the return; before that, one
! non-speculative addd reads %dg19 (kl_gspin_use19).  io must be 16-byte
! aligned.  The reads of %g24..%g31 are in ALC0 and ALC2 only, where an
! empty slot of %bgr.val does not trap (spec/isa/loops.md).
	.macro	gl n
	{
	  ldd,0	%dr0, 8 * (\n - 15), %dg\n
	}
	.endm
	.macro	go n
	{
	  gettagd,2	%dg\n, %r8
	}
	{
	  puttagd,2	%dg\n, 0x0, %dr9
	  movfi,0	%dg\n, %r10
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 8 * (32 + \n - 16), %dr9
	  stw,5	%dr0, 8 * (48 + \n - 16), %r8
	}
	{
	  stw,2	%dr0, 8 * (66 + \n - 16), %r10
	}
	.ifdef	KL_V5
	{
	  qpand,0	%qpg\n, %qpg\n, %qpr12
	}
	{
	  nop 2
	}
	{
	  stqp,2	%dr0, 8 * (96 + 2 * (\n - 16)), %qpr12
	}
	.endif
	.endm

	.global	kl_gspin
	.type	kl_gspin, #function
	.align	8
kl_gspin:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  rrs,0	%bgr, %r20
	  ldd,2	%dr0, 0x0, %dr21
	}
	{
	  nop 2
	}
	{
	  rws,0	%r21, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	{
	  rrs,0	%bgr, %r22
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr0, 8 * 65, %r22
	}
	.irp	n, 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	gl	\n
	.endr
	{
	  nop 2
	}
.Lkl_spin:
	{
	  ldd,0	%dr0, 8 * 20, %dr6
	}
	{
	  nop 2
	}
	{
	  cmpedb,0	%dr6, 0x0, %pred0
	}
	{
	  nop 2
	}
	{
	  ibranch	.Lkl_spin ? %pred0
	}
	{
	  rrs,0	%bgr, %r22
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr0, 8 * 64, %r22
	}
	.irp	n, 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	go	\n
	.endr
	.global	kl_gspin_use19
kl_gspin_use19:
	{
	  addd,0	0x0, %dg19, %dr11
	}
	{
	  rws,0	%r20, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	{
	  return	%ctpr3
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.global	kl_gspin_end
kl_gspin_end:
	.size	kl_gspin, .-kl_gspin

! void kl_hbgr (int, siginfo_t *, void *): a handler that stores %bgr as
! it finds it (its first instruction) in kl_hbgr_val, then calls
! kl_handler_c with its three arguments.
	.global	kl_hbgr
	.type	kl_hbgr, #function
	.align	8
kl_hbgr:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	  rrs,0	%bgr, %r6
	}
	{
	  addd,0	0x0, [ _f64,_lts0 kl_hbgr_val ], %dr7
	  disp	%ctpr1, kl_handler_c
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr7, 0x0, %r6
	  addd,0	0x0, %dr0, %db[0]
	  addd,1	0x0, %dr1, %db[1]
	  addd,3	0x0, %dr2, %db[2]
	}
	{
	  nop 4
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  return	%ctpr3
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	kl_hbgr, .-kl_hbgr

! void kl_grot_ld (long *io), kl_grot_alu (long *io): no signal.  %bgr =
! io[0]; then %dg24..%dg31 = io[9..16] (logical names), written by ldd
! (kl_grot_ld) or by addd from window registers (kl_grot_alu);
! io[32 + k] = %dg(24+k) read under io[0]; then %bgr = 0x0ff (cur 0) and
! io[40 + k] = %dg(24+k) read again (the physical registers).  Reads are
! in ALC0.  %bgr is put back as it was at entry.
	.macro	grd n, slot
	{
	  addd,0	0x0, %dg\n, %dr8
	}
	{
	  std,2	%dr0, 8 * \slot, %dr8
	}
	.endm

	.macro	grot name, alu
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  rrs,0	%bgr, %r20
	  ldd,2	%dr0, 0x0, %dr21
	}
	{
	  nop 2
	}
	{
	  rws,0	%r21, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	.if	\alu
	{
	  ldd,0	%dr0, 8 * 9, %dr10
	  ldd,2	%dr0, 8 * 10, %dr11
	  ldd,3	%dr0, 8 * 11, %dr12
	  ldd,5	%dr0, 8 * 12, %dr13
	}
	{
	  ldd,0	%dr0, 8 * 13, %dr14
	  ldd,2	%dr0, 8 * 14, %dr15
	  ldd,3	%dr0, 8 * 15, %dr16
	  ldd,5	%dr0, 8 * 16, %dr17
	}
	{
	  nop 3
	}
	{
	  addd,0	0x0, %dr10, %dg24
	}
	{
	  addd,0	0x0, %dr11, %dg25
	}
	{
	  addd,0	0x0, %dr12, %dg26
	}
	{
	  addd,0	0x0, %dr13, %dg27
	}
	{
	  addd,0	0x0, %dr14, %dg28
	}
	{
	  addd,0	0x0, %dr15, %dg29
	}
	{
	  addd,0	0x0, %dr16, %dg30
	}
	{
	  addd,0	0x0, %dr17, %dg31
	}
	.else
	.irp	n, 24,25,26,27,28,29,30,31
	gl	\n
	.endr
	.endif
	{
	  nop 3
	}
	grd	24, 32
	grd	25, 33
	grd	26, 34
	grd	27, 35
	grd	28, 36
	grd	29, 37
	grd	30, 38
	grd	31, 39
	{
	  addd,0	0x0, 0xff, %dr21
	}
	{
	  rws,0	%r21, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	grd	24, 40
	grd	25, 41
	grd	26, 42
	grd	27, 43
	grd	28, 44
	grd	29, 45
	grd	30, 46
	grd	31, 47
	{
	  rws,0	%r20, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	{
	  return	%ctpr3
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	grot	kl_grot_ld, 0
	grot	kl_grot_alu, 1

! void kl_grot_st (long *io): no signal.  Under %bgr = 0x0ff (cur 0)
! %dg24..%dg31 = io[9..16] (so physical 24 + p holds io[9 + p]); then
! %bgr = io[0] and %dg24..%dg31 (names under io[0]) are read three ways:
! io[32 + k] by addd (ALC0) then std, io[40 + k] stored directly as the
! data of std in ALC2, io[48 + k] stored directly by std in ALC5 (the
! kernel's v3 save uses strd,2 and strd,5 of %dg24..%dg31).  %bgr is put
! back as it was at entry.
	.macro	gsd n, slot, ch
	{
	  std,\ch	%dr0, 8 * \slot, %dg\n
	}
	.endm

	.global	kl_grot_st
	.type	kl_grot_st, #function
	.align	8
kl_grot_st:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  rrs,0	%bgr, %r20
	  ldd,2	%dr0, 0x0, %dr21
	  addd,1	0x0, 0xff, %dr22
	}
	{
	  nop 2
	}
	{
	  rws,0	%r22, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	.irp	n, 24,25,26,27,28,29,30,31
	gl	\n
	.endr
	{
	  nop 3
	}
	{
	  rws,0	%r21, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	grd	24, 32
	grd	25, 33
	grd	26, 34
	grd	27, 35
	grd	28, 36
	grd	29, 37
	grd	30, 38
	grd	31, 39
	gsd	24, 40, 2
	gsd	25, 41, 2
	gsd	26, 42, 2
	gsd	27, 43, 2
	gsd	28, 44, 2
	gsd	29, 45, 2
	gsd	30, 46, 2
	gsd	31, 47, 2
	gsd	24, 48, 5
	gsd	25, 49, 5
	gsd	26, 50, 5
	gsd	27, 51, 5
	gsd	28, 52, 5
	gsd	29, 53, 5
	gsd	30, 54, 5
	gsd	31, 55, 5
	{
	  rws,0	%r20, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	{
	  return	%ctpr3
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	kl_grot_st, .-kl_grot_st
