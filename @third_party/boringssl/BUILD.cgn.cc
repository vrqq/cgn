// 
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

bool is_msan(cxx::CxxContext &x) {
    return (x.cfg["cxx_msan"] != "");
}
void apply_build_setting(cxx::CxxContext &x, bool for_test = false) {
    x.pub.include_dirs += src_prefix({"include"});
    x.include_dirs     += src_prefix({"include"});
    if (!for_test)
        x.defines += {"BORINGSSL_IMPLEMENTATION"};
    // x.defines += {"OPENSSL_SMALL"};
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
    if (is_msan(x))
        x.pub.defines += {"OPENSSL_NO_ASM"};

    if (x.cfg["os"] == "win") {
        x.defines += {"_HAS_EXCEPTIONS=0", "NOMINMAX", 
            "WIN32_LEAN_AND_MEAN", "_CRT_SECURE_NO_WARNINGS",
        };
    }
    
    if (x.cfg["os"] == "linux") {
        x.defines += {"_XOPEN_SOURCE=700"};
        if (x.cfg["cxx_toolchain"] == "gcc")
            x.cflags += {"-Wno-free-nonheap-object"};
        if (x.cfg["cxx_toolchain"] == "llvm")
            x.cflags += {"-Wctad-maybe-unsupported",
                         "-Wframe-larger-than=25344"};
        
    }
} //apply_build_setting()

// for windows using nasm (only use when is_msan()==false)
// for linux using gcc / clang instead
nasm_object("_crypto_asm_msvc", x) {
    x.srcs = src_prefix(bcm_sources_nasm + crypto_sources_nasm);
}

// -- The first form: libssl.a, libcrypto.a, (internal) libpki.a
cxx_static("ssl", x) {
    x.srcs = src_prefix(ssl_sources);
    apply_build_setting(x);
    x.add_dep(":crypto", cxx::inherit);
}

cxx_static("crypto", x) {
    x.srcs = src_prefix(bcm_sources + crypto_sources);
    apply_build_setting(x);
    if (is_msan(x) == false) {
        if (x.cfg["cxx_toolchain"] == "msvc")
            x.add_dep(":_crypto_asm_msvc", cxx::private_dep | cxx::pack_obj);
        else
            x.srcs += src_prefix(bcm_sources_asm + crypto_sources_asm);
    }
}

cxx_static("pki", x) {
    x.srcs = src_prefix(pki_sources);
    apply_build_setting(x);
    x.add_dep(":crypto", cxx::inherit);
}

cxx_executable("bssl", x) {
    x.srcs = src_prefix(bssl_sources);
    apply_build_setting(x);
    x.add_dep(":ssl", cxx::private_dep);
    x.add_dep(":crypto", cxx::private_dep);
    x.add_dep(":pki", cxx::private_dep);
}

// test suite
nasm_object("_test_support_msvc", x) {
    x.srcs = src_prefix(test_support_sources_nasm);
}
cxx_sources("_test_support", x) {
    x.srcs = src_prefix(test_support_sources);
    if (x.cfg["cxx_toolchain"] != "msvc")
        x.srcs += src_prefix(test_support_sources_asm);
    else
        x.add_dep(":_test_support_msvc", cxx::inherit);
    apply_build_setting(x);
    x.add_dep("@third_party//googletest", cxx::inherit);
}
cxx_executable("ssl_test", x) {
    x.srcs = src_prefix(ssl_test_sources);
    x.add_dep(":ssl", cxx::private_dep);
    x.add_dep(":crypto", cxx::private_dep);
    x.add_dep(":_test_support", cxx::private_dep);
}

cxx_executable("crypto_test", x) {
    x.srcs = src_prefix(crypto_test_sources);
    x.add_dep(":crypto", cxx::private_dep);
    x.add_dep(":_test_support", cxx::private_dep);
}

// lib collection
// copy("lib_collection", x) {
//     cgn::CGNTarget ssl    = api.analyse_target("@third_party//boringssl:ssl", x.cfg);
//     cgn::CGNTarget crypto = api.analyse_target("@third_party//boringssl:crypto", x.cfg);
//     cgn::CGNTarget pki    = api.analyse_target("@third_party//boringssl:pki", x.cfg);

//     x.target_results = {
//         x.copy_wrbase_to_output(ssl.outputs + crypto.outputs + pki.outputs, "")
//     };
// }
file_utility("devel", x) {
    FileUtility::DevelOpt collect_op;
    collect_op.allow_linknrun = true;
    x.collect_devel_on_build(":ssl", collect_op);
    x.collect_devel_on_build(":crypto", collect_op);
    x.collect_devel_on_build(":pki", collect_op);
}

// -- The second form: libboringssl.a
cxx_static("libboringssl", x)
{
    x.srcs = src_prefix(
        bcm_internal_headers + bcm_sources + crypto_internal_headers +
        crypto_sources + ssl_internal_headers + ssl_sources + pki_sources);
    apply_build_setting(x);
    
    if (is_msan(x) == false) {
        if (x.cfg["cxx_toolchain"] == "msvc")
            x.add_dep(":_crypto_asm", cxx::private_dep);
        else
            x.srcs += src_prefix(bcm_sources_asm + crypto_sources_asm);
    }
    if (x.cfg["os"] == "win")
        x.perferred_binary_name = "boringssl.lib";
    else
        x.perferred_binary_name = "libboringssl.a";
    
}