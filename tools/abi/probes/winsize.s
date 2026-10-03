! SPDX-License-Identifier: GPL-3.0-or-later
! Callers with setwd wsz = 8 and call wbs = 4, 6, 7 (so the callee inherits
! 4, 2, 1 pairs of the caller's window) and callees with wsz = 1..5; see
! winsize.c.
	.ignore	ld_st_style
	.text

	.type	w_callee_4_1, #function
	.align	8
w_callee_4_1:
	{
	  setwd	wsz = 0x1, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_4_1, .-w_callee_4_1
	.global	w_4_1
	.type	w_4_1, #function
	.align	8
w_4_1:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_4_1
	}
	{
	  nop 5
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
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_4_1, .-w_4_1

	.type	w_callee_4_2, #function
	.align	8
w_callee_4_2:
	{
	  setwd	wsz = 0x2, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_4_2, .-w_callee_4_2
	.global	w_4_2
	.type	w_4_2, #function
	.align	8
w_4_2:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_4_2
	}
	{
	  nop 5
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
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_4_2, .-w_4_2

	.type	w_callee_4_3, #function
	.align	8
w_callee_4_3:
	{
	  setwd	wsz = 0x3, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_4_3, .-w_callee_4_3
	.global	w_4_3
	.type	w_4_3, #function
	.align	8
w_4_3:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_4_3
	}
	{
	  nop 5
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
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_4_3, .-w_4_3

	.type	w_callee_4_4, #function
	.align	8
w_callee_4_4:
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
	.size	w_callee_4_4, .-w_callee_4_4
	.global	w_4_4
	.type	w_4_4, #function
	.align	8
w_4_4:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_4_4
	}
	{
	  nop 5
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
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_4_4, .-w_4_4

	.type	w_callee_4_5, #function
	.align	8
w_callee_4_5:
	{
	  setwd	wsz = 0x5, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_4_5, .-w_callee_4_5
	.global	w_4_5
	.type	w_4_5, #function
	.align	8
w_4_5:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_4_5
	}
	{
	  nop 5
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
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_4_5, .-w_4_5

	.type	w_callee_6_1, #function
	.align	8
w_callee_6_1:
	{
	  setwd	wsz = 0x1, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_6_1, .-w_callee_6_1
	.global	w_6_1
	.type	w_6_1, #function
	.align	8
w_6_1:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_6_1
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x6
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_6_1, .-w_6_1

	.type	w_callee_6_2, #function
	.align	8
w_callee_6_2:
	{
	  setwd	wsz = 0x2, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_6_2, .-w_callee_6_2
	.global	w_6_2
	.type	w_6_2, #function
	.align	8
w_6_2:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_6_2
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x6
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_6_2, .-w_6_2

	.type	w_callee_6_3, #function
	.align	8
w_callee_6_3:
	{
	  setwd	wsz = 0x3, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_6_3, .-w_callee_6_3
	.global	w_6_3
	.type	w_6_3, #function
	.align	8
w_6_3:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_6_3
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x6
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_6_3, .-w_6_3

	.type	w_callee_6_4, #function
	.align	8
w_callee_6_4:
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
	.size	w_callee_6_4, .-w_callee_6_4
	.global	w_6_4
	.type	w_6_4, #function
	.align	8
w_6_4:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_6_4
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x6
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_6_4, .-w_6_4

	.type	w_callee_6_5, #function
	.align	8
w_callee_6_5:
	{
	  setwd	wsz = 0x5, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_6_5, .-w_callee_6_5
	.global	w_6_5
	.type	w_6_5, #function
	.align	8
w_6_5:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_6_5
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x6
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_6_5, .-w_6_5

	.type	w_callee_7_1, #function
	.align	8
w_callee_7_1:
	{
	  setwd	wsz = 0x1, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_7_1, .-w_callee_7_1
	.global	w_7_1
	.type	w_7_1, #function
	.align	8
w_7_1:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_7_1
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x7
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_7_1, .-w_7_1

	.type	w_callee_7_2, #function
	.align	8
w_callee_7_2:
	{
	  setwd	wsz = 0x2, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_7_2, .-w_callee_7_2
	.global	w_7_2
	.type	w_7_2, #function
	.align	8
w_7_2:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_7_2
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x7
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_7_2, .-w_7_2

	.type	w_callee_7_3, #function
	.align	8
w_callee_7_3:
	{
	  setwd	wsz = 0x3, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_7_3, .-w_callee_7_3
	.global	w_7_3
	.type	w_7_3, #function
	.align	8
w_7_3:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_7_3
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x7
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_7_3, .-w_7_3

	.type	w_callee_7_4, #function
	.align	8
w_callee_7_4:
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
	.size	w_callee_7_4, .-w_callee_7_4
	.global	w_7_4
	.type	w_7_4, #function
	.align	8
w_7_4:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_7_4
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x7
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_7_4, .-w_7_4

	.type	w_callee_7_5, #function
	.align	8
w_callee_7_5:
	{
	  setwd	wsz = 0x5, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	w_callee_7_5, .-w_callee_7_5
	.global	w_7_5
	.type	w_7_5, #function
	.align	8
w_7_5:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, w_callee_7_5
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x7
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  addd,0 0x0, 0x1, %dr0
	}
	.size	w_7_5, .-w_7_5
