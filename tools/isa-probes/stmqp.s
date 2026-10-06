! SPDX-License-Identifier: GPL-3.0-or-later
! stmqp for stmqp.c: void stm (void *addr, unsigned mask, const u64 v[2])
	.ignore	ld_st_style
	.text
	.global	stm
	.type	stm, #function
	.align	8
stm:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldqp,0	%dr2, 0x0, %qpr4
	}
	{
	  nop 5
	}
	{
	  stmqp,2	%dr0, %r1, %qpr4
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	stm, .-stm
