! SPDX-License-Identifier: GPL-3.0-or-later
! Process entry stub for glibcbuild_entry.c (build and run instructions
! there).  The program is linked with -Wl,-e,glibcbuild_entry, so this
! runs before crt1.o's _start: on the kernel's entry state in a static
! program, on ld.so's in a dynamic one.  It records the state in
! glibcbuild_state and jumps (ct, not call) to _start, which then sees
! %dr0-%dr7, the global registers, the predicates, USD and the chain
! registers as they were; only the window size (setwd here) and the
! registers the larger window adds (%dr16 and up) differ.  Only reads of
! registers other probes read in user mode (stackregs.s, sysabi.s,
! sregs.s): nothing here writes a state register.
!
! glibcbuild_state, 64-bit words:
!   0-7      %dr0-%dr7 (std,sm: the bits, whatever the tag)
!   8-15     their tags (gettagd)
!   16-47    %dg0-%dg31
!   48-79    their tags
!   80-111   %pred0-%pred31: merged,sm 0, 1 (0 false, 1 true)
!   112-143  the tags of those results (5: the predicate was diagnostic)
!   144-     the state registers, in the order of SREG below
!   175      1 once this stub has run
	.ignore	ld_st_style
	.text
	.global	glibcbuild_entry
	.type	glibcbuild_entry, #function
	.align	8
glibcbuild_entry:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	}
	{
	  addd,0	0x0, [ _f64,_lts0 glibcbuild_state ], %dr16
	}

	! %dr0-%dr7 and %dg0-%dg31: the bits and the tag.
	.macro	saveg reg, slot, tagslot
	{
	  std,2,sm	%dr16, \slot * 8, \reg
	  gettagd,5	\reg, %r18
	}
	{
	  stw,2	%dr16, \tagslot * 8, %r18
	}
	.endm

	saveg	%dr0, 0, 8
	saveg	%dr1, 1, 9
	saveg	%dr2, 2, 10
	saveg	%dr3, 3, 11
	saveg	%dr4, 4, 12
	saveg	%dr5, 5, 13
	saveg	%dr6, 6, 14
	saveg	%dr7, 7, 15
	.irp	n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	saveg	%dg\n, (16 + \n), (48 + \n)
	.endr

	! The predicates: 0 when false, 1 when true, diagnostic (tag 5)
	! when the predicate is.
	{
	  addd,0	0x0, 0x0, %dr20
	  addd,1	0x0, 0x1, %dr21
	}
	.irp	n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	{
	  merged,0,sm	%dr20, %dr21, %dr22, %pred\n
	}
	saveg	%dr22, (80 + \n), (112 + \n)
	.endr

	! State registers, one rrd/rrs each.
	.macro	sregd reg, slot
	{
	  rrd,0	\reg, %dr22
	}
	{
	  std,2	%dr16, \slot * 8, %dr22
	}
	.endm
	.macro	sregs reg, slot
	{
	  rrs,0	\reg, %r22
	}
	{
	  stw,2	%dr16, \slot * 8, %r22
	}
	.endm

	sregd	%wd, 144
	sregs	%br, 145
	sregd	%usd.lo, 146
	sregd	%usd.hi, 147
	sregd	%cr0.hi, 148
	sregd	%cr1.lo, 149
	sregd	%cr1.hi, 150
	sregd	%psp.lo, 151
	sregd	%psp.hi, 152
	sregd	%pshtp, 153
	sregd	%pcsp.lo, 154
	sregd	%pcsp.hi, 155
	sregd	%pcshtp, 156
	sregd	%sbr, 157
	sregs	%upsr, 158
	sregs	%pfpfr, 159
	sregs	%fpcr, 160
	sregs	%fpsr, 161
	sregd	%lsr, 162
	sregd	%ilcr, 163
	sregs	%bgr, 164
	sregd	%rndpr, 165
	sregd	%rpr.lo, 166
	sregd	%rpr.hi, 167
	{
	  getsp,0	0x0, %dr22
	}
	{
	  std,2	%dr16, 168 * 8, %dr22
	  addd,0	0x0, 0x1, %dr23
	}
	{
	  std,2	%dr16, 175 * 8, %dr23
	  disp	%ctpr1, _start
	}
	{
	  ct	%ctpr1
	}
	.size	glibcbuild_entry, .-glibcbuild_entry
