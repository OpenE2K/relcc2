! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for pocabi2.c (see there for how to build and run):
!   part 1, the window base of a `call` (wbs): frames that inherited
!   1-4 pairs hand over 1-4 pairs with 0-8 pairs left below them, to a
!   system call (entries 3 and 6), an ordinary function or user copies
!   of the kernel entries' first instructions; also no pair handed over
!   and wbs beyond the window;
!   part 2, the 16-bit part of an extended register (movif .. movfi)
!   across a page fault, a signal, system calls, a deep call and timer
!   interrupts, under setwd nfx = 1 and nfx = 0, also in %g16/%g20, and
!   whether ordinary writes of the register keep it.
	.ignore	ld_st_style
	.text

! ---------------------------------------------------------------- part 1
! long pa_tP (long (*b) (long, long, long), long x, long y, long z)
! calls b (x, y, z) handing over P pairs (wsz 16, wbs 16 - P): x and y
! reach b's %dr0, %dr1 (pair 0), z its %dr2 when P >= 2.  Returns b's
! result.
.macro	tramp p, r0, r1, r2, wbs
	.global	pa_t\p
	.type	pa_t\p, #function
	.align	8
pa_t\p:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	}
	.if	\p >= 2
	{
	  movtd,0	%dr0, %ctpr1
	  addd,1	0x0, %dr1, %dr\r0
	  addd,2	0x0, %dr2, %dr\r1
	  addd,3	0x0, %dr3, %dr\r2
	}
	.else
	{
	  movtd,0	%dr0, %ctpr1
	  addd,1	0x0, %dr1, %dr\r0
	  addd,2	0x0, %dr2, %dr\r1
	}
	.endif
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = \wbs
	}
	{
	  return	%ctpr3
	  addd,0	0x0, %dr\r0, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pa_t\p, .-pa_t\p
.endm
	! For P = 1, b's z is not passed (its %dr2 is outside pair 0).
	tramp	1, 30, 31, 32, 0xf
	tramp	2, 28, 29, 30, 0xe
	tramp	3, 26, 27, 28, 0xd
	tramp	4, 24, 25, 26, 0xc

! long pa_K_wW_pP (long x, long y, long z), called with P pairs: sets
! wsz = W + P, puts x, y (and z when P >= 2) at pair W and calls with
! wbs = W, so the callee gets the same P pairs and W pairs stay below.
!   K = s3: system call through entry 3, number x, arguments y, z;
!   K = s6: fast system call (entry 6), number x, arguments y, z;
!   K = u:  ordinary call of the function at x, with (x, y, z).
! Returns the callee's %dr0.
.macro	frame k, w, p, r0, r1, r2
	.global	pa_\k\()_w\w\()_p\p
	.type	pa_\k\()_w\w\()_p\p, #function
	.align	8
pa_\k\()_w\w\()_p\p:
	{
	  setwd	wsz = \w + \p, nfx = 0x1, dbl = 0x0
	}
	.ifc	\k, s3
	{
	  sdisp	%ctpr1, 0x3
	}
	.endif
	.ifc	\k, s6
	{
	  sdisp	%ctpr1, 0x6
	}
	.endif
	.ifc	\k, u
	{
	  movtd,0	%dr0, %ctpr1
	}
	.endif
	.if	\p >= 2
	{
	  addd,0	0x0, %dr0, %dr\r0
	  addd,1	0x0, %dr1, %dr\r1
	  addd,2	0x0, %dr2, %dr\r2
	}
	.else
	{
	  addd,0	0x0, %dr0, %dr\r0
	  addd,1	0x0, %dr1, %dr\r1
	}
	.endif
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = \w
	}
	{
	  return	%ctpr3
	  addd,0	0x0, %dr\r0, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pa_\k\()_w\w\()_p\p, .-pa_\k\()_w\w\()_p\p
.endm

.macro	row w, r0, r1, r2
	.irp	p, 1, 2, 3, 4
	.irp	k, s3, s6, u
	frame	\k, \w, \p, \r0, \r1, \r2
	.endr
	.endr
.endm
	row	0, 0, 1, 2
	row	1, 2, 3, 4
	row	2, 4, 5, 6
	row	3, 6, 7, 8
	row	4, 8, 9, 10
	row	5, 10, 11, 12
	row	8, 16, 17, 18

! Callees for K = u, called as f (f, y, z) with 1-4 pairs; each returns
! y + 1.  pa_leaf: setwd wsz = 4 only.  pa_vf4: the first instruction
! of the kernel's entries 1, 3 and 4 (VFRPSZ_SETWD (4), kernel
! include/asm/trap_table.S.h:49).  pa_vf6: that of entry 6 (kernel
! kernel/trap_table.S, ttable_entry6).
	.global	pa_leaf
	.type	pa_leaf, #function
	.align	8
