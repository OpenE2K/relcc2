! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for hwscan.c (finding pointers in the procedure stack).  Build
! and run lines are in hwscan.c.
	.ignore	ld_st_style
	.text

! void hs_marks_fxN (void (*fn) (void *), void *arg, unsigned long base,
!                    long *bad)
! A frame with known values in all of %r0..%r15: %rN = base + N, except
! %r14, which also gets the extended (exponent) part 0x1234 with movif,
! and %r15, a diagnostic value (a speculative load from BAD).  Then it
! calls fn (arg) with wbs = 8, so %r0..%r15 (8 pairs) are this frame's
! part of the procedure stack.  nfx = 1 and nfx = 0 variants.
.macro	marks name, nfx
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0xa, nfx = \nfx, dbl = 0x0
	  movtd,0	%dr0, %ctpr1
	  ldd,3,sm	%dr3, 0x0, %dr15
	  addd,1	0x0, %dr1, %dr16
	  addd,4	0x0, 0x1234, %dr17
	}
	{
	  addd,0	%dr2, 0x4, %dr4
	  addd,1	%dr2, 0x5, %dr5
	  addd,2	%dr2, 0x6, %dr6
	  addd,3	%dr2, 0x7, %dr7
	  addd,4	%dr2, 0x8, %dr8
	  addd,5	%dr2, 0x9, %dr9
	}
	{
	  addd,0	%dr2, 0xa, %dr10
	  addd,1	%dr2, 0xb, %dr11
	  addd,2	%dr2, 0xc, %dr12
	  addd,3	%dr2, 0xd, %dr13
	  addd,4	%dr2, 0xe, %dr14
	  addd,5	%dr2, 0x3, %dr3
	}
	{
	  addd,0	%dr2, 0x0, %dr0
	  addd,1	%dr2, 0x1, %dr1
	  movif,4	%dr14, %r17, %r14
	}
	{
	  addd,0	%dr2, 0x2, %dr2
	}
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
.endm
	marks	hs_marks_fx1, 0x1
	marks	hs_marks_fx0, 0x0

! long hs_direct (long mode, unsigned long long *off, void *buf,
!                 unsigned long size, void *real, unsigned long base)
! access_hw_stacks made here (entry 3), not through glibc's syscall (),
! with %r6..%r15 = base + 6 .. base + 15 in this frame; %r16..%r23 carry
! the number and arguments (wbs = 8).  Returns the result.
	.global	hs_direct
	.type	hs_direct, #function
	.align	8
hs_direct:
	{
	  setwd	wsz = 0xc, nfx = 0x1, dbl = 0x0
	  sdisp	%ctpr1, 0x3
	  addd,0	0x0, 0xfe, %dr16
	  addd,1	0x0, %dr0, %dr17
	  addd,2	0x0, %dr1, %dr18
	  addd,3	0x0, %dr2, %dr19
	  addd,4	0x0, %dr3, %dr20
	  addd,5	0x0, %dr4, %dr21
	}
	{
	  addd,0	%dr5, 0x6, %dr6
	  addd,1	%dr5, 0x7, %dr7
	  addd,2	%dr5, 0x8, %dr8
	  addd,3	%dr5, 0x9, %dr9
	  addd,4	%dr5, 0xa, %dr10
	  addd,5	%dr5, 0xb, %dr11
	}
	{
	  addd,0	%dr5, 0xc, %dr12
	  addd,1	%dr5, 0xd, %dr13
	  addd,2	%dr5, 0xe, %dr14
	  addd,3	%dr5, 0xf, %dr15
	}
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  return	%ctpr3
	  addd,0	0x0, %dr16, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	hs_direct, .-hs_direct

