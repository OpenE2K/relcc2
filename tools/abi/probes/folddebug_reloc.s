! SPDX-License-Identifier: GPL-3.0-or-later
! Does MCST's ld check a 32-bit DWARF offset (R_E2K_32_ABS in a debug
! section) for overflow?  In the 32-bit DWARF format every offset from
! one debug section into another is such a relocation, and an offset
! past 4 GiB is what a linked program whose .debug_str or .debug_info
! outgrows 4 GiB would need.  Building such a section is impractical, so
! the offset comes from an absolute symbol instead:
!
!   B=toolchains/binutils/bin
!   $B/e2k-linux-as tools/abi/probes/folddebug_reloc.s -o reloc.o
!   $B/e2k-linux-ld --defsym big_off=0xfffffff0 reloc.o -o reloc
!   $B/e2k-linux-ld --defsym big_off=0x100000010 reloc.o -o reloc
!
! Result (binutils b4bebd940a3, 2026-10-16): the first links; the second
! fails, "relocation truncated to fit: R_E2K_32_ABS against symbol
! `big_off'", while the R_E2K_64_ABS beside it (the 64-bit format's
! offset) takes it.  bfd's howto is complain_overflow_bitfield
! (elfxx-e2k.c:183-186), which accepts up to 2^32 - 1.

	.section .debug_abbrev,"",@progbits
	.byte	0
	.section .debug_info,"",@progbits
	.4byte	big_off
	.8byte	big_off
	.text
	.global	_start
_start:
	{ nop }
