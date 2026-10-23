! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for foldi55.c (see there for how to build and run): the high
! half of a v5+ 128-bit window register after 64-bit writes, in the
! pairs a caller hands over and the ones a window gains, and after the
! window is spilled and filled (a deep call, a signal, system calls,
! context switches), with setwd nfx = 1 and nfx = 0.  Every value is
! stored with stqp,sm (no trap, tags kept in memory for the fill MAS);
! a plain stqp of a chosen register then shows whether it traps.
	.ignore	ld_st_style
	.text

! unsigned long fi_tags (const void *p): the tags of the doublewords at
! p and p + 8, read with the fill MAS: (tag (p + 8) << 4) | tag (p).
	.global	fi_tags
	.type	fi_tags, #function
	.align	8
fi_tags:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	}
	{
	  ldd,0	%dr0, 0x0, %dr2, mas=0x4
	  ldd,2	%dr0, 0x8, %dr3, mas=0x4
	}
	{
	  nop 3
	}
	{
	  gettagd,2	%dr2, %r4
	  gettagd,5	%dr3, %r5
	}
	{
	  nop 3
	}
	{
	  shld,0	%dr5, 0x4, %dr5
	}
	{
	  ord,0	%dr4, %dr5, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_tags, .-fi_tags

! ---------------------------------------------------------------- part 1
! void fi_wr_K (void *out, unsigned long hi, unsigned long val, long *p)
! %qpr6 = {hi, hi} (qppackdl), then one 64-bit (or narrower) write of
! %dr6 / %r6, then stqp,sm %qpr6 to out.  K: none (no write), addd,
! adds under dbl = 0 and dbl = 1, ldd, ldw, faddd, ldd,sm from p (a
! PROT_NONE page: a diagnostic low half), sxt.
.macro	wr k, dbl, op
	.global	fi_wr_\k
	.type	fi_wr_\k, #function
	.align	8
fi_wr_\k:
	{
	  setwd	wsz = 0x8, nfx = 0x0, dbl = \dbl
	  return	%ctpr3
	}
	{
	  qppackdl,1	%dr1, %dr1, %qpr6
	}
	{
	  nop 3
	}
	{
	  \op
	}
	{
	  nop 5
	}
	{
	  stqp,2,sm	%dr0, 0x0, %qpr6
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_wr_\k, .-fi_wr_\k
.endm
	wr	none, 0x0, "nop"
	wr	addd, 0x0, "addd,0 0x0, %dr2, %dr6"
	wr	adds0, 0x0, "adds,0 0x0, %r2, %r6"
	wr	adds1, 0x1, "adds,0 0x0, %r2, %r6"
	wr	ldd, 0x0, "ldd,0 %dr3, 0x0, %dr6"
	wr	ldw, 0x0, "ldw,0 %dr3, 0x0, %r6"
	wr	faddd, 0x0, "faddd,0 %dr2, %dr2, %dr6"
	wr	lddsm, 0x0, "ldd,0,sm %dr3, 0x0, %dr6"
	wr	sxt, 0x0, "sxt,0 0x2, %r2, %dr6"

! void fi_gain_setwd_nfxN (void *out): %qpr13 = {6666..., 6666...} and
! %qpr12 = {7777..., 7777...} at wsz = 8, then setwd 4 and setwd 8
! again (the window loses and regains them), then addd 5 into %dr13
! only; stores %qpr13 to out, %qpr12 to out + 16.
.macro	gain nfx
	.global	fi_gain_setwd_nfx\nfx
	.type	fi_gain_setwd_nfx\nfx, #function
	.align	8
fi_gain_setwd_nfx\nfx:
	{
	  setwd	wsz = 0x8, nfx = \nfx, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, _f64,_lts0 0x6666666666666666, %dr13
	}
	{
	  addd,0	0x0, _f64,_lts0 0x7777777777777777, %dr12
	}
	{
	  qppackdl,0	%dr13, %dr13, %qpr13
	  qppackdl,1	%dr12, %dr12, %qpr12
	}
	{
	  nop 3
	}
	{
	  setwd	wsz = 0x4, nfx = \nfx, dbl = 0x0
	}
	{
	  setwd	wsz = 0x8, nfx = \nfx, dbl = 0x0
	}
	{
	  nop 1
	}
	{
	  addd,0	0x0, 0x5, %dr13
	}
	{
	  nop 3
	}
	{
	  stqp,2,sm	%dr0, 0x0, %qpr13
	  stqp,5,sm	%dr0, 0x10, %qpr12
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_gain_setwd_nfx\nfx, .-fi_gain_setwd_nfx\nfx
.endm
	gain	0
	gain	1

! void fi_gain_set (void): %qpr13 = {6666..., 6666...}, %qpr12 =
! {7777..., 7777...} (wsz = 8), and returns.  void fi_gain_get (void
! *out), called next from the same frame with the same wbs: its window
! takes the same registers; addd 5 into %dr13 only, then stores %qpr13
! to out, %qpr12 to out + 16.  The write waits one instruction after
! the setwd (tagempty.s, N).
	.global	fi_gain_set
	.type	fi_gain_set, #function
	.align	8
fi_gain_set:
	{
	  setwd	wsz = 0x8, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, _f64,_lts0 0x6666666666666666, %dr13
	}
	{
	  addd,0	0x0, _f64,_lts0 0x7777777777777777, %dr12
	}
	{
	  qppackdl,0	%dr13, %dr13, %qpr13
	  qppackdl,1	%dr12, %dr12, %qpr12
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_gain_set, .-fi_gain_set

	.global	fi_gain_get
	.type	fi_gain_get, #function
	.align	8
fi_gain_get:
	{
	  setwd	wsz = 0x8, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x5, %dr13
	}
	{
	  nop 3
	}
	{
	  stqp,2,sm	%dr0, 0x0, %qpr13
	  stqp,5,sm	%dr0, 0x10, %qpr12
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_gain_get, .-fi_gain_get

! ---------------------------------------------------------------- part 2
! unsigned long fi_t_A_nfxN (void *out, unsigned long hi, unsigned long
!                            val, long x, long n, long sel)
! wsz = 12 pairs, nfx = N.  Sets up four 128-bit registers:
!   %qpr6  = {hi, hi}, then val + 6 into %dr6 with addd (a 64-bit
!            write over a known high half);
!   %qpr7  : val + 7 into %dr7 with addd only (the PoC's case: a value
!            written with 64 bits; %r0-%r7 are the pairs the caller
!            hands over, %r8 up the ones the window gains);
!   %qpr8  = {hi, val + 8} (a full 128-bit write);
!   %qpr9  : not written at all;
!   %dr10  : val + 10; for A = spin first a diagnostic 4afafafa4afafaf8
!            (ldd,sm from x, a PROT_NONE page, then andd,sm with ~7, a
!            failed speculative pointer), and val + 10 written over it
!            with addd after the spin;
!   %dr11  : val + 11;
! then does A:
!   none   nothing;
!   call   calls the function x (n) with wbs = 8 (a deep recursion
!          spills this window: %r0-%r15 lie below the callee);
!   spin   counts n down (interrupts, preemption, signals arrive);
!          x is a PROT_NONE page for %dr10;
!   fault  loads from x (a PROT_NONE page: SIGSEGV, the handler fixes
!          the page and returns);
!   sys    system call n (x) through entry 3 (wbs = 8).
! Then stores %qpr6 .. %qpr9 with stqp,sm to out, out + 16, + 32, + 48,
! %qpr10 and %qpr11 to out + 80, + 96, and, if sel is 7, 6, 8 or 10,
! that register with a plain stqp to out + 64 (a diagnostic half traps
! here: SIGILL).  Returns %clkr cycles spent in A.
.macro	tfn a, nfx
	.global	fi_t_\a\()_nfx\nfx
	.type	fi_t_\a\()_nfx\nfx, #function
	.align	8
fi_t_\a\()_nfx\nfx:
	{
	  setwd	wsz = 0xc, nfx = \nfx, dbl = 0x0
	}
	{
	  rrd,0	%clkr, %dr12
	  qppackdl,1	%dr1, %dr1, %qpr6
	  addd,3	%dr2, 0x8, %dr8
	  addd,4	%dr2, 0xb, %dr11
	}
	{
	  qppackdl,1	%dr1, %dr8, %qpr8
	}
	{
	  nop 3
	}
	{
	  addd,0	%dr2, 0x6, %dr6
	  addd,1	%dr2, 0x7, %dr7
	}
	.ifc	\a, spin
	{
	  ldd,0,sm	%dr3, 0x0, %dr10
	}
	{
	  nop 3
	}
	{
	  andd,0,sm	%dr10, _f64,_lts0 0xfffffffffffffff8, %dr10
	}
	.else
	{
	  addd,0	%dr2, 0xa, %dr10
	}
	.endif
	.ifc	\a, call
	{
	  movtd,0	%dr3, %ctpr1
	  addd,1	0x0, %dr4, %dr16
	}
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	.endif
	.ifc	\a, spin
	{
	  disp	%ctpr1, 1f
	  addd,0	0x0, %dr4, %dr14
	}
	{
	  nop 5
	}
1:
	{
	  subd,0	%dr14, 0x1, %dr14
	}
	{
	  cmpedb,0	%dr14, 0x0, %pred0
	}
	{
	  ct	%ctpr1 ? ~%pred0
	}
	{
	  addd,0	%dr2, 0xa, %dr10
	}
	.endif
	.ifc	\a, fault
	{
	  ldd,0	%dr3, 0x0, %dr14
	}
	{
	  nop 3
	}
	{
	  addd,0	0x0, %dr14, %dr15
	}
	.endif
	.ifc	\a, sys
	{
	  sdisp	%ctpr1, 0x3
	  addd,0	0x0, %dr4, %dr16
	  addd,1	0x0, %dr3, %dr17
	  addd,2	0x0, 0x0, %dr18
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	.endif
	{
	  rrd,0	%clkr, %dr13
	}
	{
	  nop 2
	}
	{
	  subd,0	%dr13, %dr12, %dr13
	  cmpedb,1	%dr5, 0x7, %pred1
	  cmpedb,3	%dr5, 0x6, %pred2
	  cmpedb,4	%dr5, 0x8, %pred3
	}
	{
	  cmpedb,0	%dr5, 0xa, %pred4
	}
	{
	  stqp,2,sm	%dr0, 0x0, %qpr6
	  stqp,5,sm	%dr0, 0x10, %qpr7
	}
	{
	  stqp,2,sm	%dr0, 0x20, %qpr8
	  stqp,5,sm	%dr0, 0x30, %qpr9
	}
	{
	  stqp,2,sm	%dr0, 0x50, %qpr10
	  stqp,5,sm	%dr0, 0x60, %qpr11
	}
	{
	  nop 2
	}
	.global	fi_t_\a\()_nfx\nfx\()_st
fi_t_\a\()_nfx\nfx\()_st:
	{
	  stqp,2	%dr0, 0x40, %qpr7 ? %pred1
	}
	{
	  stqp,2	%dr0, 0x40, %qpr6 ? %pred2
	}
	{
	  stqp,2	%dr0, 0x40, %qpr8 ? %pred3
	}
	{
	  stqp,2	%dr0, 0x40, %qpr10 ? %pred4
	}
	{
	  return	%ctpr3
	  addd,0	0x0, %dr13, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_t_\a\()_nfx\nfx, .-fi_t_\a\()_nfx\nfx
.endm
	.irp	a, none, call, spin, fault, sys
	tfn	\a, 1
	tfn	\a, 0
	.endr

! void fi_prime (long (*f) (long), long n): wsz = 16 pairs, nfx = 0.
! Gives each of %qpr0 .. %qpr23 a diagnostic high half
! 0x4a0000KK4afafaf8 (KK the register number, tags 1/1: puttagqp 0x50)
! and the low half 0x1100000000000000 (tag 0), then calls f (n) with wbs = 12, so that a deep f spills %r0-%r23 with
! their high halves into the procedure stack.  Called just before
! fi_t_* from the same frame, its window lies where fi_t_*'s will.
.macro	prime1 k, hi
	{
	  addd,0	0x0, _f64,_lts0 \hi, %dr\k
	}
	{
	  qppackdl,0	%dr\k, %dr25, %qpr\k
	}
	{
	  puttagqp,0	%qpr\k, %r30, %qpr\k
	}
.endm
	.global	fi_prime
	.type	fi_prime, #function
	.align	8
fi_prime:
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	}
	{
	  movtd,0	%dr0, %ctpr1
	  addd,1	0x0, %dr1, %dr24
	  addd,3	0x0, 0x50, %dr30
	  addd,4	0x0, _f64,_lts0 0x1100000000000000, %dr25
	}
	prime1	0, 0x4a0000004afafaf8
	prime1	1, 0x4a0000014afafaf8
	prime1	2, 0x4a0000024afafaf8
	prime1	3, 0x4a0000034afafaf8
	prime1	4, 0x4a0000044afafaf8
	prime1	5, 0x4a0000054afafaf8
	prime1	6, 0x4a0000064afafaf8
	prime1	7, 0x4a0000074afafaf8
	prime1	8, 0x4a0000084afafaf8
	prime1	9, 0x4a0000094afafaf8
	prime1	10, 0x4a00000a4afafaf8
	prime1	11, 0x4a00000b4afafaf8
	prime1	12, 0x4a00000c4afafaf8
	prime1	13, 0x4a00000d4afafaf8
	prime1	14, 0x4a00000e4afafaf8
	prime1	15, 0x4a00000f4afafaf8
	prime1	16, 0x4a0000104afafaf8
	prime1	17, 0x4a0000114afafaf8
	prime1	18, 0x4a0000124afafaf8
	prime1	19, 0x4a0000134afafaf8
	prime1	20, 0x4a0000144afafaf8
	prime1	21, 0x4a0000154afafaf8
	prime1	22, 0x4a0000164afafaf8
	prime1	23, 0x4a0000174afafaf8
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = 0xc
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
	.size	fi_prime, .-fi_prime

! long fi_deepq (long n): recursion n deep (wsz = 8, nfx = 0, wbs = 4)
! that gives %qpr1 .. %qpr7 of every frame the high half
! 0x0d0d0d0d0d0d0d0d (tag 0) before calling on, so the physical
! registers a spilled window comes back to last held that.
	.global	fi_deepq
	.type	fi_deepq, #function
	.align	8
fi_deepq:
	{
	  setwd	wsz = 0x8, nfx = 0x0, dbl = 0x0
	  disp	%ctpr1, fi_deepq
	}
	{
	  addd,0	0x0, _f64,_lts0 0x0d0d0d0d0d0d0d0d, %dr1
	  cmpedb,1	%dr0, 0x0, %pred0
	  subd,3	%dr0, 0x1, %dr8
	}
	{
	  nop 3
	}
	{
	  qppackdl,0	%dr1, %dr1, %qpr2
	  qppackdl,1	%dr1, %dr1, %qpr3
	  qppackdl,3	%dr1, %dr1, %qpr4
	  qppackdl,4	%dr1, %dr1, %qpr5
	}
	{
	  qppackdl,0	%dr1, %dr1, %qpr6
	  qppackdl,1	%dr1, %dr1, %qpr7
	  qppackdl,3	%dr1, %dr1, %qpr1
	}
	{
	  nop 3
	}
	{
	  call	%ctpr1, wbs = 0x4 ? ~%pred0
	}
	{
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_deepq, .-fi_deepq

! ---------------------------------------------------------------- part 3
! void fi_gset_nfxN (unsigned long hi, long tags, long (*f) (long), long n)
! wsz = 10 pairs, nfx = N: %qpr10 .. %qpr13 = {hi, 2222222222222222}
! with puttagqp TAGS (0x50: tag 1 on the high half's two words), then
! calls f (n) with wbs = 8 (a deep f spills and fills this window; f =
! fi_leaf does not).  void fi_gget_0 (void *out, long *p), called next
! from the same frame with the same wbs, takes the same registers
! (wsz 8): writes %dr13 with addd 5, %dr12 with ldd from p, %r11 with
! adds 7 (32 bits, dbl = 0), leaves %r10 alone, and stores %qpr13,
! %qpr12, %qpr11, %qpr10 with stqp,sm to out, + 16, + 32, + 48.
.macro	gset nfx
	.global	fi_gset_nfx\nfx
	.type	fi_gset_nfx\nfx, #function
	.align	8
fi_gset_nfx\nfx:
	{
	  setwd	wsz = 0xa, nfx = \nfx, dbl = 0x0
	}
	{
	  movtd,0	%dr2, %ctpr1
	  addd,1	0x0, %dr3, %dr16
	  addd,3	0x0, _f64,_lts0 0x2222222222222222, %dr9
	}
	{
	  nop 3
	}
	{
	  qppackdl,0	%dr0, %dr9, %qpr10
	  qppackdl,1	%dr0, %dr9, %qpr11
	  qppackdl,3	%dr0, %dr9, %qpr12
	  qppackdl,4	%dr0, %dr9, %qpr13
	}
	{
	  nop 3
	}
	{
	  puttagqp,0	%qpr10, %r1, %qpr10
	  puttagqp,1	%qpr11, %r1, %qpr11
	  puttagqp,3	%qpr12, %r1, %qpr12
	  puttagqp,4	%qpr13, %r1, %qpr13
	}
	{
	  nop 5
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
	.size	fi_gset_nfx\nfx, .-fi_gset_nfx\nfx
.endm
	gset	0
	gset	1

! Variants: fi_gget_K.  K = 0: as above.  K = d: three nop 7 between
! the setwd and the writes.  K = w: wsz 12 instead of 8.  K = q: a
! qppackdl into %qpr12 instead of the ldd (r13's pair partner gets a
! 128-bit write).  K = t: fi_t's order: wsz 12, an rrd and two
! qppackdl (into %qpr12 and %qpr10) one instruction after the setwd,
! nop 3, then addd into %dr12 and %dr13 (and the adds into %r11).
.macro	gget k, wsz, delay, op12, first
	.global	fi_gget_\k
	.type	fi_gget_\k, #function
	.align	8
fi_gget_\k:
	{
	  setwd	wsz = \wsz, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	.rept	\delay
	{
	  nop 7
	}
	.endr
	\first
	{
	  addd,0	0x0, 0x5, %dr13
	  adds,1	0x0, 0x7, %r11
	  \op12
	}
	{
	  nop 3
	}
	{
	  stqp,2,sm	%dr0, 0x0, %qpr13
	  stqp,5,sm	%dr0, 0x10, %qpr12
	}
	{
	  stqp,2,sm	%dr0, 0x20, %qpr11
	  stqp,5,sm	%dr0, 0x30, %qpr10
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_gget_\k, .-fi_gget_\k
.endm
	gget	0, 0x8, 0, "ldd,2 %dr1, 0x0, %dr12", ""
	gget	d, 0x8, 3, "ldd,2 %dr1, 0x0, %dr12", ""
	gget	w, 0xc, 0, "ldd,2 %dr1, 0x0, %dr12", ""
	gget	q, 0x8, 0, "qppackdl,3 %dr0, %dr0, %qpr12", ""
	gget	t, 0xc, 0, "addd,3 0x0, 0x6, %dr12", "{ rrd,0 %clkr, %dr20 ; qppackdl,1 %dr0, %dr0, %qpr12 ; qppackdl,3 %dr0, %dr0, %qpr10 } { nop 3 }"

! long fi_leaf (long n): returns n; a small window, no spill.
	.global	fi_leaf
	.type	fi_leaf, #function
	.align	8
fi_leaf:
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
	.size	fi_leaf, .-fi_leaf

! ---------------------------------------------------------------- part 4
! void fi_hrun_W (void *out, unsigned long hi, long sel): a caller with
! wsz = W pairs that calls fi_hset (hi) and then fi_hget (out, sel),
! both with wbs = 12, so each callee's %r0 .. %r(2W - 25) are pairs the
! caller hands over (its own %r24 up) and the rest of its 8 pairs are
! ones the call gains.  fi_hset (wsz 8): %qpr0 .. %qpr15 = {hi,
! 2222222222222222} with tag 1 on the high half's words (puttagqp 0x50;
! diagnostic when hi has bit 30 set in both words).  fi_hget (wsz 8):
! writes %dr1 .. %dr15 with addd (64 bits: the value k), stores
! %qpr1 .. %qpr15 with stqp,sm to out + 16k, then a plain stqp of %qpr3
! (sel 3) or %qpr9 (sel 9), or a plain std of %dr3 (sel 13), to out.
	.global	fi_hset
	.type	fi_hset, #function
	.align	8
fi_hset:
	{
	  setwd	wsz = 0x8, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,0	0x0, _f64,_lts0 0x2222222222222222, %dr2
	}
	{
	  nop 3
	}
	{
	  qppackdl,0	%dr0, %dr2, %qpr3
	  qppackdl,1	%dr0, %dr2, %qpr4
	  qppackdl,3	%dr0, %dr2, %qpr5
	  qppackdl,4	%dr0, %dr2, %qpr6
	}
	{
	  qppackdl,0	%dr0, %dr2, %qpr7
	  qppackdl,1	%dr0, %dr2, %qpr8
	  qppackdl,3	%dr0, %dr2, %qpr9
	  qppackdl,4	%dr0, %dr2, %qpr10
	}
	{
	  qppackdl,0	%dr0, %dr2, %qpr11
	  qppackdl,1	%dr0, %dr2, %qpr12
	  qppackdl,3	%dr0, %dr2, %qpr13
	  qppackdl,4	%dr0, %dr2, %qpr14
	}
	{
	  qppackdl,0	%dr0, %dr2, %qpr15
	  qppackdl,1	%dr0, %dr2, %qpr0
	  qppackdl,3	%dr0, %dr2, %qpr1
	  qppackdl,4	%dr0, %dr2, %qpr2
	}
	{
	  nop 3
	}
	.irp	k, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	{
	  puttagqp,0	%qpr\k, 0x50, %qpr\k
	}
	.endr
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_hset, .-fi_hset

	.global	fi_hget
	.type	fi_hget, #function
	.align	8
fi_hget:
	{
	  setwd	wsz = 0x8, nfx = 0x0, dbl = 0x0
	  return	%ctpr3
	}
	{
	  cmpedb,0	%dr1, 0x3, %pred1
	  cmpedb,1	%dr1, 0x9, %pred2
	  cmpedb,3	%dr1, 0xd, %pred3
	}
	{
	  addd,0	0x0, 0x1, %dr1
	  addd,1	0x0, 0x2, %dr2
	  addd,2	0x0, 0x3, %dr3
	  addd,3	0x0, 0x4, %dr4
	  addd,4	0x0, 0x5, %dr5
	  addd,5	0x0, 0x6, %dr6
	}
	{
	  addd,0	0x0, 0x7, %dr7
	  addd,1	0x0, 0x8, %dr8
	  addd,2	0x0, 0x9, %dr9
	  addd,3	0x0, 0xa, %dr10
	  addd,4	0x0, 0xb, %dr11
	  addd,5	0x0, 0xc, %dr12
	}
	{
	  addd,0	0x0, 0xd, %dr13
	  addd,1	0x0, 0xe, %dr14
	  addd,2	0x0, 0xf, %dr15
	}
	{
	  nop 3
	}
	{
	  stqp,2,sm	%dr0, 0x10, %qpr1
	  stqp,5,sm	%dr0, 0x20, %qpr2
	}
	{
	  stqp,2,sm	%dr0, 0x30, %qpr3
	  stqp,5,sm	%dr0, 0x40, %qpr4
	}
	{
	  stqp,2,sm	%dr0, 0x50, %qpr5
	  stqp,5,sm	%dr0, 0x60, %qpr6
	}
	{
	  stqp,2,sm	%dr0, 0x70, %qpr7
	  stqp,5,sm	%dr0, 0x80, %qpr8
	}
	{
	  stqp,2,sm	%dr0, 0x90, %qpr9
	  stqp,5,sm	%dr0, 0xa0, %qpr10
	}
	{
	  stqp,2,sm	%dr0, 0xb0, %qpr11
	  stqp,5,sm	%dr0, 0xc0, %qpr12
	}
	{
	  stqp,2,sm	%dr0, 0xd0, %qpr13
	  stqp,5,sm	%dr0, 0xe0, %qpr14
	}
	{
	  stqp,2,sm	%dr0, 0xf0, %qpr15
	}
	.global	fi_hget_st
fi_hget_st:
	{
	  stqp,2	%dr0, 0x0, %qpr3 ? %pred1
	}
	{
	  stqp,2	%dr0, 0x0, %qpr9 ? %pred2
	}
	{
	  std,2	%dr0, 0x0, %dr3 ? %pred3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	fi_hget, .-fi_hget

.macro	hrun w
	.global	fi_hrun_\w
	.type	fi_hrun_\w, #function
	.align	8
fi_hrun_\w:
	{
	  setwd	wsz = \w, nfx = 0x0, dbl = 0x0
	  disp	%ctpr1, fi_hset
	}
	{
	  addd,0	0x0, %dr1, %dr24
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0xc
	}
	{
	  disp	%ctpr1, fi_hget
	  addd,0	0x0, %dr0, %dr24
	  addd,1	0x0, %dr2, %dr25
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0xc
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
	.size	fi_hrun_\w, .-fi_hrun_\w
.endm
	hrun	0x10
	hrun	0x14
