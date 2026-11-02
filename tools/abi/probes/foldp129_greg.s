! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for foldp129_greg.c: a diagnostic value in the global %g24,
! left by a callee or by the caller, read by a function that never
! wrote %g24.
	.ignore	ld_st_style
	.ignore	strict_delay
	.text
! g24_spoil(void *bad): a failed speculative ldd from BAD into %dg24,
! which stays there when this returns.
	.global	g24_spoil
	.type	g24_spoil, #function
	.align	8
g24_spoil:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  ldd,0,sm	%dr0, 0x0, %dg24
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	g24_spoil, .- g24_spoil
! g24_peek(long *out): %dg24's bits (std,sm, which doesn't trap) into
! out[0] and its tag (gettagd) into out[1].
	.global	g24_peek
	.type	g24_peek, #function
	.align	8
g24_peek:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  gettagd,2	%dg24, %dr2
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	  std,2,sm	%dr0, 0x0, %dg24
	  std,5	%dr0, 0x8, %dr2
	}
	.size	g24_peek, .- g24_peek
! The readers: each reads %dg24, which it never wrote, with one plain
! operation, and stores what it got into *out (argument %dr0).
! g24_std: a plain std of %dg24 (bison's yylval in foldi56_undef.c).
	.global	g24_std
	.type	g24_std, #function
	.align	8
g24_std:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  std,2	%dr0, 0x0, %dg24
	}
	.size	g24_std, .- g24_std
! g24_addd: addd %dg24, 1 (P129's step of an unused offset).
	.global	g24_addd
	.type	g24_addd, #function
	.align	8
g24_addd:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  addd,0	%dg24, 0x1, %dr2
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  std,2	%dr0, 0x0, %dr2
	}
	.size	g24_addd, .- g24_addd
! g24_muld: muld %dg24, 3 (P124's stride of an absent mask).
	.global	g24_muld
	.type	g24_muld, #function
	.align	8
g24_muld:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  muld,0	%dg24, 0x3, %dr2
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  std,2	%dr0, 0x0, %dr2
	}
	.size	g24_muld, .- g24_muld
! g24_cmp: cmpedb %dg24, 3 (P124's extent of an absent argument);
! stores 1 if equal, else 0.
	.global	g24_cmp
	.type	g24_cmp, #function
	.align	8
g24_cmp:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  cmpedb,0	%dg24, 0x3, %pred0
	  addd,1	0x0, 0x0, %dr2
	}
	{
	  nop 2
	}
	{
	  addd,1	0x0, 0x1, %dr2 ? %pred0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr3
	  std,2	%dr0, 0x0, %dr2
	}
	.size	g24_cmp, .- g24_cmp
! g24_caller(void *bad, void (*fn)(long *), long *out): the caller
! leaves the diagnostic value itself (ldd,sm into %dg24), then calls
! FN (OUT).  wsz = 8, wbs = 6: FN's %dr0 is this frame's %dr12.
	.global	g24_caller
	.type	g24_caller, #function
	.align	8
g24_caller:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  movtd,0	%dr1, %ctpr1
	  ldd,3,sm	%dr0, 0x0, %dg24
	}
	{
	  addd,1	0x0, %dr2, %dr12
	}
	{
	  nop 7
	}
	{
	  nop 2
	}
	{
	  call	%ctpr1, wbs = 0x6
	}
	{
	  return	%ctpr3; ipd 2
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	g24_caller, .- g24_caller
! g24_after(void *bad, void (*fn)(long *), long *out): calls
! g24_spoil (BAD), then FN (OUT) with nothing in between: what the
! first callee left in %g24 reaches the second.
	.global	g24_after
	.type	g24_after, #function
	.align	8
g24_after:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  disp	%ctpr1, g24_spoil; ipd 2
	}
	{
	  addd,0	0x0, %dr0, %dr12
	  addd,1	0x0, %dr1, %dr4
	  addd,2	0x0, %dr2, %dr5
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x6
	}
	{
	  movtd,0	%dr4, %ctpr1
	  addd,1	0x0, %dr5, %dr12
	}
	{
	  nop 7
	}
	{
	  nop 2
	}
	{
	  call	%ctpr1, wbs = 0x6
	}
	{
	  return	%ctpr3; ipd 2
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	g24_after, .- g24_after
! g24_sys(void *bad, void (*fn)(long *), long *out): as g24_caller,
! with a system call (getpid, entry 3) between the ldd,sm and the call
! of FN: system calls empty %g16-%g31 (kernel-interface.md).
	.global	g24_sys
	.type	g24_sys, #function
	.align	8
g24_sys:
	{
	  setwd	wsz = 0xc, nfx = 0x1, dbl = 0x0
	  sdisp	%ctpr1, 0x3
	  ldd,3,sm	%dr0, 0x0, %dg24
	}
	{
	  addd,0	0x0, 0x14, %dr16
	  addd,1	0x0, %dr1, %dr4
	  addd,2	0x0, %dr2, %dr5
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  movtd,0	%dr4, %ctpr1
	  addd,1	0x0, %dr5, %dr16
	}
	{
	  nop 7
	}
	{
	  nop 2
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  return	%ctpr3; ipd 2
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	g24_sys, .- g24_sys
