! SPDX-License-Identifier: GPL-3.0-or-later
! The smallest protected-mode (e2k128) program: _start makes an array
! pointer to a string in its globals (gdtoap + aptoapb: both fault in a
! 64-bit process), writes it with write (1, msg, 3) and calls exit (42),
! both through the protected system call entry 8 (glibc
! sysdeps/unix/sysv/linux/e2k/e2k128/sysdep.h: the number in %qb[0],
! the arguments from %qb[2] on, each a quad; 8 register pairs handed
! over).  run.sh --left links it twice, as ELF64 and as ELF32, to see
! whether the kernel runs a 32-bit protected ELF file (it has a loader
! for them, binfmt_elf32_3P.c).  lcc never makes one: gas -mptr128
! always writes elf64-e2k-pm, so the ELF32 object comes from objcopy:
!
!   as -mptr128 -mcpu=elbrus-v3 pmmin.s -o pmmin64.o
!   ld -m elf64_e2k_pm -static pmmin64.o -o pmmin64
!   objcopy -O elf32-e2k-pm pmmin64.o pmmin32.o
!   ld -m elf32_e2k_pm -static pmmin32.o -o pmmin32
!   ./pmmin64; echo $?; ./pmmin32; echo $?    ("pm" and 42 if it ran)

	.text
	.global	_start
	.type	_start, @function
_start:
	{
	  setwd	wsz = 0x10, nfx = 0x1
	  setbn	rsz = 0x7, rbs = 0x8, rcur = 0x0
	}
	{
	  nop 2
	  gdtoap,0	[ _f32s,_lts0 msg ], %qr0
	}
	{
	  nop 1
	  aptoapb,0	%qr0, 0x3, %qb[4]
	  addd,3	0x0, 0x4, %db[0]
	  addd,4	0x0, 0x0, %db[1]
	}
	{
	  sdisp	%ctpr1, 0x8
	  addd,0	0x0, 0x1, %db[2]
	  addd,1	0x0, 0x0, %db[3]
	  addd,3	0x0, 0x3, %db[6]
	  addd,4	0x0, 0x0, %db[7]
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	{
	  sdisp	%ctpr1, 0x8
	  addd,0	0x0, 0x1, %db[0]
	  addd,1	0x0, 0x0, %db[1]
	  addd,3	0x0, 0x2a, %db[2]
	  addd,4	0x0, 0x0, %db[3]
	}
	{
	  call	%ctpr1, wbs = 0x8
	}
	.size	_start, .-_start

	.section .rodata
	.type	msg, @object
	.size	msg, 4
	.align	4
msg:
	.ascii	"pm\n\000"
