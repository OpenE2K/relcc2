! SPDX-License-Identifier: GPL-3.0-or-later
! Timings and user-mode checks of the x86 translator's facilities for
! native code: see native.c (spec/isa/x86-support.md).
	.ignore	ld_st_style
	.text

! long nat_clkr (void): %clkr.
	.global	nat_clkr
	.type	nat_clkr, #function
	.align	8
nat_clkr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrd,0	%clkr, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	nat_clkr, .-nat_clkr

! long nat_loop (long n), nat_srpcrp (long n), nat_srp (long n):
! cycles for n iterations of a 4-instruction loop with nothing, with
! srp and crp in it, or with srp alone (then crp after the loop).
	.global	nat_loop
	.type	nat_loop, #function
	.align	8
nat_loop:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  disp	%ctpr1, .Lloop
	  rrd,0	%clkr, %dr4
	}
	{
	  nop 4
	}
.Lloop:
	{
	  subd,0	%dr0, 0x1, %dr0
	}
	{
	  nop
	}
	{
	  cmpedb,0	%dr0, 0x0, %pred0
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr1 ? ~%pred0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  nop 2
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	  ct	%ctpr3
	}
	.size	nat_loop, .-nat_loop

	.global	nat_srpcrp
	.type	nat_srpcrp, #function
	.align	8
nat_srpcrp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  disp	%ctpr1, .Lsrpcrp
	  rrd,0	%clkr, %dr4
	}
	{
	  nop 4
	}
.Lsrpcrp:
	{
	  subd,0	%dr0, 0x1, %dr0
	  srp
	}
	{
	  nop
	}
	{
	  cmpedb,0	%dr0, 0x0, %pred0
	  crp
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr1 ? ~%pred0
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  nop 2
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	  ct	%ctpr3
	}
	.size	nat_srpcrp, .-nat_srpcrp

	.global	nat_srp
	.type	nat_srp, #function
	.align	8
nat_srp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  disp	%ctpr1, .Lsrp
	  rrd,0	%clkr, %dr4
	}
	{
	  nop 4
	}
.Lsrp:
	{
	  subd,0	%dr0, 0x1, %dr0
	  srp
	}
	{
	  nop
	}
	{
	  cmpedb,0	%dr0, 0x0, %pred0
	}
	{
	  nop 1
	}
	{
	  ct	%ctpr1 ? ~%pred0
	}
	{
	  crp
	}
	{
	  rrd,0	%clkr, %dr5
	}
	{
	  nop 2
	}
	{
	  subd,0	%dr5, %dr4, %dr0
	  ct	%ctpr3
	}
	.size	nat_srp, .-nat_srp

! long nat_rd_upsr (void), void nat_wr_upsr (long): %upsr.
	.global	nat_rd_upsr
	.type	nat_rd_upsr, #function
	.align	8
nat_rd_upsr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrs,0	%upsr, %r0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	nat_rd_upsr, .-nat_rd_upsr

	.global	nat_wr_upsr
	.type	nat_wr_upsr, #function
	.align	8
nat_wr_upsr:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rws,0	%r0, %upsr
	}
	{
	  wait	all_c = 1
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	nat_wr_upsr, .-nat_wr_upsr

! long nat_fcmp (long *ab, long kind): an FP compare of ab[0] with
! ab[1] (doubles); returns bit 0 the predicate result of fcmpltdb,
! bit 1 fcmpledb, bit 2 fcmpeqdb, bit 3 fcmpneqdb, bit 4 fcmpuoddb,
! bits 8-15 the fcmpuddf (quiet, register) result, bits 16-23
! fcmpoddf, bits 32-37 the %pfpfr flags after all of them (cleared
! before).
	.global	nat_fcmp
	.type	nat_fcmp, #function
	.align	8
nat_fcmp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  ldd,0	%dr0, 0x0, %dr2
	  ldd,2	%dr0, 0x8, %dr3
	}
	{
	  rrs,0	%pfpfr, %r8
	}
	{
	  nop 2
	  ands,0	%r8, 0xffffffc0, %r9
	}
	{
	  rws,0	%r9, %pfpfr
	}
	{
	  nop 5
	}
	{
	  fcmpltdb,0	%dr2, %dr3, %pred0
	  fcmpledb,1	%dr2, %dr3, %pred1
	  fcmpeqdb,3	%dr2, %dr3, %pred2
	  fcmpneqdb,4	%dr2, %dr3, %pred3
	}
	{
	  fcmpuoddb,0	%dr2, %dr3, %pred4
	  fcmpuddf,1	%dr2, %dr3, %dr10
	  fcmpoddf,3	%dr2, %dr3, %dr11
	}
	{
	  nop 5
	  addd,0	0x0, 0x0, %dr12
	}
	{
	  ord,0	%dr12, 0x1, %dr12 ? %pred0
	}
	{
	  ord,0	%dr12, 0x2, %dr12 ? %pred1
	}
	{
	  ord,0	%dr12, 0x4, %dr12 ? %pred2
	}
	{
	  ord,0	%dr12, 0x8, %dr12 ? %pred3
	}
	{
	  ord,0	%dr12, 0x10, %dr12 ? %pred4
	  andd,1	%dr10, 0xff, %dr10
	  andd,3	%dr11, 0xff, %dr11
	}
	{
	  shld,0	%dr10, 0x8, %dr10
	  shld,1	%dr11, 0x10, %dr11
	}
	{
	  rrs,0	%pfpfr, %r13
	}
	{
	  nop 2
	  ord,0	%dr12, %dr10, %dr12
	  andd,1	%dr13, 0x3f, %dr13
	}
	{
	  ord,0	%dr12, %dr11, %dr12
	  shld,1	%dr13, 0x20, %dr13
	}
	{
	  ord,0	%dr12, %dr13, %dr0
	}
	{
	  rws,0	%r8, %pfpfr
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	nat_fcmp, .-nat_fcmp