! long hs_peek (const long *p, unsigned long *out): *out = the doubleword
! at p with its tag cleared; returns its tag (gettagd: 0 = none, 5 = a
! diagnostic doubleword).  The load uses the fill MAS (mas=0x4 without
! ,sm), the one load that returns memory tags (speculation.md, "Spill and
! fill"); an ordinary or ,sm load would read every word with tag 0.  The
! tag operations (ALC2, ALC5) take a diagnostic operand without trapping
! (abg.s, ctxsig.s).
	.global	hs_peek
	.type	hs_peek, #function
	.align	8
hs_peek:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  ldd,0	%dr0, 0x0, %dr2, mas=0x4
	}
	{
	  nop 2
	}
	{
	  gettagd,2	%dr2, %r3
	  puttagd,5	%dr2, 0x0, %dr4
	}
	{
	  nop 1
	}
	{
	  std,2	%dr1, 0x0, %dr4
	  sxt,0	0x2, %r3, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	hs_peek, .-hs_peek

! long hs_spin (volatile long *flag, long marker): %r4 = marker,
! %g20 = marker + 1, %g4 = marker + 2, spin until *flag != 0 (a signal
! handler sets it), return %r4 + (%g20 - marker - 1) + (%g4 - marker - 2):
! marker if all three survived.
	.global	hs_spin
	.type	hs_spin, #function
	.align	8
hs_spin:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  addd,0	%dr1, 0x0, %dr4
	  addd,1	%dr1, 0x1, %dg20
	  addd,2	%dr1, 0x2, %dg4
	}
.Lhs_spin:
	{
	  ldd,0	%dr0, 0x0, %dr5
	}
	{
	  nop 2
	}
	{
	  cmpedb,0	%dr5, 0x0, %pred0
	}
	{
	  nop 2
	}
	{
	  ibranch	.Lhs_spin ? %pred0
	}
	{
	  return	%ctpr3
	  subd,0	%dg20, %dr1, %dr6
	  subd,1	%dg4, %dr1, %dr7
	}
	{
	  subd,0	%dr6, 0x1, %dr6
	  subd,1	%dr7, 0x2, %dr7
	}
	{
	  nop 1
	}
	{
	  addd,0	%dr4, %dr6, %dr0
	}
	{
	  nop 1
	}
	{
	  addd,0	%dr0, %dr7, %dr0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr3
	}
	.size	hs_spin, .-hs_spin

! long hs_tagtest (long *dst, long *bad): a diagnostic value (a
! speculative load from BAD) stored with std,sm at DST; returns the
! register's tag (gettagd), to check what hs_peek reads back.
	.global	hs_tagtest
	.type	hs_tagtest, #function
	.align	8
hs_tagtest:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0,sm	%dr1, 0x0, %dr2
	}
	{
	  nop 5
	}
	{
	  gettagd,2	%dr2, %r3
	}
	{
	  nop 1
	}
	{
	  std,2,sm	%dr0, 0x0, %dr2
	  sxt,0	0x2, %r3, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	hs_tagtest, .-hs_tagtest

! long hs_spin_REG (volatile long *flag, long a, long b): REG = a ^ b,
! the only copy of it (a and b are cleared), spin until *flag != 0, then
! clear REG and return 0.  REG is %r1, %r4, %r7 (this window: wsz = 4),
! %g20 (saved in the signal frame) or %g4 (not saved: registers.md);
! hs_spin_r1r4 sets both %r1 and %r4, hs_spin_g20p1 sets %g20 to
! a ^ b + 1 (a pointer into the object).
.macro	spinreg name, reg, reg2=%dr3, add=0x0
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  xord,0	%dr1, %dr2, %dr3
	}
	{
	  addd,0	0x0, 0x0, %dr1
	  addd,1	0x0, 0x0, %dr2
	  addd,2	0x0, 0x0, %dr4
	  addd,3	0x0, 0x0, %dr5
	  addd,4	0x0, 0x0, %dr6
	  addd,5	0x0, 0x0, %dr7
	}
	{
	  addd,0	\add, %dr3, \reg
	  addd,1	\add, %dr3, \reg2
	}
	{
	  addd,0	0x0, 0x0, %dr3
	}
1:
	{
	  ldd,0	%dr0, 0x0, %dr5
	}
	{
	  nop 2
	}
	{
	  cmpedb,0	%dr5, 0x0, %pred0
	}
	{
	  nop 2
	}
	{
	  ibranch	1b ? %pred0
	}
	{
	  return	%ctpr3
	  addd,0	0x0, 0x0, \reg
	  addd,1	0x0, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
.endm
	spinreg	hs_spin_r1, %dr1
	spinreg	hs_spin_r4, %dr4
	spinreg	hs_spin_r7, %dr7
	spinreg	hs_spin_g20, %dg20
	spinreg	hs_spin_r1r4, %dr1, %dr4
	spinreg	hs_spin_g20p1, %dg20, %dr3, 0x1
	spinreg	hs_spin_g4, %dg4