pa_leaf:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	%dr1, 0x1, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pa_leaf, .-pa_leaf

	.global	pa_vf4
	.type	pa_vf4, #function
	.align	8
pa_vf4:
	{
	  vfrpsz	rpsz = 0x4
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  setbp	psz = 0x0
	  return	%ctpr3
	  addd,0	%dr1, 0x1, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pa_vf4, .-pa_vf4

	.global	pa_vf6
	.type	pa_vf6, #function
	.align	8
pa_vf6:
	{
	  vfrpsz	rpsz = 0x4
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	  setbp	psz = 0x0
	  return	%ctpr3
	  addd,0	%dr1, 0x1, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pa_vf6, .-pa_vf6

! ---------------------------------------------------------------- part 2
! Every function builds the extended value (mant, exp) in %r6 with
! movif in its first instruction, does something, and returns the
! 16-bit part read back with movfi; out[0] gets the ordinary 64-bit
! part, out[1] what the something returned.  NFX is the setwd's nfx.

! unsigned long pe_fault_nfxN (mant, exp, long *addr, unsigned long *out)
! loads *addr: a page fault, or a signal whose handler fixes the page.
.macro	fault nfx
	.global	pe_fault_nfx\nfx
	.type	pe_fault_nfx\nfx, #function
	.align	8
pe_fault_nfx\nfx:
	{
	  setwd	wsz = 0x8, nfx = \nfx, dbl = 0x0
	  movif,1	%dr0, %r1, %r6
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr2, 0x0, %dr8
	}
	{
	  nop 3
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	  std,2	%dr3, 0x0, %dr6
	  std,5	%dr3, 0x8, %dr8
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pe_fault_nfx\nfx, .-pe_fault_nfx\nfx
.endm
	fault	1
	fault	0

! pe_faultbig_nfxN: the same with a window of 96 pairs, so that the
! signal handler's frames no longer fit beside it in the register file.
.macro	faultbig nfx
	.global	pe_faultbig_nfx\nfx
	.type	pe_faultbig_nfx\nfx, #function
	.align	8
pe_faultbig_nfx\nfx:
	{
	  setwd	wsz = 0x60, nfx = \nfx, dbl = 0x0
	  movif,1	%dr0, %r1, %r6
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr2, 0x0, %dr8
	}
	{
	  nop 3
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	  std,2	%dr3, 0x0, %dr6
	  std,5	%dr3, 0x8, %dr8
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pe_faultbig_nfx\nfx, .-pe_faultbig_nfx\nfx
.endm
	faultbig	1
	faultbig	0

! unsigned long pe_sys_nfxN (mant, exp, long nr, unsigned long *out,
!                            long a1, long a2)
! makes system call nr (a1, a2) through entry 3 (4 pairs handed over).
.macro	sys nfx
	.global	pe_sys_nfx\nfx
	.type	pe_sys_nfx\nfx, #function
	.align	8
pe_sys_nfx\nfx:
	{
	  setwd	wsz = 0xc, nfx = \nfx, dbl = 0x0
	  movif,1	%dr0, %r1, %r6
	  sdisp	%ctpr1, 0x3
	}
	{
	  addd,0	0x0, %dr2, %dr16
	  addd,1	0x0, %dr4, %dr17
	  addd,2	0x0, %dr5, %dr18
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	  std,2	%dr3, 0x0, %dr6
	  std,5	%dr3, 0x8, %dr16
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pe_sys_nfx\nfx, .-pe_sys_nfx\nfx
.endm
	sys	1
	sys	0

! unsigned long pe_call_nfxN (mant, exp, long (*f) (long), unsigned long *out,
!                             long a)
! calls f (a) (a recursion deep enough to spill this window).
.macro	callf nfx
	.global	pe_call_nfx\nfx
	.type	pe_call_nfx\nfx, #function
	.align	8
pe_call_nfx\nfx:
	{
	  setwd	wsz = 0xc, nfx = \nfx, dbl = 0x0
	  movif,1	%dr0, %r1, %r6
	  movtd,0	%dr2, %ctpr1
	}
	{
	  addd,0	0x0, %dr4, %dr16
	}
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	  std,2	%dr3, 0x0, %dr6
	  std,5	%dr3, 0x8, %dr16
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pe_call_nfx\nfx, .-pe_call_nfx\nfx
.endm
	callf	1
	callf	0

! unsigned long pe_spin_nfxN (mant, exp, long n, unsigned long *out)
! counts n down to 0 in a loop of three instructions (timer interrupts,
! or signals the C side arranges, arrive meanwhile); out[1] = %clkr
! cycles spent.
.macro	spin nfx
	.global	pe_spin_nfx\nfx
	.type	pe_spin_nfx\nfx, #function
	.align	8
pe_spin_nfx\nfx:
	{
	  setwd	wsz = 0x8, nfx = \nfx, dbl = 0x0
	  movif,1	%dr0, %r1, %r6
	  disp	%ctpr1, 1f
	  rrd,0	%clkr, %dr9
	}
1:
	{
	  subd,0	%dr2, 0x1, %dr2
	}
	{
	  cmpedb,0	%dr2, 0x0, %pred0
	}
	{
	  ct	%ctpr1 ? ~%pred0
	}
	{
	  rrd,0	%clkr, %dr8
	}
	{
	  nop 2
	}
	{
	  subd,0	%dr8, %dr9, %dr8
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	  std,2	%dr3, 0x0, %dr6
	  std,5	%dr3, 0x8, %dr8
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pe_spin_nfx\nfx, .-pe_spin_nfx\nfx
.endm
	spin	1
	spin	0

! unsigned long pe_call8_nfxN (mant, exp, long (*f) (long), unsigned long *out,
!                              long a)
! pe_call with ldouble.s's window: wsz 8, wbs 4 (4 pairs below the callee).
.macro	call8 nfx
	.global	pe_call8_nfx\nfx
	.type	pe_call8_nfx\nfx, #function
	.align	8
pe_call8_nfx\nfx:
	{
	  setwd	wsz = 0x8, nfx = \nfx, dbl = 0x0
	  movif,1	%dr0, %r1, %r6
	  movtd,0	%dr2, %ctpr1
	}
	{
	  addd,0	0x0, %dr4, %dr8
	}
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	  std,2	%dr3, 0x0, %dr6
	  std,5	%dr3, 0x8, %dr8
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pe_call8_nfx\nfx, .-pe_call8_nfx\nfx
.endm
	call8	1
	call8	0

! unsigned long pe_wr_K (mant, exp, long *p, unsigned long *out)
! movif, then an ordinary write of the same register, then movfi: does
! the write keep the 16-bit part?  K = addd (64-bit integer result),
! adds (32-bit, dbl = 0), ldd (a load of *p), faddd (a double result).
.macro	wr k, op
	.global	pe_wr_\k
	.type	pe_wr_\k, #function
	.align	8
pe_wr_\k:
	{
	  setwd	wsz = 0x8, nfx = 0x0, dbl = 0x0
	  movif,1	%dr0, %r1, %r6
	}
	{
	  nop 5
	}
	{
	  \op
	}
	{
	  nop 5
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	  std,2	%dr3, 0x0, %dr6
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pe_wr_\k, .-pe_wr_\k
.endm
	wr	addd, "addd,0 0x0, 0x5, %dr6"
	wr	adds, "adds,0 0x0, 0x5, %r6"
	wr	ldd, "ldd,0 %dr2, 0x0, %dr6"
	wr	faddd, "faddd,0 %dr2, %dr2, %dr6"

! long pa_u_p0 (long f, long y, long z): wsz = 4, calls f with wbs = 4,
! handing over no pairs.  long pa_u_over (...): wsz = 4, calls f with
! wbs = 5, beyond its window.  Both return the callee's %dr0 (which
! for pa_u_p0 is f's y + 1 with y whatever f's %dr1 holds).
	.global	pa_u_p0
	.type	pa_u_p0, #function
	.align	8
pa_u_p0:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  movtd,0	%dr0, %ctpr1
	}
	{
	  nop 7
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
	}
	.size	pa_u_p0, .-pa_u_p0

	.global	pa_u_over
	.type	pa_u_over, #function
	.align	8
pa_u_over:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  movtd,0	%dr0, %ctpr1
	}
	{
	  nop 7
	}
	{
	  call	%ctpr1, wbs = 0x5
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
	.size	pa_u_over, .-pa_u_over

! unsigned long pe_gfault (mant, exp, long *addr, unsigned long *out)
! pe_fault with the extended value in the global registers %g16 and %g20
! (lcc keeps short-lived ones in %g16, %g17): returns %g16's 16-bit part,
! out[0] its ordinary part, out[1] %g20's 16-bit part.
	.global	pe_gfault
	.type	pe_gfault, #function
	.align	8
pe_gfault:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  movif,1	%dr0, %r1, %g16
	  movif,4	%dr0, %r1, %g20
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr2, 0x0, %dr8
	}
	{
	  nop 3
	}
	{
	  movfi,1	%g16, %r0
	  movfi,4	%g20, %r9
	  std,2	%dr3, 0x0, %dg16
	}
	{
	  nop 3
	}
	{
	  return	%ctpr3
	  std,5	%dr3, 0x8, %dr9
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	pe_gfault, .-pe_gfault
