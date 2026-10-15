! SPDX-License-Identifier: GPL-3.0-or-later
! Hand-written functions and hand-written DWARF for debuginfo.c (see
! there for how to build and run): how MCST's gdb reads DWARF register
! locations.  Every variable below is described by a DW_AT_location
! that names a register (DW_OP_regN, DW_OP_regx, pieces, a location
! list) or a TLS offset; the functions put known values in those
! registers and stop at global labels (di_*_s*) where gdb prints them.
!
!   di_rot    %b[] under setbn (rbs 4, rsz 3, rcur 1: %b[0] = %r10) and
!             in a loop whose branch rotates with abn (three stops),
!             then under rbs 0x20 (%b[0] = %r64, above %r63)
!   di_outer  a caller with values below and above the call's wbs and
!             its own setbn (rcur 2: %b[0] = %r12); calls di_leaf
!   di_leaf   a callee that first runs on the inherited %br, then sets
!             its own (rbs 1, rsz 1, rcur 1: %b[0] = %r4)
!   di_wide   16-byte values: long double as a pair (mantissa in %r4,
!             sign and exponent in %r5) and in one register (movif:
!             %r6 with its extended part), a vector of two longs and
!             an __int128 as pairs, a __float128 as a pair, the
!             16-byte floats also as DW_OP_piece locations;
!             with --defsym V5=1 also a 128-bit value in one v5
!             register (%qpr14, high half in the extended part)
!   di_wcall  calls di_wide with other 16-byte values in its own %r4/%r5
!             and %r12/%r13
!   di_tls    an 8-byte TLS variable described four ways
!
! Register values: %dr<i> = 0x800 + 0x100 * (i - 8) in di_rot,
! 0x1000 + i in di_outer, 0x2000 + i in di_leaf.
	.ignore	ld_st_style
	.text
.Ltext0:

! ------------------------------------------------------------- di_rot
! long di_rot (long n): n loop iterations, each rotating %b[] once.
	.global	di_rot
	.type	di_rot, #function
	.align	8
di_rot:
	{
	  setwd	wsz = 0x28, nfx = 0x1, dbl = 0x0
	}
	{
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x1
	}
	{
	  addd,0	0x0, 0x800, %dr8
	  addd,1	0x0, 0x900, %dr9
	  addd,2	0x0, 0xa00, %dr10
	  addd,3	0x0, 0xb00, %dr11
	  addd,4	0x0, 0xc00, %dr12
	  addd,5	0x0, 0xd00, %dr13
	}
	{
	  addd,0	0x0, 0xe00, %dr14
	  addd,1	0x0, 0xf00, %dr15
	  addd,2	0x0, 0x1000, %dr16
	  addd,3	0x0, %dr0, %dr2
	  disp	%ctpr1, di_rot_loop
	}
	.global	di_rot_s1
di_rot_s1:
	{
	  nop
	}
	.global	di_rot_loop
di_rot_loop:
	{
	  addd,0	0x0, %dr2, %dr3
	}
	{
	  subd,0	%dr2, 0x1, %dr2
	}
	{
	  cmpedb,0	%dr2, 0x0, %pred0
	}
	{
	  nop 2
	}
	{
	  ct	%ctpr1 ? ~%pred0
	  abn	abnf=1, abnt=1
	}
	.global	di_rot_s2
di_rot_s2:
	{
	  nop
	}
! lcc's layout for loops without rotation: the based area above %r63
! (rbs 0x20: %b[0] = %r64, which no %r name reaches)
	{
	  setbn	rsz = 0x3, rbs = 0x20, rcur = 0x0
	}
	{
	  addd,0	0x0, 0x4000, %b[0]
	  addd,1	0x0, 0x4001, %b[1]
	  addd,2	0x0, 0x4007, %b[7]
	}
	.global	di_rot_s3
