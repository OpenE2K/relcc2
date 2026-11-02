// SPDX-License-Identifier: GPL-3.0-or-later
/* The visibility of a template instantiation whose template arguments
   are hidden (spec/abi/calling-convention.md, "C++ ABI beyond calls";
   docs/spec-poc-findings.md A57).  mapnik 3.1.0 builds with
   -fvisibility=hidden and marks singleton and datasource_cache visible
   (MAPNIK_DECL) but not the creation policy CreateStatic; programs
   linked against libmapnik.so call
   singleton<datasource_cache, CreateStatic>::instance().  Compile
   only, read the symbols:

     F="-fvisibility=hidden -fvisibility-inlines-hidden -O3 -fPIC"
     X=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/l++    (or 1.31.05)
     $X $F -c folda57_tmplvis.cc -o v.o            # also -DARGS_VISIBLE
     readelf -sW v.o | awk '$8 ~ /_Z/ {print $5, $6, $8}' | c++filt | sort
     $X -O3 -fPIC -DARGS_HIDDEN -c folda57_tmplvis.cc -o v.o   # no -fvisibility
     (and the same with any GCC's g++)

   2026-11-03:
     instantiation over a hidden    template template arg   type arg
     l++ 1.29.16 (v5.8c2), 1.31.05 (v3)   WEAK DEFAULT     WEAK DEFAULT
     GCC 11.3.0 (x86-64, OS Elbrus's PDK host)   WEAK DEFAULT   WEAK HIDDEN
     GCC 16.2.1 (x86-64), PoC 0.6+33       WEAK HIDDEN      WEAK HIDDEN
   tt is singleton<datasource_cache, CreateStatic>::instance(), type is
   holder<HiddenType>::get(); their static locals and guard follow
   (UNIQUE).  With -DARGS_VISIBLE every compiler gives WEAK DEFAULT.
   -DARGS_HIDDEN (the arguments marked hidden, no -fvisibility) gives
   the same table.  l++ hides the hidden class's own members
   (CreateStatic<datasource_cache>::create()::t), but never limits an
   instantiation by its arguments.  GCC has limited it by type
   arguments since 4.2 (b9e75696307, 2006, constrain_visibility_for_
   template) and by template template and pointer-to-member arguments
   since 14 (7226f825db0, PR c++/70413, min_vis_expr_r in decl2.cc).  */

#define DECL __attribute__ ((visibility ("default")))
#if defined ARGS_VISIBLE
# define ADECL DECL
#elif defined ARGS_HIDDEN
# define ADECL __attribute__ ((visibility ("hidden")))
#else
# define ADECL
#endif
#define NOINLINE __attribute__ ((noinline))

/* A template template argument: mapnik's singleton and policy.  */
template <typename T> class ADECL CreateStatic
{
public:
  static T *create () { static T t; return &t; }
};

template <typename T, template <typename> class Create> class DECL singleton
{
public:
  static T &instance ();
};

template <typename T, template <typename> class Create>
NOINLINE T &singleton<T, Create>::instance ()
{
  static T *p = Create<T>::create ();
  return *p;
}

class DECL datasource_cache : public singleton<datasource_cache, CreateStatic>
{
public:
  int x;
};

DECL int tt_get () { return datasource_cache::instance ().x; }

/* A type argument.  */
struct ADECL HiddenType { int v; };

template <typename T> struct DECL holder { static int get (); };

template <typename T> NOINLINE int holder<T>::get ()
{
  static T t;
  return t.v;
}

DECL int type_get () { return holder<HiddenType>::get (); }
