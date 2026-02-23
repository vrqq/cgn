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
        rv.is_compiler_controlled_link = true;
        rv.exe_arg.compiler_driven_ldflags = rv.so_arg.compiler_driven_ldflags = {"-shared"};
        rv.c_arg.cflags = rv.cpp_arg.cflags = rv.asm_arg.cflags = {"-fPIC","-pthread"};
    }
    if (cfg["os"] == "linux" && cfg["cxx_toolchain"] == "llvm") {
        rv.exe_cc = rv.exe_asm = rv.exe_solink = rv.exe_xlink = prefix + "clang";
        rv.exe_cxx = prefix + "clang++";
        rv.exe_ar  = prefix + "ar";
        rv.is_compiler_controlled_link = true;
        rv.c_arg.cflags = rv.cpp_arg.cflags = rv.asm_arg.cflags = {"-fPIC", "-pthread"};
        rv.exe_arg.compiler_driven_ldflags = rv.so_arg.compiler_driven_ldflags = {"-shared"};
    }
    if (cfg["os"] == "mac" && cfg["cxx_toolchain"] == "xcode") {
        rv.exe_cc = rv.exe_asm = rv.exe_solink = rv.exe_xlink = prefix + "clang";
        rv.exe_cxx = prefix + "clang++";
        rv.exe_ar  = prefix + "ar";
        rv.exe_arg.compiler_driven_ldflags = rv.so_arg.compiler_driven_ldflags = {"-shared"};
        rv.is_compiler_controlled_link = true;
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
        rv.is_compiler_controlled_link = false;
    }

    return rv;
}

static std::pair<CxxToolchainInfo, std::string> step1_win_msvc(cgn::Configuration &cfg)
{
    std::string mimimum_winver = cfg["cxx_winapi_winver"];
    if (mimimum_winver.empty())
        mimimum_winver = CxxWorker::DEFAULT_MINIMUM_WINVER;

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
    interp.is_compiler_controlled_link = false;

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
    // For toolchain gcc, the cross compiler is present by compiler filename
    // like /toolchain_X/arm-none-linux-gnueabi-gcc, and the kernel path 
    // (--sysroot) usually hard-coding inside compiler.
    std::string prefix = cfg["cxx_prefix"];
    interp.exe_cc  = (prefix + "gcc");
    interp.exe_cxx = (prefix + "g++");
    interp.exe_asm = (prefix + "gcc");
    interp.exe_ar  = (prefix + "gcc-ar");
    interp.exe_solink = (prefix + "g++");
    interp.exe_xlink  = (prefix + "g++");
    interp.is_compiler_controlled_link = true;
    
    interp.c_arg.cflags   = {"-std=c17"};
    interp.cpp_arg.cflags = {"-std=c++17"};
    
    interp.so_arg.compiler_driven_ldflags = {"-shared"};

    interp.exe_arg.ldflags = interp.so_arg.ldflags = {
        "--warn-common", "-z,origin", 
        "--export-dynamic",  // force export from executable
        // "--warn-section-align", 
        // "-Bsymbolic", "-Bsymbolic-functions",
    };

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
    if (cfg["cxx_sysroot"] != "")
        cflags_1st += {"--sysroot=" + (std::string)cfg["cxx_sysroot"]};

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

    interp.so_arg.compiler_driven_ldflags  += ldflags_1st;
    interp.exe_arg.compiler_driven_ldflags += ldflags_1st;

    return {interp, {}};
} //step1_linux_gcc()


