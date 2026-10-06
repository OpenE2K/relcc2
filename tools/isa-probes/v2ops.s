! SPDX-License-Identifier: GPL-3.0-or-later
! v1-v2-only operations for v2ops.c; assemble with -mcpu=elbrus-v2.
	.ignore	ld_st_style
	.text
	.global	t_cast
	.type	t_cast, #function
	.align	8
t_cast:
	{
	  setwd	wsz = 0x8, nfx = 0x1
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x0, %dr2
	  addd,1	0x0, 0x0, %dr3
	  addd,3	0x0, 0x0, %dr4
	}
	{
	  cast,0 %qr2, %qr4, %qr6
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_tdtomp
	.type	t_tdtomp, #function
	.align	8
t_tdtomp:
	{
	  setwd	wsz = 0x8, nfx = 0x1
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x0, %dr2
	  addd,1	0x0, 0x0, %dr3
	  addd,3	0x0, 0x0, %dr4
	}
	{
	  tdtomp,0 %dr2, %dr3, %dr4
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_odtoap
	.type	t_odtoap, #function
	.align	8
t_odtoap:
	{
	  setwd	wsz = 0x8, nfx = 0x1
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x0, %dr2
	  addd,1	0x0, 0x0, %dr3
	  addd,3	0x0, 0x0, %dr4
	}
	{
	  odtoap,0 %qr2, %dr4, %qr6
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_ldodwd
	.type	t_ldodwd, #function
	.align	8
t_ldodwd:
	{
	  setwd	wsz = 0x8, nfx = 0x1
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x0, %dr2
	  addd,1	0x0, 0x0, %dr3
	  addd,3	0x0, 0x0, %dr4
	}
	{
	  ldodwd,0 %qr2, %r4, %dr6
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_gettd
	.type	t_gettd, #function
	.align	8
t_gettd:
	{
	  setwd	wsz = 0x8, nfx = 0x1
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x0, %dr2
	  addd,1	0x0, 0x0, %dr3
	  addd,3	0x0, 0x0, %dr4
	}
	{
	  gettd,0 %dr2, %dr4
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_getsod
	.type	t_getsod, #function
	.align	8
t_getsod:
	{
	  setwd	wsz = 0x8, nfx = 0x1
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x0, %dr2
	  addd,1	0x0, 0x0, %dr3
	  addd,3	0x0, 0x0, %dr4
	}
	{
	  getsod,0 %qr2, %qr4
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_stodwd
	.type	t_stodwd, #function
	.align	8
t_stodwd:
	{
	  setwd	wsz = 0x8, nfx = 0x1
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x0, %dr2
	  addd,1	0x0, 0x0, %dr3
	  addd,3	0x0, 0x0, %dr4
	}
	{
	  stodwd,2 %qr2, %r4, %dr6
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_addd
	.type	t_addd, #function
	.align	8
t_addd:
	{
	  setwd	wsz = 0x8, nfx = 0x1
	  return	%ctpr3
	}
	{
	  addd,0	0x0, 0x0, %dr2
	  addd,1	0x0, 0x0, %dr3
	  addd,3	0x0, 0x0, %dr4
	}
	{
	  addd,0 %dr2, %dr3, %dr4
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
