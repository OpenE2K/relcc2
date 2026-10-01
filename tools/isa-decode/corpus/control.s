/* Hand-written bundles exercising syllables rarely produced by compilers.
   Assembled for v3..v7 by build_corpus.sh.  */

	.text
	.global	ctl
ctl:
{
	setwd wsz = 0x10, nfx = 0x1, dbl = 0x0
	setbn rbs = 0x8, rsz = 0x7, rcur = 0x0
	setbp psz = 0x3
}
{
	vfrpsz rpsz = 0x5
	setwd wsz = 0x10, nfx = 0x0, dbl = 0x1
}
{
	disp %ctpr1, ctl
}
{
	ldisp %ctpr2, ctl
}
{
	sdisp %ctpr3, 0x5
}
{
	return %ctpr3
}
{
	nop 5
}
{
	loop_mode
	alc alcf = 1, alct = 1
	abn abnf = 1, abnt = 0
	abp abpf = 0, abpt = 1
}
{
	wait all_c = 1, all_e = 1, st_c = 0, ld_c = 1, fl_c = 0, ma_c = 1
}
{
	ibranch ctl ? %pred3
}
{
	ibranch ctl ? ~%pred4
}
{
	ct %ctpr1 ? %LOOP_END
}
{
	ct %ctpr1 ? %NOT_LOOP_END
}
{
	ct %ctpr2 ? %pred2 || %LOOP_END
}
{
	ct %ctpr2 ? ~%pred2 && %NOT_LOOP_END
}
{
	ct %ctpr3 ? ~%pred5 || %LOOP_END
}
{
	ct %ctpr3 ? %pred5 && %NOT_LOOP_END
}
{
	call %ctpr1, wbs = 0x8
}
{
	call %ctpr2, wbs = 0x4 ? %pred1
}
{
	ipd 1
	bap
}
{
	ipd 2
	eap
}
{
	flushr
	flushc
}
{
	setei 0x12
}
{
	rrd,0 %psr, %dr1
}
{
	rws,0 %r2, %upsr
}
{
	rrd,0 %ip, %dr2
}
/* A store in ALC2 can't share a bundle with a load in ALC3, nor a store
   in ALC5 with a load in ALC0 (gas: "the combination ... is prohibited").  */
{
	ldd,0 %dr0, 0x8, %dr1, mas = 0x4
	ldw,3 %dr0, %dr2, %r3, mas = 0x2
}
{
	std,2 %dr0, 0x10, %dr1, mas = 0x7
	stw,5 %dr0, %dr2, %r3, mas = 0x5
}
{
	addd,0 %dr1, %dr2, %dr3 ? %pred1
	addd,1 %dr1, %dr2, %dr4 ? ~%pred1
	addd,3 %dr1, %dr2, %dr5 ? %pred2
	subd,4 %dr1, %dr2, %dr6 ? ~%pred2
}
{
	merges,0 %r1, %r2, %r3, %pred1
	merged,3 %dr1, %dr2, %dr3, ~%pred2
}
{
	cmpesb,0 %r1, %r2, %pred1
	cmplsb,1 %r1, %r2, %pred2
	pass %pred1, @p0
	pass %pred2, @p1
	andp @p0, @p1, @p4
	pass @p4, %pred3
}
{
	pass %pred4, @p0
	pass %pred5, @p1
	landp ~@p0, @p1, @p4
	pass @p4, %pred6
	pass %pred7, @p2
	pass %pred8, @p3
	andp @p2, ~@p3, @p5
	pass @p5, %pred9
}
{
	addd,0 0x5, _f64 0x123456789abcdef0, %dr1
	addd,1 %dr2, _f32s 0x7fffffff, %dr3
	adds,3 %r2, _f16s 0x1234, %r4
}
{
	movab,0 area = 1, ind = 2, am = 0, be = 0, %dr4
	movaw,1 area = 2, ind = 3, am = 1, be = 0, %dr5
}
{
	staad,2 %dr1, %aad3 [ %aasti1 ]
	ldaad,5 %aad2 [ %aasti4 + _f32s,_lts0 0x10 ], %dr6
}
{
	aaurwd,2 %dr1, %aasti3
	aaurrd,5 %aaind2, %dr2
}
{
	getsp,0 _f32s 0x40, %dr7
}
{
	nop
}
