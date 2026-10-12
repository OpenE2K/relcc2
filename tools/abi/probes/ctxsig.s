! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for ctxsig.c (user contexts and signal frames): %clkr, the FP
! control registers, a frame that sets %g16..%g19 around a system call or
! a timer signal, every global around a system call, system calls handed
! 2 to 8 window pairs, handlers that read %g16..%g31 before compiled code
! can, and tag probes.  Build and run lines are in ctxsig.c:
!   $L -O2 -rdynamic ctxsig.c ctxsig.s -o ctxsig -lpthread -lm
	.ignore	ld_st_style
	.text

! long cs_clkr (void): %clkr.
	.global	cs_clkr
	.type	cs_clkr, #function
	.align	8
cs_clkr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrd,0	%clkr, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	cs_clkr, .-cs_clkr

! void cs_rd_fp (unsigned *out): out[0] = %fpcr, out[1] = %fpsr,
! out[2] = %pfpfr.
	.global	cs_rd_fp
	.type	cs_rd_fp, #function
	.align	8
cs_rd_fp:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrs,0	%fpcr, %r1
	}
	{
	  rrs,0	%fpsr, %r2
	}
	{
	  rrs,0	%pfpfr, %r3
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr0, 0x0, %r1
	  stw,5	%dr0, 0x4, %r2
	}
	{
	  stw,2	%dr0, 0x8, %r3
	}
	{
	  ct	%ctpr3
	}
	.size	cs_rd_fp, .-cs_rd_fp

! long cs_gsig (long *io): put known values in %g16..%g19, then either
! send ourselves a signal with tgkill (a system call made here, so this
! frame is the interrupted one) or spin until a handler sets a flag (a
! timer signal interrupts this loop); then read the globals back.
!   in:  io[0..3]  values for %g16..%g19
!        io[4]     signal number for tgkill (0: a tgkill that sends none)
!        io[5]     the flag (the handler writes it)
!        io[6], io[7]  pid and tid for tgkill
!        io[20]    nonzero: no system call, spin until io[5] != 0
!   out: io[8..11]  %g16..%g19 afterwards (value, tag cleared)
!        io[12..15] their tags (gettagd)
!        io[16], io[17]  getsp 0 before and after
!        io[18]  %db[0] afterwards: %br is rbs = 6, rsz = 0 throughout,
!                so %b[0] is %r12 (0xcccc); 0xeeee means %b[0] became
!                %r14 (rbs = 7)
!        io[19]  the tgkill result (0 when spinning)
! The globals are read with gettagd (ALC2) and puttagd (ALC5), which
! don't trap on a diagnostic value (abg.s).  The system call's window
! starts at %r16 (wbs = 8), so %r12..%r15 survive it.
	.global	cs_gsig
	.type	cs_gsig, #function
	.align	8
cs_gsig:
	{
	  setwd	wsz = 0xc, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x0, rbs = 0x6, rcur = 0x0
	  ldd,0	%dr0, 0x0, %dg16
	  ldd,2	%dr0, 0x8, %dg17
	  ldd,3	%dr0, 0x10, %dg18
	  ldd,5	%dr0, 0x18, %dg19
	}
	{
	  ldd,0	%dr0, 0x20, %dr1
	  ldd,2	%dr0, 0x30, %dr2
	  ldd,3	%dr0, 0x38, %dr3
	  ldd,5	%dr0, 0xa0, %dr10
	  addd,1	0x0, 0xcccc, %dr12
	  addd,4	0x0, 0xeeee, %dr14
	}
	{
	  nop 2
	}
	{
	  cmpedb,0	%dr10, 0x0, %pred0
	  sdisp	%ctpr1, 0x3
	  addd,1	0x0, 0x134, %dr16
	  addd,3	0x0, %dr2, %dr17
	  addd,4	0x0, %dr3, %dr18
	}
	{
	  addd,1	0x0, %dr1, %dr19
	  getsp,0	0x0, %dr4
	}
	{
	  nop 2
	}
	{
	  ibranch	.Lgs_spin ? ~%pred0
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  addd,0	0x0, %dr16, %dr5
	  ibranch	.Lgs_after
	}
