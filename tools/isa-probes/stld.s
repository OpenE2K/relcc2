! SPDX-License-Identifier: GPL-3.0-or-later
! Store-to-load timing for stld.c: 32 steps of a store of %dr0 to
! [%dr1 + 0] and a load into %dr0 from [%dr1 + OFF], each in its own
! instruction (the load waits for nothing but what the hardware imposes),
! so the next store's data depends on the load.  Returns cycles.
	.ignore	ld_st_style
	.text

	.global	stld_same_d
	.type	stld_same_d, #function
	.align	8
stld_same_d:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_same_d, .-stld_same_d

	.global	stld_same_w
	.type	stld_same_w, #function
	.align	8
stld_same_w:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_same_w, .-stld_same_w

	.global	stld_narrow
	.type	stld_narrow, #function
	.align	8
stld_narrow:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_narrow, .-stld_narrow

	.global	stld_wide
	.type	stld_wide, #function
	.align	8
stld_wide:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_wide, .-stld_wide

	.global	stld_off8
	.type	stld_off8, #function
	.align	8
stld_off8:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_off8, .-stld_off8

	.global	stld_off32
	.type	stld_off32, #function
	.align	8
stld_off32:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x20, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_off32, .-stld_off32

	.global	stld_off64
	.type	stld_off64, #function
	.align	8
stld_off64:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_off64, .-stld_off64

	.global	stld_off256
	.type	stld_off256, #function
	.align	8
stld_off256:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x100, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_off256, .-stld_off256

	.global	stld_off4096
	.type	stld_off4096, #function
	.align	8
stld_off4096:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x1000, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_off4096, .-stld_off4096

	.global	stld_load_only
	.type	stld_load_only, #function
	.align	8
stld_load_only:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_load_only, .-stld_load_only

	.global	stld_dist0_same
	.type	stld_dist0_same, #function
	.align	8
stld_dist0_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist0_same, .-stld_dist0_same

	.global	stld_dist0_other
	.type	stld_dist0_other, #function
	.align	8
stld_dist0_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist0_other, .-stld_dist0_other

	.global	stld_dist1_same
	.type	stld_dist1_same, #function
	.align	8
stld_dist1_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist1_same, .-stld_dist1_same

	.global	stld_dist1_other
	.type	stld_dist1_other, #function
	.align	8
stld_dist1_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist1_other, .-stld_dist1_other

	.global	stld_dist2_same
	.type	stld_dist2_same, #function
	.align	8
stld_dist2_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist2_same, .-stld_dist2_same

	.global	stld_dist2_other
	.type	stld_dist2_other, #function
	.align	8
stld_dist2_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist2_other, .-stld_dist2_other

	.global	stld_dist3_same
	.type	stld_dist3_same, #function
	.align	8
stld_dist3_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist3_same, .-stld_dist3_same

	.global	stld_dist3_other
	.type	stld_dist3_other, #function
	.align	8
stld_dist3_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist3_other, .-stld_dist3_other

	.global	stld_dist4_same
	.type	stld_dist4_same, #function
	.align	8
stld_dist4_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist4_same, .-stld_dist4_same

	.global	stld_dist4_other
	.type	stld_dist4_other, #function
	.align	8
stld_dist4_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist4_other, .-stld_dist4_other

	.global	stld_dist6_same
	.type	stld_dist6_same, #function
	.align	8
stld_dist6_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist6_same, .-stld_dist6_same

	.global	stld_dist6_other
	.type	stld_dist6_other, #function
	.align	8
stld_dist6_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist6_other, .-stld_dist6_other

	.global	stld_dist8_same
	.type	stld_dist8_same, #function
	.align	8
stld_dist8_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist8_same, .-stld_dist8_same

	.global	stld_dist8_other
	.type	stld_dist8_other, #function
	.align	8
stld_dist8_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist8_other, .-stld_dist8_other

	.global	stld_dist12_same
	.type	stld_dist12_same, #function
	.align	8
stld_dist12_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist12_same, .-stld_dist12_same

	.global	stld_dist12_other
	.type	stld_dist12_other, #function
	.align	8
stld_dist12_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist12_other, .-stld_dist12_other

	.global	stld_dist16_same
	.type	stld_dist16_same, #function
	.align	8
stld_dist16_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist16_same, .-stld_dist16_same

	.global	stld_dist16_other
	.type	stld_dist16_other, #function
	.align	8
stld_dist16_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr2
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_dist16_other, .-stld_dist16_other

! A store, 0 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use0_same
	.type	stld_use0_same, #function
	.align	8
stld_use0_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use0_same, .-stld_use0_same

! A store, 0 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use0_other
	.type	stld_use0_other, #function
	.align	8