di_rot_s3:
	{
	  return	%ctpr3
	  addd,0	0x0, %b[0], %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Ldi_rot_end:
	.size	di_rot, .-di_rot

! ----------------------------------------------------------- di_outer
! long di_outer (void): calls di_leaf with wbs 0xc (4 pairs handed
! over: the leaf's %dr0..7 are di_outer's %dr24..31).
	.global	di_outer
	.type	di_outer, #function
	.align	8
di_outer:
	{
	  setwd	wsz = 0x10, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x2
	}
	{
	  addd,0	0x0, 0x1000, %dr0
	  addd,1	0x0, 0x1001, %dr1
	  addd,2	0x0, 0x1002, %dr2
	  addd,3	0x0, 0x1003, %dr3
	  addd,4	0x0, 0x1004, %dr4
	  addd,5	0x0, 0x1005, %dr5
	}
	{
	  addd,0	0x0, 0x1006, %dr6
	  addd,1	0x0, 0x1007, %dr7
	  addd,2	0x0, 0x1008, %dr8
	  addd,3	0x0, 0x1009, %dr9
	  addd,4	0x0, 0x100a, %dr10
	  addd,5	0x0, 0x100b, %dr11
	}
	{
	  addd,0	0x0, 0x100c, %dr12
	  addd,1	0x0, 0x100d, %dr13
	  addd,2	0x0, 0x100e, %dr14
	  addd,3	0x0, 0x100f, %dr15
	  addd,4	0x0, 0x1010, %dr16
	  addd,5	0x0, 0x1017, %dr23
	}
	{
	  addd,0	0x0, 0x1018, %dr24
	  addd,1	0x0, 0x1019, %dr25
	  addd,2	0x0, 0x101f, %dr31
	  disp	%ctpr1, di_leaf
	}
	{
	  nop 5
	}
.Lcall_o:
	{
	  call	%ctpr1, wbs = 0xc
	}
.Lret_o:
	{
	  return	%ctpr3
	  addd,0	0x0, %dr2, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Ldi_outer_end:
	.size	di_outer, .-di_outer

! ------------------------------------------------------------ di_leaf
	.global	di_leaf
	.type	di_leaf, #function
	.align	8
di_leaf:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	}
	{
	  addd,0	0x0, 0x2000, %dr0
	  addd,1	0x0, 0x2001, %dr1
	  addd,2	0x0, 0x2002, %dr2
	  addd,3	0x0, 0x2003, %dr3
	  addd,4	0x0, 0x2004, %dr4
	  addd,5	0x0, 0x2005, %dr5
	}
	{
	  addd,0	0x0, 0x2006, %dr6
	  addd,1	0x0, 0x2007, %dr7
	  addd,2	0x0, 0x2008, %dr8
	  addd,3	0x0, 0x2009, %dr9
	  addd,4	0x0, 0x200a, %dr10
	  addd,5	0x0, 0x200b, %dr11
	}
	{
	  addd,0	0x0, 0x200c, %dr12
	  addd,1	0x0, 0x200d, %dr13
	  addd,2	0x0, 0x200e, %dr14
	  addd,3	0x0, 0x200f, %dr15
	}
	.global	di_leaf_s0
di_leaf_s0:
	{
	  nop
	}
	{
	  setbn	rsz = 0x1, rbs = 0x1, rcur = 0x1
	}
	.global	di_leaf_s1
di_leaf_s1:
	{
	  nop
	}
	{
	  return	%ctpr3
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Ldi_leaf_end:
	.size	di_leaf, .-di_leaf

! ------------------------------------------------------------ di_wide
! long di_wide (void): 16-byte values in registers; calls di_leaf with
! wbs 8 so that the values below %r16 are also read in an outer frame.
	.global	di_wide
	.type	di_wide, #function
	.align	8
di_wide:
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	}
	{
	  addd,0	0x0, _f64 0xc000000000000000, %dr4
	  addd,1	0x0, 0x3fff, %dr5
	  addd,2	0x0, 0x4000, %dr7
	  addd,3	0x0, 0x3fff, %dr21
	}
	{
	  addd,0	0x0, _f64 0xc000000000000000, %dr20
	  addd,1	0x0, 0x1111, %dr8
	  addd,2	0x0, 0x2222, %dr9
	  addd,3	0x0, 0x3333, %dr10
	  addd,4	0x0, 0x4444, %dr11
	}
	{
	  movif,1	%dr20, %r21, %r6
	  addd,0	0x0, 0x0, %dr12
	  addd,2	0x0, _f64 0x3fff000000000000, %dr13
	}
.ifdef V5
	{
	  addd,0	0x0, 0x5555, %dr22
	  addd,1	0x0, 0x6666, %dr23
	  addd,2	0x0, 0x7777, %dr15
	}
	{
	  qppackdl,0	%dr22, %dr23, %qpr14
	}
.endif
	{
	  disp	%ctpr1, di_leaf
	}
	.global	di_wide_s
di_wide_s:
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  return	%ctpr3
	  movfi,1	%r6, %r0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
.Ldi_wide_end:
	.size	di_wide, .-di_wide

! ----------------------------------------------------------- di_wcall
! long di_wcall (void): calls di_wide with wbs 8 after putting other
! 16-byte values in its own %r4/%r5 (long double 2.5) and %r12/%r13
! (__float128 2.0), below the call's wbs: a reader that takes a frame's
! registers from the frame above shows these instead of di_wide's.
	.global	di_wcall
	.type	di_wcall, #function
	.align	8
di_wcall:
	{
	  setwd	wsz = 0x10, nfx = 0x0, dbl = 0x0
	}
	{
	  addd,0	0x0, _f64 0xa000000000000000, %dr4
	  addd,1	0x0, 0x4000, %dr5
	  addd,2	0x0, 0x0, %dr12
	  addd,3	0x0, _f64 0x4000000000000000, %dr13
	  disp	%ctpr1, di_wide
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  return	%ctpr3
	  addd,0	0x0, %dr16, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	di_wcall, .-di_wcall
.Letext0:

! --------------------------------------------------------- TLS object
	.section .tdata,"awT",@progbits
	.align	8
	.global	di_tls
	.type	di_tls, @object
	.size	di_tls, 8
di_tls:
	.dword	0x1234

! ================================================== DWARF (version 4)
! Abbreviations
	.section .debug_abbrev,"",@progbits
.Labbrev0:
	! 1 compile unit
	.uleb128 1
	.uleb128 0x11		! DW_TAG_compile_unit
	.byte	1		! children
	.uleb128 0x25, 0x08	! DW_AT_producer, string
	.uleb128 0x13, 0x05	! DW_AT_language, data2
	.uleb128 0x03, 0x08	! DW_AT_name, string
	.uleb128 0x11, 0x01	! DW_AT_low_pc, addr
	.uleb128 0x12, 0x07	! DW_AT_high_pc, data8 (length)
	.uleb128 0, 0
	! 2 base type
	.uleb128 2
	.uleb128 0x24
	.byte	0
	.uleb128 0x03, 0x08	! name
	.uleb128 0x3e, 0x0b	! DW_AT_encoding, data1
	.uleb128 0x0b, 0x0b	! DW_AT_byte_size, data1
	.uleb128 0, 0
	! 3 subprogram
	.uleb128 3
	.uleb128 0x2e
	.byte	1
	.uleb128 0x03, 0x08	! name
	.uleb128 0x3f, 0x19	! DW_AT_external, flag_present
	.uleb128 0x11, 0x01	! low_pc
	.uleb128 0x12, 0x07	! high_pc (length)
	.uleb128 0, 0
	! 4 variable, exprloc
	.uleb128 4
	.uleb128 0x34
	.byte	0
	.uleb128 0x03, 0x08	! name
	.uleb128 0x49, 0x13	! DW_AT_type, ref4
	.uleb128 0x02, 0x18	! DW_AT_location, exprloc
	.uleb128 0, 0
	! 5 variable, location list
	.uleb128 5
	.uleb128 0x34
	.byte	0
	.uleb128 0x03, 0x08
	.uleb128 0x49, 0x13
	.uleb128 0x02, 0x17	! DW_AT_location, sec_offset
	.uleb128 0, 0
	! 6 vector type
	.uleb128 6
	.uleb128 0x01		! DW_TAG_array_type
	.byte	1
	.uleb128 0x2107, 0x19	! DW_AT_GNU_vector, flag_present
	.uleb128 0x49, 0x13	! element type
	.uleb128 0, 0
	! 7 subrange
	.uleb128 7
	.uleb128 0x21
	.byte	0
	.uleb128 0x2f, 0x0b	! DW_AT_upper_bound, data1
	.uleb128 0, 0
	! 8 external variable, exprloc
	.uleb128 8
	.uleb128 0x34
	.byte	0
	.uleb128 0x03, 0x08
	.uleb128 0x49, 0x13
	.uleb128 0x3f, 0x19	! external
	.uleb128 0x02, 0x18
	.uleb128 0, 0
	.byte	0

! Debugging information entries
	.section .debug_info,"",@progbits
.Linfo0:
	.long	.Linfo_end - .Linfo_start
.Linfo_start:
	.short	4			! DWARF version
	.long	.Labbrev0
	.byte	8			! address size
	.uleb128 1			! compile unit
	.string	"hand-written (debuginfo.s)"
	.short	0x0c			! DW_LANG_C99
	.string	"debuginfo.s"
	.dword	.Ltext0
	.dword	.Letext0 - .Ltext0
.Ttlong:
	.uleb128 2
	.string	"long int"
	.byte	5, 8			! DW_ATE_signed
.Ttld:
	.uleb128 2
	.string	"long double"
	.byte	4, 16			! DW_ATE_float
.Ttf128:
	.uleb128 2
	.string	"__float128"
	.byte	4, 16
.Tti128:
	.uleb128 2
	.string	"__int128"
	.byte	5, 16
.Ttv2di:
	.uleb128 6
	.long	.Ttlong - .Linfo0
	.uleb128 7
	.byte	1			! upper bound: two elements
	.byte	0

! di_rot: %b[] through the live %br
	.uleb128 3
	.string	"di_rot"
	.dword	di_rot
	.dword	.Ldi_rot_end - di_rot
	.uleb128 4
	.string	"vb0"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 64		! DW_OP_regx 64: %b[0]
	.uleb128 4
	.string	"vb1"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 65
	.uleb128 4
	.string	"vb5"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 69
	.uleb128 4
	.string	"vb6"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 70
	.uleb128 4
	.string	"vb7"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 71
	.uleb128 4
	.string	"vb8"			! beyond the ring (rsz 3: 8 registers)
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 72
	.uleb128 4
	.string	"vr10"
	.long	.Ttlong - .Linfo0
	.uleb128 1
	.byte	0x5a			! DW_OP_reg10
	.byte	0

! di_outer: a caller's registers, %b[] through cr1_hi.br
	.uleb128 3
	.string	"di_outer"
	.dword	di_outer
	.dword	.Ldi_outer_end - di_outer
	.uleb128 4
	.string	"o_r2"
	.long	.Ttlong - .Linfo0
	.uleb128 1
	.byte	0x52			! DW_OP_reg2
	.uleb128 4
	.string	"o_r23"			! last register below 2 * wbs
	.long	.Ttlong - .Linfo0
	.uleb128 1
	.byte	0x50 + 23
	.uleb128 4
	.string	"o_r24"			! = the callee's %r0
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 24
	.uleb128 4
	.string	"o_r31"			! = the callee's %r7
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 31
	.uleb128 4
	.string	"o_r40"			! beyond di_outer's window (wsz 0x10)
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 40
	.uleb128 4
	.string	"o_b0"			! %b[0] = %r12 under di_outer's %br
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 64
	.uleb128 4
	.string	"o_b7"			! %b[7] = %r11
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 71
	.uleb128 4
	.string	"o_b8"			! beyond the ring
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 72
	.uleb128 5
	.string	"o_ll_call"		! %r2 only within the call's instruction
	.long	.Ttlong - .Linfo0
	.long	.Tloc_call
	.uleb128 5
	.string	"o_ll_ret"		! %r2 only from the return address on
	.long	.Ttlong - .Linfo0
	.long	.Tloc_ret
	.byte	0

! di_leaf
	.uleb128 3
	.string	"di_leaf"
	.dword	di_leaf
	.dword	.Ldi_leaf_end - di_leaf
	.uleb128 4
	.string	"l_b0"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 64
	.uleb128 4
	.string	"l_b3"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 67
	.uleb128 4
	.string	"l_b4"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 68
	.uleb128 4
	.string	"l_r15"			! last register of the leaf's window
	.long	.Ttlong - .Linfo0
	.uleb128 1
	.byte	0x5f
	.uleb128 4
	.string	"l_r16"			! beyond it
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 16
	.uleb128 4
	.string	"l_r20"
	.long	.Ttlong - .Linfo0
	.uleb128 2
	.byte	0x90, 20
	.byte	0

! di_wide: 16-byte values
	.uleb128 3
	.string	"di_wide"
	.dword	di_wide
	.dword	.Ldi_wide_end - di_wide
	.uleb128 4
	.string	"ld_pair"		! %r4 mantissa, %r5 sign and exponent: 1.5
	.long	.Ttld - .Linfo0
	.uleb128 1
	.byte	0x54
	.uleb128 4
	.string	"ld_fx"			! %r6 with extended part 0x3fff: 1.5 (%r7 = 0x4000)
	.long	.Ttld - .Linfo0
	.uleb128 1
	.byte	0x56
	.uleb128 4
	.string	"ld_fx_piece16"		! DW_OP_reg6 piece 16: %r6's mantissa, %r7 as exponent: 3
	.long	.Ttld - .Linfo0
	.uleb128 3
	.byte	0x56, 0x93, 16
	.uleb128 4
	.string	"vec2"			! %r8, %r9
	.long	.Ttv2di - .Linfo0
	.uleb128 1
	.byte	0x58
	.uleb128 4
	.string	"vec2_pieces"		! DW_OP_reg8 piece 8, DW_OP_reg9 piece 8
	.long	.Ttv2di - .Linfo0
	.uleb128 6
	.byte	0x58, 0x93, 8, 0x59, 0x93, 8
	.uleb128 4
	.string	"vec2_swapped"		! DW_OP_reg9 piece 8, DW_OP_reg8 piece 8
	.long	.Ttv2di - .Linfo0
	.uleb128 6
	.byte	0x59, 0x93, 8, 0x58, 0x93, 8
	.uleb128 4
	.string	"i128"			! %r10 low, %r11 high
	.long	.Tti128 - .Linfo0
	.uleb128 1
	.byte	0x5a
	.uleb128 4
	.string	"f128"			! %r12 low, %r13 high: 1.0
	.long	.Ttf128 - .Linfo0
	.uleb128 1
	.byte	0x5c
	.uleb128 4
	.string	"ld_pieces"		! DW_OP_reg4 piece 8, DW_OP_reg5 piece 8: 1.5
	.long	.Ttld - .Linfo0
	.uleb128 6
	.byte	0x54, 0x93, 8, 0x55, 0x93, 8
	.uleb128 4
	.string	"ld_piece16"		! DW_OP_reg4 piece 16: 1.5
	.long	.Ttld - .Linfo0
	.uleb128 3
	.byte	0x54, 0x93, 16
	.uleb128 4
	.string	"f128_pieces"		! DW_OP_reg12 piece 8, DW_OP_reg13 piece 8: 1.0
	.long	.Ttf128 - .Linfo0
	.uleb128 6
	.byte	0x5c, 0x93, 8, 0x5d, 0x93, 8
	.uleb128 4
	.string	"qv"			! v5: %qpr14 = {0x6666, 0x5555}, %r15 = 0x7777
	.long	.Ttv2di - .Linfo0
	.uleb128 1
	.byte	0x5e
	.uleb128 4
	.string	"qv_low"		! DW_OP_reg14 piece 8, an empty piece 8
	.long	.Ttv2di - .Linfo0
	.uleb128 5
	.byte	0x5e, 0x93, 8, 0x93, 8
	.byte	0

! TLS: di_tls = 0x1234 in the main thread
	.uleb128 8
	.string	"di_tls_addr"		! lcc's form
	.long	.Ttlong - .Linfo0
	.uleb128 10
	.byte	0x03			! DW_OP_addr
	.dword	di_tls@TLS_DTPREL
	.byte	0xe0			! DW_OP_GNU_push_tls_address
	.uleb128 8
	.string	"di_tls_gnu"		! GCC's form before DWARF 5
	.long	.Ttlong - .Linfo0
	.uleb128 10
	.byte	0x0e			! DW_OP_const8u
	.dword	di_tls@TLS_DTPREL
	.byte	0xe0
	.uleb128 8
	.string	"di_tls_form"		! GCC's form from DWARF 5 on
	.long	.Ttlong - .Linfo0
	.uleb128 10
	.byte	0x0e
	.dword	di_tls@TLS_DTPREL
	.byte	0x9b			! DW_OP_form_tls_address
	.uleb128 8
	.string	"di_tls_4"		! a 4-byte offset (.word: R_E2K_TLS_32_DTPREL)
	.long	.Ttlong - .Linfo0
	.uleb128 6
	.byte	0x0c			! DW_OP_const4u
	.word	di_tls@TLS_DTPREL
	.byte	0x9b
	.byte	0
.Linfo_end:

! Location lists (offsets from the unit's low_pc)
	.section .debug_loc,"",@progbits
.Tloc_call:
	.dword	.Lcall_o - .Ltext0
	.dword	.Lret_o - .Ltext0
	.short	1
	.byte	0x52
	.dword	0, 0
.Tloc_ret:
	.dword	.Lret_o - .Ltext0
	.dword	.Lret_o + 8 - .Ltext0
	.short	1
	.byte	0x52
	.dword	0, 0

! Address ranges
	.section .debug_aranges,"",@progbits
	.long	.Laranges_end - .Laranges_start
.Laranges_start:
	.short	2
	.long	.Linfo0
	.byte	8, 0
	.long	0			! pad to 16
	.dword	.Ltext0
	.dword	.Letext0 - .Ltext0
	.dword	0, 0
.Laranges_end:
	.section .note.GNU-stack,"",@progbits