.Lgs_spin:
	{
	  ldd,0	%dr0, 0x28, %dr6
	}
	{
	  nop 2
	}
	{
	  cmpedb,0	%dr6, 0x0, %pred1
	}
	{
	  nop 2
	}
	{
	  ibranch	.Lgs_spin ? %pred1
	}
	{
	  addd,0	0x0, 0x0, %dr5
	}
.Lgs_after:
	{
	  getsp,0	0x0, %dr6
	  addd,1	0x0, %db[0], %dr7
	}
	{
	  gettagd,2	%dg16, %r8
	  puttagd,5	%dg16, 0x0, %dr9
	}
	{
	  gettagd,2	%dg17, %r10
	  puttagd,5	%dg17, 0x0, %dr11
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x40, %dr9
	  std,5	%dr0, 0x60, %dr8
	}
	{
	  std,2	%dr0, 0x48, %dr11
	  std,5	%dr0, 0x68, %dr10
	}
	{
	  gettagd,2	%dg18, %r8
	  puttagd,5	%dg18, 0x0, %dr9
	}
	{
	  gettagd,2	%dg19, %r10
	  puttagd,5	%dg19, 0x0, %dr11
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x50, %dr9
	  std,5	%dr0, 0x70, %dr8
	}
	{
	  std,2	%dr0, 0x58, %dr11
	  std,5	%dr0, 0x78, %dr10
	}
	{
	  std,2	%dr0, 0x80, %dr4
	  std,5	%dr0, 0x88, %dr6
	}
	{
	  return	%ctpr3
	  std,2	%dr0, 0x90, %dr7
	  std,5	%dr0, 0x98, %dr5
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr3
	}
	.global	cs_gsig_end
cs_gsig_end:
	.size	cs_gsig, .-cs_gsig

! void cs_diag_g (long *out), cs_diag_r (long *out): a diagnostic value
! (a speculative mas=0x3 load from address 0) in %dg20 or in a window
! register, then a non-speculative addd of it: out[0] = its tag, out[1]
! the addd's result (tag cleared), out[2] the result's tag.  Run in a
! child: the addd may trap.
	.global	cs_diag_g
	.type	cs_diag_g, #function
	.align	8
cs_diag_g:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,1	0x0, 0x0, %dr1
	}
	{
	  ldd,0,sm	%dr1, 0x0, %dg20, mas=0x3
	}
	{
	  nop 3
	}
	{
	  gettagd,2	%dg20, %r2
	}
	{
	  addd,0	%dg20, 0x1, %dr3
	}
	{
	  nop 1
	}
	{
	  gettagd,2	%dr3, %r4
	  puttagd,5	%dr3, 0x0, %dr5
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr0, 0x0, %r2
	  std,5	%dr0, 0x8, %dr5
	}
	{
	  stw,2	%dr0, 0x10, %r4
	}
	{
	  ct	%ctpr3
	}
	.size	cs_diag_g, .-cs_diag_g

	.global	cs_diag_r
	.type	cs_diag_r, #function
	.align	8
cs_diag_r:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,1	0x0, 0x0, %dr1
	}
	{
	  ldd,0,sm	%dr1, 0x0, %dr7, mas=0x3
	}
	{
	  nop 3
	}
	{
	  gettagd,2	%dr7, %r2
	}
	{
	  addd,0	%dr7, 0x1, %dr3
	}
	{
	  nop 1
	}
	{
	  gettagd,2	%dr3, %r4
	  puttagd,5	%dr3, 0x0, %dr5
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr0, 0x0, %r2
	  std,5	%dr0, 0x8, %dr5
	}
	{
	  stw,2	%dr0, 0x10, %r4
	}
	{
	  ct	%ctpr3
	}
	.size	cs_diag_r, .-cs_diag_r

