#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""What lcc 1.31.05 emits for the builtins that are not one operation
(spec/abi/lcc-builtins.md): memory accesses with a MAS and a channel,
barriers, CPU detection, system registers, protected-mode descriptors,
and the helper macros of <e2kintrin.h>.

  python3 tools/abi/probes/gen_builtins_misc.py [-o DIR]

Each case is one C function compiled alone at -O2 (lcc reports some
errors only after its front end, one per compilation), traced as
gen_builtins.py does, with channel numbers kept.  Results go to
spec/abi/lcc-builtins-misc.yaml.
"""

import argparse
import re
from pathlib import Path

import yaml

import gen_builtins as gb

ROOT = gb.ROOT
HEAD = ("typedef long long v2di __attribute__ ((vector_size (16)));\n"
        "typedef unsigned __int128 u128;\n#include <e2kintrin.h>\n")
U64, PTR, V2, U128 = "unsigned long long", "void *", "v2di", "u128"
KIND = {U64: "int", PTR: "int", "unsigned": "int", "int": "int",
        "const char *": "int", V2: "pair", U128: "pair", "void": "void",
        "unsigned char": "int", "unsigned short": "int",
        "unsigned int": "int"}


def case(cases, key, iset, ret, params, body, opts=()):
    """A function `ret f (params) { body }`; body is an expression for a
    non-void RET, a statement otherwise."""
    plist = ", ".join(f"{t} a{i}" for i, t in enumerate(params)) or "void"
    stmt = body if ret == "void" else f"return {body};"
    cases.append({"key": key, "iset": iset, "opts": list(opts),
                  "c": f"{ret} f0 ({plist}) {{ {stmt} }}",
                  "kinds": [KIND[t] for t in params], "ret": KIND[ret]})


def mas_cases(c):
    sizes = [("8u", "unsigned char", 3), ("16u", "unsigned short", 3),
             ("32u", "unsigned int", 3), ("64u", U64, 3), ("128v", V2, 5)]
    flags = ["__LCC_MAS_NO", "0x8", "0x20", "0x40", "0x60", "0x3", "0x4",
             "0x7", "__LCC_MAS_SPEC", "__LCC_MAS_VOLATILE",
             "__LCC_MAS_CLEARTAG", "__LCC_MAS_SPEC | __LCC_MAS_CLEARTAG",
             "__LCC_MAS_SPEC | 0x8", "__LCC_MAS_VOLATILE | 0x8"]
    for sz, t, iset in sizes:
        for m in flags:
            case(c, f"loadmas_{sz} mas={m}", iset, t, [PTR],
                 f"__builtin_loadmas_{sz} (a0, {m}, __LCC_CHAN_ANY)")
        for ch in range(6):
            case(c, f"loadmas_{sz} chan={ch}", iset, t, [PTR],
                 f"__builtin_loadmas_{sz} (a0, __LCC_MAS_NO, {ch})")
        case(c, f"loadmas_{sz} mas=variable", iset, t, [PTR, "int"],
             f"__builtin_loadmas_{sz} (a0, a1, __LCC_CHAN_ANY)")
        case(c, f"loadmas_{sz} chan=variable", iset, t, [PTR, "int"],
             f"__builtin_loadmas_{sz} (a0, __LCC_MAS_NO, a1)")
        for m in flags:
            case(c, f"storemas_{sz} mas={m}", iset, "void", [PTR, t],
                 f"__builtin_storemas_{sz} (a1, a0, {m}, __LCC_CHAN_ANY);")
        for ch in range(6):
            case(c, f"storemas_{sz} chan={ch}", iset, "void", [PTR, t],
                 f"__builtin_storemas_{sz} (a1, a0, __LCC_MAS_NO, {ch});")
    for m in ("__LCC_MAS_NO", "0x8", "__LCC_MAS_SPEC"):
        case(c, f"pstoremas_128v mas={m}", 5, "void", [PTR, V2, "unsigned"],
             f"__builtin_pstoremas_128v (a1, a0, a2, {m}, __LCC_CHAN_ANY);")
    for ch in range(6):
        case(c, f"pstoremas_128v chan={ch}", 5, "void", [PTR, V2, "unsigned"],
             f"__builtin_pstoremas_128v (a1, a0, a2, __LCC_MAS_NO, {ch});")


def barrier_cases(c):
    for iset in (3, 6):
        for b in ("read", "write", "mem", ""):
            name = f"__builtin_e2k_{b}_barrier" if b else "__builtin_e2k_barrier"
            for m in [0] + [1 << i for i in range(12)]:
                case(c, f"{name.split('builtin_e2k_')[1]} {m:#x} v{iset}", iset,
                     "void", [], f"{name} ({m:#x});")
            case(c, f"{name.split('builtin_e2k_')[1]} variable v{iset}", iset,
                 "void", ["unsigned"], f"{name} (a0);")
    for b in ("read", "write", "mem", ""):
        name = f"__builtin_{b}_barrier" if b else "__builtin_barrier"
        case(c, f"{name} (lcc 1.29)", 3, "void", [], f"{name} ();")


CPUS = ["elbrus", "elbrus-2c+", "elbrus-4c", "elbrus-8c", "elbrus-1c+",
        "elbrus-8c2", "elbrus-12c", "elbrus-16c", "elbrus-2c3", "elbrus-48c",
        "elbrus-8v7", "elbrus-v1", "elbrus-v2", "elbrus-v3", "elbrus-v4",
        "elbrus-v5", "elbrus-v6", "elbrus-v7", "elbrus-2c", "e8c"]


def cpu_cases(c):
    case(c, "cpu_init", 3, "void", [], "__builtin_cpu_init ();")
    for s in CPUS:
        case(c, f'cpu_is "{s}"', 3, "int", [], f'__builtin_cpu_is ("{s}")')
    case(c, "cpu_is variable", 3, "int", ["const char *"], "__builtin_cpu_is (a0)")
    case(c, "cpu_name", 3, "const char *", [], "__builtin_cpu_name ()")
    case(c, "cpu_arch", 3, "const char *", [], "__builtin_cpu_arch ()")
    case(c, "e2k_cpu_iset (the manual's name)", 3, "unsigned", [],
         "__builtin_e2k_cpu_iset ()")
    for iset in (3, 7):
        case(c, f"e2k_get_cpu_iset v{iset}", iset, "unsigned", [],
             "__builtin_e2k_get_cpu_iset ()")


def sysreg_cases(c):
    modes = ["core_mode.descr_v7", "core_mode.macp_enbl", "madmr.mode_ld",
             "madmr.mode_st"]
    for iset, opts in ((3, ()), (7, ()), (7, ("-mptr128",))):
        sfx = f" v{iset}" + (" ptr128" if opts else "")
        for m in modes:
            case(c, f'get_cpu_mode "{m}"{sfx}', iset, "unsigned", [],
                 f'__builtin_e2k_get_cpu_mode ("{m}")', opts)
            case(c, f'set_cpu_mode "{m}"{sfx}', iset, "void", ["unsigned"],
                 f'__builtin_e2k_set_cpu_mode ("{m}", a0);', opts)
        for f in ("rwap_base", "rwap_size", "rwap_curptr", "rwsap_base",
                  "rwsap_size", "rwsap_curptr"):
            case(c, f"get_{f}{sfx}", iset, U64, [U128],
                 f"__builtin_e2k_get_{f} (a0)", opts)
    for r in ("psp", "pcsp", "usd", "cud", "gd", "cutd"):
        case(c, f'rrq "{r}"', 3, U128, [], f'__builtin_e2k_rrq ("{r}")')


def protected_cases(c):
    one = ["get_ap_base", "get_ap_size", "get_ap_curptr", "get_ap_address",
           "get_ap_color", "is_ptr_ap", "is_ptr_pl", "is_ptr_nv",
           "is_ptr_nv_number", "is_ptr_nv_image", "get_ptr_tag",
           "load_gettags", "load_gettagd", "load_gettagq", "is_ptr_ap_overbound"]
    for iset in (3, 7):
        for f in one:
            case(c, f"{f} v{iset} ptr128", iset, U64, [PTR],
                 f"__builtin_e2k_{f} (a0)", ("-mptr128",))
        case(c, f"is_ptr_ap_overbound 2 v{iset} ptr128", iset, "int", [PTR, U64],
             "__builtin_e2k_is_ptr_ap_overbound (a0, a1)", ("-mptr128",))
        case(c, f"is_ptrs_exact_eq v{iset} ptr128", iset, "int", [PTR, PTR],
             "__builtin_e2k_is_ptrs_exact_eq (a0, a1)", ("-mptr128",))
        for f in ("create_ap_subarray", "create_ap_subarray_color"):
            case(c, f"{f} v{iset} ptr128", iset, PTR, [PTR, U64, U64],
                 f"__builtin_e2k_{f} (a0, a1, a2)", ("-mptr128",))
        case(c, f"apincr_color v{iset} ptr128", iset, PTR, [PTR, U64],
             "__builtin_e2k_apincr_color (a0, a1)", ("-mptr128",))
        case(c, f"size_to_align v{iset} ptr128", iset, "unsigned", [U64],
             "__builtin_e2k_size_to_align (a0)", ("-mptr128",))
        case(c, f"get_ap_base v{iset} (64-bit mode)", iset, U64, [PTR],
             "__builtin_e2k_get_ap_base (a0)")


def header_cases(c):
    for h in range(4):
        case(c, f"e2k_prefetch hint {h}", 3, "void", [PTR],
             f"__builtin_e2k_prefetch (a0, {h});")
        for rw in (0, 1):
            case(c, f"gnu prefetch rw {rw} locality {h}", 3, "void", [PTR],
                 f"__builtin_prefetch (a0, {rw}, {h});")
    for iset in (3, 5):
        for v in ("", "_BE"):
            case(c, f"E2K_PREPARE_ALIGN{v} + E2K_ALIGN_DATA{v} v{iset}", iset,
                 U64, [U64, U64, U64],
                 f"({{ {U64} s, d; E2K_PREPARE_ALIGN{v} (a2, s); "
                 f"E2K_ALIGN_DATA{v} (a0, a1, d, s); d; }})")
        for v in ("_ARITH", "_ARITH_BE", "_ARITH32", "_INSF32"):
            case(c, f"E2K_PREPARE_ALIGN{v} + E2K_ALIGN_DATA{v} v{iset}", iset,
                 U64, [U64, U64, PTR],
                 f"({{ {U64} s, d; E2K_PREPARE_ALIGN{v} (a2, s); "
                 f"E2K_ALIGN_DATA{v} (a0, a1, d, s); d; }})")
    for v in ("", "_BE"):
        case(c, f"E2K_PREPARE_ALIGN128{v} + E2K_ALIGN_DATA128{v}", 5, V2,
             [V2, V2, U64],
             f"({{ v2di s, d; E2K_PREPARE_ALIGN128{v} (a2, s); "
             f"E2K_ALIGN_DATA128{v} (a0, a1, d, s); d; }})")
        case(c, f"E2K_PREPARE_ALIGN128{v}", 5, V2, [U64],
             f"({{ v2di s; E2K_PREPARE_ALIGN128{v} (a0, s); s; }})")
    for iset in (3, 5):
        for v in ("", "_BE", "_ARITH", "_ARITH_BE", "_ARITH32", "_INSF32"):
            t = U64 if v in ("", "_BE") else PTR
            case(c, f"E2K_PREPARE_ALIGN{v} v{iset}", iset, U64, [t],
                 f"({{ {U64} s; E2K_PREPARE_ALIGN{v} (a0, s); s; }})")
    for m in ("E2K_ALIGN_PTR", "E2K_ALIGN_PTR_BACK"):
        for al in (8, 16, 64):
            case(c, f"{m} {al}", 3, PTR, [PTR], f"{m} (a0, {al})")
    for m in ("E2K_BYTES_TO_ALIGN", "E2K_BYTES_FROM_ALIGN"):
        for al in (8, 16, 64):
            case(c, f"{m} {al}", 3, U64, [PTR], f"{m} (a0, {al})")
    for iset in (3, 5):
        for m in ("__CLZB", "__CTZB"):
            case(c, f"{m} v{iset}", iset, "int", [U64],
                 f"({{ int n; {m} (n, a0); n; }})")
    case(c, "__GETPFPFR", 3, "unsigned", [], "({ unsigned w; __GETPFPFR (w); w; })")
    case(c, "__SETPFPFR", 3, "void", ["unsigned"], "__SETPFPFR (a0);")
    case(c, "E2K_TRANSPOSE2_64F", 5, "void", [V2, V2, PTR],
         "E2K_TRANSPOSE2_64F (a0, a1, ((v2di *) a2)[0], ((v2di *) a2)[1]);")
    case(c, "E2K_TRANSPOSE4_32F", 5, "void", [V2, V2, V2, V2, PTR],
         "E2K_TRANSPOSE4_32F (a0, a1, a2, a3, ((v2di *) a4)[0], "
         "((v2di *) a4)[1], ((v2di *) a4)[2], ((v2di *) a4)[3]);")
    for m in ("E2K_CLZB", "E2K_CTZB"):
        case(c, f"{m} defined, expansion length", 3, "int", [],
             f"({{ static const char s[] = X_STR ({m}); (int) sizeof s - 1; }})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", type=Path, default=ROOT / "toolchains" / "builtins131")
    a = ap.parse_args()
    a.o.mkdir(parents=True, exist_ok=True)
    cases = []
    for f in (mas_cases, barrier_cases, cpu_cases, sysreg_cases,
              protected_cases, header_cases):
        f(cases)
    out = {}
    src, asm = a.o / "misc.c", a.o / "misc.s"
    for cs in cases:
        src.write_text(HEAD + "#define X_STR(x) X_STR2 (x)\n#define X_STR2(x) #x\n"
                       + cs["c"] + "\n")
        if asm.exists():
            asm.unlink()
        rc, err = gb.compile_(cs["iset"], src, asm) if not cs["opts"] else \
            compile_opts(cs["iset"], cs["opts"], src, asm)
        e = {"c": cs["c"], "target": " ".join(
            [gb.TARGET[cs["iset"]][0]] + gb.TARGET[cs["iset"]][1] + cs["opts"])}
        if rc != 0:
            e["error"] = gb.first_error(err)[:200]
        else:
            text = asm.read_text()
            e["asm"] = gb.trace(gb.bundles(text).get(0, []), cs["kinds"], cs["ret"],
                                channels=True)
            calls = sorted(set(re.findall(r"disp\s+%ctpr\d,\s*([\w.@]+)", text)))
            calls = [x for x in calls if x != "f0"]
            data = sorted(set(re.findall(r"\b(__\w+)\b", text)) - set(calls))
            if calls:
                e["calls"] = calls
            if data:
                e["symbols"] = data
            if "warning #223" in err:
                e["note"] = "lcc: declared implicitly (not a builtin)"
        out[cs["key"]] = e
    (ROOT / "spec/abi/lcc-builtins-misc.yaml").write_text(
        "# SPDX-License-Identifier: CC-BY-SA-4.0\n"
        f"# Generated by tools/abi/probes/gen_builtins_misc.py: what lcc\n"
        f"# {gb.VERSION} emits at -O2 for the builtins that are not one\n"
        "# operation and for <e2kintrin.h>'s helper macros. Each case: the C\n"
        "# function compiled (c), the lcc build and options (target), and\n"
        "# either lcc's error or the traced operations (asm, as in\n"
        "# lcc-builtins-check.yaml, with channel numbers), the functions it\n"
        "# calls (calls) and the other __ symbols it references (symbols).\n"
        + yaml.safe_dump({"cases": out}, sort_keys=False,
                         default_flow_style=None, width=100, allow_unicode=True))
    print(len(cases), "cases,", sum("error" in e for e in out.values()), "rejected")


def compile_opts(iset, opts, src, asm):
    target, topts = gb.TARGET[iset]
    import subprocess
    p = subprocess.run(gb.lcc_cmd(ROOT / "toolchains", target) + topts + opts +
                       ["-O2", "-S", "-fmax-errors=100000", str(src), "-o",
                        str(asm)], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


if __name__ == "__main__":
    main()
