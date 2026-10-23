! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for foldi56_undef.c: diagnostic values left in a window's
! registers for the next callee to find.
	.ignore	ld_st_style
	.ignore	strict_delay
	.text
! spoil_low(void *bad): a speculative ldd from BAD into %dr2..%dr7, the
! inherited pairs beyond the first argument pair, so that each holds a
! diagnostic value when this returns (the caller's %b[2]..%b[7]).
	.global	spoil_low
	.type	spoil_low, #function
	.align	8
spoil_low:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  ldd,0,sm	%dr0, 0x0, %dr2
	  ldd,2,sm	%dr0, 0x0, %dr3
	  ldd,3,sm	%dr0, 0x0, %dr4
	  ldd,5,sm	%dr0, 0x0, %dr5
	}
	{
	  ldd,0,sm	%dr0, 0x0, %dr6
	  ldd,2,sm	%dr0, 0x0, %dr7
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	spoil_low, .- spoil_low
! spoil_high(void *bad): setwd 8, then a speculative ldd from BAD into
! %dr13, a register the window gained (tagempty.c's probe_n_set with a
! diagnostic value).
	.global	spoil_high
	.type	spoil_high, #function
	.align	8
spoil_high:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	}
	{
	  ldd,0,sm	%dr0, 0x0, %dr13
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	spoil_high, .- spoil_high
! get_high(long *out): setwd 8, then a plain std of %dr13, which this
! window gained (tagempty.c's probe_n_get with a non-speculative store).
	.global	get_high
	.type	get_high, #function
	.align	8
get_high:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr3
	  std,2	%dr0, 0x0, %dr13
	}
	.size	get_high, .- get_high