static std::pair<CxxToolchainInfo, std::string> step1_linuxllvm_and_xcode(cgn::Configuration &cfg)
{
    CxxToolchainInfo interp;

    // For toolchain llvm, user should assign the target os/cpu and sysroot for 
    // cross compile.
    std::string prefix = cfg["cxx_prefix"];
    interp.exe_cc  = (prefix + "clang");
    interp.exe_cxx = (prefix + "clang++");
    interp.exe_asm = (prefix + "clang");
    interp.exe_ar     = (prefix + "ar");
    interp.exe_solink = (prefix + "clang++");
    interp.exe_xlink  = (prefix + "clang++");
    interp.is_compiler_controlled_link = true;

    interp.so_arg.compiler_driven_ldflags = {"-shared"};
    if (cfg["os"] == "linux") {
        interp.exe_ar     = (prefix + "llvm-ar");
        interp.exe_solink = (prefix + "clang++");
        interp.exe_xlink  = (prefix + "clang++");
        interp.exe_arg.compiler_driven_ldflags = {"-fuse-ld=lld"};
        interp.so_arg.compiler_driven_ldflags  = {"-fuse-ld=lld", "-shared"};
    }

    interp.c_arg.cflags   = {"-std=c17"};
    interp.cpp_arg.cflags = {"-std=c++17"};

    std::vector<std::string> cflags_1st = {
        "-fvisibility=hidden",
        "-fno-common",
        "-fcolor-diagnostics", "-Wreturn-type", 
        "-I.", "-fPIC", "-pthread"};
    
    // '-Wl,$(ldflags_lnk[])' for clang++ linker driver
    std::vector<std::string> ldflags_lnk;
    
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
        ldflags_lnk += {
            "--exclude-libs=ALL", 
            "--discard-all",
            // "--thinlto-jobs=0", 
            // "--thinlto-cache-dir=./thinlto_cache", 
            // "--thinlto-cache-policy,cache_size_bytes=1g",
            "--warn-unresolved-symbols"
        };
    }

    //["llvm_stl"]
    if (cfg["llvm_stl"] == "libc++")
        cflags_1st += {"-stdlib=libc++"};

    //["cxx_sysroot"]
    if (cfg["cxx_sysroot"] != "")
        cflags_1st += {
            "--sysroot=" + (std::string)(cfg["cxx_sysroot"])};

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

    return {interp, ""};
} //step1_linuxllvm_and_xcode()

std::string CxxWorker::step1_test_param(cgn::Configuration &cfg, const std::string &via)
{
    if (via == "cmake" || via == "minimum")
        return this->s1out = step1_cmake_minumum(cfg), "";
    auto rtn = [this](std::pair<CxxToolchainInfo, std::string> &&input){
        this->s1out = input.first;
        return input.second;
    };
    if (cfg["cxx_toolchain"] == "msvc" && cfg["os"] == "win")
        return rtn(step1_win_msvc(cfg));
    else if (cfg["cxx_toolchain"] == "gcc" && cfg["os"] == "linux")
        return rtn(step1_linux_gcc(cfg));
    else if (
        (cfg["cxx_toolchain"] == "xcode" && cfg["os"] == "mac") ||
        (cfg["cxx_toolchain"] == "llvm"  && cfg["os"] == "linux")
    )
        return rtn(step1_linuxllvm_and_xcode(cfg));
    return "Unsupported toolchain";
} // CxxWorker::step1_test_param()

// ****************
//
// Step 2 config confirm
// ================

