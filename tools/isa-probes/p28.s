! SPDX-License-Identifier: GPL-3.0-or-later
! A pipelined loop with a DAM lock/check and rotating predicates, for
! p28.c (docs/spec-poc-findings.md P28; spec/isa/speculation.md).
	.ignore	ld_st_style
	.text

	.global	p28_full
	.type	p28_full, #function
	.align	8
p28_full:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x8, rcur = 0x0
	  setbp	psz = 0x1
	}
	{
	  return	%ctpr3
	}
	{
	  addd,0	%dr0, 0x8, %dr10
	  addd,1	%dr0, 0x0, %dr11
	  addd,2	%dr1, 0x0, %dr12
	  addd,3	0x0, 0x0, %dr13
	}
	{
	  disp	%ctpr1, .Lfull_k
	}
	{
	  rwd,0	%dr2, %lsr
	}
	{
	  ldd,0,sm	%dr0, 0x0, %b[2], mas=0x4
	}
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[2], 0x1, %pred1
	}
	{
	  nop 7
	}
.Lfull_k:
	{
	  ldd,0,sm	%dr10, 0x0, %b[0], mas=0x4
	  ldd,5	%dr11, 0x0, %b[2], mas=0x3
	  rbranch	.Lfull_comp
	}
.Lfull_back:
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[0], 0x1, %pred0
	  subd,1	0x0, %b[2], %dr7
	  xord,3	%b[0], 0x1, %dr9
	}
	{
	  merged,0	%dr7, %b[2], %dr8, %pred1
	}
	{
	  std,2	%dr12, 0x0, %dr8
	  std,5	%dr10, 0x0, %dr9
	  addd,0	%dr10, 0x8, %dr10
	  addd,1	%dr11, 0x8, %dr11
	  addd,3	%dr12, 0x8, %dr12
	}
	{
	  alc	alcf=1, alct=1
	  abn	abnf=1, abnt=1
	  abp	abpf=1, abpt=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	}
	{
	  addd,0	%dr13, 0x0, %dr0
	  setbp	psz = 0x0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Lfull_comp:
	{
	  nop 2
	}
	{
	  addd,1	%dr13, 0x1, %dr13
	  cmpandedb,0	%b[2], 0x1, %pred1
	}
	{
	  ibranch	.Lfull_back
	}
	.size	p28_full, .-p28_full

	.global	p28_norecompute
	.type	p28_norecompute, #function
	.align	8
p28_norecompute:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x8, rcur = 0x0
	  setbp	psz = 0x1
	}
	{
	  return	%ctpr3
	}
	{
	  addd,0	%dr0, 0x8, %dr10
	  addd,1	%dr0, 0x0, %dr11
	  addd,2	%dr1, 0x0, %dr12
	  addd,3	0x0, 0x0, %dr13
	}
	{
	  disp	%ctpr1, .Lnorecompute_k
	}
	{
	  rwd,0	%dr2, %lsr
	}
	{
	  ldd,0,sm	%dr0, 0x0, %b[2], mas=0x4
	}
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[2], 0x1, %pred1
	}
	{
	  nop 7
	}
.Lnorecompute_k:
	{
	  ldd,0,sm	%dr10, 0x0, %b[0], mas=0x4
	  ldd,5	%dr11, 0x0, %b[2], mas=0x3
	  rbranch	.Lnorecompute_comp
	}
.Lnorecompute_back:
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[0], 0x1, %pred0
	  subd,1	0x0, %b[2], %dr7
	  xord,3	%b[0], 0x1, %dr9
	}
	{
	  merged,0	%dr7, %b[2], %dr8, %pred1
	}
	{
	  std,2	%dr12, 0x0, %dr8
	  std,5	%dr10, 0x0, %dr9
	  addd,0	%dr10, 0x8, %dr10
	  addd,1	%dr11, 0x8, %dr11
	  addd,3	%dr12, 0x8, %dr12
	}
	{
	  alc	alcf=1, alct=1
	  abn	abnf=1, abnt=1
	  abp	abpf=1, abpt=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	}
	{
	  addd,0	%dr13, 0x0, %dr0
	  setbp	psz = 0x0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Lnorecompute_comp:
	{
	  nop 2
	}
	{
	  addd,1	%dr13, 0x1, %dr13
	}
	{
	  ibranch	.Lnorecompute_back
	}
	.size	p28_norecompute, .-p28_norecompute

	.global	p28_nostore
	.type	p28_nostore, #function
	.align	8
p28_nostore:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x8, rcur = 0x0
	  setbp	psz = 0x1
	}
	{
	  return	%ctpr3
	}
	{
	  addd,0	%dr0, 0x8, %dr10
	  addd,1	%dr0, 0x0, %dr11
	  addd,2	%dr1, 0x0, %dr12
	  addd,3	0x0, 0x0, %dr13
	}
	{
	  disp	%ctpr1, .Lnostore_k
	}
	{
	  rwd,0	%dr2, %lsr
	}
	{
	  ldd,0,sm	%dr0, 0x0, %b[2], mas=0x4
	}
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[2], 0x1, %pred1
	}
	{
	  nop 7
	}
.Lnostore_k:
	{
	  ldd,0,sm	%dr10, 0x0, %b[0], mas=0x4
	  ldd,5	%dr11, 0x0, %b[2], mas=0x3
	  rbranch	.Lnostore_comp
	}
