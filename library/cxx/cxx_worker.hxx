#pragma once
#include <cassert>
#include <cstring>
#include "cxx.cgn.h"

namespace cxx {

//
// Step 1 test param
// ================

static CxxToolchainInfo step1_cmake_minumum(cgn::Configuration &cfg)
{
    CxxToolchainInfo rv;
    std::string prefix = cfg["cxx_prefix"];
    
    if (cfg["os"] == "linux" && cfg["cxx_toolchain"] == "gcc") {
        rv.exe_cc = rv.exe_asm = rv.exe_solink = rv.exe_xlink = prefix + "gcc";
        rv.exe_cxx = prefix + "g++";
        rv.exe_ar  = prefix + "ar";
        rv.exe_arg.ldflags = rv.so_arg.ldflags = {"-shared"};
        rv.c_arg.cflags = rv.cpp_arg.cflags = rv.asm_arg.cflags = {"-fPIC","-pthread"};
        rv.exe_arg.is_compiler_controlled_link = rv.so_arg.is_compiler_controlled_link = true;

        if (cfg["cxx_sysroot"] != "") {
            std::vector<std::string> common_values = {"--sysroot=" + (std::string)(cfg["cxx_sysroot"])};
            rv.c_arg.cflags    += common_values;
            rv.cpp_arg.cflags  += common_values;
            rv.exe_arg.ldflags += common_values;
            rv.so_arg.ldflags  += common_values;
        }
    }

    if (cfg["os"] == "linux" && cfg["cxx_toolchain"] == "llvm") {
        rv.exe_cc = rv.exe_asm = rv.exe_solink = rv.exe_xlink = prefix + "clang";
        rv.exe_cxx = prefix + "clang++";
        rv.exe_ar  = prefix + "ar";
        rv.exe_arg.is_compiler_controlled_link = rv.so_arg.is_compiler_controlled_link = true;
        rv.c_arg.cflags = rv.cpp_arg.cflags = rv.asm_arg.cflags = {"-fPIC", "-pthread"};
        rv.exe_arg.ldflags = rv.so_arg.ldflags = {"-shared"};

        std::vector<std::string> common_values;
        if (cfg["cxx_gcctoolchain"] != "")
            common_values += {"--gcc-toolchain=" + (std::string)cfg["cxx_gcctoolchain"]};
        if (cfg["cxx_sysroot"] != "")
            common_values += {"--sysroot=" + (std::string)(cfg["cxx_sysroot"])};
        rv.c_arg.cflags   += common_values;
        rv.cpp_arg.cflags += common_values;
        rv.exe_arg.ldflags += common_values;
        rv.so_arg.ldflags  += common_values;
    }
    
    if (cfg["os"] == "mac" && cfg["cxx_toolchain"] == "xcode") {
        rv.exe_cc = rv.exe_asm = rv.exe_solink = rv.exe_xlink = prefix + "clang";
        rv.exe_cxx = prefix + "clang++";
        rv.exe_ar  = prefix + "ar";
        rv.exe_arg.ldflags = rv.so_arg.ldflags = {"-shared"};
        rv.exe_arg.is_compiler_controlled_link = rv.so_arg.is_compiler_controlled_link = true;
    }
    
    if (cfg["cxx_toolchain"] == "msvc") {
        auto envdep = api.create_target("@cgn.d//library/cxx/vsenv_loader", cfg);
        assert(envdep.errmsg.empty());
        cfg.visit_keys(envdep.trimmed_cfg);
        rv.env_loader_script = envdep.outputs[0];
        rv.env_loader_script_anode = envdep.anode;
        rv.exe_cc = prefix + "cl.exe";
        rv.exe_cxx = prefix + "cl.exe";
        rv.exe_ar = prefix + "lib.exe";
        rv.exe_solink = rv.exe_xlink = prefix + "link.exe";
        rv.so_arg.ldflags = {"/DLL"};
        rv.exe_asm = prefix + (cfg["host_cpu"]=="x86"? "ml.exe":"ml64.exe");
        rv.exe_arg.is_compiler_controlled_link = rv.so_arg.is_compiler_controlled_link = false;
    }

    return rv;
}

static std::pair<CxxToolchainInfo, std::string> step1_win_msvc(cgn::Configuration &cfg)
{
    // win10==0x0A00; win7==0x0601;
    // win8.1/Server2012R2==0x0603;
    constexpr const char* DEFAULT_MINIMUM_WINVER = "0x0A00";

    std::string mimimum_winver = cfg["cxx_winapi_winver"];
    if (mimimum_winver.empty())
        mimimum_winver = DEFAULT_MINIMUM_WINVER;

    if (mimimum_winver.size() != 6)
        return {{}, "Invalid cfg['cxx_winapi_winver']"};
    CxxToolchainInfo interp;

    auto envdep = api.create_target("@cgn.d//library/cxx/vsenv_loader", cfg);
    assert(envdep.errmsg.empty());
    cfg.visit_keys(envdep.trimmed_cfg);
    interp.env_loader_script = envdep.outputs[0];
    interp.env_loader_script_anode = envdep.anode;

    bool target_x86 = (cfg["cpu"] == "x86");
    bool target_x64 = (cfg["cpu"] == "x86_64");

    std::string exe_prefix = cfg["cxx_prefix"];
    interp.exe_cc     = (exe_prefix + "cl.exe");
    interp.exe_cxx    = (exe_prefix + "cl.exe");
    interp.exe_asm    = (exe_prefix + (target_x64?"ml64.exe":"ml.exe"));
    interp.exe_ar     = (exe_prefix + "lib.exe");
    interp.exe_solink = (exe_prefix + "link.exe");
    interp.exe_xlink  = (exe_prefix + "link.exe");
    interp.so_arg.ldflags = {"/DLL"};
    interp.cpp_arg.cflags = {"/std:c++17"};
    interp.c_arg.cflags   = {"/std:c17"};
    interp.exe_arg.is_compiler_controlled_link = interp.so_arg.is_compiler_controlled_link = false;

    std::vector<std::string> common_defines = {
        //"UNICODE", "_UNICODE",   // default for NO unicode WidthType (encoding UTF-8 only)
        "_CONSOLE",                //"WIN32",
        "_CRT_SECURE_NO_WARNINGS", //for strcpy instead of strcpy_s
        "WINVER=" + std::string{mimimum_winver},        // win10==0x0A00; win7==0x0601;
        "_WIN32_WINNT=" + std::string{mimimum_winver},  // win8.1/Server2012R2==0x0603;
    };

    std::vector<std::string> common_cflags = {
        "/I.",

        // https://docs.microsoft.com/en-us/cpp/build/reference/permissive-standards-conformance?view=msvc-160
        "/permissive-", // Standards conformance
        // "/Yu\"pch.h\"", 
        "/GS",          // Checks buffer security.
        // "/Gm-",      // /Gm is deprecated
        "/Zc:wchar_t",  // Parse wchar_t as a built-in type according to the C++ standard.
        "/Zc:inline",   // remove unreferenced code and data
        "/fp:precise",  // floating point model
        "/errorReport:prompt",
        "/Zc:forScope", // Enforce Standard C++ for scoping rules (on by default).
        // "/WX-",      // /WX Treat Linker Warnings as Errors (default in vs project)
        "/Gd",          // x86 __cdecl calling convention
        "/sdl",   // Enables recommended Security Development Lifecycle (SDL) checks.
        "/utf-8", "/wd4828",   // illegal character in UTF-8
        // "/nologo",      // /nologo is in ninja file
        "/diagnostics:column",
        "/EHsc",       // Enables standard C++ stack unwinding
        "/FS",         // force synchoronous PDB write for parallel to serializes.
        // "/await",
    };

    std::vector<std::string> common_ldflags = {
        "/NXCOMPAT",    // Compatible with Data Execution Prevention
        "/DYNAMICBASE",
        
        "kernel32.lib", "user32.lib", "gdi32.lib", "winspool.lib", "comdlg32.lib", 
        "advapi32.lib", "shell32.lib", "ole32.lib", "oleaut32.lib", "uuid.lib", 
        "odbc32.lib", "odbccp32.lib", "legacy_stdio_definitions.lib"
    };
    
    //["cpu"]
    // https://stackoverflow.com/questions/13545010/amd64-not-defined-in-vs2010
    if (cfg["cpu"] == "x86") {
        common_defines += {"WIN32", "_X86_"};
        // The /LARGEADDRESSAWARE option tells the linker that the application 
        // can handle addresses larger than 2 gigabytes.
        common_ldflags += {"/SAFESEH", "/MACHINE:X86", "/LARGEADDRESSAWARE"};
        interp.ar_arg_arflags += {"/MACHINE:X86"};
    }
    if (cfg["cpu"] == "x86_64"){
        common_defines += {"_AMD64_"};  
        common_ldflags += {"/MACHINE:X64"};
        interp.ar_arg_arflags += {"/MACHINE:X64"};
    }

    //["msvc_runtime"]
    if (cfg["msvc_runtime"] == "MDd") {
        common_defines += {"_DEBUG"};
        common_cflags  += {"/MDd"};
        common_ldflags += {"msvcrtd.lib"};
    }
    if (cfg["msvc_runtime"] == "MD") {
        common_defines += {"NDEBUG"};
        common_cflags  += {"/MD"};
        common_ldflags += {"msvcrt.lib"};
    }
    if (cfg["msvc_runtime"] == "MTd") {
        common_defines += {"_DEBUG"};
        common_cflags  += {"/MTd"};
        common_ldflags += {"libcmtd.lib"};
    }
    if (cfg["msvc_runtime"] == "MT") {
        common_defines += {"NDEBUG"};
        common_cflags  += {"/MT"};
        common_ldflags += {"libcmt.lib"};
    }

    //["optimization"]
    if (cfg["optimization"] == "debug") {
        common_cflags += {
            "/JMC",   // Debug only user code with Just My Code
            // "/GR",    // Enable Run-Time Type Information (Debug only)
            // "/guard:cf",
            "/ZI",    // Includes debug information in a program database 
                      // compatible with Edit and Continue.
            "/Od",
            "/RTC1",  // Run-time error checks (stack frame && variable is 
                      // used without inited.)
            "/FC"     // Full Path of Source Code File in Diagnostics, and 
                      // also full path for the __FILE__ macro
        };
        common_ldflags += {
            "/DEBUG:FULL", 
            "/INCREMENTAL"
        };
    }
    if (cfg["optimization"] == "release") {
        common_cflags += {
            "/GL",  // Enables whole program optimization.
            "/Gy",  // Enables function-level linking.
            "/O2",
            "/Oi",  // generates intrinsic functions for appropriate function calls.
            "/Zi",  // The /Zi option produces a separate PDB file that contains 
                    // all the symbolic debugging information for use with the 
                    // debugger. The debugging information isn't included in the 
                    // object files or executable, which makes them much smaller.
            "/guard:cf"
        };
        common_ldflags += {
            "/OPT:ICF",
            "/DEBUG:FASTLINK",    // /Zi does imply /debug, the default is FASTLINK
            "/LTCG:incremental",  //TODO: using profile to guide optimization here (PGOptimize)
            "/RELEASE",           //sets the Checksum in the header of an .exe file.
        };
    }

    //["msvc_subsystem"]
    if (cfg["msvc_subsystem"] == "CONSOLE")
        common_ldflags += {"/SUBSYSTEM:CONSOLE"};
    if (cfg["msvc_subsystem"] == "WINDOW")
        common_ldflags += {"/SUBSYSTEM:WINDOW"};

    interp.c_arg.defines   += common_defines;
    interp.cpp_arg.defines += common_defines;

    interp.c_arg.cflags   += common_cflags;
    interp.cpp_arg.cflags += common_cflags;

    interp.so_arg.ldflags  += common_ldflags;
    interp.exe_arg.ldflags += common_ldflags;
    
    return {interp, ""};
} //step1_win_msvc()


static std::pair<CxxToolchainInfo, std::string> step1_linux_gcc(cgn::Configuration &cfg)
{
    CxxToolchainInfo interp;
    // For GCC toolchain, cross-compiler binaries are identified by filename.
    // like /toolchain_X/arm-none-linux-gnueabi-gcc, and the kernel path 
    // (--sysroot) usually hard-coding inside compiler.
    std::string prefix = cfg["cxx_prefix"];
    interp.exe_cc  = (prefix + "gcc");
    interp.exe_cxx = (prefix + "g++");
    interp.exe_asm = (prefix + "gcc");
    interp.exe_ar  = (prefix + "gcc-ar");
    interp.exe_solink = (prefix + "g++");
    interp.exe_xlink  = (prefix + "g++");
    interp.exe_arg.is_compiler_controlled_link = interp.so_arg.is_compiler_controlled_link = true;
    
    interp.c_arg.cflags   = {"-std=c17"};
    interp.cpp_arg.cflags = {"-std=c++17"};
    
    interp.exe_arg.ldflags = interp.so_arg.ldflags = {
        "-Wl,--warn-common", "-Wl,-z,origin", 
        "-Wl,--export-dynamic",  // force export from executable
        // "-Wl,--warn-section-align", 
        // "-Wl,-Bsymbolic", "-Wl,-Bsymbolic-functions",
    };

    interp.so_arg.ldflags += {"-shared"};

    std::vector<std::string> defines_1st;

    std::vector<std::string> cflags_1st = {
        "-I.",
        "-fno-common",
        "-fdiagnostics-color=always",
        "-fvisibility=hidden",
        "-Wl,--exclude-libs,ALL"
    };

    std::vector<std::string> ldflags_1st = {
        "-L."
    };

    //["os"]
    if (cfg["os"] == "linux") {
        cflags_1st  += {"-fPIC","-pthread"};
        ldflags_1st += {"-ldl", "-lrt", "-lpthread"};
    }

    //["optimization"]
    if (cfg["optimization"] == "debug") {
        defines_1st += {"_DEBUG"};
        cflags_1st += {
            "-Og", "-g", "-Wall", "-ggdb", "-O0",
            "-fno-eliminate-unused-debug-symbols", 
            "-fno-eliminate-unused-debug-types"};
        interp.cpp_arg.cflags += {"-ftemplate-backtrace-limit=0"};
    }
    if (cfg["optimization"] == "release")
        cflags_1st += {"-O2", "-flto", "-fwhole-program"};
    
    //["cxx_sysroot"]
    if (cfg["cxx_sysroot"] != ""){
        cflags_1st  += {"--sysroot=" + (std::string)cfg["cxx_sysroot"]};
        ldflags_1st += {"--sysroot=" + (std::string)cfg["cxx_sysroot"]};
    }

    //["cxx_asan/..."]
    std::string sanitizer;
    auto append_sanitizer = [&](bool cond, const std::string &ss) {
        if(cond) sanitizer += (sanitizer.size()?",":"") + ss;
    };
    append_sanitizer((cfg["cxx_asan"] != ""), "address");
    append_sanitizer((cfg["cxx_ubsan"] != ""), "undefined");
    append_sanitizer((cfg["cxx_tsan"] != ""), "thread");
    append_sanitizer((cfg["cxx_msan"] != ""), "memory");
    append_sanitizer((cfg["cxx_lsan"] != ""), "leak");
    if (sanitizer.size()) {
        cflags_1st  += {"-fsanitize=" + sanitizer};
        ldflags_1st += {"-fsanitize=" + sanitizer};
    }

    interp.c_arg.defines   += defines_1st;
    interp.cpp_arg.defines += defines_1st;
    interp.asm_arg.defines += defines_1st;

    interp.c_arg.cflags   += cflags_1st;
    interp.cpp_arg.cflags += cflags_1st;
    interp.asm_arg.cflags += cflags_1st;

    interp.so_arg.ldflags  += ldflags_1st;
    interp.exe_arg.ldflags += ldflags_1st;

    return {interp, {}};
} //step1_linux_gcc()


static std::pair<CxxToolchainInfo, std::string> step1_linuxllvm_and_xcode(cgn::Configuration &cfg)
{
    CxxToolchainInfo interp;

    // For LLVM toolchain, set target os/cpu and sysroot for cross-compilation.
    std::string prefix = cfg["cxx_prefix"];
    interp.exe_cc  = (prefix + "clang");
    interp.exe_cxx = (prefix + "clang++");
    interp.exe_asm = (prefix + "clang");
    interp.exe_ar     = (prefix + "ar");
    interp.exe_solink = (prefix + "clang++");
    interp.exe_xlink  = (prefix + "clang++");
    interp.exe_arg.is_compiler_controlled_link = interp.so_arg.is_compiler_controlled_link = true;

    if (cfg["os"] == "linux") {
        interp.exe_ar     = (prefix + "llvm-ar");
        interp.exe_solink = (prefix + "clang++");
        interp.exe_xlink  = (prefix + "clang++");
        interp.exe_arg.ldflags += {"-fuse-ld=lld"};
        interp.so_arg.ldflags  += {"-fuse-ld=lld", "-shared"};
    }

    interp.c_arg.cflags   = {"-std=c17"};
    interp.cpp_arg.cflags = {"-std=c++17"};

    std::vector<std::string> cflags_1st = {
        "-fvisibility=hidden",
        "-fno-common",
        "-fcolor-diagnostics", "-Wreturn-type", 
        "-I.", "-fPIC", "-pthread"};
    
    // '-Wl,$(ldflags_lnk[])' for clang++ linker driver
    // std::vector<std::string> ldflags_lnk;
    
    std::vector<std::string> ldflags_1st = {
        "-L.",
        "-lpthread"
    };

    if (cfg["os"] == "linux") {
        if (cfg["cxx_asan"] == "")
            ldflags_1st += {"-Wl,--warn-common"};
        ldflags_1st += {"-Wl,--warn-backrefs", "-lrt"};
    }
    if (cfg["os"] == "mac") //for macos : using warn-commons instead of warn-common
        ldflags_1st += {"-fprofile-instr-generate", "-Wl,-warn_commons"};

    std::vector<std::string> defines_1st = {"_GNU_SOURCE"};
    
    //["optimization"]
    if (cfg["optimization"] == "debug") {
        defines_1st += {"_DEBUG"};
        cflags_1st += {
            "-g", "-Wall", "-Wextra", "-Wno-unused-parameter",
            "-fno-omit-frame-pointer", "-fno-optimize-sibling-calls",
            "-ftemplate-backtrace-limit=0", "-fno-limit-debug-info",
            "-fstandalone-debug",  "-glldb", //"-march=native",
            // "-fdebug-macro", // this would trigger clang-cc1 bug to crash
            "-fcoverage-mapping", "-fprofile-instr-generate", "-ftime-trace"
            // "-flto=thin"
        };
    }
    if (cfg["optimization"] == "release") {
        cflags_1st += {"-O3", "-flto"};
        ldflags_1st += {
            "-flto", 
        };
        ldflags_1st += {
            "-Wl,--exclude-libs=ALL", 
            "-Wl,--discard-all",
            // "-Wl,--thinlto-jobs=0", 
            // "-Wl,--thinlto-cache-dir=./thinlto_cache", 
            // "-Wl,--thinlto-cache-policy,cache_size_bytes=1g",
            "-Wl,--warn-unresolved-symbols"
        };
    }

    //["llvm_stl"]
    if (cfg["llvm_stl"] == "libc++")
        cflags_1st += {"-stdlib=libc++"};

    //["cxx_sysroot"]
    if (cfg["cxx_sysroot"] != ""){
        cflags_1st  += {"--sysroot=" + (std::string)(cfg["cxx_sysroot"])};
        ldflags_1st += {"--sysroot=" + (std::string)(cfg["cxx_sysroot"])};
    }

    //["cxx_gcctoolchain"]
    if (cfg["cxx_gcctoolchain"] != "") {
        cflags_1st  += {"--gcc-toolchain=" + (std::string)cfg["cxx_gcctoolchain"]};
        ldflags_1st += {"--gcc-toolchain=" + (std::string)cfg["cxx_gcctoolchain"]};
    }

    //llvm cross compile argument
    if (cfg["os"] != cfg["host_os"] || cfg["cpu"] != cfg["host_cpu"]) {
        std::string cpu = cfg["cpu"];
        if (cpu == "x86_64")
            cpu = "amd64";
        if (cpu == "arm64")
            cpu = "aarch64";
        cflags_1st += {"--target=" + cpu + "-pc-" + (std::string)cfg["os"]};
    }
    
    //["cxx_asan/..."]
    std::string sanitizer;
    auto append_sanitizer = [&](bool cond, const std::string &ss) {
        if(cond) sanitizer += (sanitizer.size()?",":"") + ss;
    };
    append_sanitizer((cfg["cxx_asan"] != ""), "address");
    append_sanitizer((cfg["cxx_ubsan"] != ""), "undefined");
    append_sanitizer((cfg["cxx_tsan"] != ""), "thread");
    append_sanitizer((cfg["cxx_msan"] != ""), "memory");
    append_sanitizer((cfg["cxx_lsan"] != ""), "leak");
    if (sanitizer.size()) {
        cflags_1st  += {"-fsanitize=" + sanitizer};
        if (cfg["optimization"] != "debug")
            cflags_1st += {"-fno-omit-frame-pointer"};
        ldflags_1st += {"-fsanitize=" + sanitizer};
    }


    interp.c_arg.defines   += defines_1st;
    interp.cpp_arg.defines += defines_1st;
    interp.asm_arg.defines += defines_1st;

    interp.c_arg.cflags   += cflags_1st;
    interp.cpp_arg.cflags += cflags_1st;
    interp.asm_arg.cflags += cflags_1st;

    interp.exe_arg.ldflags += ldflags_1st;
    interp.so_arg.ldflags  += ldflags_1st;
    return {interp, ""};
} //step1_linuxllvm_and_xcode()

std::pair<CxxToolchainInfo, std::string> 
CxxWorker::step1_test_param(cgn::Configuration &cfg, const std::string &via) const
{
    if (via == "cmake" || via == "minimum")
        return {step1_cmake_minumum(cfg), ""};
    if (cfg["cxx_toolchain"] == "msvc" && cfg["os"] == "win")
        return step1_win_msvc(cfg);
    else if (cfg["cxx_toolchain"] == "gcc" && cfg["os"] == "linux")
        return step1_linux_gcc(cfg);
    else if (
        (cfg["cxx_toolchain"] == "xcode" && cfg["os"] == "mac") ||
        (cfg["cxx_toolchain"] == "llvm"  && cfg["os"] == "linux")
    )
        return step1_linuxllvm_and_xcode(cfg);
    return {{}, "Unsupported toolchain"};
} // CxxWorker::step1_test_param()

// ****************
//
// Step 2 config confirm
// ================

CxxWorker::Stage3In CxxWorker::step2_confirm(CxxToolchainInfo &s1out, CxxContext &x) const
{
    CxxWorker::Stage3In s2out;
    // preprocess "x.srcs = file_glob(*)"
    // if the source file is not at the same/sub folder of BUILD.cgn.cc
    // using absolutely path to locate.
    // BUG HERE: file_glob(*) cannot found file newly added (in ninja cache)
    //     TODO: target with file_glob() would re-analyse each time.
    //           so we should use external executable to generate ninja dyndep.
    //           @cgn.d//library/advtools
    for (auto &p : x.srcs) {
        if (p.type == p.BASE_ON_OUTPUT)
            return x.opt->set_fail("Unsupported src " + p.to_string()), s2out;

        auto check_and_add = [&](const std::string &p2) {
            if (api.lowercase_extension_of_path(p2) == ".def")
                s2out.def_file = p2;
            else
                s2out.src_files += {p2};
        };
        if (p.rpath.find('*') == p.rpath.npos) //if not file_glob
            check_and_add(api.rebase_path(p, ".", x.opt));
        else {
            std::string path2 = api.rebase_path(p, ".", x.opt);
            for (const auto &it : api.file_glob(path2))
                check_and_add(it);
        }
    }

    // confirm configuration
    // 'host_shell' for function two_escape()
    x.opt->cfg.visit_keys({"host_shell", "os", "cpu", "cxx_toolchain"});
    s2out.mk = x.opt->confirm();
    if (!s2out.mk) //return if cache found
        return s2out;

    s2out.src_extra = x._lnr_to_self;
    cgn::Tools::remove_duplicate_inplace(s2out.src_extra.object_files);
    cgn::Tools::remove_duplicate_inplace(s2out.src_extra.static_files);
    cgn::Tools::remove_duplicate_inplace(s2out.src_extra.shared_files);
    s2out.src_extra_no_whole = &x._self_no_whole_archive;

    s2out.target_role = x.role;
    
    PackOut &pack_out = s2out.pack_out_suggestion;
    if (x.perferred_binary_name.size()) {
        pack_out.packout_file = s2out.mk->out_prefix + x.perferred_binary_name;
        if (x.role == 's' || x.role == 'x') {
            if (x.cfg["os"] == "win") {
                pack_out.packout_rt_filepath = pack_out.packout_file + ".lib";
                pack_out.packout_rt_filename = x.perferred_binary_name + ".lib";
            }else {
                pack_out.packout_rt_filepath = pack_out.packout_file;
                pack_out.packout_rt_filename = x.perferred_binary_name;
            }
        }
    }
    else {
        std::string ext1, ext2;
        if (x.role == 'a')
            ext1 = (x.cfg["os"]=="win"? ".lib": ".a");
        else
            ext1 = (x.cfg["os"]=="win"? (x.role=='s'?".dll":".exe"): (x.role=='s'?".so":""));
        std::string prefix = (x.role != 'x' && x.cfg["os"]!="win")? "lib":"";
        pack_out.packout_file = s2out.mk->out_prefix + prefix + x.name + ext1;

        if (x.role == 's' || x.role == 'x') {
            if (x.cfg["os"] == "win") {
                pack_out.packout_rt_filepath = s2out.mk->out_prefix + x.name + ".lib";
                pack_out.packout_rt_filename = x.name + ".lib";
            }else {
                pack_out.packout_rt_filepath = pack_out.packout_file;
                pack_out.packout_rt_filename = prefix + x.name + ext1;
            }
        }
    }

    // x.include_dirs CGNPath[] rebase
    for (auto &p : x.include_dirs)
        if (p.type != p.BASE_ON_WORKINGROOT)
            p = cgn::make_path_base_working(api.rebase_path(p, ".", s2out.mk));

    // x.pub.include_dirs CGNPath rebase
    for (auto &p : x.pub.include_dirs)
        if (p.type != p.BASE_ON_WORKINGROOT)
            p = cgn::make_path_base_working(api.rebase_path(p, ".", s2out.mk));
    
    // merge to $s2out
    s2out.toolchain = &s1out;
    for (auto xsrc : {&s1out.c_arg, &s1out.cpp_arg, &s1out.asm_arg}){
        xsrc->cflags += x._cxx_to_self.cflags + x.cflags;
        xsrc->defines += x._cxx_to_self.defines + x.defines;

        std::vector<std::string> final_inc;
        for (auto &p : x.include_dirs + x._cxx_to_self.include_dirs)
            final_inc += {api.rebase_path(p, ".", s2out.mk)};
        final_inc += xsrc->include_dirs;
        cgn::Tools::remove_duplicate_inplace(final_inc);
        std::swap(xsrc->include_dirs, final_inc);
    }
    for (auto xout : {&s1out.exe_arg, &s1out.so_arg})
        xout->ldflags += x._cxx_to_self.ldflags + x.ldflags;

    // generate $ninja_order_only_dep
    // TODO: cannot use variable in ninja order_dep region.
    if (true || x.quickdep_ninja_target.size() <= 1)
        s2out.ninja_order_only_dep = x.quickdep_ninja_target;
    else {
        auto *field = s2out.mk->ninja->append_build();
        s2out.mk->ninja->append_variable("target_dep", 
            api.convert_list_to_string(cgn::NinjaFile::escape_path(x.quickdep_ninja_target)));
        s2out.ninja_order_only_dep = {"$target_dep"};
    }

    // generate result InfoTable
    if (!s2out.mk->merge_from(x._pub_infos) || !s2out.mk->merge_entry(&x.pub))
        s2out.mk->errmsg = "CxxInterpreter: internal error on generate InfoTable";
    
    // remove duplicate in current.pub[CxxInfo and LinkAndRunInfo]
    cxx::CxxInfo *pubcxxinfo = s2out.mk->get<cxx::CxxInfo>(false);
    if (pubcxxinfo) {
        cgn::Tools::remove_duplicate_inplace(pubcxxinfo->include_dirs);
        cgn::Tools::remove_duplicate_inplace(pubcxxinfo->defines, false);
    }
    cgn::LinkAndRunInfo *publnr = s2out.mk->get<cgn::LinkAndRunInfo>(false);
    if (publnr) {
        cgn::Tools::remove_duplicate_inplace(publnr->object_files);
        cgn::Tools::remove_duplicate_inplace(publnr->static_files);
        cgn::Tools::remove_duplicate_inplace(publnr->shared_files);
    }

    return s2out;
} //CxxWorker::step2_confirm()

// ****************
//
// Step 3 ninja file generation
// ================

// case for path_out: xxx.so / .lib may have same name with folder-name 
//                    in src folder, so we have to add '_' before path_out.
// case for path_out: cgn-out/.../ in same folder of current interpreter
//                    add '__' (two underline) before path_out.
// @return pair<path_out, type_of_src> :
//         - path_out : "output file relpath"
//         - type of src : A/+/C/0 (asm, c++, c, 0:igonre)
// TODO : in windows, ninja have bug that cannot mkdir end with '..'
std::pair<std::string, char> CxxWorker::Stage3In::suggest_objout(std::string &file_in)
{
    std::string ext = api.lowercase_extension_of_path(file_in);
    
    char rv_type = 0;
    if (ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".c++")
        rv_type = '+';
    if (ext == ".c")
        rv_type = 'C';
    if (ext == ".s" || ext == ".asm")
        rv_type = 'A';

    std::string obj_file;
    std::string src_dir = api.locale_path(mk->src_prefix);
    bool start_with_outprefix = (file_in.size() > mk->out_prefix.size()
        && memcmp(file_in.c_str(), mk->out_prefix.c_str(), mk->out_prefix.size())==0);
    bool start_with_srcprefix = (file_in.size() > src_dir.size()
        && memcmp(file_in.c_str(), src_dir.c_str(), src_dir.size())==0);

    // file_in is inside src_prefix
    if (start_with_srcprefix) 
        obj_file = cgn::Tools::locale_path(mk->out_prefix + "_" 
                + file_in.substr(mk->src_prefix.size()) 
                + (mk->trimmed_cfg["os"]=="win"? ".obj":".o"));

    // file_in is inside current out_prefix
    if (start_with_outprefix) {
        std::string probe1 = file_in.substr(mk->out_prefix.size());
        obj_file = cgn::Tools::locale_path(
                mk->out_prefix + "__" + probe1 + (mk->trimmed_cfg["os"]=="win"? ".obj":".o"));
    }

    // otherwise : abspath or other dirs
    obj_file = mk->out_prefix + api.mangle_path_to_relative(file_in) + (mk->trimmed_cfg["os"]=="win"? ".obj":".o");

    // NinjaBuild bug DirtyPatch:
    // add ./ prefix of src filepath to avoid string starting with '@'
    // if we add "./" in ninja input file, ninja.exe would auto remove it 
    // when writing down to .rspfile, then it cause cl.exe parse it as
    // another rspfile.
    if (mk->trimmed_cfg["os"] == "win" && file_in[0] == '@')
        file_in = "." + mk->PATH_SEPARATOR + file_in;
    
    return {obj_file, rv_type};
} //suggest_objout()

std::vector<std::string> CxxWorker::Stage3In::gen_cflags(
    const std::string &include_prefix,
    const std::string &define_prefix,
    const CxxToolchainInfo::CompilingOption &info
) const {
    std::vector<std::string> cflags = info.cflags;
    for (const std::string &inc : info.include_dirs)
        cflags += {include_prefix + api.shell_escape(inc, mk->trimmed_cfg["host_shell"])};
    for (const std::string &def : info.defines)
        cflags += {define_prefix + api.shell_escape(def, mk->trimmed_cfg["host_shell"])};
    return cflags;
} //gen_cflags()

std::string CxxWorker::Stage3In::two_escape(const std::string &in) const {
    return cgn::NinjaFile::escape_path(
        cgn::CGN::shell_escape(in, mk->trimmed_cfg["host_shell"])
    );
}

std::vector<std::string> CxxWorker::Stage3In::two_escape(std::vector<std::string> in) const {
    for (auto &it : in)
        it = two_escape(it);
    return in;
}

std::string CxxWorker::Stage3In::two_escape_to_string(std::vector<std::string> in) const {
    return api.convert_list_to_string(two_escape(in));
}

cgn::LinkAndRunInfo CxxWorker::Stage3In::done_with_entry(const std::vector<std::string> &out)
{
    assert(target_role == 'o');
    cgn::LinkAndRunInfo s3out;
    s3out.object_files = out;
    mk->outputs = out;
    mk->merge_entry(&s3out);

    if (mk->ninja == nullptr)
        return s3out;
    auto *entry = mk->ninja->append_build();
    entry->rule = "phony";
    entry->inputs = cgn::NinjaFile::escape_path(out);
    entry->outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry)};
    return s3out;
} //done_with_entry()