! What a system call leaves in %g16..%g19, and what the "empty" value it
! leaves does under non-speculative use.
! void cs_empty_tag (long *out, long fast): a system call (fast = 0:
! getppid through entry 3; fast = 1: fast call 3, siggetmask, through
! entry 6, into out[8]) right after %g16..%g19 = 0x16.., 0x17..,
! 0x18.., 0x19..; out[0..3] = %g16..%g19 afterwards (tags cleared),
! out[4..7] their tags.
	.global	cs_empty_tag
	.type	cs_empty_tag, #function
	.align	8
cs_empty_tag:
	{
	  setwd	wsz = 0xc, nfx = 0x1, dbl = 0x0
	}
	{
	  addd,0	0x0, [ _f64,_lts0 0x1616161616161616 ], %dg16
	}
	{
	  addd,0	0x0, [ _f64,_lts0 0x1717171717171717 ], %dg17
	}
	{
	  addd,0	0x0, [ _f64,_lts0 0x1818181818181818 ], %dg18
	}
	{
	  addd,0	0x0, [ _f64,_lts0 0x1919191919191919 ], %dg19
	  cmpedb,1	%dr1, 0x0, %pred0
	}
	{
	  addd,0	0x0, 0x40, %dr16
	  addd,2	%dr0, 0x40, %dr17
	  addd,3	0x0, 0x8, %dr18
	}
	{
	  addd,1	0x3, 0x0, %dr16 ? ~%pred0
	}
	{
	  sdisp	%ctpr1, 0x3
	}
	{
	  sdisp	%ctpr2, 0x6
	}
	{
	  nop 4
	}
	{
	  call	%ctpr1, wbs = 0x8 ? %pred0
	}
	{
	  call	%ctpr2, wbs = 0x8 ? ~%pred0
	}
	{
	  gettagd,2	%dg16, %r8
	  puttagd,5	%dg16, 0x0, %dr9
	}
	{
	  gettagd,2	%dg17, %r10
	  puttagd,5	%dg17, 0x0, %dr11
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x0, %dr9
	  std,5	%dr0, 0x20, %dr8
	}
	{
	  std,2	%dr0, 0x8, %dr11
	  std,5	%dr0, 0x28, %dr10
	}
	{
	  gettagd,2	%dg18, %r8
	  puttagd,5	%dg18, 0x0, %dr9
	}
	{
	  gettagd,2	%dg19, %r10
	  puttagd,5	%dg19, 0x0, %dr11
	}
	{
	  nop 2
	}
	{
	  return	%ctpr3
	  std,2	%dr0, 0x10, %dr9
	  std,5	%dr0, 0x30, %dr8
	}
	{
	  std,2	%dr0, 0x18, %dr11
	  std,5	%dr0, 0x38, %dr10
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr3
	}
	.size	cs_empty_tag, .-cs_empty_tag

! void cs_sysg (long *io, long mode): every global but %g12 and %g13
! (the static chain and the thread pointer, left alone) around a system
! call: number io[112], arguments io[110] and io[111].  Mode bit 0:
! through entry 6 (a fast call) instead of entry 3; bit 1: set %g0..%g11,
! %g14, %g15 = io[N]; bit 2: set %g16..%g31 = io[N].  Then io[32 + N] =
! %gN (tag cleared) and the low word of io[64 + N] = its tag.  The caller's %g0..%g11, %g14, %g15 are kept in
! %dr1..%dr14 (movtd keeps tags; the call leaves %r0..%r23 alone) and put
! back before the return.  The call hands over 4 pairs (wsz 0x10, wbs
! 0xc): with more than 4 the kernel kills the process.  ctxsig.c runs it
! in a child.
	.macro	sg_save g, r
	{
	  movtd,0	%dg\g, %dr\r
	}
	.endm
	.macro	sg_rest g, r
	{
	  movtd,0	%dr\r, %dg\g
	}
	.endm
	.macro	sg_load g, p
	{
	  ldd,0	%dr0, 8 * \g, %dg\g ? %pred\p
	}
	.endm
	.macro	sg_out g
	{
	  gettagd,2	%dg\g, %r16
	  puttagd,5	%dg\g, 0x0, %dr17
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr0, 8 * (64 + \g), %r16
	  std,5	%dr0, 8 * (32 + \g), %dr17
	}
	.endm

	.global	cs_sysg
	.type	cs_sysg, #function
	.align	8
