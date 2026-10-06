! SPDX-License-Identifier: GPL-3.0-or-later
! Control and system operations for ctlops.c, one per function.
	.ignore	ld_st_style
	.text
	.global	t_flushr
	.type	t_flushr, #function
	.align	8
t_flushr:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  flushr
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_flushc
	.type	t_flushc, #function
	.align	8
t_flushc:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  flushc
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_flushts
	.type	t_flushts, #function
	.align	8
t_flushts:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  flushts
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_pref
	.type	t_pref, #function
	.align	8
t_pref:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  pref	%ipr0, t_pref
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_sdisp
	.type	t_sdisp, #function
	.align	8
t_sdisp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  sdisp	%ctpr1, 0x3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_gettsd
	.type	t_gettsd, #function
	.align	8
t_gettsd:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  gettsd	%ctpr1
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_vfrpsz
	.type	t_vfrpsz, #function
	.align	8
t_vfrpsz:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  vfrpsz	rpsz = 0x0
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_settr
	.type	t_settr, #function
	.align	8
t_settr:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  settr	type = 0x0
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_setei
	.type	t_setei, #function
	.align	8
t_setei:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  setei	0x0
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_setsft
	.type	t_setsft, #function
	.align	8
t_setsft:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  setsft
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_srp
	.type	t_srp, #function
	.align	8
t_srp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  srp
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_crp
	.type	t_crp, #function
	.align	8
t_crp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  crp
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_slrp
	.type	t_slrp, #function
	.align	8
t_slrp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  slrp
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_vfdi
	.type	t_vfdi, #function
	.align	8
t_vfdi:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  vfdi
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_set_mark
	.type	t_set_mark, #function
	.align	8
t_set_mark:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  set_mark
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_movep
	.type	t_movep, #function
	.align	8
t_movep:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  cmpesb,0	0x0, 0x0, %pred0
	  cmpesb,1	0x0, 0x1, %pred1
	}
	{
	  pass	%pred0, @p0
	  pass	%pred1, @p1
	  movep	@p0, @p1, @p4
	  pass	@p4, %pred2
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_done
	.type	t_done, #function
	.align	8
t_done:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  done
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	t_clpandp
	.type	t_clpandp, #function
	.align	8
t_clpandp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  cmpesb,0	0x0, 0x0, %pred0
	  cmpesb,1	0x0, 0x0, %pred1
	}
	clpandp	%pred0, %pred1, %pred2
	clplandp	%pred0, %pred1, %pred3
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
