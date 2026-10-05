! SPDX-License-Identifier: GPL-3.0-or-later
! Global register rotation (abg, %bgr) for abg.c.  Every function is
!   void f (unsigned long *out, unsigned long arg)
! and leaves %bgr as it found it, unless it traps.  %g24..%g31 are
! scratch globals (spec/abi/registers.md).
	.ignore	ld_st_style
	.text

	.macro	head name
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrs,0	%bgr, %r2
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x0, %dr2
	}
	.endm

	! Restore the entry %bgr (%r2) and return.
	.macro	tail name
	{
	  rws,0	%r2, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	! Write %bgr from %r\reg and wait for it to take effect.
	.macro	wbgr reg
	{
	  rws,0	%r\reg, %bgr
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	.endm

	! out[\slot] = %bgr
	.macro	rbgr slot
	{
	  rrs,0	%bgr, %r3
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, \slot * 8, %dr3
	}
	.endm

	! Fill the rotating globals with 0x100 + n as seen now.
	.macro	fill
	{
	  addd,0	0x0, 0x100, %dg24
	  addd,1	0x0, 0x101, %dg25
	  addd,2	0x0, 0x102, %dg26
	  addd,3	0x0, 0x103, %dg27
	  addd,4	0x0, 0x104, %dg28
	  addd,5	0x0, 0x105, %dg29
	}
	{
	  addd,0	0x0, 0x106, %dg30
	  addd,1	0x0, 0x107, %dg31
	}
	{
	  nop 2
	}
	.endm

	! out[\slot..\slot+7] = %dg24..%dg31, read in ALC0..2.
	.macro	dump slot
	{
	  addd,0	0x0, %dg24, %dr8
	  addd,1	0x0, %dg25, %dr9
	  addd,2	0x0, %dg26, %dr10
	}
	{
	  addd,0	0x0, %dg27, %dr11
	  addd,1	0x0, %dg28, %dr12
	  addd,2	0x0, %dg29, %dr13
	}
	{
	  addd,0	0x0, %dg30, %dr14
	  addd,1	0x0, %dg31, %dr15
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, (\slot + 0) * 8, %dr8
	  std,5	%dr0, (\slot + 1) * 8, %dr9
	}
	{
	  std,2	%dr0, (\slot + 2) * 8, %dr10
	  std,5	%dr0, (\slot + 3) * 8, %dr11
	}
	{
	  std,2	%dr0, (\slot + 4) * 8, %dr12
	  std,5	%dr0, (\slot + 5) * 8, %dr13
	}
	{
	  std,2	%dr0, (\slot + 6) * 8, %dr14
	  std,5	%dr0, (\slot + 7) * 8, %dr15
	}
	.endm

	! An abg with field value \n (2 = abgi, 1 = abgd), then a wait.
	.macro	doabg n
	{
	  abg	\n
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	.endm

! Stage 0: out[0] = %bgr.
	head	bgr_read
	tail	bgr_read

! Stage 1: write %bgr unchanged; out[1] = %bgr after.
	head	bgr_same
	wbgr	2
	rbgr	1
	tail	bgr_same

! Stage 2: out[1] = %bgr after %bgr = arg (keep val all ones), out[2..9]
! = %g24..31 after filling them under the entry %bgr.
	head	bgr_set
	fill
	wbgr	1
	rbgr	1
	dump	2
	tail	bgr_set

! Stage 3: one abg under the entry %bgr; out[1] = %bgr after.  No
! global is touched.
	head	abgi
	doabg	2
	rbgr	1
	tail	abgi

	head	abgd
	doabg	1
	rbgr	1
	tail	abgd

	! Both bits at once.
	head	abgid
	doabg	3
	rbgr	1
	tail	abgid

! Stage 4: %bgr = arg, then one abg; out[1] = %bgr after.
	head	abgi_from
	wbgr	1
	doabg	2
	rbgr	1
	tail	abgi_from

	head	abgd_from
	wbgr	1
	doabg	1
	rbgr	1
	tail	abgd_from

! Stage 5: fill under the entry %bgr, abg, out[1] = %bgr, out[2..9] =
! %g24..31 read in ALC0..2 (these may trap).
	head	abgi_dump
	fill
	doabg	2
	rbgr	1
	dump	2
	tail	abgi_dump

	head	abgd_dump
	fill
	doabg	1
	rbgr	1
	dump	2
	tail	abgd_dump

! Stage 6: %bgr = arg with some val bits clear, then %dg24 read in one
! channel into out[2] (may trap: fp_stack_u, SIGFPE).
	.macro	readin ch
	head	read_alc\ch
	fill
	wbgr	1
	rbgr	1
	{
	  addd,\ch	0x0, %dg24, %dr8
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x10, %dr8
	}
	tail	read_alc\ch
	.endm
	readin	0
	readin	1
	readin	2
	readin	3
	readin	4
	readin	5

! Stage 7: %bgr = arg, write %dg24, out[1] = %bgr after (does a write
! set a val bit?), then restore.
	head	write_g24
	wbgr	1
	{
	  addd,0	0x0, 0x55, %dg24
	}
	{
	  nop 2
	}
	rbgr	1
	tail	write_g24

! Stage 8: both abg bits from %bgr = arg.
	head	abgid_from
	wbgr	1
	doabg	3
	rbgr	1
	tail	abgid_from

! Stage 9: %bgr = arg, %dg24 read speculatively in ALC4; out[2] = the
! result's tag (gettagd, which doesn't trap on a diagnostic value),
! out[3] its value when the tag is 0.
	head	read_sm4
	fill
	wbgr	1
	rbgr	1
	{
	  addd,4,sm	0x0, %dg24, %dr8
	}
	{
	  nop 2
	}
	{
	  gettagd,5	%dr8, %r9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x10, %dr9
	  std,5	%dr0, 0x18, %dr8
	}
	tail	read_sm4

! Stage 10: %bgr = arg, then quad reads of the rotating globals:
! out[2..3] = movtq %qg24 (the pair %g24, %g25), out[4..5] = movtq
! %qg30, out[6..7] = qpand %qpg24 (one 128-bit register; v5 builds
! only, assembled with --defsym ABG_V5=1).
	head	quad_read
	fill
	wbgr	1
	rbgr	1
	{
	  movtq,0	%qg24, %qr8
	  movtq,3	%qg30, %qr10
	}
	.ifdef	ABG_V5
	{
	  qpand,0	%qpg24, %qpg24, %qpr12
	}
	.endif
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x10, %dr8
	  std,5	%dr0, 0x18, %dr9
	}
	{
	  std,2	%dr0, 0x20, %dr10
	  std,5	%dr0, 0x28, %dr11
	}
	.ifdef	ABG_V5
	{
	  stqp,2	%dr0, 0x30, %qpr12
	}
	.endif
	tail	quad_read

! Stage 11: %bgr = arg, then a load from a fresh page (out[10], never
! touched, so it faults and the kernel completes it) into %dg24;
! out[2..9] = %g24..31 as seen under arg.
	head	load_g24
	fill
	{
	  ldd,0	%dr0, 0x50, %dr8
	}
	wbgr	1
	rbgr	1
	{
	  ldd,0	%dr8, 0x0, %dg24
	}
	{
	  nop 2
	}
	dump	2
	tail	load_g24

! Stage 12: load_g24, then for %g24..31 (logical order) out[2+n] = the
! value with its tag cleared (puttagd) and out[16+n] = the tag
! (gettagd); neither traps on a diagnostic value.  ALC5 checks val, so
! use it only with all val bits set.
	head	load_g24_tags
	fill
	{
	  ldd,0	%dr0, 0x50, %dr8
	}
	wbgr	1
	rbgr	1
	{
	  ldd,0	%dr8, 0x0, %dg24
	}
	{
	  nop 2
	}
	{
	  gettagd,2	%dg24, %r8
	  puttagd,5	%dg24, 0x0, %dr9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x80, %dr8
	  std,5	%dr0, 0x10, %dr9
	}
	{
	  gettagd,2	%dg25, %r8
	  puttagd,5	%dg25, 0x0, %dr9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x88, %dr8
	  std,5	%dr0, 0x18, %dr9
	}
	{
	  gettagd,2	%dg26, %r8
	  puttagd,5	%dg26, 0x0, %dr9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x90, %dr8
	  std,5	%dr0, 0x20, %dr9
	}
	{
	  gettagd,2	%dg27, %r8
	  puttagd,5	%dg27, 0x0, %dr9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0x98, %dr8
	  std,5	%dr0, 0x28, %dr9
	}
	{
	  gettagd,2	%dg28, %r8
	  puttagd,5	%dg28, 0x0, %dr9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0xa0, %dr8
	  std,5	%dr0, 0x30, %dr9
	}
	{
	  gettagd,2	%dg29, %r8
	  puttagd,5	%dg29, 0x0, %dr9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0xa8, %dr8
	  std,5	%dr0, 0x38, %dr9
	}
	{
	  gettagd,2	%dg30, %r8
	  puttagd,5	%dg30, 0x0, %dr9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0xb0, %dr8
	  std,5	%dr0, 0x40, %dr9
	}
	{
	  gettagd,2	%dg31, %r8
	  puttagd,5	%dg31, 0x0, %dr9
	}
	{
	  nop 2
	}
	{
	  std,2	%dr0, 0xb8, %dr8
	  std,5	%dr0, 0x48, %dr9
	}
	tail	load_g24_tags
