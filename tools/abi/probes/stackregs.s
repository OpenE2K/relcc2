! SPDX-License-Identifier: GPL-3.0-or-later
! Reads of the stack and window registers for stackregs.c (build and run
! instructions there).  Only reads: nothing here writes a state register.
!
!   long sr_NAME (void): one rrd (or rrs) of one register.
!   void sr_snap (unsigned long *out): a snapshot.  %pcshtp, %pcsp.lo/.hi,
!     %pshtp, %psp.lo/.hi are read between a lock-wait load (MAS 0x7) and a
!     wait-unlock store (MAS 0x2) and the snapshot is retried on
!     ibranch ? %MLOCK, as glibc's setjmp does for %pcsp/%pcshtp and
!     MCST libgcc's E2K_READ_STACK_REGS macro (unwind-dw2-e2k.c, defined
!     but unused) would, so a spill or fill between the reads is not
!     seen half done.  out[0..13]:
!     pcshtp, pcsp.lo, pcsp.hi, pshtp, psp.lo, psp.hi, usd.lo, usd.hi,
!     sbr, wd, br (rrs, 32 bits), cr1.lo, cr1.hi, getsp 0; out[16] is the
!     lock word.
	.ignore	ld_st_style
	.text

	.macro	rdd name reg
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	\reg, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	.macro	rds name reg
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrs,0	\reg, %r0
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	rdd	sr_usd_lo, %usd.lo
	rdd	sr_usd_hi, %usd.hi
	rdd	sr_psp_lo, %psp.lo
	rdd	sr_psp_hi, %psp.hi
	rdd	sr_pshtp, %pshtp
	rdd	sr_pcsp_lo, %pcsp.lo
	rdd	sr_pcsp_hi, %pcsp.hi
	rdd	sr_pcshtp, %pcshtp
	rds	sr_pcshtp_s, %pcshtp
	rdd	sr_sbr, %sbr
	rdd	sr_wd, %wd
	rds	sr_br_s, %br
	rdd	sr_cr1_lo, %cr1.lo
	rdd	sr_cr1_hi, %cr1.hi
	rds	sr_cr1_hi_s, %cr1.hi

	.global	sr_snap
	.type	sr_snap, #function
	.align	8
sr_snap:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
.Lsnap_retry:
	{
	  ldd,0	%dr0, 0x80, %dr15, mas = 0x7
	}
	{
	  rrd,0	%pcshtp, %dr1
	}
	{
	  rrd,0	%pcsp.lo, %dr2
	}
	{
	  rrd,0	%pcsp.hi, %dr3
	}
	{
	  rrd,0	%pshtp, %dr4
	}
	{
	  rrd,0	%psp.lo, %dr5
	}
	{
	  rrd,0	%psp.hi, %dr6
	}
	{
	  std,2	%dr0, 0x80, %dr15, mas = 0x2
	  ibranch	.Lsnap_retry ? %MLOCK
	}
	{
	  rrd,0	%usd.lo, %dr7
	}
	{
	  rrd,0	%usd.hi, %dr8
	}
	{
	  rrd,0	%sbr, %dr9
	}
	{
	  rrd,0	%wd, %dr10
	}
	{
	  rrs,0	%br, %r11
	}
	{
	  rrd,0	%cr1.lo, %dr12
	}
	{
	  rrd,0	%cr1.hi, %dr13
	}
	{
	  getsp,0	0x0, %dr14
	}
	{
	  std,2	%dr0, 0x0, %dr1
	  std,5	%dr0, 0x8, %dr2
	}
	{
	  std,2	%dr0, 0x10, %dr3
	  std,5	%dr0, 0x18, %dr4
	}
	{
	  std,2	%dr0, 0x20, %dr5
	  std,5	%dr0, 0x28, %dr6
	}
	{
	  std,2	%dr0, 0x30, %dr7
	  std,5	%dr0, 0x38, %dr8
	}
	{
	  std,2	%dr0, 0x40, %dr9
	  std,5	%dr0, 0x48, %dr10
	}
	{
	  stw,2	%dr0, 0x50, %r11
	  std,5	%dr0, 0x58, %dr12
	}
	{
	  std,2	%dr0, 0x60, %dr13
	  std,5	%dr0, 0x68, %dr14
	}
	{
	  ct	%ctpr3
	}
	.size	sr_snap, .-sr_snap