cgn::LinkAndRunInfo CxxWorker::Stage3In::done_with_entry(const PackOut &out)
{
    assert(target_role != 'o');
    cgn::LinkAndRunInfo s3out;
    if (target_role == 'a')
        s3out.static_files += {out.packout_file};
    else if (target_role == 's'){
        s3out.shared_files += {out.packout_file};
        if (out.packout_rt_filepath.size()) //implib for dll in windows
            s3out.runtime_files[cgn::make_path_base_out(out.packout_rt_filename)] = out.packout_rt_filepath;
    }
    mk->outputs = {out.packout_file};

    if (mk->ninja == nullptr)
        return s3out;
    auto *entry = mk->ninja->append_build();
    entry->rule = "phony";
    entry->inputs = {cgn::NinjaFile::escape_path(out.packout_file)};
    entry->outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry)};
    return s3out;
}

constexpr const char *rule_ninja = "@cgn.d//library/cxx/cxx_rule.ninja";

static cgn::LinkAndRunInfo default_step3_win(CxxWorker::Stage3In *s3)
{
    // add setenv batch dependency before cc.exe run
    std::string ccenv;
    std::vector<std::string> njdep_env;
    if (s3->toolchain->env_loader_script.size()) {
        ccenv = "cmd.exe /c " + s3->two_escape(s3->toolchain->env_loader_script) + " && ";
        njdep_env = {cgn::NinjaFile::escape_path(s3->toolchain->env_loader_script)};
    }
    
    // predefine ninja variable: cflags_c, cflags_cc, cflags_asm
    const std::string njenv_cflags_c = "cflags_c", 
                      njenv_cflags_cpp = "cflags_cpp", 
                      njenv_cflags_asm = "cflags_asm";
    if (s3->mk->ninja) {
        s3->mk->ninja->append_variable(njenv_cflags_c, s3->gen_cflags("/I", "/D", s3->toolchain->c_arg));
        s3->mk->ninja->append_variable(njenv_cflags_cpp, s3->gen_cflags("/I", "/D", s3->toolchain->cpp_arg));
        s3->mk->ninja->append_variable(njenv_cflags_asm, s3->gen_cflags("/I", "/D", s3->toolchain->asm_arg));

        static std::string rule_path = api.get_filepath(rule_ninja);
        s3->mk->ninja->append_include(rule_path);
    }

    // patch for .lib in windows : 
    //   if field->inputs empty, lib.exe would not generate any files,
    //   so here we feed a empty source file here.
    if (s3->target_role == 'a' && s3->src_files.empty() && s3->src_extra.object_files.empty())
        s3->src_files = {api.get_filepath("@cgn.d//library/cxx/vsenv_loader/empty_file.c")};

    // build.ninja : source file => .o
    std::string pdbfile = s3->mk->out_prefix + "__vc.pdb";
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;
    for (auto &src_in : s3->src_files) {
        auto src_out = s3->suggest_objout(src_in);
        if (src_out.second == 0)
            continue;

        //field->input has been moved into cflags
        cgn::NinjaFile::BuildSection field;
        field.outputs = {cgn::NinjaFile::escape_path(src_out.first)};
        field.implicit_inputs = njdep_env + cgn::SList{cgn::NinjaFile::escape_path(src_in)};
        field.order_only = s3->ninja_order_only_dep;
        if (src_out.second == 'A') {
            field.rule = "msvc_ml";
            field.variables["cc"] = ccenv + s3->two_escape(s3->toolchain->exe_asm);
            field.variables["cflags"] = "$" + njenv_cflags_asm;
        }
        if (src_out.second == '+') {
            field.rule = "msvc_cl";
            field.variables["cc"] = ccenv + s3->two_escape(s3->toolchain->exe_cxx);
            field.variables["cflags"] = "$" + njenv_cflags_cpp;
            field.variables["pdb"] = cgn::NinjaFile::escape_path(pdbfile);
        }
        else {
            field.rule = "msvc_cl";
            field.variables["cc"] = ccenv + s3->two_escape(s3->toolchain->exe_cc);
            field.variables["cflags"] = "$" + njenv_cflags_c;
            field.variables["pdb"] = cgn::NinjaFile::escape_path(pdbfile);
        }

        field.variables["cflags"] += "/c " + s3->two_escape(src_in);

        if (s3->mk->ninja)
            s3->mk->ninja->append_build(field);
        
        obj_out.push_back(src_out.first);
        obj_out_ninja_esc.push_back(field.outputs[0]);
    } //endfor (auto &path_in : self_src)

    // build.ninja : cxx_sources()
    // cxx_sources() cannot process any field of LinkAndRunInfo
    // so add the .obj file generated by itself then return
    if (s3->target_role == 'o')
        return s3->done_with_entry(obj_out);

    const CxxWorker::PackOut &pack_out = s3->suggest_packout();

    // build.ninja : cxx_static()
    //  deps.obj + self.srcs.o => rv[LRinfo].a
    //  deps.rt / deps.so / deps.a => rv[LRinfo]
    if (s3->mk->ninja && s3->target_role == 'a') {
        cgn::NinjaFile::BuildSection field;
        field.rule = "msvc_lib";
        field.inputs = obj_out_ninja_esc
                     + cgn::NinjaFile::escape_path(s3->src_extra.object_files);
        field.outputs = {cgn::NinjaFile::escape_path(pack_out.packout_file)};
        field.implicit_inputs = njdep_env;
        field.variables["restat"] = "1";
        field.variables["libexe"] = ccenv + s3->toolchain->exe_ar;
        field.variables["arflags"] = api.convert_list_to_string(s3->toolchain->ar_arg_arflags);
        if (s3->def_file.size()) {
            field.variables["arflags"] += "/DEF:" + s3->def_file + " ";
            field.implicit_inputs += {cgn::NinjaFile::escape_path(s3->def_file)};
        }
        s3->mk->ninja->append_build(field);
    } //endif (target_role == 'a')

    // build.ninja : cxx_shared() / cxx_executable()
    //   deps.object + deps.static + self.srcs.o => self.so / self.exe
    //   with carg.ldflags and -wholearchive:src_extra.static_files
    //   self.so + {so from deps} => rv[LRinfo].so
    if (s3->mk->ninja && (s3->target_role == 's' || s3->target_role == 'x')) {
        //prepare link.exe or cl.exe /link
        auto &ldflag_list = (s3->target_role=='s'?s3->toolchain->so_arg:s3->toolchain->exe_arg).ldflags;
        for (auto elib : s3->src_extra.static_files)
            ldflag_list += {"/WHOLEARCHIVE:" + elib};
        
        //prepare rpath argument
        //  this is seen as target ldflags, so put on the tail of cargs.ldflags
        //TODO: manifest and .runtime

        cgn::NinjaFile::BuildSection field;
        field.rule = "msvc_link";
        field.inputs = obj_out_ninja_esc
                     + cgn::NinjaFile::escape_path(s3->src_extra.object_files)
                     + cgn::NinjaFile::escape_path(s3->src_extra.static_files) 
                     + cgn::NinjaFile::escape_path(*s3->src_extra_no_whole) 
                     + cgn::NinjaFile::escape_path(s3->src_extra.shared_files);
        field.outputs = {cgn::NinjaFile::escape_path(pack_out.packout_file)};
        field.implicit_inputs = njdep_env;
        field.variables["restat"] = "1";
        if (s3->target_role == 's') //only add .lib for .dll
            field.implicit_outputs = {cgn::NinjaFile::escape_path(pack_out.packout_rt_filepath)};
        field.variables["link"] = ccenv + s3->two_escape(s3->target_role=='s'? s3->toolchain->exe_solink:s3->toolchain->exe_xlink);
        field.variables["ldflags"] = api.convert_list_to_string(s3->two_escape(ldflag_list));
        if (s3->def_file.size()) {
            field.variables["ldflags"] += " /DEF:" + s3->def_file + " ";
            field.implicit_inputs += {cgn::NinjaFile::escape_path(s3->def_file)};
        }

        // copy runtime when cxx_executable() and not pkg_mode
        if (s3->target_role == 'x')
            for (auto &one_entry : s3->src_extra.runtime_files) {
                const cgn::CGNPath &dst1 = one_entry.first;
                if (dst1.type != dst1.BASE_ON_OUTPUT)
                    continue;
                std::string dst = api.rebase_path(dst1, ".", s3->mk);
                const std::string &src = one_entry.second;
                auto *cpfield = s3->mk->ninja->append_build();
                //TODO: copy runtime by custom command (like symbolic-link)
                cpfield->rule    = "win_file_copy_cppdeprule";
                cpfield->inputs  = {cgn::NinjaFile::escape_path(src)};
                cpfield->outputs = {cgn::NinjaFile::escape_path(dst)};
                field.order_only += cpfield->outputs;
                // entry->order_only += cpfield->outputs;
            }

        s3->mk->ninja->append_build(field);
    } // if (role=='s' or 'x')

    return s3->done_with_entry(pack_out);
} //default_step3_win()


