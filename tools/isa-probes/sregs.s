! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for sregs.c (hand-written): reads and writes of the user-mode
! state registers %pfpfr, %fpcr, %fpsr, %upsr, %idr, %clkr, %core_mode
! and %ip, FP operations under a chosen FP environment, the fxdivt*
! operations (also with %fpsr preset, and their ,sm forms), %clkr
! brackets, and latency chains and edges by the gap method.  Build and
! run lines are in sregs.c.
	.ignore	ld_st_style
	.text

! void sr_<name> (u64 out[2]): preset %dr2 to 0x1234567887654321, run
! INSN (which writes %r2 or %dr2) under setwd dbl = DBL; out[0] = %dr2
! with its tags cleared, out[1] = its tags (gettagd).
	.macro RDW name, dbl, insn:vararg
	.global	sr_\name
	.type	sr_\name, #function
	.align	8
sr_\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = \dbl
	  return	%ctpr3
	}
	{
	  addd,1	0x0, _f64,_lts0 0x1234567887654321, %dr2
	}
	{
	  \insn
	}
	{
	  nop 2
	}
	{
	  gettagd,2	%dr2, %r3
	  puttagd,5	%dr2, 0x0, %dr4
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x0, %dr4
	  std,5	%dr0, 0x8, %dr3
	}
	{
	  ct	%ctpr3
	}
	.size	sr_\name, .-sr_\name
	.endm

	RDW	rrs_pfpfr_0, 0x0, rrs,0 %pfpfr, %r2
	RDW	rrs_pfpfr_1, 0x1, rrs,0 %pfpfr, %r2
	RDW	rrd_pfpfr, 0x0, rrd,0 %pfpfr, %dr2
	RDW	rrs_fpcr_0, 0x0, rrs,0 %fpcr, %r2
	RDW	rrs_fpcr_1, 0x1, rrs,0 %fpcr, %r2
	RDW	rrd_fpcr, 0x0, rrd,0 %fpcr, %dr2
	RDW	rrs_fpsr_0, 0x0, rrs,0 %fpsr, %r2
	RDW	rrs_fpsr_1, 0x1, rrs,0 %fpsr, %r2
	RDW	rrd_fpsr, 0x0, rrd,0 %fpsr, %dr2
	RDW	rrs_upsr_0, 0x0, rrs,0 %upsr, %r2
	RDW	rrs_upsr_1, 0x1, rrs,0 %upsr, %r2
	RDW	rrd_upsr, 0x0, rrd,0 %upsr, %dr2
	RDW	rrs_idr_0, 0x0, rrs,0 %idr, %r2
	RDW	rrs_idr_1, 0x1, rrs,0 %idr, %r2
	RDW	rrd_idr, 0x0, rrd,0 %idr, %dr2
	RDW	rrs_clkr_0, 0x0, rrs,0 %clkr, %r2
	RDW	rrs_clkr_1, 0x1, rrs,0 %clkr, %r2
	RDW	rrd_clkr, 0x0, rrd,0 %clkr, %dr2
	RDW	rrs_ip_0, 0x0, rrs,0 %ip, %r2
	RDW	rrs_ip_1, 0x1, rrs,0 %ip, %r2
	RDW	rrd_ip, 0x0, rrd,0 %ip, %dr2
	RDW	rrs_core_mode_0, 0x0, rrs,0 %core_mode, %r2
	RDW	rrs_core_mode_1, 0x1, rrs,0 %core_mode, %r2
	RDW	rrd_core_mode, 0x0, rrd,0 %core_mode, %dr2
	RDW	rrs_core_mode_sm_0, 0x0, rrs,0,sm %core_mode, %r2
	RDW	rrs_core_mode_sm_1, 0x1, rrs,0,sm %core_mode, %r2
	RDW	rrd_core_mode_sm, 0x0, rrd,0,sm %core_mode, %dr2

! void sr_w_<name> (u64 val, u64 out[2]): save REG (rrs), write val with
! INSN, read REG back with rrd (out[0]) and rrs into a register preset
! to 0x1234567887654321 (out[1]), restore REG.
	.macro WR name, reg, insn:vararg
	.global	sr_w_\name
	.type	sr_w_\name, #function
	.align	8