cs_sysg:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  andd,0	%dr1, 0x1, %dr15
	  andd,1	%dr1, 0x2, %dr16
	  andd,3	%dr1, 0x4, %dr17
	}
	{
	  cmpedb,0	%dr15, 0x0, %pred0
	  cmpedb,1	%dr16, 0x0, %pred3
	  cmpedb,3	%dr17, 0x0, %pred4
	}
	{
	  pass	%pred3, @p0
	  pass	%pred4, @p1
	  landp	~@p0, ~@p0, @p4
	  pass	@p4, %pred1
	  landp	~@p1, ~@p1, @p5
	  pass	@p5, %pred2
	}
	sg_save	0, 1
	sg_save	1, 2
	sg_save	2, 3
	sg_save	3, 4
	sg_save	4, 5
	sg_save	5, 6
	sg_save	6, 7
	sg_save	7, 8
	sg_save	8, 9
	sg_save	9, 10
	sg_save	10, 11
	sg_save	11, 12
	sg_save	14, 13
	sg_save	15, 14
	.irp	g, 0,1,2,3,4,5,6,7,8,9,10,11,14,15
	sg_load	\g, 1
	.endr
	.irp	g, 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	sg_load	\g, 2
	.endr
	{
	  ldd,0	%dr0, 0x380, %dr24
	  ldd,2	%dr0, 0x370, %dr25
	  ldd,3	%dr0, 0x378, %dr26
	}
	{
	  sdisp	%ctpr1, 0x3
	}
	{
	  sdisp	%ctpr2, 0x6
	}
	{
	  nop 4
	}
	{
	  call	%ctpr1, wbs = 0xc ? %pred0
	}
	{
	  call	%ctpr2, wbs = 0xc ? ~%pred0
	}
	.irp	g, 0,1,2,3,4,5,6,7,8,9,10,11,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	sg_out	\g
	.endr
	{
	  return	%ctpr3
	}
	sg_rest	0, 1
	sg_rest	1, 2
	sg_rest	2, 3
	sg_rest	3, 4
	sg_rest	4, 5
	sg_rest	5, 6
	sg_rest	6, 7
	sg_rest	7, 8
	sg_rest	8, 9
	sg_rest	9, 10
	sg_rest	10, 11
	sg_rest	11, 12
	sg_rest	14, 13
	sg_rest	15, 14
	{
	  ct	%ctpr3
	}
	.size	cs_sysg, .-cs_sysg

