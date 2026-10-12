! SPDX-License-Identifier: GPL-3.0-or-later
! Helpers for run32.c (32-bit mode, lcc -mptr32): raw system calls through
! each entry, loads in the address forms 32-bit code can use, and the
! registers that hold addresses. Built with run32.c (see run.sh):
!   lcc -mptr32 -O1 run32.c mode32.s ...
! Every function follows the 32-bit calling convention: arguments in
! %r0.. (a long long takes a whole %dr), the result in %r0 or %dr0.
	.ignore	ld_st_style
	.text

! long long m32_sysE (long long nr, long long a1, long long a2,
!                     long long a3): system call through entry E with the
! four values copied whole (upper halves included) to %db[0]..%db[3].
	.macro	sys name entry
	.global	\name
	.type	\name, #function
	.align	8
\name:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	}
	{
	  sdisp	%ctpr1, \entry
	  addd,0	0x0, %dr0, %db[0]
	  addd,1	0x0, %dr1, %db[1]
	  addd,3	0x0, %dr2, %db[2]
	  addd,4	0x0, %dr3, %db[3]
	}
	{
	  nop 4
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  return	%ctpr3
	  addd,0	0x0, %db[0], %dr0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	\name, .-\name
	.endm

	sys	m32_sys1, 0x1
	sys	m32_sys3, 0x3
	sys	m32_sys5, 0x5
	sys	m32_sys6, 0x6

! int m32_ldgdw2 (unsigned a, unsigned b): ldgdw from a + b.
	.global	m32_ldgdw2
	.type	m32_ldgdw2, #function
	.align	8
m32_ldgdw2:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  ldgdw,0	%r0, %r1, %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_ldgdw2, .-m32_ldgdw2

! int m32_ldw64 (long long addr): an ordinary (flat, 64-bit) ldw.
	.global	m32_ldw64
	.type	m32_ldw64, #function
	.align	8
m32_ldw64:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  ldw,0	%dr0, 0x0, %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_ldw64, .-m32_ldw64

! void m32_stw64 (long long addr, int v): an ordinary (flat) stw.
	.global	m32_stw64
	.type	m32_stw64, #function
	.align	8
m32_stw64:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  stw,2	%dr0, 0x0, %r1
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_stw64, .-m32_stw64

! long long m32_getpl (unsigned x): getpl into a register.
	.global	m32_getpl
	.type	m32_getpl, #function
	.align	8
m32_getpl:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  getpl,0	%r0, %dr0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_getpl, .-m32_getpl

! int m32_callpl (int (*f) (int), int x): getpl into %ctpr1 and call,
! as lcc's indirect calls do.
	.global	m32_callpl
	.type	m32_callpl, #function
	.align	8
m32_callpl:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x0
	  setbn	rsz = 0x3, rbs = 0x4, rcur = 0x0
	  getsp,0	-0x10, %r2
	}
	{
	  getpl,0	%r0, %ctpr1
	  adds,1	0x0, %r1, %b[0]
	}
	{
	  nop 5
	}
	{
	  call	%ctpr1, wbs = 0x4
	}
	{
	  return	%ctpr3
	  adds,0	0x0, %b[0], %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_callpl, .-m32_callpl

! long long m32_ip (void): %ip, 64 bits.
	.global	m32_ip
	.type	m32_ip, #function
	.align	8
m32_ip:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  rrd,0	%ip, %dr0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_ip, .-m32_ip

! long long m32_g13 (void): the thread pointer, 64 bits.
	.global	m32_g13
	.type	m32_g13, #function
	.align	8
m32_g13:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, %dg13, %dr0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_g13, .-m32_g13

! long long m32_sp (void): getsp 0, 64 bits (this function's stack top).
	.global	m32_sp
	.type	m32_sp, #function
	.align	8
m32_sp:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  getsp,0	0x0, %dr0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_sp, .-m32_sp

! long long m32_raw (int x): returns its first argument register whole,
! so a C caller that passes an int shows what it leaves in the upper half.
	.global	m32_raw
	.type	m32_raw, #function
	.align	8
m32_raw:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_raw, .-m32_raw

! int m32_getpl_tag (unsigned x): the tags of getpl's register result:
! gettagd of %dr0 in bits 0-7, of %dr1 in bits 8-15 (on v6 getpl writes
! a 128-bit procedure label to the pair %dr0/%dr1; on v3-v5 only %dr0).
	.global	m32_getpl_tag
	.type	m32_getpl_tag, #function
	.align	8
m32_getpl_tag:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x0, %dr1
	}
	{
	  getpl,0	%r0, %dr0
	}
	{
	  nop 4
	}
	{
	  gettagd,2	%dr0, %r2
	  gettagd,5	%dr1, %r3
	}
	{
	  nop 4
	}
	{
	  shls,0	%r3, 0x8, %r3
	}
	{
	  ors,0	%r2, %r3, %r0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_getpl_tag, .-m32_getpl_tag

! long long m32_getpl_hi (unsigned x): %dr1 after getpl %r0, %dr0, copied
! speculatively (a plain read of a tagged value may trap); 0x1111 if getpl
! did not write it.
	.global	m32_getpl_hi
	.type	m32_getpl_hi, #function
	.align	8
m32_getpl_hi:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	0x0, 0x1111, %dr1
	}
	{
	  getpl,0	%r0, %dr0
	}
	{
	  nop 4
	}
	{
	  addd,0,sm	0x0, %dr1, %dr0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_getpl_hi, .-m32_getpl_hi

! long long m32_getpl_odd (unsigned x): getpl into the odd register %dr1,
! then a speculative copy of it.
	.global	m32_getpl_odd
	.type	m32_getpl_odd, #function
	.align	8
m32_getpl_odd:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	}
	{
	  getpl,0	%r0, %dr1
	}
	{
	  nop 4
	}
	{
	  addd,0,sm	0x0, %dr1, %dr0
	}
	{
	  nop 4
	}
	{
	  ct	%ctpr3
	}
	.size	m32_getpl_odd, .-m32_getpl_odd
