! SPDX-License-Identifier: GPL-3.0-or-later
! Array prefetch loops for apb-loop.c (spec/isa/apb.md).
! Each sums a[0..n-1] with one movad (am=1) per iteration and stores the
! iteration count to *iters.  %lsr = 0xff2000000000 with lcnt = n in the
! low half, as lcc writes it, runs a loop_mode loop exactly n times.
	.ignore	ld_st_style
	.text
! long loopA(const long *a, long n, long *iters): loop_mode + %lsr,
! %lsr written before bap
	.global	loopA
	.type	loopA, #function
	.align	8
loopA:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  ldisp	%ctpr2, .LAprog
	  aaurwd,2	%dr0, %aad0
	  addd,0	0x0, _f64,_lts0 0xff2000000000, %dr3
	  addd,1	0x0, 0x0, %dr8
	  addd,3	0x0, 0x0, %dr9
	  addd,4	0x0, 0x0, %dr10
	}
	{
	  insfd,0	%dr3, _f32s,_lts0 0x8800, %dr1, %dr3
	}
	{
	  disp	%ctpr1, .LAloop
	  rwd,0	%dr3, %lsr
	}
	{
	  nop 7
	  bap
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
.LAloop:
	{
	  loop_mode
	  alc	alcf=1, alct=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	  movad,0	area=0, ind=0, am=1, be=0, %dr8
	  addd,0	%dr9, %dr8, %dr9
	  addd,1	%dr10, 0x1, %dr10
	}
	{
	  nop 7
	  disp	%ctpr2, disp=0x0
	}
	{
	  addd,0	%dr9, %dr8, %dr0
	  std,2	%dr2, 0x0, %dr10
	}
	{
	  ct	%ctpr3
	}
	.align	8
.LAprog:
	{
	  fapb	ct=1, dcd=0, fmt=4, mrng=8, d=0, incr=0, ind=0, asz=5, abs=0, disp=0
	}
	.size	loopA, .-loopA

! long loopB(const long *a, long n, long *iters): ordinary branch loop
! counted in %dr10
	.global	loopB
	.type	loopB, #function
	.align	8
loopB:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  ldisp	%ctpr2, .LBprog
	  aaurwd,2	%dr0, %aad0
	  addd,1	0x0, 0x0, %dr8
	  addd,3	0x0, 0x0, %dr9
	  addd,4	0x0, 0x0, %dr10
	}
	{
	  disp	%ctpr1, .LBloop
	}
	{
	  nop 7
	  bap
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
.LBloop:
	{
	  movad,0	area=0, ind=0, am=1, be=0, %dr8
	  addd,1	%dr10, 0x1, %dr10
	}
	{
	  addd,0	%dr9, %dr8, %dr9
	  cmpldb,1	%dr10, %dr1, %pred0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr1 ? %pred0
	}
	{
	  nop 7
	  disp	%ctpr2, disp=0x0
	}
	{
	  addd,0	%dr9, 0x0, %dr0
	  std,2	%dr2, 0x0, %dr10
	}
	{
	  ct	%ctpr3
	}
	.align	8
.LBprog:
	{
	  fapb	ct=1, dcd=0, fmt=4, mrng=8, d=0, incr=0, ind=0, asz=5, abs=0, disp=0
	}
	.size	loopB, .-loopB
! long loopC(const long *a, long n, long *iters, char *fresh): a 777-iteration
! loop first leaves %lsr/%ilcr stale; bap; a page fault; then rwd %lsr
	.global	loopC
	.type	loopC, #function
	.align	8
loopC:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, _f64,_lts0 0xff2000000309, %dr4
	  disp	%ctpr1, .LCstale
	}
	{
	  rwd,0	%dr4, %lsr
	}
	{
	  nop 3
	}