! void cs_sysw2 (void), cs_sysw4, cs_sysw6, cs_sysw8: getppid through
! entry 3 with 2, 4, 6 or 8 pairs handed over (wsz - wbs); cs_sysf6:
! fast call 2 (getcpu, null pointers) through entry 6 with 6 pairs.
! ctxsig.c runs each in a child.
	.macro	sysw name, wsz, wbs, entry, nr
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = \wsz, nfx = 0x1, dbl = 0x0
	}
	{
	  addd,0	0x0, \nr, %dr16
	  addd,1	0x0, 0x0, %dr17
	  addd,3	0x0, 0x0, %dr18
	  sdisp	%ctpr1, \entry
	}
	{
	  nop 4
	}
	{
	  call	%ctpr1, wbs = \wbs
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

	sysw	cs_sysw2, 0xa, 0x8, 0x3, 0x40
	sysw	cs_sysw4, 0xc, 0x8, 0x3, 0x40
	sysw	cs_sysw6, 0xe, 0x8, 0x3, 0x40
	sysw	cs_sysw8, 0x10, 0x8, 0x3, 0x40
	sysw	cs_sysf6, 0xe, 0x8, 0x6, 0x2

! void cs_empty_add (long *out): getppid, then a non-speculative
! addd %dg16, 1: out[0] = the result (tag cleared), out[1] its tag.
	.global	cs_empty_add
	.type	cs_empty_add, #function
	.align	8
cs_empty_add:
	{
	  setwd	wsz = 0xc, nfx = 0x1, dbl = 0x0
	  sdisp	%ctpr1, 0x3
	  addd,0	0x0, 0x40, %dr16
	}
	{
	  nop 4
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  addd,0	%dg16, 0x1, %dr6
	}
	{
	  nop 1
	}
	{
	  gettagd,2	%dr6, %r7
	  puttagd,5	%dr6, 0x0, %dr5
	}
	{
	  nop 2
	}
	{
	  return	%ctpr3
	  std,2	%dr0, 0x0, %dr5
	  std,5	%dr0, 0x8, %dr7
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr3
	}
	.size	cs_empty_add, .-cs_empty_add

! void cs_empty_st (long *out): getppid, then a non-speculative std of
! %dg16 to out[2], a fill load (mas=0x4) of it into a window register:
! out[0] = that register's tag; then a non-speculative addd of it:
! out[1] = the result.
	.global	cs_empty_st
	.type	cs_empty_st, #function
	.align	8
cs_empty_st:
	{
	  setwd	wsz = 0xc, nfx = 0x1, dbl = 0x0
	  sdisp	%ctpr1, 0x3
	  addd,0	0x0, 0x40, %dr16
	}
	{
	  nop 4
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  std,2	%dr0, 0x10, %dg16
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr0, 0x10, %dr7, mas=0x4
	}
	{
	  nop 3
	}
	{
	  gettagd,2	%dr7, %r8
	}
	{
	  nop 1
	}
	{
	  stw,2	%dr0, 0x0, %r8
	}
	{
	  addd,0	%dr7, 0x1, %dr9
	}
	{
	  nop 1
	}
	{
	  return	%ctpr3
	  std,2	%dr0, 0x8, %dr9
	}
	{
	  nop 3
	}
	{
	  ct	%ctpr3
	}
	.size	cs_empty_st, .-cs_empty_st

! void cs_tagadd (long val, long tag, long *out): puttagd val, tag into a
! window register, out[0] = gettagd of it; then a non-speculative addd of
! it and 0: out[1] = the result.  Run in a child: puttagd or the addd may
! trap (si_addr tells which).
	.global	cs_tagadd
	.type	cs_tagadd, #function
	.align	8
cs_tagadd:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  puttagd,5	%dr0, %r1, %dr3
	}
	{
	  nop 2
	}
	{
	  gettagd,2	%dr3, %r4
	}
	{
	  nop 1
	}
	{
	  stw,2	%dr2, 0x0, %r4
	}
	{
	  addd,0	%dr3, 0x0, %dr5
	}
	{
	  nop 1
	}
	{
	  std,2	%dr2, 0x8, %dr5
	}
	{
	  ct	%ctpr3
	}
	.size	cs_tagadd, .-cs_tagadd

