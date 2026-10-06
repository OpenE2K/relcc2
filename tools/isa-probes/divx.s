! SPDX-License-Identifier: GPL-3.0-or-later
! The 64/32 divides for divx.c: unsigned long OP (unsigned long x, unsigned long y)
	.ignore	ld_st_style
	.text
	.global	sdivx_
	.type	sdivx_, #function
	.align	8
sdivx_:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,1	0x0, _f64,_lts0 0x5555555555555555, %dr2
	}
	{
	  sdivx,5	%dr0, %r1, %r2
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	sdivx_, .-sdivx_
	.global	udivx_
	.type	udivx_, #function
	.align	8
udivx_:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,1	0x0, _f64,_lts0 0x5555555555555555, %dr2
	}
	{
	  udivx,5	%dr0, %r1, %r2
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	udivx_, .-udivx_
	.global	smodx_
	.type	smodx_, #function
	.align	8
smodx_:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,1	0x0, _f64,_lts0 0x5555555555555555, %dr2
	}
	{
	  smodx,5	%dr0, %r1, %r2
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	smodx_, .-smodx_
	.global	umodx_
	.type	umodx_, #function
	.align	8
umodx_:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,1	0x0, _f64,_lts0 0x5555555555555555, %dr2
	}
	{
	  umodx,5	%dr0, %r1, %r2
	}
	{
	  nop 7
	}
	{
	  nop 7
	}
	{
	  addd,0	0x0, %dr2, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	umodx_, .-umodx_
