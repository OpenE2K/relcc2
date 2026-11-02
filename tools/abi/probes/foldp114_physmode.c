/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp114_physmode.c: data and calls a compiler adds to code that runs
 * before virtual memory (MCST's arch/e2k/p2v, the decompressor in
 * arch/e2k/boot).  p2v runs at physical addresses and reaches the
 * kernel's data only through boot_vp_to_pp () and the like
 * (asm/p2v/boot_v2p.h:38-78, 165-166), so a table or constant the
 * compiler puts in .rodata and addresses with an absolute literal is read
 * at its virtual address (spec/abi/kernel-build.md, "Physical mode").
 * Three shapes from the kernel:
 *
 *   machine_id   boot_get_e2k_machine_id's if-chain (p2v/boot_e2k_sic.c:
 *                22-54): GCC's if-to-switch and switch conversion make a
 *                CSWTCH table, -fno-jump-tables or not
 *   clear_table  boot_pte_alloc's zeroing loop (p2v/boot_map.c:174-177,
 *                inlined into boot_get_pte): a memset call, or with
 *                -ffreestanding a vectorized loop whose 16-byte zero may
 *                come from .rodata.cst16 (v5 and later)
 *   is_ex_sig    parse_console_params's strncmp of a literal with a bound
 *                past its NUL (boot/decompress.c:615,
 *                arch/l/include/asm/bootinfo.h:31-32): GCC calls strcmp,
 *                which the decompressor doesn't define
 *
 * Compile only; list the relocations against .rodata* and the undefined
 * symbols:
 *
 *   P=tools/abi/probes/foldp114_physmode.c
 *   /opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc -O3 -fkernel \
 *       -fno-semi-spec-ld -fno-spec-ld -fno-jump-tables -c $P -o l.o
 *   for f in "" -ffreestanding \
 *       "-ffreestanding -fno-tree-switch-conversion -fno-tree-vectorize"; do
 *     e2k-linux-gnu-gcc -O3 -mtune=elbrus-8c2 -fno-PIE -fno-jump-tables \
 *         $f -c $P -o g.o     # (bash; in zsh write ${=f})
 *     readelf -rW g.o | grep -o 'rodata[.a-z0-9]*\|CSWTCH[.0-9]*'
 *     readelf -sW g.o | awk '$7 == "UND" { print $8 }'
 *   done
 *
 * (also e2k-v3 and e2k-v6.2c3 and lcc 1.31.05; the host's gcc -O3
 * -fno-jump-tables shows the CSWTCH table and strcmp too).
 *
 * 2026-11-03: lcc 1.29.16 (v3, v5.8c2, v6.2c3) and 1.31.05 (v3, v5.8c2):
 * one relocation against .rodata, the string literal's; undefined only
 * strncmp and mdl; the if-chain stays compares and the loop a loop.  The
 * proof-of-concept GCC (0.6 crosses for v3 and the 8C2) without flags:
 * machine_id reads a CSWTCH table (32 bytes, .rodata.cst32; the kernel's
 * 48 bytes go to .rodata) through an R_E2K_64_ABS_LIT, clear_table calls
 * memset, is_ex_sig calls strcmp.  With -ffreestanding the table stays,
 * strncmp stays, and for the 8C2 clear_table reads a 16-byte zero from
 * .rodata.cst16 (v3 has no 16-byte vectors: nothing).  With all three
 * flags only the literal is left.  With -fpic the table is addressed
 * @GOTOFF, which works at any load address (the decompressor's case).
 */

int strncmp (const char *, const char *, unsigned long);

extern unsigned int mdl (void);

unsigned int
machine_id (void)
{
  unsigned int m = mdl (), id;

  if (m == 3)
    id = 1;
  else if (m == 4)
    id = 2;
  else if (m == 5)
    id = 3;
  else if (m == 6)
    id = 5;
  else if (m == 7)
    id = 4;
  else if (m == 8)
    id = 8;
  else if (m == 9)
    id = 9;
  else if (m == 10)
    id = 11;
  else
    return 0;
  return id;
}

void
clear_table (unsigned long *p)
{
  for (int i = 0; i < 512; i++)
    p[i] = 0;
}

int
is_ex_sig (const char *s)
{
  return !strncmp (s, "KERNEL_ARGS_STRING_EX", 22);
}