sr_w_\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrs,0	\reg, %r8
	}
	{
	  addd,1	0x0, _f64,_lts0 0x1234567887654321, %dr2
	}
	{
	  \insn
	}
	{
	  nop 7
	}
	{
	  rrd,0	\reg, %dr3
	}
	{
	  rrs,0	\reg, %r2
	}
	{
	  nop 2
	}
	{
	  rws,0	%r8, \reg
	}
	{
	  std,2	%dr1, 0x0, %dr3
	  std,5	%dr1, 0x8, %dr2
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sr_w_\name, .-sr_w_\name
	.endm

	WR	rws_pfpfr, %pfpfr, rws,0 %r0, %pfpfr
	WR	rwd_pfpfr, %pfpfr, rwd,0 %dr0, %pfpfr
	WR	rws_fpcr, %fpcr, rws,0 %r0, %fpcr
	WR	rwd_fpcr, %fpcr, rwd,0 %dr0, %fpcr
	WR	rws_fpsr, %fpsr, rws,0 %r0, %fpsr
	WR	rwd_fpsr, %fpsr, rwd,0 %dr0, %fpsr

! void sr_rdfp (u64 out[3]): %pfpfr, %fpcr, %fpsr (for signal handlers).
	.global	sr_rdfp
	.type	sr_rdfp, #function
	.align	8
sr_rdfp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	}
	{
	  rrs,0	%pfpfr, %r2
	}
	{
	  rrs,0	%fpcr, %r3
	}
	{
	  rrs,0	%fpsr, %r4
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x0, %dr2
	  std,5	%dr0, 0x8, %dr3
	}
	{
	  std,2	%dr0, 0x10, %dr4
	}
	{
	  ct	%ctpr3
	}
	.size	sr_rdfp, .-sr_rdfp

! void sr_f_<name> (const u64 in[4], u64 out[6], u64 pfpfr, u64 fpcr,
! u64 fpsr): run INSN with %pfpfr, %fpcr and %fpsr set to the arguments
! and restore them afterwards.  Operands: doubles a = in[0] (%dr5),
! b = in[2] (%dr7); 80-bit x = (in[0], in[1]) (%dr10), y = (in[2],
! in[3]) (%dr11).  INSN writes %dr9 (preset 0x5555555555555555).  out:
! result with tags cleared, its tags, %pfpfr, %fpsr, %fpcr after, the
! result's 16-bit extended part (movfi,sm, tags cleared).
	.macro FPH name, insn:vararg
	.global	sr_f_\name
	.type	sr_f_\name, #function
	.align	8