.LCstale:
	{
	  loop_mode
	  alc	alcf=1, alct=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	}
	{
	  ldisp	%ctpr2, .LCprog
	  aaurwd,2	%dr0, %aad0
	  addd,0	0x0, _f64,_lts0 0xff2000000000, %dr6
	  addd,1	0x0, 0x0, %dr8
	  addd,3	0x0, 0x0, %dr9
	  addd,4	0x0, 0x0, %dr10
	}
	{
	  insfd,0	%dr6, _f32s,_lts0 0x8800, %dr1, %dr6
	}
	{
	  nop 7
	  bap
	}
	{
	  ldd,0	%dr3, 0x0, %dr5
	}
	{
	  nop 7
	}
	{
	  disp	%ctpr1, .LCloop
	  rwd,0	%dr6, %lsr
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
.LCloop:
	{
	  loop_mode
	  alc	alcf=1, alct=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	  movad,0	area=0, ind=0, am=1, be=0, %dr8
	  addd,0	%dr9, %dr8, %dr9
	  addd,1	%dr10, 0x1, %dr10
	}
	{
	  nop 7
	  disp	%ctpr2, disp=0x0
	}
	{
	  addd,0	%dr9, %dr8, %dr0
	  std,2	%dr2, 0x0, %dr10
	}
	{
	  ct	%ctpr3
	}
	.align	8
.LCprog:
	{
	  fapb	ct=1, dcd=0, fmt=4, mrng=8, d=0, incr=0, ind=0, asz=5, abs=0, disp=0
	}
	.size	loopC, .-loopC


! long loopE(const long *a, unsigned long lsr, long *out): a loop_mode
! loop with one movad per iteration, %lsr as given (to set ecnt, ldmc,
! ldovl); iteration i stores the value movad returned (tag cleared) at
! out[2i] and its tag at out[2i+1], from instructions without loop_mode.
! Returns the iteration count.
	.global	loopE
	.type	loopE, #function
	.align	8
loopE:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  ldisp	%ctpr2, .LEprog
	  aaurwd,2	%dr0, %aad0
	  addd,1	0x0, 0x0, %dr10
	  addd,3	%dr2, 0x0, %dr14
	}
	{
	  disp	%ctpr1, .LEloop
	  rwd,0	%dr1, %lsr
	}
	{
	  nop 7
	  bap
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
.LEloop:
	{
	  loop_mode
	  movad,0	area=0, ind=0, am=1, be=0, %dr8
	  addd,1	%dr10, 0x1, %dr10
	}
	{
	  nop 4
	}
	{
	  gettagd,2	%dr8, %r12
	  puttagd,5	%dr8, 0x0, %dr13
	}
	{
	  nop 2
	}
	{
	  std,2	%dr14, 0x0, %dr13
	  stw,5	%dr14, 0x8, %r12
	}
	{
	  loop_mode
	  alc	alcf=1, alct=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	  addd,0	%dr14, 0x10, %dr14
	}
	{
	  nop 7
	  disp	%ctpr2, disp=0x0
	}
	{
	  addd,0	%dr10, 0x0, %dr0
	}
	{
	  ct	%ctpr3
	}
	.align	8
.LEprog:
	{
	  fapb	ct=1, dcd=0, fmt=4, mrng=8, d=0, incr=0, ind=0, asz=5, abs=0, disp=0
	}
	.size	loopE, .-loopE

! long loopF(const long *a, unsigned long lsr, long *iters): loopA with
! %lsr as given, to run epilogue iterations (ecnt) with movas kept on
! (ldmc = 1): sums a[0..lcnt+ecnt-1].
	.global	loopF
	.type	loopF, #function
	.align	8
loopF:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  ldisp	%ctpr2, .LFprog
	  aaurwd,2	%dr0, %aad0
	  addd,1	0x0, 0x0, %dr8
	  addd,3	0x0, 0x0, %dr9
	  addd,4	0x0, 0x0, %dr10
	}
	{
	  disp	%ctpr1, .LFloop
	  rwd,0	%dr1, %lsr
	}
	{
	  nop 7
	  bap
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
.LFloop:
	{
	  loop_mode
	  alc	alcf=1, alct=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	  movad,0	area=0, ind=0, am=1, be=0, %dr8
	  addd,0	%dr9, %dr8, %dr9
	  addd,1	%dr10, 0x1, %dr10
	}
	{
	  nop 7
	  disp	%ctpr2, disp=0x0
	}
	{
	  addd,0	%dr9, %dr8, %dr0
	  std,2	%dr2, 0x0, %dr10
	}
	{
	  ct	%ctpr3
	}
	.align	8
.LFprog:
	{
	  fapb	ct=1, dcd=0, fmt=4, mrng=8, d=0, incr=0, ind=0, asz=5, abs=0, disp=0
	}
	.size	loopF, .-loopF