stld_use0_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use0_other, .-stld_use0_other

! A store, 1 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use1_same
	.type	stld_use1_same, #function
	.align	8
stld_use1_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use1_same, .-stld_use1_same

! A store, 1 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use1_other
	.type	stld_use1_other, #function
	.align	8
stld_use1_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use1_other, .-stld_use1_other

! A store, 2 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use2_same
	.type	stld_use2_same, #function
	.align	8
stld_use2_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use2_same, .-stld_use2_same

! A store, 2 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use2_other
	.type	stld_use2_other, #function
	.align	8
stld_use2_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use2_other, .-stld_use2_other

! A store, 3 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use3_same
	.type	stld_use3_same, #function
	.align	8
stld_use3_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use3_same, .-stld_use3_same

! A store, 3 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use3_other
	.type	stld_use3_other, #function
	.align	8
stld_use3_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 2
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use3_other, .-stld_use3_other

! A store, 4 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use4_same
	.type	stld_use4_same, #function
	.align	8
stld_use4_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use4_same, .-stld_use4_same

! A store, 4 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use4_other
	.type	stld_use4_other, #function
	.align	8
stld_use4_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use4_other, .-stld_use4_other

! A store, 5 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use5_same
	.type	stld_use5_same, #function
	.align	8
stld_use5_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use5_same, .-stld_use5_same

! A store, 5 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use5_other
	.type	stld_use5_other, #function
	.align	8
stld_use5_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use5_other, .-stld_use5_other

! A store, 6 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use6_same
	.type	stld_use6_same, #function
	.align	8
stld_use6_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use6_same, .-stld_use6_same

! A store, 6 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use6_other
	.type	stld_use6_other, #function
	.align	8
stld_use6_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 5
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use6_other, .-stld_use6_other

! A store, 8 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use8_same
	.type	stld_use8_same, #function
	.align	8
stld_use8_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use8_same, .-stld_use8_same

! A store, 8 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use8_other
	.type	stld_use8_other, #function
	.align	8
stld_use8_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use8_other, .-stld_use8_other

! A store, 10 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use10_same
	.type	stld_use10_same, #function
	.align	8
stld_use10_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use10_same, .-stld_use10_same

! A store, 10 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use10_other
	.type	stld_use10_other, #function
	.align	8
stld_use10_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 1
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use10_other, .-stld_use10_other

! A store, 12 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use12_same
	.type	stld_use12_same, #function
	.align	8
stld_use12_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use12_same, .-stld_use12_same

! A store, 12 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use12_other
	.type	stld_use12_other, #function
	.align	8
stld_use12_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 3
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use12_other, .-stld_use12_other

! A store, 16 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use16_same
	.type	stld_use16_same, #function
	.align	8
stld_use16_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use16_same, .-stld_use16_same

! A store, 16 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use16_other
	.type	stld_use16_other, #function
	.align	8
stld_use16_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use16_other, .-stld_use16_other

! A store, 24 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use24_same
	.type	stld_use24_same, #function
	.align	8
stld_use24_same:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use24_same, .-stld_use24_same

! A store, 24 empty cycles, a load of the same address (or + 64) whose
! value the next store's data depends on.
	.global	stld_use24_other
	.type	stld_use24_other, #function
	.align	8
stld_use24_other:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
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
	  ldd,0	%dr1, 0x40, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_use24_other, .-stld_use24_other

! Same doubleword, no overlap (or neighbours): stw,2 then ldw,0.
	.global	stld_w0_w4
	.type	stld_w0_w4, #function
	.align	8
stld_w0_w4:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stw,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x4, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_w0_w4, .-stld_w0_w4

! Same doubleword, no overlap (or neighbours): stb,2 then ldb,0.
	.global	stld_b0_b1
	.type	stld_b0_b1, #function
	.align	8
stld_b0_b1:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  stb,2	%dr1, 0x0, %dr0
	}
	{
	  ldb,0	%dr1, 0x1, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_b0_b1, .-stld_b0_b1

! Same doubleword, no overlap (or neighbours): std,2 then ldw,0.
	.global	stld_d0_w8
	.type	stld_d0_w8, #function
	.align	8
stld_d0_w8:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x0, %dr0
	}
	{
	  ldw,0	%dr1, 0x8, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_d0_w8, .-stld_d0_w8

! Same doubleword, no overlap (or neighbours): std,2 then ldd,0.
	.global	stld_q0_d8
	.type	stld_q0_d8, #function
	.align	8
stld_q0_d8:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd,0	%clkr, %dr4
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  std,2	%dr1, 0x8, %dr0
	}
	{
	  ldd,0	%dr1, 0x0, %dr0
	}
	{
	  ord,0	%dr0, 0x0, %dr0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stld_q0_d8, .-stld_q0_d8
