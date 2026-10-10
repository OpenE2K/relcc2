! SPDX-License-Identifier: GPL-3.0-or-later
! Recovery point operations (srp, slrp, crp) and %rpr: see rp.c.
	.ignore	ld_st_style
	.text

! long rp_rd_lo (long *), rp_rd_hi (long *): %rpr.lo, %rpr.hi.
	.global	rp_rd_lo
	.type	rp_rd_lo, #function
	.align	8
rp_rd_lo:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd	%rpr.lo, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	rp_rd_lo, .-rp_rd_lo

	.global	rp_rd_hi
	.type	rp_rd_hi, #function
	.align	8
rp_rd_hi:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rrd	%rpr.hi, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	rp_rd_hi, .-rp_rd_hi

! long rp_wr (long lo, long hi): write %rpr, return what %rpr.lo reads.
	.global	rp_wr
	.type	rp_wr, #function
	.align	8
rp_wr:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  rwd	%dr0, %rpr.lo
	}
	{
	  rwd	%dr1, %rpr.hi
	}
	{
	  wait	all_e = 1
	}
	{
	  nop 5
	}
	{
	  rrd	%rpr.lo, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	rp_wr, .-rp_wr

! long rp_srp (long *out): srp at rp_srp_at, then out[0] = %rpr.lo,
! out[1] = %rpr.hi; with the register base rotated (setbn rbs 4, rsz 3,
! rcur 1; setbp psz 2) to see what BR %rpr keeps.
	.global	rp_srp
	.type	rp_srp, #function
	.align	8
rp_srp:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x1
	  setbp	psz = 0x2
	  return	%ctpr3
	}
	{
	  nop 2
	}
	.global	rp_srp_at
rp_srp_at:
	{
	  srp
	}
	{
	  nop 7
	}
	{
	  rrd	%rpr.lo, %dr2
	}
	{
	  rrd	%rpr.hi, %dr3
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr2
	  std,5	%dr0, 0x8, %dr3
	}
	{
	  ct	%ctpr3
	}
	.size	rp_srp, .-rp_srp

! long rp_slrp (long *out): the same with slrp at rp_slrp_at.
	.global	rp_slrp
	.type	rp_slrp, #function
	.align	8
rp_slrp:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x1
	  setbp	psz = 0x2
	  return	%ctpr3
	}
	{
	  nop 2
	}
	.global	rp_slrp_at
rp_slrp_at:
	{
	  slrp
	}
	{
	  nop 7
	}
	{
	  rrd	%rpr.lo, %dr2
	}
	{
	  rrd	%rpr.hi, %dr3
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr2
	  std,5	%dr0, 0x8, %dr3
	}
	{
	  ct	%ctpr3
	}
	.size	rp_slrp, .-rp_slrp

! long rp_crp (long *out): srp, crp, then out[0..1] = %rpr.
	.global	rp_crp
	.type	rp_crp, #function
	.align	8
rp_crp:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  srp
	}
	{
	  nop 3
	}
	{
	  crp
	}
	{
	  nop 7
	}
	{
	  rrd	%rpr.lo, %dr2
	}
	{
	  rrd	%rpr.hi, %dr3
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr2
	  std,5	%dr0, 0x8, %dr3
	}
	{
	  ct	%ctpr3
	}
	.size	rp_crp, .-rp_crp

! long rp_srp_cond (long *out, long c): srp ? %pred0 with %pred0 = (c
! != 0), then out[0..1] = %rpr.
	.global	rp_srp_cond
	.type	rp_srp_cond, #function
	.align	8
rp_srp_cond:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  cmpedb,0	%dr1, 0x0, %pred1
	}
	{
	  nop 1
	}
	{
	  pass	%pred1, @p0
	  andp	~@p0, ~@p0, @p4
	  pass	@p4, %pred0
	}
	{
	  nop 2
	}
	.global	rp_srpc_at
rp_srpc_at:
	{
	  srp ? %pred0
	}
	{
	  nop 7
	}
	{
	  rrd	%rpr.lo, %dr2
	}
	{
	  rrd	%rpr.hi, %dr3
	}
	{
	  nop 5
	}
	{
	  std,2	%dr0, 0x0, %dr2
	  std,5	%dr0, 0x8, %dr3
	}
	{
	  ct	%ctpr3
	}
	.size	rp_srp_cond, .-rp_srp_cond

! long rp_store (long *mem, long how): srp, mem[0] = 1, then
! how 0: return; 1: crp, then return; 2: load from address 0 (SIGSEGV
! inside the region); 3: load mem[0] back and return it, then crp.
	.global	rp_store
	.type	rp_store, #function
	.align	8
rp_store:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x1, %dr4
	  cmpedb,1	%dr1, 0x1, %pred1
	  cmpedb,3	%dr1, 0x2, %pred2
	  cmpedb,4	%dr1, 0x3, %pred3
	}
	{
	  nop 3
	}
	{
	  srp
	}
	{
	  std,2	%dr0, 0x0, %dr4
	}
	{
	  nop 2
	}
	{
	  ldd,0	0x0, 0x0, %dr5 ? %pred2
	  ldd,3	%dr0, 0x0, %dr6 ? %pred3
	}
	{
	  nop 3
	}
	{
	  addd,0	0x0, %dr6, %dr0 ? %pred3
	}
	{
	  crp ? %pred1
	}
	{
	  crp ? %pred3
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.size	rp_store, .-rp_store
