/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Why lcc's 32-bit AddressSanitizer starts on the 4C but stops at start
   on the 8C2 and 2C3 (spec/abi/mode32.md, "C runtime"): this program
   reads its own /proc/self/maps and replays what lcc 1.29.16's
   compiler-rt does with it in a 32-bit process before the shadow is
   mapped:

   - MCST's ReadProcMaps drops the last three lines of the file (the
     hardware stacks, which a 32-bit address can't hold; compiler-rt
     11.1.0 lib/sanitizer_common/sanitizer_procmaps_linux.cpp:27-49 in
     MCST's runtime sources, toolchains/src/lcc-1.29.16/lib/);
   - MemoryMappingLayout::Next parses each remaining line with
     ParseHex into a 32-bit uptr, so an address above 4 GiB keeps only
     its low 32 bits (sanitizer_procmaps_common.cpp:37-64);
   - MemoryRangeIsAvailable skips empty ranges, checks
     CHECK_NE(0, segment.end), then IntervalsAreSeparate checks
     CHECK(start1 <= end1) with start1 = start, end1 = end - 1
     (sanitizer_posix.cpp:217-240; line 219 is the CHECK that fails),
     and returns false at the first line that overlaps the shadow.

   It prints every line, the lines above 4 GiB that the runtime parses
   with their 32-bit values, and which line stops the check first.
   Built without and with ASan (on the 4C the ASan build reaches main
   and prints the maps of a running ASan process, whose shadow then
   overlaps the range, as it should):

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc   # v3 on the 4C
     $L -mptr32 -O1 asan32.c -o asan32
     $L -mptr32 -O1 -fsanitize=address asan32.c -o asan32-asan
     ./asan32; ./asan32-asan  */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The range InitializeShadowMemory checks (asan_shadow_setup.cpp:
   91-114): kLowShadowBeg minus a page to kHighShadowEnd, with the
   32-bit shadow offset 1 << 29 (asan_mapping.h, kDefaultShadowOffset32)
   and kHighMemEnd 0xffffffff (sanitizer_linux.cpp, GetMaxVirtualAddress;
   GetKernelAreaSize is 0, since [stack] is writable and ends above
   3 GiB).  */
#define SHADOW_BEG 0x1ffff000u
#define SHADOW_END 0x3fffffffu

static char data[1 << 16];

/* compiler-rt's ParseNumber with a 32-bit uptr.  */
static unsigned
parse_hex32 (const char **p)
{
  unsigned n = 0;
  for (;;)
    {
      char c = **p;
      int d = c >= '0' && c <= '9' ? c - '0'
	: c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
      if (d < 0)
	return n;
      n = n * 16 + d;
      (*p)++;
    }
}

int
main (void)
{
  FILE *f = fopen ("/proc/self/maps", "r");
  if (!f)
    {
      perror ("maps");
      return 1;
    }
  size_t len = fread (data, 1, sizeof data - 1, f);
  fclose (f);
  data[len] = 0;
  printf ("maps (%zu bytes):\n%s", len, data);

  /* MCST's ReadProcMaps: four times, cut at the last newline.  */
  size_t cut = len;
  for (int i = 0; i < 4; i++)
    {
      char *nl = memrchr (data, '\n', cut);
      if (!nl)
	{
	  printf ("fewer than four newlines: CHECK (elim_str != 0) fails\n");
	  return 0;
	}
      cut = nl - data;
    }
  printf ("dropped by MCST's ReadProcMaps:%s", data + cut);
  printf ("\nparsed with a 32-bit uptr:\n");

  const char *pos = data, *last = data + cut;
  int line = 0, check = 0, overlap = 0;
  while (pos < last)
    {
      const char *nl = memchr (pos, '\n', last - pos);
      if (!nl)
	nl = last;
      const char *q = pos;
      unsigned start = parse_hex32 (&q);
      q++;			/* '-' */
      unsigned end = parse_hex32 (&q);
      line++;
      /* Lines with addresses of more than 8 digits, and those that fail
	 a CHECK or overlap the shadow.  */
      int wide = strchr (pos, '-') - pos > 8;
      int bad = start != end && (end == 0 || start > end - 1);
      int ovl = start != end && !bad
	&& !(end - 1 < SHADOW_BEG || SHADOW_END < start);
      if (wide || bad || ovl)
	printf ("  line %d: %.*s\n    -> start 0x%x end 0x%x%s\n", line,
		(int) (nl - pos), pos, start, end,
		end == 0 && start != end
		? ": CHECK_NE (0, segment.end) fails (sanitizer_posix.cpp:235)"
		: bad ? ": CHECK (start1 <= end1) fails "
		"(sanitizer_posix.cpp:219), \"(0x0, 0x0)\""
		: ovl ? ": overlaps the shadow range" : "");
      if (bad && !check)
	check = line;
      if (ovl && !overlap)
	overlap = line;
      pos = nl + 1;
    }
  /* MemoryRangeIsAvailable stops at the first overlap (in an ASan
     process, the shadow itself once it is mapped).  */
  if (check && (!overlap || check < overlap))
    printf ("verdict: line %d fails a CHECK: the runtime stops at start\n",
	    check);
  else if (overlap)
    printf ("verdict: line %d overlaps the shadow range first%s\n",
	    overlap, check ? " (a later line would fail a CHECK)" : "");
  else
    printf ("verdict: every parsed line passes the CHECKs\n");
  return 0;
}
