/* SPDX-License-Identifier: GPL-3.0-or-later */
/* QEMU TCG plugin for isa-score: counts how often each guest instruction
   (each E2K wide instruction) executes.  When the program exits it writes
   one line per translation block to the file given as out=PATH:

     START COUNT1 COUNT2 ...

   START is the block's first address and COUNTi the executions of its
   i-th wide instruction.  score.py assigns the counts to addresses by
   walking the instruction sizes in the binary: qemu-e2k reports a wrong
   address for every instruction after the first of a block (its
   translator advances pc_next by the instruction size twice).

     qemu-e2k -plugin ./libbbcount.so,out=counts.txt ./program

   Built by count.sh against toolchains/qemu-mcst/include/qemu-plugin.h.
   Every instruction has its own counter: qemu-e2k's translation blocks
   continue past conditional transfers, so a block's count would not be
   its instructions' counts.  */

#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

typedef struct
{
  size_t n;
  uint64_t *addrs;
  struct qemu_plugin_scoreboard *count;
} Tb;

static GMutex lock;
static GPtrArray *tbs;
static char *out_path;

static void
tb_trans (qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
  Tb *t = g_new0 (Tb, 1);
  t->n = qemu_plugin_tb_n_insns (tb);
  t->addrs = g_new (uint64_t, t->n);
  for (size_t i = 0; i < t->n; i++)
    t->addrs[i] = qemu_plugin_insn_vaddr (qemu_plugin_tb_get_insn (tb, i));
  t->count = qemu_plugin_scoreboard_new (t->n * sizeof (uint64_t));
  g_mutex_lock (&lock);
  g_ptr_array_add (tbs, t);
  g_mutex_unlock (&lock);
  for (size_t i = 0; i < t->n; i++)
    {
      qemu_plugin_u64 c = { t->count, i * sizeof (uint64_t) };
      qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu (
          qemu_plugin_tb_get_insn (tb, i), QEMU_PLUGIN_INLINE_ADD_U64, c, 1);
    }
}

static void
plugin_exit (qemu_plugin_id_t id, void *p)
{
  FILE *f = fopen (out_path ? out_path : "bbcount.txt", "w");
  if (!f)
    {
      perror ("bbcount");
      return;
    }
  for (guint k = 0; k < tbs->len; k++)
    {
      Tb *t = g_ptr_array_index (tbs, k);
      uint64_t any = 0;
      for (size_t i = 0; i < t->n; i++)
        {
          qemu_plugin_u64 u = { t->count, i * sizeof (uint64_t) };
          any |= qemu_plugin_u64_sum (u);
        }
      if (!any)
        continue;
      fprintf (f, "%" PRIx64, t->addrs[0]);
      for (size_t i = 0; i < t->n; i++)
        {
          qemu_plugin_u64 u = { t->count, i * sizeof (uint64_t) };
          fprintf (f, " %" PRIu64, qemu_plugin_u64_sum (u));
        }
      fputc ('\n', f);
    }
  fclose (f);
}

QEMU_PLUGIN_EXPORT int
qemu_plugin_install (qemu_plugin_id_t id, const qemu_info_t *info, int argc,
                     char **argv)
{
  for (int i = 0; i < argc; i++)
    if (strncmp (argv[i], "out=", 4) == 0)
      out_path = g_strdup (argv[i] + 4);
  tbs = g_ptr_array_new ();
  qemu_plugin_register_vcpu_tb_trans_cb (id, tb_trans);
  qemu_plugin_register_atexit_cb (id, plugin_exit, NULL);
  return 0;
}
