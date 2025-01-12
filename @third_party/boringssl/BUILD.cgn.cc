#include <cgn>
#include <@third_party/nasm/nasm.cgn.h>
#include "sources.cgn.h"

// BoringSSL Thu Jan 09 16:46:59 2025 HEAD of master version
git("boringssl.git", x) {
    x.repo = "https://boringssl.googlesource.com/boringssl";
    x.commit_id = "e4b6d4f754ba9ec2f1b40a4a091d3f6ad01ea084";
    x.dest_dir = "repo";
}

// DEPECRATED
// https://github.com/google/boringssl/blob/master/BUILDING.md
cmake("boringssl.cmake", x) {
    x.sources_dir = "repo";
    x.outputs = {"lib64/libboringssl.a"};

    // TODO: cfg["host_release"]
    cgn::CGNTarget nasm = x.add_dep_with_named_config("@third_party//nasm", "host_release");
    if (x.cfg["host_os"] == "win")
        x.vars["CMAKE_ASM_NASM_COMPILER"] = nasm.outputs[0];
}

std::vector<std::string> src_prefix(const std::vector<std::string> &in) {
    std::vector<std::string> rv;
    for (auto &it : in)
        rv.push_back("repo/" + it);
    return rv;
}

static std::vector<std::string> all_sources
    = src_prefix(bcm_internal_headers + bcm_sources + crypto_internal_headers +
                 crypto_sources + ssl_internal_headers + ssl_sources + pki_sources);


nasm_object("boringssl_asm", x) {
    x.srcs = src_prefix(bcm_sources_nasm + crypto_sources_nasm);
}

cxx_static("boringssl", x) {
    x.srcs = all_sources;
    x.pub.include_dirs = x.include_dirs = {"repo/include"};
    x.defines = {"BORINGSSL_IMPLEMENTATION", "OPENSSL_SMALL"};
    if (x.cfg["cxx_toolchain"] == "msvc") {
        x.cflags += {
            "/wd4100", 
            "/wd4127",
            "/wd4244",
            "/wd4267",
            "/wd4702",
            "/wd4706",
        };
    }

    if (x.cfg["os"] == "win") {
        x.defines += {"_HAS_EXCEPTIONS=0", "NOMINMAX", 
            "WIN32_LEAN_AND_MEAN", "_CRT_SECURE_NO_WARNINGS"};
    }
    
    if (x.cfg["os"] == "linux") {
        x.defines += {"_XOPEN_SOURCE=700"};
        if (x.cfg["cxx_toolchain"] == "gcc")
            x.cflags += {"-Wno-free-nonheap-object"};
        if (x.cfg["cxx_toolchain"] == "llvm")
            x.cflags += {"-Wctad-maybe-unsupported",
                         "-Wframe-larger-than=25344"};
    }
    x.add_dep(":boringssl_asm", cxx::private_dep);

}