/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What __builtin___clear_cache compiles to: the call a trampoline
   initialiser makes after writing code (tools/abi/probes/execstack.sh). */
void flush(char *b, char *e) { __builtin___clear_cache(b, e); }
