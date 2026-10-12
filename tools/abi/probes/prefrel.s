! SPDX-License-Identifier: GPL-3.0-or-later
! Where does `pref %iprN, label` point after linking, for a .L label, a
! plain local label and a global symbol (spec/abi/elf.md, "Relocation
! types": R_E2K_PREF)?  Linked only, never run: prefrel.sh builds it,
! prints the relocations gas emits and checks each linked pref's target.
! The *_at symbols are global aliases of the local labels, only so that
! prefrel.sh can find their addresses with nm.
	.text
	.global	main
main:
	{ setwd wsz = 0x8, nfx = 0x1, dbl = 0x0 ; return %ctpr3 }
	{ pref %ipr0, .Lloc }
	{ pref %ipr1, glob }
	{ pref %ipr2, loc2 }
	{ pref %ipr3, main }
	{ nop 5 }
	{ ct %ctpr3 }
	.balign	4096
	.global	lloc_at
lloc_at:
.Lloc:
	{ nop }
	.global	glob
glob:
	{ nop }
	.global	loc2_at
loc2_at:
loc2:
	{ nop }
