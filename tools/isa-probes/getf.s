! SPDX-License-Identifier: GPL-3.0-or-later
! getfs and getfd with a register field descriptor, for getf.c:
! unsigned long getfd_ (unsigned long x, unsigned long desc), getfs_ likewise.
	.ignore	ld_st_style
	.text
	.global	getfd_
	.type	getfd_, #function
	.align	8
getfd_:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  getfd,0	%dr0, %dr1, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	getfd_, .-getfd_
	.global	getfs_
	.type	getfs_, #function
	.align	8
getfs_:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  addd,1	0x0, _f64,_lts0 0x5555555555555555, %dr2
	}
	{
	  getfs,0	%r0, %r1, %r2
	}
	{
	  ct	%ctpr3
	  addd,0	0x0, %dr2, %dr0
	}
	.size	getfs_, .-getfs_
