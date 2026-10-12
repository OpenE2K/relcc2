! SPDX-License-Identifier: GPL-3.0-or-later
! Empty (not diagnostic) tags in the upper half of a spilled register;
! see tagempty.c.  probe_X(long *out, long *unused, int x).
	.ignore	ld_st_style
	.ignore	strict_delay
	.text
! G: an empty doubleword (0, tag 5) from puttagd, a 32-bit adds into its
! low half under dbl = 0, then a plain std.
	.global	probe_g
	.type	probe_g, #function
	.align	8
probe_g:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  puttagd,2	0x0, 0x5, %dr3
	}
	{
	  nop 2
	}
	{
	  adds,0	0x0, %r2, %r3
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %dr3
	}
	.size	probe_g, .- probe_g
! H: as G with adds,sm, the form lcc uses to fill an argument slot.
	.global	probe_h
	.type	probe_h, #function
	.align	8
probe_h:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  puttagd,2	0x0, 0x5, %dr3
	}
	{
	  nop 2
	}
	{
	  adds,0,sm	0x0, %r2, %r3
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %dr3
	}
	.size	probe_h, .- probe_h
! I: the empty doubleword itself stored with a plain std.
	.global	probe_i
	.type	probe_i, #function
	.align	8
probe_i:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  puttagd,2	0x0, 0x5, %dr3
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %dr3
	}
	.size	probe_i, .- probe_i
! J: a register the window grows into (setwd 4, then setwd 8: %r13 is
! new), a 32-bit adds into its low half under dbl = 0, then a plain std.
	.global	probe_j
	.type	probe_j, #function
	.align	8
probe_j:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	}
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	}
	{
	  adds,0	0x0, %r2, %r13
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %dr13
	}
	.size	probe_j, .- probe_j
! K: as J without the adds: the new register stored as it is.
	.global	probe_k
	.type	probe_k, #function
	.align	8
probe_k:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	}
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %dr13
	}
	.size	probe_k, .- probe_k
! L: as J, then copied with a plain 64-bit addd and the copy stored.
	.global	probe_l
	.type	probe_l, #function
	.align	8
probe_l:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	}
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	}
	{
	  adds,0	0x0, %r2, %r13
	}
	{
	  nop 1
	}
	{
	  addd,0	0x0, %dr13, %dr14
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %dr14
	}
	.size	probe_l, .- probe_l
! N: does a call find what an earlier callee left in its registers?
! probe_n_set writes 5555555555555555 to %dr13 (wsz = 8) and returns;
! probe_n_get, called next from the same frame with the same wbs, reads
! %dr13 (std,sm, so that a stale diagnostic value cannot trap) and its
! tag (gettagd).  probe_n_get(long *out, long *tag).  An operation in
! the setwd's own instruction still sees the old window: reading %dr13
! there is the window-bounds exception (SIGSEGV), so the read waits for
! the next instruction.
	.global	probe_n_set
	.type	probe_n_set, #function
	.align	8
probe_n_set:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  addd,0	0x0, _f64 0x5555555555555555, %dr13
	}
	{
	  ct	%ctpr3
	}
	.size	probe_n_set, .- probe_n_set
	.global	probe_n_get
	.type	probe_n_get, #function
	.align	8
probe_n_get:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	}
	{
	  gettagd,2	%dr13, %r14
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2,sm	%r0, 0x0, %dr13
	  stw,5	%r1, 0x0, %r14
	}
	.size	probe_n_get, .- probe_n_get
! O: the same within one function: %dr13 = 6666666666666666 at wsz = 8,
! then setwd 4 and setwd 8 again, then read %dr13 and its tag as N does.
! probe_o(long *out, long *tag).
	.global	probe_o
	.type	probe_o, #function
	.align	8
probe_o:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  addd,0	0x0, _f64 0x6666666666666666, %dr13
	}
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	}
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	}
	{
	  gettagd,2	%dr13, %r14
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2,sm	%r0, 0x0, %dr13
	  stw,5	%r1, 0x0, %r14
	}
	.size	probe_o, .- probe_o
! P: as O with 1234567812345678 (bit 30 clear in both halves).
! probe_p(long *out, long *tag).
	.global	probe_p
	.type	probe_p, #function
	.align	8
probe_p:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  addd,0	0x0, _f64 0x1234567812345678, %dr13
	}
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	}
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	}
	{
	  gettagd,2	%dr13, %r14
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2,sm	%r0, 0x0, %dr13
	  stw,5	%r1, 0x0, %r14
	}
	.size	probe_p, .- probe_p