.Lnostore_back:
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[0], 0x1, %pred0
	  subd,1	0x0, %b[2], %dr7
	  xord,3	%b[0], 0x1, %dr9
	}
	{
	  merged,0	%dr7, %b[2], %dr8, %pred1
	}
	{
	  std,2	%dr12, 0x0, %dr8
	  addd,0	%dr10, 0x8, %dr10
	  addd,1	%dr11, 0x8, %dr11
	  addd,3	%dr12, 0x8, %dr12
	}
	{
	  alc	alcf=1, alct=1
	  abn	abnf=1, abnt=1
	  abp	abpf=1, abpt=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	}
	{
	  addd,0	%dr13, 0x0, %dr0
	  setbp	psz = 0x0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Lnostore_comp:
	{
	  nop 2
	}
	{
	  addd,1	%dr13, 0x1, %dr13
	  cmpandedb,0	%b[2], 0x1, %pred1
	}
	{
	  ibranch	.Lnostore_back
	}
	.size	p28_nostore, .-p28_nostore

	.global	p28_deep
	.type	p28_deep, #function
	.align	8
p28_deep:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x8, rcur = 0x0
	  setbp	psz = 0x2
	}
	{
	  return	%ctpr3
	}
	{
	  addd,0	%dr0, 0x10, %dr10
	  addd,1	%dr0, 0x8, %dr11
	  addd,2	%dr1, 0x0, %dr12
	  addd,3	0x0, 0x0, %dr13
	}
	{
	  disp	%ctpr1, .Ldeep_k
	}
	{
	  rwd,0	%dr2, %lsr
	}
	{
	  ldd,0,sm	%dr0, 0x0, %b[4], mas=0x4
	}
	{
	  ldd,0,sm	%dr0, 0x8, %b[2], mas=0x4
	}
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[4], 0x1, %pred2
	  xord,1	%b[2], 0x1, %dr9
	}
	{
	  cmpandedb,0	%b[2], 0x1, %pred1
	  std,5	%dr0, 0x8, %dr9
	}
	{
	  ldd,5	%dr0, 0x0, %b[4], mas=0x3
	}
	{
	  nop 7
	}
.Ldeep_k:
	{
	  ldd,0,sm	%dr10, 0x0, %b[0], mas=0x4
	  ldd,5	%dr11, 0x0, %b[2], mas=0x3
	  rbranch	.Ldeep_comp
	}
.Ldeep_back:
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[0], 0x1, %pred0
	  subd,1	0x0, %b[4], %dr7
	  xord,3	%b[0], 0x1, %dr9
	}
	{
	  merged,0	%dr7, %b[4], %dr8, %pred2
	}
	{
	  std,2	%dr12, 0x0, %dr8
	  std,5	%dr10, 0x0, %dr9
	  addd,0	%dr10, 0x8, %dr10
	  addd,1	%dr11, 0x8, %dr11
	  addd,3	%dr12, 0x8, %dr12
	}
	{
	  alc	alcf=1, alct=1
	  abn	abnf=1, abnt=1
	  abp	abpf=1, abpt=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	}
	{
	  addd,0	%dr13, 0x0, %dr0
	  setbp	psz = 0x0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Ldeep_comp:
	{
	  nop 2
	}
	{
	  addd,1	%dr13, 0x1, %dr13
	  cmpandedb,0	%b[2], 0x1, %pred1
	}
	{
	  ibranch	.Ldeep_back
	}
	.size	p28_deep, .-p28_deep

	.global	p28_deep_norecompute
	.type	p28_deep_norecompute, #function
	.align	8
p28_deep_norecompute:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x8, rcur = 0x0
	  setbp	psz = 0x2
	}
	{
	  return	%ctpr3
	}
	{
	  addd,0	%dr0, 0x10, %dr10
	  addd,1	%dr0, 0x8, %dr11
	  addd,2	%dr1, 0x0, %dr12
	  addd,3	0x0, 0x0, %dr13
	}
	{
	  disp	%ctpr1, .Ldeep_norecompute_k
	}
	{
	  rwd,0	%dr2, %lsr
	}
	{
	  ldd,0,sm	%dr0, 0x0, %b[4], mas=0x4
	}
	{
	  ldd,0,sm	%dr0, 0x8, %b[2], mas=0x4
	}
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[4], 0x1, %pred2
	  xord,1	%b[2], 0x1, %dr9
	}
	{
	  cmpandedb,0	%b[2], 0x1, %pred1
	  std,5	%dr0, 0x8, %dr9
	}
	{
	  ldd,5	%dr0, 0x0, %b[4], mas=0x3
	}
	{
	  nop 7
	}
.Ldeep_norecompute_k:
	{
	  ldd,0,sm	%dr10, 0x0, %b[0], mas=0x4
	  ldd,5	%dr11, 0x0, %b[2], mas=0x3
	  rbranch	.Ldeep_norecompute_comp
	}
.Ldeep_norecompute_back:
	{
	  nop 2
	}
	{
	  cmpandedb,0	%b[0], 0x1, %pred0
	  subd,1	0x0, %b[4], %dr7
	  xord,3	%b[0], 0x1, %dr9
	}
	{
	  merged,0	%dr7, %b[4], %dr8, %pred2
	}
	{
	  std,2	%dr12, 0x0, %dr8
	  std,5	%dr10, 0x0, %dr9
	  addd,0	%dr10, 0x8, %dr10
	  addd,1	%dr11, 0x8, %dr11
	  addd,3	%dr12, 0x8, %dr12
	}
	{
	  alc	alcf=1, alct=1
	  abn	abnf=1, abnt=1
	  abp	abpf=1, abpt=1
	  ct	%ctpr1 ? %NOT_LOOP_END
	}
	{
	  addd,0	%dr13, 0x0, %dr0
	  setbp	psz = 0x0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Ldeep_norecompute_comp:
	{
	  nop 2
	}
	{
	  addd,1	%dr13, 0x1, %dr13
	}
	{
	  ibranch	.Ldeep_norecompute_back
	}
	.size	p28_deep_norecompute, .-p28_deep_norecompute
