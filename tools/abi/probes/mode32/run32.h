/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shared declarations of run32.c and run32-lib.c (32-bit mode probes,
   spec/abi/mode32.md).  */

#include <stdarg.h>

struct s4 { short a, b; };
struct s12 { int a, b, c; };
struct s16 { long long a, b; };
struct s68 { int a[17]; };

#define MIXED_SUM(m_c, m_ll, m_s, m_i, m_t, m_d, m_u, m_f, m_ld, m_p,	\
		  m_uc, m_sh)						\
	((long long) (m_c) + (m_ll) * 3 + (m_s).a * 5 + (m_s).b * 7	\
	 + (m_s).c * 11 + (m_i) * 13 + (m_t).a * 17 + (m_t).b * 19	\
	 + (long long) ((m_d) * 23) + (m_u).a * 29 + (m_u).b * 31	\
	 + (long long) ((m_f) * 37) + (long long) ((m_ld) * 41)		\
	 + (long long) (unsigned long) (m_p) * 43 + (m_uc) * 47		\
	 + (m_sh) * 53)

extern __thread int lib_tls;
extern int lib_data;
int *lib_tls_addr (void);
int *lib_data_addr (void);
int lib_fn (int);
int (*lib_fp (void)) (int);
int (*lib_fn_addr (void)) (int);
long long lib_mixed (char, long long, struct s12, int, struct s16, double,
		     struct s4, float, long double, void *, unsigned char,
		     short);
long long lib_va (int, ...);
int lib_vasize (va_list);
struct s68 lib_ret68 (int);
struct s12 lib_ret12 (int);
char lib_rc (int);
unsigned short lib_rus (int);
long lib_rl (long);
long long lib_rll (long long);

/* mode32.s */
long long m32_sys1 (long long, long long, long long, long long);
long long m32_sys3 (long long, long long, long long, long long);
long long m32_sys5 (long long, long long, long long, long long);
long long m32_sys6 (long long, long long, long long, long long);
int m32_ldgdw2 (unsigned, unsigned);
int m32_ldw64 (long long);
void m32_stw64 (long long, int);
long long m32_getpl (unsigned);
int m32_getpl_tag (unsigned);
long long m32_getpl_hi (unsigned);
long long m32_getpl_odd (unsigned);
int m32_callpl (int (*) (int), int);
long long m32_ip (void);
long long m32_g13 (void);
long long m32_sp (void);
long long m32_raw (int);