sr_f_\name:
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrs,0	%pfpfr, %r12
	}
	{
	  rrs,0	%fpcr, %r13
	}
	{
	  rrs,0	%fpsr, %r14
	}
	{
	  ldd,0	%dr0, 0x0, %dr5
	  ldd,2	%dr0, 0x8, %dr6
	  ldd,3	%dr0, 0x10, %dr7
	  ldd,5	%dr0, 0x18, %dr8
	}
	{
	  addd,1	0x0, _f64,_lts0 0x5555555555555555, %dr9
	}
	{
	  nop 2
	}
	{
	  movif,0	%dr5, %r6, %dr10
	  movif,3	%dr7, %r8, %dr11
	}
	{
	  rws,0	%r2, %pfpfr
	}
	{
	  rws,0	%r3, %fpcr
	}
	{
	  rws,0	%r4, %fpsr
	}
	{
	  nop 7
	}
	{
	  \insn
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
	  rrs,0	%pfpfr, %r15
	}
	{
	  rrs,0	%fpsr, %r16
	}
	{
	  rrs,0	%fpcr, %r17
	}
	{
	  gettagd,2	%dr9, %r18
	  puttagd,5	%dr9, 0x0, %dr19
	  movfi,0,sm	%dr9, %r20
	}
	{
	  nop 2
	}
	{
	  puttagd,5	%dr20, 0x0, %dr21
	}
	{
	  rws,0	%r12, %pfpfr
	}
	{
	  rws,0	%r13, %fpcr
	}
	{
	  rws,0	%r14, %fpsr
	}
	{
	  std,2	%dr1, 0x0, %dr19
	  std,5	%dr1, 0x8, %dr18
	}
	{
	  std,2	%dr1, 0x10, %dr15
	  std,5	%dr1, 0x18, %dr16
	}
	{
	  std,2	%dr1, 0x20, %dr17
	  std,5	%dr1, 0x28, %dr21
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sr_f_\name, .-sr_f_\name
	.endm

	FPH	faddd, faddd,0 %dr5, %dr7, %dr9
	FPH	faddd_sm, faddd,0,sm %dr5, %dr7, %dr9
	FPH	fmuld, fmuld,0 %dr5, %dr7, %dr9
	FPH	fmuld_sm, fmuld,0,sm %dr5, %dr7, %dr9
	FPH	fdivd, fdivd,5 %dr5, %dr7, %dr9
	FPH	fdivd_sm, fdivd,5,sm %dr5, %dr7, %dr9
	FPH	fxaddxx, fxaddxx,0 %dr10, %dr11, %dr9
	FPH	fxaddxx_sm, fxaddxx,0,sm %dr10, %dr11, %dr9
	FPH	fxmulxx, fxmulxx,0 %dr10, %dr11, %dr9
	FPH	fxmulxx_sm, fxmulxx,0,sm %dr10, %dr11, %dr9
	FPH	fxdivxx, fxdivxx,5 %dr10, %dr11, %dr9
	FPH	fxdivxx_sm, fxdivxx,5,sm %dr10, %dr11, %dr9
	FPH	fxdivtsx, fxdivtsx,5 %dr5, %r11, %dr9
	FPH	fxdivtsx_sm, fxdivtsx,5,sm %dr5, %r11, %dr9
	FPH	fxcmpudxf, fxcmpudxf,0 %dr10, %dr11, %r9
	FPH	fxcmpodxf, fxcmpodxf,0 %dr10, %dr11, %r9
	FPH	pfadds, pfadds,0 %dr5, %dr7, %dr9
	FPH	pfadds_sm, pfadds,0,sm %dr5, %dr7, %dr9
	FPH	fdtoid, fdtoid,0 %dr5, %dr9
	FPH	fdtoid_sm, fdtoid,0,sm %dr5, %dr9

! For the "c1" cases: more 80-bit operations, and lcc's square root
! chain (SQRTC closes and reopens FPH's braces).
	FPH	fxsubxx, fxsubxx,0 %dr10, %dr11, %dr9
	FPH	fxdivxd, fxdivxd,5 %dr10, %dr11, %dr9
	FPH	fxtofd, fxtofd,0 %dr10, %dr9
	FPH	fxtoid, fxtoid,0 %dr10, %dr9
	.macro SQRTC
	  fxsqrtixx,5	%dr10, %dr22
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  fxsqrtuxx,5	%dr10, %dr22, %dr22
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  fxsqrttxx,5	%dr10, %dr22, %dr9
	.endm
	FPH	fxsqrt, SQRTC

! A speculative operation, then an ordinary one that reads its result
! (which traps if it is diagnostic): what %pfpfr and %fpsr hold then.
	.macro USED op, ch, a, b
	  \op,\ch,sm	\a, \b, %dr22
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  \op,\ch	%dr22, %dr22, %dr9
	.endm
	.macro FADDUSE
	USED	faddd, 0, %dr5, %dr7
	.endm
	.macro FXADDUSE
	USED	fxaddxx, 0, %dr10, %dr11
	.endm
	FPH	faddd_use, FADDUSE
	FPH	fxaddxx_use, FXADDUSE
	FPH	fxdivtss, fxdivtss,5 %dr5, %r11, %r9

! void sr_dt_<name> (const u64 in[4], u64 out[3], u64 fpcr): fxdivt*
! with src1 = in[0] (a single or double in its low bits, %dr5) and
! src2 the 80-bit (in[2], in[3]) (%dr11), under %fpcr = fpcr with %fpsr
! cleared.  out: the result's 64 bits, its 16-bit extended part (movfi),
! %fpsr after.
	.macro DT name, insn:vararg
	.global	sr_dt_\name
	.type	sr_dt_\name, #function
	.align	8
sr_dt_\name:
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrs,0	%fpcr, %r12
	}
	{
	  rrs,0	%fpsr, %r13
	}
	{
	  ldd,0	%dr0, 0x0, %dr5
	  ldd,2	%dr0, 0x10, %dr7
	  ldd,3	%dr0, 0x18, %dr8
	}
	{
	  addd,1	0x0, _f64,_lts0 0x5555555555555555, %dr9
	}
	{
	  nop 2
	}
	{
	  movif,0	%dr7, %r8, %dr11
	}
	{
	  rws,0	%r2, %fpcr
	}
	{
	  rws,0	0x0, %fpsr
	}
	{
	  nop 7
	}
	{
	  \insn
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
	  rrs,0	%fpsr, %r16
	}
	{
	  movfi,0	%dr9, %r20
	}
	{
	  nop 2
	}
	{
	  rws,0	%r12, %fpcr
	}
	{
	  rws,0	%r13, %fpsr
	}
	{
	  std,2	%dr1, 0x0, %dr9
	  std,5	%dr1, 0x8, %dr20
	}
	{
	  std,2	%dr1, 0x10, %dr16
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sr_dt_\name, .-sr_dt_\name
	.endm

	DT	fxdivtss, fxdivtss,5 %dr5, %r11, %r9
	DT	fxdivtdd, fxdivtdd,5 %dr5, %r11, %dr9
	DT	fxdivtsx, fxdivtsx,5 %dr5, %r11, %dr9
	DT	fxdivtdx, fxdivtdx,5 %dr5, %r11, %dr9

! long sr_lat_<name> (void): %clkr cycles from before N dependent
! operations INSN to after an integer consumer of the last result RES.
! Registers: %dr4 1.0f, %dr5 1.0, %dr6 80-bit 1.5, %dr7 80-bit
! 1.00000000000000001388, %dr8 1.5f, %dr9 1.5.
	.macro LAT name, n, res, insn:vararg
	.global	sr_lat_\name
	.type	sr_lat_\name, #function
	.align	8
sr_lat_\name:
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, _f64,_lts0 0xc000000000000000, %dr10
	}
	{
	  addd,0	0x0, _f64,_lts0 0x8000000000000100, %dr11
	}
	{
	  addd,0	0x0, _f64,_lts0 0x3ff8000000000000, %dr9
	}
	{
	  addd,0	0x0, _f64,_lts0 0x3ff0000000000000, %dr5
	}
	{
	  addd,0	0x0, _f32s,_lts0 0x3fc00000, %dr8
	}
	{
	  addd,0	0x0, _f32s,_lts0 0x3f800000, %dr4
	}
	{
	  addd,0	0x0, _f32s,_lts0 0x3fff, %dr12
	}
	{
	  nop 2
	}
	{
	  movif,0	%dr10, %r12, %dr6
	  movif,3	%dr11, %r12, %dr7
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  rrd,0	%clkr, %dr20
	}
	.rept	\n
	{
	  \insn
	}
	.endr
	{
	  addd,0	\res, 0x0, %dr13
	}
	{
	  rrd,0	%clkr, %dr21
	}
	{
	  subd,0	%dr21, %dr20, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sr_lat_\name, .-sr_lat_\name
	.endm

	! LAT2: as LAT with two instructions a step.
	.macro LAT2 name, n, res, insn1, insn2
	.global	sr_lat_\name
	.type	sr_lat_\name, #function
	.align	8
sr_lat_\name:
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, _f64,_lts0 0x8000000000000100, %dr11
	}
	{
	  addd,0	0x0, _f64,_lts0 0x3ff8000000000000, %dr9
	}
	{
	  addd,0	0x0, _f64,_lts0 0x3ff0000000000000, %dr5
	}
	{
	  addd,0	0x0, _f32s,_lts0 0x3fff, %dr12
	}
	{
	  nop 2
	}
	{
	  movif,3	%dr11, %r12, %dr7
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  rrd,0	%clkr, %dr20
	}
	.rept	\n
	{
	  \insn1
	}
	{
	  \insn2
	}
	.endr
	{
	  addd,0	\res, 0x0, %dr13
	}
	{
	  rrd,0	%clkr, %dr21
	}
	{
	  subd,0	%dr21, %dr20, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sr_lat_\name, .-sr_lat_\name
	.endm

	LAT2	fadd_fxadddd16, 16, %dr9, "faddd,0 %dr9, %dr5, %dr9", "fxadddd,0 %dr7, %dr9, %dr9"
	LAT2	fadd_fxadddd48, 48, %dr9, "faddd,0 %dr9, %dr5, %dr9", "fxadddd,0 %dr7, %dr9, %dr9"
	LAT2	fadd_fadd16, 16, %dr9, "faddd,0 %dr9, %dr5, %dr9", "faddd,0 %dr9, %dr5, %dr9"
	LAT2	fadd_fadd48, 48, %dr9, "faddd,0 %dr9, %dr5, %dr9", "faddd,0 %dr9, %dr5, %dr9"
	LAT2	fxadddx_fxtofd16, 16, %dr9, "fxadddx,0 %dr7, %dr9, %dr10", "fxtofd,0 %dr10, %dr9"
	LAT2	fxadddx_fxtofd48, 48, %dr9, "fxadddx,0 %dr7, %dr9, %dr10", "fxtofd,0 %dr10, %dr9"
	LAT	tsx16, 16, %dr6, fxdivtsx,5 %dr4, %r6, %dr6
	LAT	tsx48, 48, %dr6, fxdivtsx,5 %dr4, %r6, %dr6
	LAT	tdx16, 16, %dr6, fxdivtdx,5 %dr5, %r6, %dr6
	LAT	tdx48, 48, %dr6, fxdivtdx,5 %dr5, %r6, %dr6
	LAT	tss16, 16, %dr8, fxdivtss,5 %dr8, %r7, %r8
	LAT	tss48, 48, %dr8, fxdivtss,5 %dr8, %r7, %r8
	LAT	tdd16, 16, %dr9, fxdivtdd,5 %dr9, %r7, %dr9
	LAT	tdd48, 48, %dr9, fxdivtdd,5 %dr9, %r7, %dr9
	LAT	xx16, 16, %dr6, fxdivxx,5 %dr7, %dr6, %dr6
	LAT	xx48, 48, %dr6, fxdivxx,5 %dr7, %dr6, %dr6
	LAT	fdivd16, 16, %dr9, fdivd,5 %dr5, %dr9, %dr9
	LAT	fdivd48, 48, %dr9, fdivd,5 %dr5, %dr9, %dr9
	LAT	ss16, 16, %dr8, fxdivss,5 %dr7, %r8, %r8
	LAT	ss48, 48, %dr8, fxdivss,5 %dr7, %r8, %r8
	LAT	dd16, 16, %dr9, fxdivdd,5 %dr7, %dr9, %dr9
	LAT	dd48, 48, %dr9, fxdivdd,5 %dr7, %dr9, %dr9
	LAT	addss16, 16, %dr8, fxaddss,0 %dr7, %r8, %r8
	LAT	addss48, 48, %dr8, fxaddss,0 %dr7, %r8, %r8
	LAT	adddd16, 16, %dr9, fxadddd,0 %dr7, %dr9, %dr9
	LAT	adddd48, 48, %dr9, fxadddd,0 %dr7, %dr9, %dr9
	LAT	idr16, 16, %dr2, rrd,0 %idr, %dr2
	LAT	idr48, 48, %dr2, rrd,0 %idr, %dr2
	LAT	clkr16, 16, %dr2, rrd,0 %clkr, %dr2
	LAT	clkr48, 48, %dr2, rrd,0 %clkr, %dr2

! %clkr brackets.  long sr_ck_<name> (u64 *p): the difference of two
! rrd %clkr with BODY between them (BODY may load from p, or compute
! %dr7 from %dr5 = 1.0 and %dr6 = 3.0); %dr7 is stored to p[1] after
! the second read.
	.macro CK name, body:vararg
	.global	sr_ck_\name
	.type	sr_ck_\name, #function
	.align	8
sr_ck_\name:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, _f64,_lts0 0x3ff0000000000000, %dr5
	}
	{
	  addd,0	0x0, _f64,_lts0 0x4008000000000000, %dr6
	  addd,1	0x0, 0x0, %dr7
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  rrd,0	%clkr, %dr20
	}
	\body
	{
	  rrd,0	%clkr, %dr21
	}
	{
	  subd,0	%dr21, %dr20, %dr22
	}
	{
	  std,2	%dr0, 0x8, %dr7
	}
	{
	  addd,0	%dr22, 0x0, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sr_ck_\name, .-sr_ck_\name
	.endm

	.macro FDIV1
	{
	  fdivd,5	%dr5, %dr6, %dr7
	}
	.endm
	.macro FDIV8
	{
	  fdivd,5	%dr5, %dr6, %dr7
	}
	.rept	7
	{
	  fdivd,5	%dr5, %dr7, %dr7
	}
	.endr
	.endm
	.macro USE7
	{
	  addd,0	%dr7, 0x0, %dr8
	}
	.endm
	.macro LD1
	{
	  ldd,0	%dr0, 0x0, %dr7
	}
	.endm
	.macro NOTHING
	.endm
	.macro FDIV1U
	FDIV1
	USE7
	.endm
	.macro FDIV8U
	FDIV8
	USE7
	.endm
	.macro LD1U
	LD1
	USE7
	.endm
	.macro FILL
	{
	  addd,1	0x0, 0x0, %dr9
	}
	.endm
	.macro FDIV1U1
	FDIV1U
	FILL
	.endm
	.macro FDIV1U2
	FDIV1U
	FILL
	FILL
	.endm
	.macro FDIV1U3
	FDIV1U
	FILL
	FILL
	FILL
	.endm
	.macro FDIV1N7
	FDIV1U
	{
	  nop 7
	}
	.endm

	CK	back, NOTHING
	CK	fdiv, FDIV1
	CK	fdiv_use, FDIV1U
	CK	fdiv8, FDIV8
	CK	fdiv8_use, FDIV8U
	CK	fdiv_use_1, FDIV1U1
	CK	fdiv_use_2, FDIV1U2
	CK	fdiv_use_3, FDIV1U3
	CK	fdiv_use_nop7, FDIV1N7
	CK	ld, LD1
	CK	ld_use, LD1U
	! the addd with nop 1 or nop 2 in its own instruction (1 instruction,
	! 2 or 3 cycles before the read), or followed by an empty nop 1
	! instruction (2 instructions, 3 cycles)
	.macro FDIV1UN1
	FDIV1
	{
	  addd,0	%dr7, 0x0, %dr8
	  nop 1
	}
	.endm
	.macro FDIV1UN2
	FDIV1
	{
	  addd,0	%dr7, 0x0, %dr8
	  nop 2
	}
	.endm
	.macro FDIV1UE1
	FDIV1U
	{
	  nop 1
	}
	.endm
	CK	fdiv_use_n1, FDIV1UN1
	CK	fdiv_use_n2, FDIV1UN2
	CK	fdiv_use_e1, FDIV1UE1

! Read latencies: a read into %dr7 (or into %dr10 for the base case,
! which leaves %dr7 = 0), an addd of %dr7 and two more instructions,
! bracketed by rrd %clkr as above.
	.macro RDU insn:vararg
	{
	  \insn
	}
	USE7
	FILL
	FILL
	.endm
	.macro RIDR
	RDU	rrd,0 %idr, %dr7
	.endm
	.macro RIDRB
	RDU	rrd,0 %idr, %dr10
	.endm
	.macro RCLK
	RDU	rrd,0 %clkr, %dr7
	.endm
	.macro RCLKB
	RDU	rrd,0 %clkr, %dr10
	.endm
	.macro RPF
	RDU	rrs,0 %pfpfr, %r7
	.endm
	.macro RPFB
	RDU	rrs,0 %pfpfr, %r10
	.endm
	.macro RCM
	RDU	rrs,0 %core_mode, %r7
	.endm
	.macro RCMB
	RDU	rrs,0 %core_mode, %r10
	.endm
	CK	rd_idr, RIDR
	CK	rd_idr_base, RIDRB
	CK	rd_clkr, RCLK
	CK	rd_clkr_base, RCLKB
	CK	rd_pfpfr, RPF
	CK	rd_pfpfr_base, RPFB
	CK	rd_core_mode, RCM
	CK	rd_core_mode_base, RCMB
	.macro R_idr_1
	{
	  rrd,0 %idr, %dr7
	  nop 1
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_idr_gap2, R_idr_1
	.macro R_idr_2
	{
	  rrd,0 %idr, %dr7
	  nop 2
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_idr_gap3, R_idr_2
	.macro R_idr_3
	{
	  rrd,0 %idr, %dr7
	  nop 3
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_idr_gap4, R_idr_3
	.macro R_idr_4
	{
	  rrd,0 %idr, %dr7
	  nop 4
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_idr_gap5, R_idr_4
	.macro R_clkr_1
	{
	  rrd,0 %clkr, %dr7
	  nop 1
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_clkr_gap2, R_clkr_1
	.macro R_clkr_2
	{
	  rrd,0 %clkr, %dr7
	  nop 2
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_clkr_gap3, R_clkr_2
	.macro R_clkr_3
	{
	  rrd,0 %clkr, %dr7
	  nop 3
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_clkr_gap4, R_clkr_3
	.macro R_clkr_4
	{
	  rrd,0 %clkr, %dr7
	  nop 4
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_clkr_gap5, R_clkr_4
	.macro R_core_mode_1
	{
	  rrs,0 %core_mode, %r7
	  nop 1
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_core_mode_gap2, R_core_mode_1
	.macro R_core_mode_2
	{
	  rrs,0 %core_mode, %r7
	  nop 2
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_core_mode_gap3, R_core_mode_2
	.macro R_core_mode_3
	{
	  rrs,0 %core_mode, %r7
	  nop 3
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_core_mode_gap4, R_core_mode_3
	.macro R_core_mode_4
	{
	  rrs,0 %core_mode, %r7
	  nop 4
	}
	USE7
	FILL
	FILL
	.endm
	CK	rd_core_mode_gap5, R_core_mode_4

! void sr_es (u64 out[3]): %fpsr read after writing %fpcr = 0x340 (all
! exceptions unmasked) and %fpsr = 0x3f, after masking them again
! (%fpcr = 0x37f), and after unmasking only the precision exception
! (%fpcr = 0x35f); no FP operation runs in between.
	.global	sr_es
	.type	sr_es, #function
	.align	8
sr_es:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	}
	{
	  rrs,0	%fpcr, %r8
	}
	{
	  rrs,0	%fpsr, %r9
	}
	{
	  rws,0	_f32s,_lts0 0x340, %fpcr
	}
	{
	  rws,0	0x3f, %fpsr
	}
	{
	  nop 7
	}
	{
	  rrs,0	%fpsr, %r2
	}
	{
	  rws,0	_f32s,_lts0 0x37f, %fpcr
	}
	{
	  nop 7
	}
	{
	  rrs,0	%fpsr, %r3
	}
	{
	  rws,0	_f32s,_lts0 0x35f, %fpcr
	}
	{
	  nop 7
	}
	{
	  rrs,0	%fpsr, %r4
	}
	{
	  rws,0	%r8, %fpcr
	}
	{
	  rws,0	%r9, %fpsr
	}
	{
	  std,2	%dr0, 0x0, %dr2
	  std,5	%dr0, 0x8, %dr3
	}
	{
	  std,2	%dr0, 0x10, %dr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sr_es, .-sr_es

! long sr_rdidr (void), long sr_rdclkr (void), void sr_empty (void).
	.global	sr_rdidr
	.type	sr_rdidr, #function
	.align	8
sr_rdidr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrd,0	%idr, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sr_rdidr, .-sr_rdidr

	.global	sr_rdclkr
	.type	sr_rdclkr, #function
	.align	8
sr_rdclkr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrd,0	%clkr, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sr_rdclkr, .-sr_rdclkr

! void *sr_tp (void): the thread pointer (%g13).
	.global	sr_tp
	.type	sr_tp, #function
	.align	8
sr_tp:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	%dg13, 0x0, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sr_tp, .-sr_tp

	.global	sr_empty
	.type	sr_empty, #function
	.align	8
sr_empty:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sr_empty, .-sr_empty

! Scheduled latencies by the gap method (spec/isa/timing.md, "Method").
! NOPS G: a nop that makes the next instruction G cycles later (G 1-8);
! NOPZ G: the same, but always an operation (G 1 gives "nop").
	.macro NOPS g
	.if (\g) == 2
	  nop 1
	.elseif (\g) == 3
	  nop 2
	.elseif (\g) == 4
	  nop 3
	.elseif (\g) == 5
	  nop 4
	.elseif (\g) == 6
	  nop 5
	.elseif (\g) == 7
	  nop 6
	.elseif (\g) == 8
	  nop 7
	.endif
	.endm
	.macro NOPZ g
	.if (\g) == 1
	  nop
	.else
	  NOPS \g
	.endif
	.endm
! PAD G: G cycles of empty instructions.
	.macro PAD g
	.if (\g) <= 8
	{
	  NOPZ \g
	}
	.else
	{
	  nop 7
	}
	PAD (\g)-8
	.endif
	.endm
! STEP G, INSN: INSN, and the next instruction G cycles later.
	.macro STEP g, insn:vararg
	.if (\g) <= 8
	{
	  \insn
	  NOPS \g
	}
	.else
	{
	  \insn
	  nop 7
	}
	PAD (\g)-8
	.endif
	.endm

! LATSET: the registers of LAT (above), and %dr10 80-bit 1.5.
	.macro LATSET
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, _f64,_lts0 0xc000000000000000, %dr10
	}
	{
	  addd,0	0x0, _f64,_lts0 0x8000000000000100, %dr11
	}
	{
	  addd,0	0x0, _f64,_lts0 0x3ff8000000000000, %dr9
	}
	{
	  addd,0	0x0, _f64,_lts0 0x3ff0000000000000, %dr5
	}
	{
	  addd,0	0x0, _f32s,_lts0 0x3fc00000, %dr8
	}
	{
	  addd,0	0x0, _f32s,_lts0 0x3f800000, %dr4
	}
	{
	  addd,0	0x0, _f32s,_lts0 0x3fff, %dr12
	}
	{
	  nop 2
	}
	{
	  movif,0	%dr10, %r12, %dr6
	  movif,3	%dr11, %r12, %dr7
	}
	{
	  movif,0	%dr10, %r12, %dr10
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  rrd,0	%clkr, %dr20
	}
	.endm
	.macro LATEND res
	{
	  addd,0	\res, 0x0, %dr13
	}
	{
	  rrd,0	%clkr, %dr21
	}
	{
	  subd,0	%dr21, %dr20, %dr0
	}
	{
	  ct	%ctpr3
	}
	.endm

! long sr_lg_<name>_<n> (void), n = 16 and 48: n steps of INSN, each G
! cycles after the previous one (a chain through RES).
	.macro LATGF name, n, g, res, insn:vararg
	.global	sr_lg_\name\()_\n
	.type	sr_lg_\name\()_\n, #function
	.align	8
sr_lg_\name\()_\n:
	LATSET
	.rept	\n
	STEP	\g, \insn
	.endr
	LATEND	\res
	.size	sr_lg_\name\()_\n, .-sr_lg_\name\()_\n
	.endm
	.macro LATG name, g, res, insn:vararg
	LATGF	\name, 16, \g, \res, \insn
	LATGF	\name, 48, \g, \res, \insn
	.endm

	.irp g, 13,14,15,16,17,18,19,20
	LATG	tsx_\g, \g, %dr6, fxdivtsx,5 %dr4, %r6, %dr6
	LATG	tdx_\g, \g, %dr6, fxdivtdx,5 %dr5, %r6, %dr6
	LATG	tss_\g, \g, %dr8, fxdivtss,5 %dr8, %r7, %r8
	LATG	tdd_\g, \g, %dr9, fxdivtdd,5 %dr9, %r7, %dr9
	LATG	xx_\g, \g, %dr6, fxdivxx,5 %dr7, %dr6, %dr6
	LATG	dd_\g, \g, %dr9, fxdivdd,5 %dr7, %dr9, %dr9
	LATG	fdivd_\g, \g, %dr9, fdivd,5 %dr5, %dr9, %dr9
	.endr

! long sr_le_<name>_<n> (void): n steps of INSNA, G cycles, INSNB, 12
! cycles: the latency of the edge A -> B is the smallest G at which a
! step costs G + 12.
	.macro LATEF name, n, g, res, insna, insnb
	.global	sr_le_\name\()_\n
	.type	sr_le_\name\()_\n, #function
	.align	8
sr_le_\name\()_\n:
	LATSET
	.rept	\n
	STEP	\g, \insna
	STEP	12, \insnb
	.endr
	LATEND	\res
	.size	sr_le_\name\()_\n, .-sr_le_\name\()_\n
	.endm
	.macro LATE name, g, res, insna, insnb
	LATEF	\name, 16, \g, \res, "\insna", "\insnb"
	LATEF	\name, 48, \g, \res, "\insna", "\insnb"
	.endm

	! xd: fxadddd (80-bit + double -> double); f: faddd; xx: fxadddx
	! (80-bit + double -> 80-bit); fd: fxtofd.
	.irp g, 1,2,3,4,5,6,7,8,9
	LATE	xd_f_\g, \g, %dr9, "fxadddd,0 %dr7, %dr9, %dr10", "faddd,0 %dr10, %dr5, %dr9"
	LATE	f_xd_\g, \g, %dr9, "faddd,0 %dr9, %dr5, %dr10", "fxadddd,0 %dr7, %dr10, %dr9"
	LATE	xd_xd_\g, \g, %dr9, "fxadddd,0 %dr7, %dr9, %dr10", "fxadddd,0 %dr7, %dr10, %dr9"
	LATE	xx_fd_\g, \g, %dr9, "fxadddx,0 %dr7, %dr9, %dr10", "fxtofd,0 %dr10, %dr9"
	LATE	fd_xx_\g, \g, %dr9, "fxtofd,0 %dr10, %dr9", "fxadddx,0 %dr7, %dr9, %dr10"
	.endr

! u64 sr_es2 (u64 fpcr, u64 fpsr): write %fpcr, then %fpsr, and read
! %fpsr back with no FP operation in between; restore both.
	.global	sr_es2
	.type	sr_es2, #function
	.align	8
sr_es2:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	}
	{
	  rrs,0	%fpcr, %r8
	}
	{
	  rrs,0	%fpsr, %r9
	}
	{
	  rws,0	%r0, %fpcr
	}
	{
	  rws,0	%r1, %fpsr
	}
	{
	  nop 7
	}
	{
	  rrs,0	%fpsr, %r2
	}
	{
	  rws,0	%r8, %fpcr
	}
	{
	  rws,0	%r9, %fpsr
	}
	{
	  nop 5
	  addd,0	%dr2, 0x0, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sr_es2, .-sr_es2

! void sr_dc_<name> (const u64 in[4], u64 out[3], u64 fpsr): as DT, but
! with %fpsr preset to the third argument (%fpcr 0x37f), and safe for a
! diagnostic result (,sm forms): out[0] and out[1] have the tags cleared.
	.macro DC name, insn:vararg
	.global	sr_dc_\name
	.type	sr_dc_\name, #function
	.align	8
sr_dc_\name:
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrs,0	%fpcr, %r12
	}
	{
	  rrs,0	%fpsr, %r13
	}
	{
	  ldd,0	%dr0, 0x0, %dr5
	  ldd,2	%dr0, 0x10, %dr7
	  ldd,3	%dr0, 0x18, %dr8
	}
	{
	  addd,1	0x0, _f64,_lts0 0x5555555555555555, %dr9
	}
	{
	  nop 2
	}
	{
	  movif,0	%dr7, %r8, %dr11
	}
	{
	  rws,0	_f32s,_lts0 0x37f, %fpcr
	}
	{
	  rws,0	%r2, %fpsr
	}
	{
	  nop 7
	}
	{
	  \insn
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
	  rrs,0	%fpsr, %r16
	}
	{
	  movfi,0,sm	%dr9, %r20
	  puttagd,5	%dr9, 0x0, %dr19
	}
	{
	  nop 2
	}
	{
	  puttagd,5	%dr20, 0x0, %dr21
	}
	{
	  rws,0	%r12, %fpcr
	}
	{
	  rws,0	%r13, %fpsr
	}
	{
	  std,2	%dr1, 0x0, %dr19
	  std,5	%dr1, 0x8, %dr21
	}
	{
	  std,2	%dr1, 0x10, %dr16
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	sr_dc_\name, .-sr_dc_\name
	.endm

	DC	fxdivtss, fxdivtss,5 %dr5, %r11, %r9
	DC	fxdivtdd, fxdivtdd,5 %dr5, %r11, %dr9
	DC	fxdivtsx, fxdivtsx,5 %dr5, %r11, %dr9
	DC	fxdivtdx, fxdivtdx,5 %dr5, %r11, %dr9
	DC	fxdivtss_sm, fxdivtss,5,sm %dr5, %r11, %r9
	DC	fxdivtdd_sm, fxdivtdd,5,sm %dr5, %r11, %dr9
	DC	fxdivtsx_sm, fxdivtsx,5,sm %dr5, %r11, %dr9
	DC	fxdivtdx_sm, fxdivtdx,5,sm %dr5, %r11, %dr9

! long sr_lp_<name>_<n> (const double *p): as sr_le_* (A, G cycles, B,
! 12 cycles, n steps), for producers A that load from p (%dr0, which
! LATSET leaves alone) or are integer operations; B doesn't feed A.
	.macro LATPF name, n, g, insna, insnb
	.global	sr_lp_\name\()_\n
	.type	sr_lp_\name\()_\n, #function
	.align	8
sr_lp_\name\()_\n:
	LATSET
	.rept	\n
	STEP	\g, \insna
	STEP	12, \insnb
	.endr
	LATEND	%dr9
	.size	sr_lp_\name\()_\n, .-sr_lp_\name\()_\n
	.endm
	.macro LATP name, g, insna, insnb
	LATPF	\name, 16, \g, "\insna", "\insnb"
	LATPF	\name, 48, \g, "\insna", "\insnb"
	.endm

	! ld: ldd in ALC0 (ld3: ALC3); i: addd; xd: fxadddd's double
	! operand; tdx: fxdivtdx's double src1; f: faddd.
	.irp g, 1,2,3,4,5,6,7,8,9
	LATP	ld_xd_\g, \g, "ldd,0 %dr0, 0x0, %dr10", "fxadddd,0 %dr7, %dr10, %dr9"
	LATP	ld3_xd_\g, \g, "ldd,3 %dr0, 0x0, %dr10", "fxadddd,0 %dr7, %dr10, %dr9"
	LATP	i_xd_\g, \g, "addd,0 %dr5, 0x0, %dr10", "fxadddd,0 %dr7, %dr10, %dr9"
	LATP	ld_tdx_\g, \g, "ldd,0 %dr0, 0x0, %dr10", "fxdivtdx,5 %dr10, %r7, %dr9"
	LATP	ld_f_\g, \g, "ldd,0 %dr0, 0x0, %dr10", "faddd,0 %dr10, %dr5, %dr9"
	.endr