std::string CxxWorker::step2_confirm(CxxContext &x)
{
    // preprocess "x.srcs = file_glob(*)"
    // if the source file is not at the same/sub folder of BUILD.cgn.cc
    // using absolutely path to locate.
    // BUG HERE: file_glob(*) cannot found file newly added (in ninja cache)
    //     TODO: target with file_glob() would re-analyse each time.
    //           so we should use external executable to generate ninja dyndep.
    //           @cgn.d//library/advtools
    for (auto &p : x.srcs) {
        if (p.type == p.BASE_ON_OUTPUT)
            return "Unsupported src " + p.to_string();
        if (p.rpath.find('*') == p.rpath.npos) //if not file_glob
            self_src += {api.rebase_path(p, ".", x.opt)};
        else {
            std::string path2 = api.rebase_path(p, ".", x.opt);
            for (const auto &it : api.file_glob(path2))
                self_src += {it};
        }
    }

    // confirm configuration
    // 'host_shell' for function two_escape()
    x.opt->cfg.visit_keys({"host_shell", "os", "cpu"});
    mk = x.opt->confirm();
    if (!mk) //return if cache found
        return "";

    self_extra = x._lnr_to_self;
    cgn::Tools::remove_duplicate_inplace(self_extra.object_files);
    cgn::Tools::remove_duplicate_inplace(self_extra.static_files);
    cgn::Tools::remove_duplicate_inplace(self_extra.shared_files);

    target_role = x.role;
    perferred_binary_name = x.perferred_binary_name;
    // self_staticlib_no_whole = &x._self_no_whole_archive;
    self_extra_no_whole = &x._self_no_whole_archive;
    
    // x.include_dirs CGNPath[] rebase
    for (auto &p : x.include_dirs)
        if (p.type != p.BASE_ON_WORKINGROOT)
            p = cgn::make_path_base_working(api.rebase_path(p, ".", mk));

    // x.pub.include_dirs CGNPath rebase
    for (auto &p : x.pub.include_dirs)
        if (p.type != p.BASE_ON_WORKINGROOT)
            p = cgn::make_path_base_working(api.rebase_path(p, ".", mk));
    
    // merge to $s2out
    s2out = s1out;
    for (auto xsrc : {&s2out.c_arg, &s2out.cpp_arg, &s2out.asm_arg}){
        xsrc->cflags += x._cxx_to_self.cflags + x.cflags;
        xsrc->defines += x._cxx_to_self.defines + x.defines;

        std::vector<std::string> final_inc;
        for (auto &p : x.include_dirs + x._cxx_to_self.include_dirs)
            final_inc += {api.rebase_path(p, ".", mk)};
        final_inc += xsrc->include_dirs;
        cgn::Tools::remove_duplicate_inplace(final_inc);
        std::swap(xsrc->include_dirs, final_inc);
    }
    for (auto xout : {&s2out.exe_arg, &s2out.so_arg})
        xout->ldflags += x._cxx_to_self.ldflags + x.ldflags;

    // generate $ninja_order_only_dep
    // TODO: cannot use variable in ninja order_dep region.
    if (true || x.quickdep_ninja_target.size() <= 1)
        ninja_order_only_dep = x.quickdep_ninja_target;
    else {
        auto *field = mk->ninja->append_build();
        mk->ninja->append_variable("target_dep", 
            list2str(cgn::NinjaFile::escape_path(x.quickdep_ninja_target)));
        ninja_order_only_dep = {"$target_dep"};
    }

    // generate result InfoTable
    if (!mk->merge_from(x._pub_infos) || !mk->merge_entry(&x.pub))
        return "CxxInterpreter: internal error on generate InfoTable";

    return "";
} //CxxWorker::step2_confirm()

// case for path_out: xxx.so / .lib may have same name with folder-name 
//                    in src folder, so we have to add '_' before path_out.
// case for path_out: cgn-out/.../ in same folder of current interpreter
//                    add '__' (two underline) before path_out.
// @param IN file_in : file_path with working_root_rel or abs_path
// @param IN mk      : confirmed opt
// @param IN dot_obj : path_out end with '.obj' or '.o'
// @return pair<path_out, type_of_src> :
//         - path_out : "output file relpath"
//         - type of src : A/+/C/0 (asm, c++, c, 0:igonre)
// TODO : in windows, ninja have bug that cannot mkdir end with '..'
static std::pair<std::string, char> src_path_convert(
    const std::string &file_in, const cgn::CGNTargetMaker *mk, bool dot_obj = false
) {
    // get extension
    auto fd_slash = file_in.rfind('/');
    auto fd = file_in.rfind('.');
    if (fd == file_in.npos || (fd_slash != file_in.npos && fd < fd_slash))
        return {"", 0};  // no extension, cpp header
    std::string left = file_in.substr(0, fd);
    std::string ext;
    for (std::size_t i=fd+1; i<file_in.size(); i++)
        ext.push_back( ('A'<=file_in[i] && file_in[i]<='Z')? (file_in[i]-'A'+'a'): file_in[i]);

    if (ext == "def")
        return {"", 'D'};
    
    char rv_type = 0;
    if (ext == "cc" || ext == "cpp" || ext == "cxx" || ext == "c++")
        rv_type = '+';
    if (ext == "c")
        rv_type = 'C';
    if (ext == "s" || ext == "asm")
        rv_type = 'A';

    std::string src_dir = api.locale_path(mk->src_prefix);
    bool start_with_outprefix = (file_in.size() > mk->out_prefix.size()
        && memcmp(file_in.c_str(), mk->out_prefix.c_str(), mk->out_prefix.size())==0);
    bool start_with_srcprefix = (file_in.size() > src_dir.size()
        && memcmp(file_in.c_str(), src_dir.c_str(), src_dir.size())==0);
    
    // file_in is inside src_prefix
    if (start_with_srcprefix) 
        return {cgn::Tools::locale_path(mk->out_prefix + "_" 
                + file_in.substr(mk->src_prefix.size()) 
                + (dot_obj?".obj":".o")),
            rv_type};

    // file_in is inside current out_prefix
    if (start_with_outprefix) {
        std::string probe1 = file_in.substr(mk->out_prefix.size());
        left = probe1.substr(0, probe1.rfind('.'));
        return {cgn::Tools::locale_path(
                mk->out_prefix + "__" + probe1 + (dot_obj?".obj":".o")),
            rv_type};
    }

    // otherwise : abspath or other dirs
    return {
        mk->out_prefix + api.mangle_path_to_relative(file_in) + (dot_obj?".obj":".o"),
        rv_type
    };
} //src_path_convert()


