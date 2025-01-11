#include <cgn>
#include "src_list.cgn.h"

// v26.1
// git("nasm.git", x) {
//     x.repo = "https://github.com/netwide-assembler/nasm.git";
//     x.commit_id = "cd37b81b320ead83ca5a6bbce5da0a6456663bc6"; //NASM 2.16.03
//     x.dest_dir = "repo";
// }

// chromium deps version
git("nasm.git", x) {
    x.repo = "https://chromium.googlesource.com/chromium/deps/nasm";
    x.commit_id = "767a169c8811b090df222a458b25dfa137fc637e";  // Mon Nov 18 13:57:11 2024
    x.dest_dir = "repo";
}

// nmake("nasm", x) {
//     x.makefile = "Mkfiles/msvc.mak";
//     x.cwd = "repo";
//     x.outputs = {"bin/nasm.exe"};
//     x.install_prefix_varname = "prefix";
//     x.install_target_name = "all";
// }

std::vector<std::string> add_prefix(const std::string &prefix, std::initializer_list<std::string> in) {
    std::vector<std::string> rv;
    for (auto it : in)
        rv.push_back(prefix + it);
    return rv;
}

cxx_executable("nasm", x) {
    x.include_dirs = add_prefix("repo/", {
                        ".",
                        "asm",
                        "disasm",
                        "include",
                        "output",
                        "x86"});
    x.defines = {"HAVE_CONFIG_H", "_CRT_NONSTDC_NO_WARNINGS"};
    x.srcs = add_prefix("repo/", nasmlib_sources) + add_prefix("repo/", nasm_sources);

    if (x.cfg["cxx_toolchain"] == "llvm") {
        x.cflags = {
        // # The inline functions in NASM's headers flag this.
        "-Wno-unused-function",

        // # NASM writes nasm_assert(!"some string literal").
        "-Wno-string-conversion",

        // # NASM sometimes redefines macros from its config.h.
        "-Wno-macro-redefined",

        // # NASM sometimes compares enums to unsigned integers.
        "-Wno-sign-compare",

        // # NASM sometimes return null from nonnull.
        "-Wno-nonnull",

        // # NASM sometimes uses uninitialized values.
        "-Wno-uninitialized",

        // # NASM sometimes set variables but doesn't use them.
        "-Wno-unused-but-set-variable",

        // # NASM undefines __STRICT_ANSI__
        "-Wno-builtin-macro-redefined",
      };
    }
    else if (x.cfg["cxx_toolchain"] == "msvc") {
        x.cflags = {
            // # NASM sometimes redefines macros from its config.h.
            "/wd4005",  //# macro redefinition

            // # NASM sometimes compares enums to unsigned integers.
            "/wd4018",  //# sign compare

            // # char VS const char mismatch.
            "/wd4028",  //# formal parameter 1 different from declaration.

            // # NASM comment: Uninitialized -> all zero by C spec
            // # Or sometimes one const struct is forward declared for no reason.
            "/wd4132",  //# const object should be initialized

            // # NASM uses "(-x) & 0xFF" pattern to negate byte.
            "/wd4146",  //# unary minus operator applied to unsigned type

            // asm\nasm.c(1018) : error C4703: potentially uninitialized local pointer variable 'param' used
            "/wd4703", 
        };
    }
}