! SPDX-License-Identifier: GPL-3.0-or-later
! _GLOBAL_OFFSET_TABLE_ and @GOTOFF in data directives (spec/abi/elf.md,
! "Assembler syntax"; docs/spec-poc-findings.md A34).  gas treats a bare
! _GLOBAL_OFFSET_TABLE_ as PC-relative (O_pc_lit, tc-e2k.c
! e2k_parse_name) in data as well as in code literals, and has no data
! relocation for that, so a data directive must name the GOT with @ABS.
!
! Linked only, never run: gotdata.sh builds it.  --defsym CASE=0 (the
! default) holds the forms that assemble; gotdata.sh links them into a
! PIE and a plain executable and checks every word against the linked
! _GLOBAL_OFFSET_TABLE_ and loc.  CASE=1 ... 8 are the forms that fail,
! one per assembly.
	.ifndef	CASE
	CASE = 0
	.endif

	.text
	.global	main
	.type	main, @function
main:
	{ setwd wsz = 0x8, nfx = 0x1, dbl = 0x0 ; return %ctpr3 }
	{ nop 5 }
	{ ct %ctpr3 }
	.size	main, . - main

	.data
	.p2align 3
loc:	.dword	0			! a local symbol
	.global	glob
glob:	.dword	0			! a global one

	.if	CASE == 0
got_abs:	.dword	_GLOBAL_OFFSET_TABLE_@ABS	! R_E2K_64_ABS
got_pc:		.dword	_GLOBAL_OFFSET_TABLE_@ABS - .	! R_E2K_64_PC
loc_gotoff64:	.dword	loc@GOTOFF			! R_E2K_64_GOTOFF
loc_gotoff32:	.word	loc@GOTOFF			! R_E2K_GOTOFF
	.section .debug_abbrev,"",@progbits
	.byte	0
	.section .debug_info,"",@progbits
	.dword	_GLOBAL_OFFSET_TABLE_@ABS		! R_E2K_64_ABS
	.dword	loc@GOTOFF				! R_E2K_64_GOTOFF
	.dword	loc					! R_E2K_64_ABS
	.endif

	.if	CASE == 1
	.dword	_GLOBAL_OFFSET_TABLE_
	.endif
	.if	CASE == 2
	.section .debug_info,"",@progbits
	.dword	_GLOBAL_OFFSET_TABLE_
	.endif
	.if	CASE == 3
	.word	_GLOBAL_OFFSET_TABLE_
	.endif
	.if	CASE == 4
	.dword	glob - _GLOBAL_OFFSET_TABLE_
	.endif
	.if	CASE == 5
	.dword	_GLOBAL_OFFSET_TABLE_ - .
	.endif
	.if	CASE == 6
	.dword	glob@PCREL
	.endif
	.if	CASE == 7
	.word	glob@ISLOCAL32
	.endif
	.if	CASE == 8
	.dword	glob@GOT
	.endif
