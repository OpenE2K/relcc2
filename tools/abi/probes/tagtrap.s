! SPDX-License-Identifier: GPL-3.0-or-later
! Diagnostic-tag probe; see tagtrap.c.
	.ignore	ld_st_style
	.ignore	strict_delay
	.text
! probe_X(long *out, long *bad, int x)
! A: speculative load from bad -> %r3 (diag tag), then std %r3.
	.global	probe_a
	.type	probe_a, #function
	.align	8
probe_a:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  ldd,0,sm	%r1, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.size	probe_a, .- probe_a
! B: same, but write the low half of %r3 with a 32-bit op under dbl = 0.
	.global	probe_b
	.type	probe_b, #function
	.align	8
probe_b:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  ldd,0,sm	%r1, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  adds,0	0x0, %r2, %r3
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.size	probe_b, .- probe_b
! C: as B with dbl = 1.
	.global	probe_c
	.type	probe_c, #function
	.align	8
probe_c:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x1
	  return	%ctpr3; ipd 2
	  ldd,0,sm	%r1, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  adds,0	0x0, %r2, %r3
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2	%r0, 0x0, %r3
	}
	.size	probe_c, .- probe_c
! D: as B, but spill with a speculative store (std,sm).
	.global	probe_d
	.type	probe_d, #function
	.align	8
probe_d:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  ldd,0,sm	%r1, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  adds,0	0x0, %r2, %r3
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr3
	  std,2,sm	%r0, 0x0, %r3
	}
	.size	probe_d, .- probe_d
! E: as B, then copy %r3 with a speculative addd,sm (as lcc writes register
! copies) and store the copy speculatively: does the copy trap, and does
! it carry the tag?
	.global	probe_e
	.type	probe_e, #function
	.align	8
probe_e:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  ldd,0,sm	%r1, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  adds,0	0x0, %r2, %r3
	}
	{
	  nop 1
	}
	{
	  addd,0,sm	0x0, %r3, %r4
	}
	{
	  ct	%ctpr3
	  std,2,sm	%r0, 0x0, %r4
	}
	.size	probe_e, .- probe_e
! F: as E with a non-speculative addd.
	.global	probe_f
	.type	probe_f, #function
	.align	8
probe_f:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3; ipd 2
	  ldd,0,sm	%r1, 0x0, %r3
	}
	{
	  nop 5
	}
	{
	  adds,0	0x0, %r2, %r3
	}
	{
	  nop 1
	}
	{
	  addd,0	0x0, %r3, %r4
	}
	{
	  ct	%ctpr3
	  std,2,sm	%r0, 0x0, %r4
	}
	.size	probe_f, .- probe_f
