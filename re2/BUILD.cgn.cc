#include <cgn>

// 2024-07-02
git("re2.git", x) {
    x.repo = "https://github.com/google/re2.git";
    x.commit_id = "6dcd83d60f7944926bfd308cc13979fc53dd69ca";
}

static std::vector<std::string> re2_sources(const std::string &prefix) {
    std::vector<std::string> src = {
        "re2/bitmap256.cc",
        "re2/bitstate.cc",
        "re2/compile.cc",
        "re2/dfa.cc",
        "re2/filtered_re2.cc",
        "re2/mimics_pcre.cc",
        "re2/nfa.cc",
        "re2/onepass.cc",
        "re2/parse.cc",
        "re2/perl_groups.cc",
        "re2/prefilter.cc",
        "re2/prefilter_tree.cc",
        "re2/prog.cc",
        "re2/re2.cc",
        "re2/regexp.cc",
        "re2/set.cc",
        "re2/simplify.cc",
        "re2/tostring.cc",
        "re2/unicode_casefold.cc",
        "re2/unicode_groups.cc",
        "util/rune.cc",
        "util/strutil.cc"
    };
    for (auto &it : src)
        it = prefix + it;
    return src;
} //re2_sources()


cxx_static("re2", x) {
    x.include_dirs = x.pub.include_dirs = {"repo"};
    x.srcs = re2_sources("repo/");
    x.add_dep("@third_party//abseil-cpp", cxx::private_dep);

    if (x.cfg["optimization"] == "release")
        x.defines = {"NDEBUG"};
    if (x.cfg["cxx_toolchain"] == "msvc") {
        x.cflags = {
            "/wd4100", "/wd4201", "/wd4456", "/wd4457", "/wd4702", "/wd4815",
            "/utf-8"
        };
        x.defines = {
            "UNICODE", "_UNICODE", "STRICT", "NOMINMAX",
            "_CRT_SECURE_NO_WARNINGS", "_SCL_SECURE_NO_WARNINGS"
        };
    }
    else {
        x.cflags = {
            "-pthread", "-Wall", "-Wextra", "-Wno-unused-parameter", 
            "-Wno-missing-field-initializers"
        };
    }
}

cxx_executable("compile_test", x) {
    x.srcs = {"repo/re2/testing/" + x.name + ".cc"};
    x.add_dep(":re2", cxx::private_dep);
    x.add_dep("@third_party//googletest:gtest_main", cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp", cxx::private_dep);
}

bin_devel("devel", x) {
    x.include = {
        {"repo", {"re2/filtered_re2.h", "re2/re2.h", "re2/set.h", "re2/stringpiece.h"}}
    };
}