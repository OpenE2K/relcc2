! SPDX-License-Identifier: GPL-3.0-or-later
! long reenter (jmp_buf buf, long depth): _setjmp (buf), write the
! address of .Lt over buf's saved IP (byte 8), set a local to 0x5678,
! call reenter_deep (depth), which longjmps to buf from DEPTH frames
! down.  At .Lt it returns the local: 0x5678 if the frame was re-entered
! at the label with the registers it had at the call; -1 if _setjmp
! returned a second time, -2 if reenter_deep returned.
	.ignore	ld_st_style
	.text
	.global	reenter
	.type	reenter, #function
	.align	8
reenter:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, _setjmp
	  addd,0	0x0, %dr0, %dr4
	  addd,1	0x0, %dr1, %dr7
	}
	{
	  addd,0	0x0, %dr0, %dr8
	  addd,1	0x0, 0x1234, %dr5
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  cmpedb,0	%dr8, 0x0, %pred0
	  addd,1	0x0, [ _f64,_lts0 .Lt ], %dr6
	  disp	%ctpr1, reenter_deep
	}
	{
	  std,2	%dr4, 0x8, %dr6
	  addd,0	0x0, 0x5678, %dr5
	  addd,1	0x0, %dr7, %dr8
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x4 ? %pred0
	}
	{
	  return	%ctpr3
	  subd,0	0x0, 0x1, %dr0 ? ~%pred0
	  subd,1	0x0, 0x2, %dr0 ? %pred0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Lt:
	{
	  return	%ctpr3
	  addd,0	0x0, %dr5, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	reenter, .-reenter