static cgn::LinkAndRunInfo default_step3_xnix(CxxWorker::Stage3In *s3)
{
    std::string ccenv;
    std::vector<std::string> njdep_setenv;
    if (s3->toolchain->env_loader_script.size()){
        ccenv += s3->two_escape(s3->toolchain->env_loader_script) + " && ";
        njdep_setenv = {cgn::NinjaFile::escape_path(s3->toolchain->env_loader_script)};
    }

    if (s3->toolchain->exe_arg.is_compiler_controlled_link == false
     || s3->toolchain->so_arg.is_compiler_controlled_link == false) {
        s3->mk->errmsg = "Only is_compiler_controlled_link==true supported.";
        return {};
    }

    if (s3->mk->ninja) {
        static std::string rule_path = api.get_filepath(rule_ninja);
        s3->mk->ninja->append_include(rule_path);

        s3->mk->ninja->append_variable("cflags_c",   s3->gen_cflags("-I", "-D", s3->toolchain->c_arg));
        s3->mk->ninja->append_variable("cflags_cpp", s3->gen_cflags("-I", "-D", s3->toolchain->cpp_arg));
        s3->mk->ninja->append_variable("cflags_asm", s3->gen_cflags("-I", "-D", s3->toolchain->asm_arg));
    }

    //=== Section 3: make ninja file ===
    // common for both GCC and LLVM
    // build.ninja : source file => .o
    // field.input and field.output : need ninja escape, instead of shell esacpe
    // carg.cflags and carg.ldflags : already two escaped
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;

    for (auto &src_in : s3->src_files) {
        auto src_out = s3->suggest_objout(src_in);

        cgn::NinjaFile::BuildSection field;
        field.rule = "gcc";
        field.inputs  = {cgn::NinjaFile::escape_path(src_in)};
        field.outputs = {cgn::NinjaFile::escape_path(src_out.first)};
        field.order_only = s3->ninja_order_only_dep;
        if (src_out.second == '+') {
            field.variables["cc"] = ccenv + s3->two_escape(s3->toolchain->exe_cxx);
            field.variables["cflags"] = "$cflags_cpp";
        }else if (src_out.second == 'A') {
            field.variables["cc"] = ccenv + s3->two_escape(s3->toolchain->exe_asm);
            field.variables["cflags"] = "$cflags_asm";
        }else {
            field.variables["cc"] = ccenv + s3->two_escape(s3->toolchain->exe_cc);
            field.variables["cflags"] = "$cflags_c";
        }

        if (s3->mk->ninja)
            s3->mk->ninja->append_build(field);
        obj_out.push_back(src_out.first);
        obj_out_ninja_esc.push_back(field.outputs[0]);
    } //endfor (auto &path_in : self_src)

    // build.ninja : cxx_sources()
    // cxx_sources() cannot process any field of LinkAndRunInfo
    // so add the .obj file generated by itself then return
    if (s3->target_role == 'o')
        return s3->done_with_entry(obj_out);

    CxxWorker::PackOut pack_out = s3->suggest_packout();

    // build.ninja : cxx_static()
    //  deps.obj + self.srcs.o => rv[LRinfo].a
    //  deps.rt / deps.so / deps.a => rv[LRinfo]
    if (s3->mk->ninja && s3->target_role == 'a') {
        std::string outfile_njesc = cgn::NinjaFile::escape_path(pack_out.packout_file);
        cgn::NinjaFile::BuildSection field;
        field.rule = "gcc_ar";
        field.inputs = obj_out_ninja_esc
                     + cgn::NinjaFile::escape_path(s3->src_extra.object_files);
        field.outputs = {outfile_njesc};
        field.variables["exe"] = ccenv + s3->two_escape(s3->toolchain->exe_ar);
        field.variables["arflags"] = s3->two_escape_to_string(s3->toolchain->ar_arg_arflags);
        s3->mk->ninja->append_build(field);
    }


    // currently we have 3 types of linker in unix-like world
    //  * linux: gnu-ld (binutils)
    //  * linux: llvm-ld
    //  * mac:   bsd-ld (os-internal)

    // build.ninja : cxx_shared() / cxx_executable()
    //   deps.object + deps.static + self.srcs.o => self.so / self.exe
    //   with carg.ldflags and -wholearchive:x._wholearchive_a
    //   self.so + {so from deps} => rv[LRinfo].so
    if (s3->mk->ninja && (s3->target_role == 's' || s3->target_role == 'x')) {
        std::vector<std::string> &ldflags = (s3->target_role=='s'? s3->toolchain->so_arg : s3->toolchain->exe_arg).ldflags;
        //prepare rpath argument
        //  this is seen as target ldflags, so put on the tail of cargs.ldflags
        // TODO: macos bsd-linker ldflags?
        if (s3->mk->trimmed_cfg["os"] == "linux") {
            if (s3->mk->trimmed_cfg["pkg_mode"] != "")
                ldflags += {"-Wl,--enable-new-dtags", "-Wl,--rpath=$ORIGIN"};
            else {
                ldflags += {"-Wl,--enable-new-dtags"};
                for (auto &so : s3->src_extra.shared_files) {
                    auto path1    = cgn::Tools::parent_path(so);
                    auto path_rel = cgn::Tools::rebase_path(path1, s3->mk->out_prefix);
                    ldflags += {"-Wl,--rpath=$ORIGIN/" + path_rel};
                }
            }

            // ldflags += {"-Wl,--version-script=" + def_file};
            // ldflags += {"-Wl,-exported_symbols_list,\"" + def_file + "\""};
            if (s3->def_file.size())
                ldflags += {"-Wl,--export-dynamic-symbol-list=" + s3->def_file};
        }

        // generate ninja target
        auto *field = s3->mk->ninja->append_build();
        field->rule = "crun_rsp";
        field->inputs = obj_out_ninja_esc 
                      + cgn::NinjaFile::escape_path(s3->src_extra.object_files);
        field->implicit_inputs = cgn::NinjaFile::escape_path(s3->src_extra.static_files) 
                               + cgn::NinjaFile::escape_path(s3->src_extra.shared_files)
                               + njdep_setenv;
        field->outputs = {cgn::NinjaFile::escape_path(pack_out.packout_file)};
        field->order_only = s3->ninja_order_only_dep;
        field->variables["exe"] = s3->two_escape(s3->target_role=='s'? s3->toolchain->exe_solink : s3->toolchain->exe_xlink);
        
        // ninja target : compile commands
        // --start-group   : {all.obj} {dep.static without whole} -l{dep.shared}
        // --whole-archive : {static_files from inherit dep}
        std::vector<std::string> ldarg_others = obj_out + s3->src_extra.object_files + *s3->src_extra_no_whole;
        for (auto &so : s3->src_extra.shared_files)
            ldarg_others += {"-l:" + so};

        ldflags += {"-o" + pack_out.packout_file};
        if (s3->mk->trimmed_cfg["cxx_toolchain"] == "xcode")
            ldflags += s3->src_extra.static_files + cgn::SList{"-Wl,-force_load"} + ldarg_others;
        else { //linux
            if (s3->src_extra.static_files.size())
                ldflags += cgn::SList{"-Wl,--whole-archive"} 
                         + s3->src_extra.static_files 
                         + cgn::SList{"-Wl,--no-whole-archive"};
            if (ldarg_others.size())
                ldflags += cgn::SList{"-Wl,--start-group"} 
                         + ldarg_others 
                         + cgn::SList{"-Wl,--end-group"};
        }

        field->variables["args"] = s3->two_escape_to_string(ldflags);
        field->variables["desc"] = "LINK " + cgn::NinjaFile::escape_path(pack_out.packout_file);

        // copy runtime when cxx_executable()
        if (s3->target_role == 'x')
            for (auto &one_entry : s3->src_extra.runtime_files) {
                const auto &dst1 = one_entry.first;
                if (dst1.type != dst1.BASE_ON_OUTPUT)
                    continue;
                auto dst  = api.rebase_path(dst1, ".", s3->mk);
                auto &src = one_entry.second;
                auto *cpfield = s3->mk->ninja->append_build();
                //TODO: copy runtime by custom command (like symbolic-link)
                cpfield->rule    = "unix_file_copy_cppdeprule";
                cpfield->inputs  = {cgn::NinjaFile::escape_path(src)};
                cpfield->outputs = {cgn::NinjaFile::escape_path(dst)};
                field->order_only += cpfield->outputs;
            }
    } // endif (role=='s' or 'x')

    // Do not need to add current .so to runtime_files without 'pkg_mode',
    // just using --rpath to locate
    if ((s3->target_role == 's' || s3->target_role == 'x') && s3->mk->trimmed_cfg["pkg_mode"] == "")
        pack_out.packout_rt_filename = pack_out.packout_rt_filepath = "";

    return s3->done_with_entry(pack_out);
} //default_step3_xnix()

cgn::LinkAndRunInfo CxxWorker::step3_gen_ninja(CxxWorker::Stage3In *s3) const
{
    if (s3->mk->trimmed_cfg["os"] == "win")
        return default_step3_win(s3);
    else
        return default_step3_xnix(s3);
} //CxxWorker::step3_gen_ninja()

}; //namespace
