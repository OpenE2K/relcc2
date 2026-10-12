/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: files next to the program. Reads smoke-data.txt (copied to
   the target with remote_download), prints it, and writes it reversed
   line by line to smoke-copy.txt (copied back with remote_upload).  */
#include <stdio.h>
#include <string.h>

int main (void)
{
  char lines[16][128];
  int n = 0;
  FILE *in = fopen ("smoke-data.txt", "r");
  if (!in)
    {
      perror ("smoke-data.txt");
      return 1;
    }
  while (n < 16 && fgets (lines[n], sizeof lines[n], in))
    fputs (lines[n++], stdout);
  fclose (in);
  FILE *out = fopen ("smoke-copy.txt", "w");
  if (!out)
    {
      perror ("smoke-copy.txt");
      return 1;
    }
  while (n > 0)
    fputs (lines[--n], out);
  return fclose (out) != 0;
}