// ****************
//
// Step 3 ninja file generation
// ================

constexpr const char *rule_ninja = "@cgn.d//library/cxx/cxx_rule.ninja";
static void write_ninja_phony_entry(
    cgn::CGNTargetMaker *mk, const std::vector<std::string> &ninja_field_output
) {
    if (mk->ninja == nullptr)
        return ;
    auto *entry = mk->ninja->append_build();
    entry->rule = "phony";
    entry->inputs = ninja_field_output;
    entry->outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry)};
}

void CxxWorker::default_step3_win()
{
    using SItem = std::vector<std::string>;
    
    // add setenv batch dependency before cc.exe run
    std::string ccenv;
    std::vector<std::string> njdep_env;
    if (s2out.env_loader_script.size()){
        ccenv = "cmd.exe /c " + two_escape(s2out.env_loader_script) + " && ";
        njdep_env = {cgn::NinjaFile::escape_path(s2out.env_loader_script)};
    }
    
    // predefine ninja variable: cflags_c, cflags_cc, cflags_asm
    std::string njenv_cflags_c = "cflags_c", 
                njenv_cflags_cpp = "cflags_cpp", 
                njenv_cflags_asm = "cflags_asm";
    auto write_env = [&](std::string *pname, std::string value) {
        if (!mk->ninja || value.empty()) *pname = "";
        else mk->ninja->append_variable(*pname, value);
    };
    write_env(&njenv_cflags_c, 
        list2str(s2out.c_arg.cflags) 
        + list2str(s2out.c_arg.include_dirs, "/I")
        + list2str(s2out.c_arg.defines, "/D")
    );
    write_env(&njenv_cflags_cpp,
        list2str(s2out.cpp_arg.cflags) 
        + list2str(s2out.cpp_arg.include_dirs, "/I")
        + list2str(s2out.cpp_arg.defines, "/D")
    );
    write_env(&njenv_cflags_asm,
        list2str(s2out.asm_arg.cflags) 
        + list2str(s2out.asm_arg.include_dirs, "/I")
        + list2str(s2out.asm_arg.defines, "/D")
    );

    if (mk->ninja) {
        static std::string rule_path = api.get_filepath(rule_ninja);
        mk->ninja->append_include(rule_path);
    }

    // patch for .lib in windows : 
    //   if field->inputs empty, lib.exe would not generate any files,
    //   so here we feed a empty source file here.
    if (target_role == 'a' && self_src.empty() && self_extra.object_files.empty())
        self_src = {api.get_filepath("@cgn.d//library/cxx/vsenv_loader/empty_file.c")};

    std::string def_file;

    // build.ninja : source file => .o
    std::string pdbfile = mk->out_prefix + "__vc.pdb";
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;
    for (auto &path_in : self_src) {
        auto conv_resp = src_path_convert(path_in, mk, true);
        auto &path_out = conv_resp.first; auto &file_type = conv_resp.second;
        if (file_type == 0)
            continue;
        if (file_type == 'D') {
            def_file = path_in;
            continue;
        }

        //field->input has been moved into cflags
        cgn::NinjaFile::BuildSection field;
        field.outputs = {cgn::NinjaFile::escape_path(path_out)};
        field.implicit_inputs = SItem{cgn::NinjaFile::escape_path(path_in)} + njdep_env;
        field.order_only = ninja_order_only_dep;
        if (file_type == 'A') {
            field.rule = "msvc_ml";
            field.variables["cc"] = ccenv + two_escape(s2out.exe_asm);
            field.variables["cflags"] = "$" + njenv_cflags_asm;
        }
        if (file_type == '+') {
            field.rule = "msvc_cl";
            field.variables["cc"] = ccenv + two_escape(s2out.exe_cxx);
            field.variables["cflags"] = "$" + njenv_cflags_cpp;
            field.variables["pdb"] = cgn::NinjaFile::escape_path(pdbfile);
        }
        else {
            field.rule = "msvc_cl";
            field.variables["cc"] = ccenv + two_escape(s2out.exe_cc);
            field.variables["cflags"] = "$" + njenv_cflags_c;
            field.variables["pdb"] = cgn::NinjaFile::escape_path(pdbfile);
        }

        // NinjaBuild bug DirtyPatch:
        // add ./ prefix of src filepath to avoid string starting with '@'
        // if we add "./" in ninja input file, ninja.exe would auto remove it 
        // when writing down to .rspfile, then it cause cl.exe parse it as
        // another rspfile.
        if (path_in.at(0) == '@')
            path_in = "." + mk->PATH_SEPARATOR + path_in;
        field.variables["cflags"] += "/c " + two_escape(path_in);

        if (mk->ninja)
            mk->ninja->append_build(field);
        obj_out.push_back(path_out);
        obj_out_ninja_esc.push_back(field.outputs[0]);
    } //endfor (auto &path_in : self_src)

    // build.ninja : cxx_sources()
    // cxx_sources() cannot process any field of LinkAndRunInfo
    // so add the .obj file generated by itself then return
    if (target_role == 'o') {
        s3out.object_files = obj_out;
        return write_ninja_phony_entry(mk, obj_out_ninja_esc);
    }

    // build.ninja : cxx_static()
    //  deps.obj + self.srcs.o => rv[LRinfo].a
    //  deps.rt / deps.so / deps.a => rv[LRinfo]
    if (target_role == 'a') {
        std::string outfile = mk->out_prefix + mk->name + ".lib";
        if (perferred_binary_name.size())
            outfile = mk->out_prefix + perferred_binary_name;
        s3out.static_files = std::vector<std::string>{outfile};
        
        if (!mk->ninja)
            return ;
        auto *field = mk->ninja->append_build();
        field->rule = "msvc_lib";
        field->inputs = obj_out_ninja_esc
                      + cgn::NinjaFile::escape_path(self_extra.object_files);
        field->outputs = {cgn::NinjaFile::escape_path(outfile)};
        field->implicit_inputs = njdep_env;
        field->variables["restat"] = "1";
        field->variables["libexe"] = ccenv + s2out.exe_ar;
        field->variables["arflags"] = list2str(s2out.ar_arg_arflags);
        if (def_file.size()) {
            field->variables["arflags"] += "/DEF:" + def_file + " ";
            field->implicit_inputs += {cgn::NinjaFile::escape_path(def_file)};
        }

        return write_ninja_phony_entry(mk, field->outputs);
    } //endif (target_role == 'a')

    // build.ninja : cxx_shared() / cxx_executable()
    //   deps.object + deps.static + self.srcs.o => self.so / self.exe
    //   with carg.ldflags and -wholearchive:x._wholearchive_a
    //   self.so + {so from deps} => rv[LRinfo].so
    if (target_role == 's' || target_role == 'x') {
        std::string outfile_fname = mk->name + (target_role=='s'? ".dll" :".exe");
        std::string outfile_implib = mk->out_prefix + mk->name + ".lib";
        if (perferred_binary_name.size()) {
            outfile_fname = perferred_binary_name;
            outfile_implib = mk->out_prefix + outfile_fname + ".lib";
        }
        std::string outpath = mk->out_prefix + outfile_fname;

        // write target result
        s3out.shared_files = std::vector<std::string>{outfile_implib};
        s3out.runtime_files[cgn::make_path_base_out(outfile_fname)] = outpath;

        if (!mk->ninja)
            return ;

        //prepare link.exe or cl.exe /link
        std::string ldflags; {
            auto &xarg = (target_role=='s'?s2out.so_arg:s2out.exe_arg);
            if (s2out.is_compiler_controlled_link)
                ldflags += list2str(xarg.compiler_driven_ldflags) + "/link ";
            ldflags += list2str(xarg.ldflags);
            for (auto elib : self_extra.static_files)
                ldflags += "/WHOLEARCHIVE:" + two_escape(elib);
        }
        
        //prepare rpath argument
        //  this is seen as target ldflags, so put on the tail of cargs.ldflags
        //TODO: manifest and .runtime

        //generate ninja section
        // --start-group   : {all.obj} {dep.static without whole} -l{dep.shared}
        // --whole-archive : {static_files from inherit dep}
        auto *field = mk->ninja->append_build();
        field->rule = "msvc_link";
        field->inputs = obj_out_ninja_esc
                      + cgn::NinjaFile::escape_path(self_extra.object_files)
                      + cgn::NinjaFile::escape_path(self_extra.static_files) 
                      + cgn::NinjaFile::escape_path(*self_extra_no_whole) 
                      + cgn::NinjaFile::escape_path(self_extra.shared_files);
        field->outputs = {cgn::NinjaFile::escape_path(outpath)};
        field->implicit_inputs = njdep_env;
        field->variables["restat"] = "1";
        if (target_role == 's') //only add .lib for .dll
            field->implicit_outputs = {cgn::NinjaFile::escape_path(outfile_implib)};
        field->variables["link"] = ccenv + two_escape(target_role=='s'? s2out.exe_solink:s2out.exe_xlink);
        field->variables["ldflags"] = ldflags;
        if (def_file.size()) {
            field->variables["ldflags"] += "/DEF:" + def_file + " ";
            field->implicit_inputs += {cgn::NinjaFile::escape_path(def_file)};
        }

        // copy runtime when cxx_executable() and not pkg_mode
        if (target_role == 'x')
            for (auto &one_entry : self_extra.runtime_files) {
                const cgn::CGNPath &dst1 = one_entry.first;
                if (dst1.type != dst1.BASE_ON_OUTPUT)
                    continue;
                std::string dst = api.rebase_path(dst1, ".", mk);
                const std::string &src = one_entry.second;
                auto *cpfield = mk->ninja->append_build();
                //TODO: copy runtime by custom command (like symbolic-link)
                cpfield->rule    = "win_file_copy_cppdeprule";
                cpfield->inputs  = {cgn::NinjaFile::escape_path(src)};
                cpfield->outputs = {cgn::NinjaFile::escape_path(dst)};
                field->order_only += cpfield->outputs;
                // entry->order_only += cpfield->outputs;
            }

        return write_ninja_phony_entry(mk, field->outputs);
    } // if (role=='s' or 'x')
} //CxxWorker::default_step3_win()