! void cs_dv_and (long *out, long mask), cs_dv_or: a diagnostic value
! (ldd,sm mas=0x3 from 0), then andd,sm (ord,sm) of it and MASK:
! out[0] = the result's tag, out[1] = its bits (puttagd 0); then a
! non-speculative addd of it into out[2].  Run in a child: the addd
! (+0x68) may trap.
	.macro	dvop name, op
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,1	0x0, 0x0, %dr2
	}
	{
	  ldd,0,sm	%dr2, 0x0, %dr7, mas=0x3
	}
	{
	  nop 3
	}
	{
	  \op,0,sm	%dr7, %dr1, %dr8
	}
	{
	  nop 1
	}
	{
	  gettagd,2	%dr8, %r4
	  puttagd,5	%dr8, 0x0, %dr5
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr0, 0x0, %r4
	  std,5	%dr0, 0x8, %dr5
	}
	{
	  addd,0	%dr8, 0x0, %dr6
	}
	{
	  nop 1
	}
	{
	  std,2	%dr0, 0x10, %dr6
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	dvop	cs_dv_and, andd
	dvop	cs_dv_or, ord

! Signal handlers that record %g16..%g31 as the handler finds them
! (cs_gval[16] with the tags cleared, cs_gtag[16]) before any compiled
! code can write them, then call cs_handler_c (sig, si, uc).
! cs_ghandler_add first reads %dg16 non-speculatively (addd) and stores
! the result in cs_gns[0] (tag cleared) and its tag in cs_gns[3];
! cs_ghandler_st first stores %dg16 with a non-speculative std to
! cs_gns[1], loads it back with the fill MAS (mas=0x4, which returns the
! memory tag) and puts that tag in cs_gns[2].  ctxsig.c runs those two in a child with SIGILL, SIGSEGV
! and SIGBUS caught.

	.macro	rdg n, k
	{
	  gettagd,2	%dg\n, %r6
	  puttagd,5	%dg\n, 0x0, %dr7
	}
	{
	  nop 2
	}
	{
	  stw,2	%dr5, 4 * \k, %r6
	  std,5	%dr4, 8 * \k, %dr7
	}
	.endm

	.macro	gbody marks=0
	{
	  addd,0	0x0, [ _f64,_lts0 cs_gval ], %dr4
	}
	{
	  addd,0	0x0, [ _f64,_lts0 cs_gtag ], %dr5
	}
	rdg	16, 0
	rdg	17, 1
	rdg	18, 2
	rdg	19, 3
	rdg	20, 4
	rdg	21, 5
	rdg	22, 6
	rdg	23, 7
	rdg	24, 8
	rdg	25, 9
	rdg	26, 10
	rdg	27, 11
	rdg	28, 12
	rdg	29, 13
	rdg	30, 14
	rdg	31, 15
	{
	  disp	%ctpr1, cs_handler_c
	}
	{
	  addd,0	0x0, %dr0, %db[0]
	  addd,1	0x0, %dr1, %db[1]
	  addd,2	0x0, %dr2, %db[2]
	}
	{
	  nop 4
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	.if	\marks
	{
	  addd,0	0x0, 0xdd16, %dg16
	  addd,1	0x0, 0xdd17, %dg17
	  addd,3	0x0, 0xdd18, %dg18
	  addd,4	0x0, 0xdd19, %dg19
	}
	.endif
	{
	  return	%ctpr3
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.endm

	.global	cs_ghandler
	.type	cs_ghandler, #function
	.align	8
cs_ghandler:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	}
	gbody
	.size	cs_ghandler, .-cs_ghandler

! cs_ghandler_w: the same, then writes 0xdd16..0xdd19 to %g16..%g19
! before it returns, to tell "restored by sigreturn" from "left as the
! handler had them".
	.global	cs_ghandler_w
	.type	cs_ghandler_w, #function
	.align	8
cs_ghandler_w:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	}
	gbody	1
	.size	cs_ghandler_w, .-cs_ghandler_w

	.global	cs_ghandler_add
	.type	cs_ghandler_add, #function
	.align	8
cs_ghandler_add:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	}
	{
	  addd,1	0x0, [ _f64,_lts0 cs_gns ], %dr3
	}
	{
	  addd,0	%dg16, 0x0, %dr6
	}
	{
	  nop 1
	}
	{
	  gettagd,2	%dr6, %r7
	  puttagd,5	%dr6, 0x0, %dr5
	}
	{
	  nop 2
	}
	{
	  std,2	%dr3, 0x0, %dr5
	  stw,5	%dr3, 0x18, %r7
	}
	gbody
	.size	cs_ghandler_add, .-cs_ghandler_add

	.global	cs_ghandler_st
	.type	cs_ghandler_st, #function
	.align	8
cs_ghandler_st:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	}
	{
	  addd,1	0x0, [ _f64,_lts0 cs_gns ], %dr3
	}
	{
	  nop 1
	}
	{
	  std,2	%dr3, 0x8, %dg16
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr3, 0x8, %dr6, mas=0x4
	}
	{
	  nop 3
	}
	{
	  gettagd,2	%dr6, %r7
	}
	{
	  nop 1
	}
	{
	  stw,2	%dr3, 0x10, %r7
	}
	gbody
	.size	cs_ghandler_st, .-cs_ghandler_st
