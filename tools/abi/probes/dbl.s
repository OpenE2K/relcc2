! SPDX-License-Identifier: GPL-3.0-or-later
! Which 32-bit operations WD.dbl affects; see dbl.c.
	.ignore	ld_st_style
	.ignore	strict_delay
	.text
	.global	p_adds_0
p_adds_0:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  adds,0 0x0, %r2, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_shls_0
p_shls_0:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  shls,0 %r2, 0x0, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_muls_0
p_muls_0:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  muls,0 %r2, 0x1, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_ldw_0
p_ldw_0:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  ldw,0 %r1, 0x0, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_fadds_0
p_fadds_0:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  fadds,0 %r2, %r2, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_movts_0
p_movts_0:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  movts,0 %r2, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_sxt_0
p_sxt_0:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  sxt,0 0x2, %r2, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_adds_1
p_adds_1:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  adds,0 0x0, %r2, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_shls_1
p_shls_1:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  shls,0 %r2, 0x0, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_muls_1
p_muls_1:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  muls,0 %r2, 0x1, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_ldw_1
p_ldw_1:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  ldw,0 %r1, 0x0, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_fadds_1
p_fadds_1:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  fadds,0 %r2, %r2, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_movts_1
p_movts_1:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  movts,0 %r2, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.global	p_sxt_1
p_sxt_1:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  ldd,0	%r0, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  sxt,0 0x2, %r2, %r3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