void CxxWorker::default_step3_xnix()
{
    std::string ccenv;
    std::vector<std::string> njdep_setenv;
    if (s2out.env_loader_script.size()){
        ccenv += two_escape(s2out.env_loader_script) + " && ";
        njdep_setenv = {cgn::NinjaFile::escape_path(s2out.env_loader_script)};
    }

    if (mk->ninja) {
        static std::string rule_path = api.get_filepath(rule_ninja);
        mk->ninja->append_include(rule_path);
    }

    //=== Section 3: make ninja file ===
    // common for both GCC and LLVM
    // build.ninja : source file => .o
    // field.input and field.output : need ninja escape, instead of shell esacpe
    // carg.cflags and carg.ldflags : already two escaped
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;
    std::string def_file;

    for (auto &path_in : self_src) {
        auto conv_resp = src_path_convert(path_in, mk, false);
        auto &path_out = conv_resp.first; auto &file_type = conv_resp.second;
        if (file_type == 0)
            continue;

        if (file_type == 'D') {
            def_file = path_in;
            continue;
        }

        cgn::NinjaFile::BuildSection field;
        field.rule = "gcc";
        field.inputs  = {cgn::NinjaFile::escape_path(path_in)};
        field.outputs = {cgn::NinjaFile::escape_path(path_out)};
        field.order_only = ninja_order_only_dep;
        if (file_type == '+') {
            field.variables["cc"] = ccenv + two_escape(s2out.exe_cxx);
            field.variables["cflags"] = list2str(s2out.cpp_arg.cflags)
                                      + list2str(s2out.cpp_arg.include_dirs, "-I")
                                      + list2str(s2out.cpp_arg.defines, "-D");
        }else if (file_type == 'A') {
            field.variables["cc"] = ccenv + two_escape(s2out.exe_asm);
            field.variables["cflags"] = list2str(s2out.asm_arg.cflags)
                                      + list2str(s2out.asm_arg.include_dirs, "-I")
                                      + list2str(s2out.asm_arg.defines, "-D");
        }else {
            field.variables["cc"] = ccenv + two_escape(s2out.exe_cc);
            field.variables["cflags"] = list2str(s2out.c_arg.cflags)
                                      + list2str(s2out.c_arg.include_dirs, "-I")
                                      + list2str(s2out.c_arg.defines, "-D");
        }

        if (mk->ninja)
            mk->ninja->append_build(field);
        obj_out.push_back(path_out);
        obj_out_ninja_esc.push_back(field.outputs[0]);
    } //endfor (auto &path_in : self_src)

    // build.ninja : cxx_sources()
    // cxx_sources() cannot process any field of LinkAndRunInfo
    // so add the .obj file generated by itself then return
    if (target_role == 'o') {
        // write target result
        s3out.object_files = obj_out;
        return write_ninja_phony_entry(mk, obj_out_ninja_esc);
    }


    // build.ninja : cxx_static()
    //  deps.obj + self.srcs.o => rv[LRinfo].a
    //  deps.rt / deps.so / deps.a => rv[LRinfo]
    if (target_role == 'a') {
        std::string outfile = mk->out_prefix + "lib" + mk->name + ".a";
        if (perferred_binary_name.size())
            outfile = mk->out_prefix + perferred_binary_name;
        
        // write target result
        s3out.static_files = std::vector<std::string>{outfile};

        if (!mk->ninja)
            return ;

        std::string outfile_njesc = cgn::NinjaFile::escape_path(outfile);
        cgn::NinjaFile::BuildSection field;
        field.rule = "gcc_ar";
        field.inputs = obj_out_ninja_esc
                     + cgn::NinjaFile::escape_path(self_extra.object_files);
        field.outputs = {outfile_njesc};
        field.variables["exe"] = ccenv + two_escape(s2out.exe_ar);
        field.variables["arflags"] = list2str(s2out.ar_arg_arflags);
        mk->ninja->append_build(field);
        return write_ninja_phony_entry(mk, field.outputs);
    }


    // currently we have 3 types of linker in unix-like world
    //  * linux: gnu-ld (binutils)
    //  * linux: llvm-ld
    //  * mac:   bsd-ld (os-internal)

    // build.ninja : cxx_shared() / cxx_executable()
    //   deps.object + deps.static + self.srcs.o => self.so / self.exe
    //   with carg.ldflags and -wholearchive:x._wholearchive_a
    //   self.so + {so from deps} => rv[LRinfo].so
    if (target_role == 's' || target_role == 'x') {
        std::string filename;
        std::string outfile;
        std::string outfile_njesc;
        if (target_role == 's')
            outfile = mk->out_prefix + (filename = "lib" + mk->name + ".so");
        else
            outfile = mk->out_prefix + (filename = mk->name);
        if (perferred_binary_name.size())
            outfile = mk->out_prefix + (filename = perferred_binary_name);
        outfile_njesc = cgn::NinjaFile::escape_path(outfile);

        std::string ldflags; {
            auto &xarg = (target_role=='s'? s2out.so_arg : s2out.exe_arg);
            if (s2out.is_compiler_controlled_link) {
                ldflags += list2str(xarg.compiler_driven_ldflags)
                         + list2str(xarg.ldflags, "-Wl,");
            }else
                ldflags = list2str(xarg.ldflags);
        }
        //prepare rpath argument
        //  this is seen as target ldflags, so put on the tail of cargs.ldflags
        // TODO: macos bsd-linker ldflags?
        if (mk->trimmed_cfg["os"] == "linux") {
            if (mk->trimmed_cfg["pkg_mode"] != "") {
                ldflags += "-Wl,--enable-new-dtags " + two_escape("-Wl,-rpath=$ORIGIN");
                s3out.runtime_files[cgn::make_path_base_out(filename)] = outfile;
            }
            else {
                ldflags += "-Wl,--enable-new-dtags ";
                for (auto &so : self_extra.shared_files) {
                    auto path1    = cgn::Tools::parent_path(so);
                    auto path_rel = cgn::Tools::rebase_path(path1, mk->out_prefix);
                    ldflags += two_escape("-Wl,--rpath=$ORIGIN/" + path_rel);
                }
            }

            // ldflags += {"-Wl,--version-script=" + def_file};
            // ldflags += {"-Wl,-exported_symbols_list,\"" + def_file + "\""};
            if (def_file.size())
                ldflags += "-Wl,--export-dynamic-symbol-list=" + def_file + " ";
        }
        s3out.shared_files = std::vector<std::string>{outfile};

        // generate ninja target
        if (!mk->ninja)
            return ;
        auto *field = mk->ninja->append_build();
        field->rule = "crun_rsp";
        field->inputs = obj_out_ninja_esc 
                      + cgn::NinjaFile::escape_path(self_extra.object_files);
        field->implicit_inputs = cgn::NinjaFile::escape_path(self_extra.static_files) 
                               + cgn::NinjaFile::escape_path(self_extra.shared_files)
                               + njdep_setenv;
        field->outputs = {outfile_njesc};
        field->order_only = ninja_order_only_dep;
        field->variables["exe"] = two_escape(target_role=='s'? s2out.exe_solink : s2out.exe_xlink);
        
        // ninja target : compile commands
        // --start-group   : {all.obj} {dep.static without whole} -l{dep.shared}
        // --whole-archive : {static_files from inherit dep}
        std::string ldarg_wholearchive = list2str(two_escape(self_extra.static_files));
        std::string ldarg_others = list2str(two_escape(obj_out))
                    + list2str(two_escape(self_extra.object_files))
                    + list2str(two_escape(*self_extra_no_whole))
                    + list2str(two_escape(self_extra.shared_files), "-l:");

        ldflags += + "-o " + two_escape(outfile) + " ";
        if (mk->trimmed_cfg["cxx_toolchain"] == "xcode")
            ldflags += ldarg_wholearchive + "-Wl,-force_load " + ldarg_others + " ";
        else { //linux
            if (ldarg_wholearchive.size())
                ldflags += "-Wl,--whole-archive " + ldarg_wholearchive 
                         + "-Wl,--no-whole-archive ";
            if (ldarg_others.size())
                ldflags += "-Wl,--start-group " + ldarg_others + "-Wl,--end-group ";
        }

        field->variables["args"] = ldflags + "-o " + two_escape(outfile);
        field->variables["desc"] = "LINK " + outfile_njesc;

        // copy runtime when cxx_executable()
        if (target_role == 'x')
            for (auto &one_entry : self_extra.runtime_files) {
                const auto &dst1 = one_entry.first;
                if (dst1.type != dst1.BASE_ON_OUTPUT)
                    continue;
                auto dst  = api.rebase_path(dst1, ".", mk);
                auto &src = one_entry.second;
                auto *cpfield = mk->ninja->append_build();
                //TODO: copy runtime by custom command (like symbolic-link)
                cpfield->rule    = "unix_cp";
                cpfield->inputs  = {cgn::NinjaFile::escape_path(src)};
                cpfield->outputs = {cgn::NinjaFile::escape_path(dst)};
                field->order_only += cpfield->outputs;
                // entry->order_only += cpfield->outputs;
            }

        return write_ninja_phony_entry(mk, field->outputs);
    } // endif (role=='s' or 'x')
} //CxxWorker::default_step3_xnix()

void CxxWorker::step3_gen_ninja()
{
    if (mk->trimmed_cfg["os"] == "win")
        return default_step3_win();
    else
        return default_step3_xnix();
} //CxxWorker::step3_gen_ninja()

}; //namespace
