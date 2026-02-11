//
//LLVM cross compile
// clang++ ./hello.cpp -o hello.fbsd --target=amd64-pc-freebsd 
//      --sysroot=/project/freebsd-14.0-amd64 -fuse-ld=lld
// clang++ ./hello.cpp -o hello.fbsd --target=aarch64-pc-freebsd 
//      --sysroot=/project/freebsd-14.0-arm64 -fuse-ld=lld
//
#define LANGCXX_CGN_BUNDLE_IMPL
#include <cassert>
#include <cstring>
// #include "ninja_dedup.hpp"
#include "cxx.cgn.h"

// CxxInterpreter
// --------------------------
namespace cxx {

const cgn::BaseInfo::VTable &CxxInfo::_glb_cxx_vtable()
{
    const static cgn::BaseInfo::VTable v = {
        []() -> std::shared_ptr<cgn::BaseInfo> {
            return std::make_shared<CxxInfo>();
        },
        [](void *ecx, const cgn::BaseInfo *rhs) {
            if (rhs == nullptr)
                return false;
            CxxInfo *self = (CxxInfo*)ecx, *r = (CxxInfo*)rhs;
            self->include_dirs += r->include_dirs;
            self->defines += r->defines;
            self->ldflags += r->ldflags;
            self->cflags  += r->cflags;
            return true;
        }, 
        [](const void *ecx, char type) -> std::string { 
            auto *self = (CxxInfo *)ecx;
            const char *indent = "           ";
            size_t len = (type=='h'?5:999);
            return std::string{"{\n"}
                + "   cflags: " + cgn::Logger::fmt_list(self->cflags, indent, len) + "\n"
                + "  ldflags: " + cgn::Logger::fmt_list(self->ldflags, indent, len) + "\n"
                + "  incdirs: " + cgn::Logger::fmt_list(self->include_dirs, indent, len) + "\n"
                + "  defines: " + cgn::Logger::fmt_list(self->defines, indent, len) + "\n"
                + "}";
        }
    };
    return v;
} //CxxInfo::_glb_cxx_vtable()

// @return {stem, type}:
//      type: '\0' to skip, '+' for cpp, 'c' for c and asm
static std::pair<std::string, char> file_check(const std::string &fpath) {
    auto fd_slash = fpath.rfind('/');
    // if (fd_slash != filename.npos)
    //     filename = filename.substr(fd_slash + 1);

    auto fd = fpath.rfind('.');
    if (fd == fpath.npos || (fd_slash != fpath.npos && fd < fd_slash))
        return {"", 0};  // no extension, cpp header
    
    std::string ext = fpath.substr(fd+1);
    // to lower case
    for (char &c : ext)
        if ('A' <= c && c <= 'Z')
            c = c - 'A' + 'a';
    
    // check current file is c/cpp source file
    if (ext == "cc" || ext == "cpp" || ext == "cxx" || ext == "c++")
        return {fpath.substr(0, fd), '+'};
    if (ext == "c" || ext == "s")
        return {fpath.substr(0, fd), 'c'};
    // if (ext == "def")
    //     return {"", '!'};
    return {"", 0};
}

//convert list to string
template<typename T> std::string 
list2str(const T &in, const std::string prefix="")
{
    std::string rv;
    for (auto &it : in)
        rv += prefix + it + " ";
    return rv;
}

// get substr and convert to lowercase
static std::string lower_substr(
    const std::string &in, std::size_t b, std::size_t e = std::string::npos
) {
    if (e == std::string::npos)
        e = in.size();
    std::string rv;
    rv.reserve(e-b);
    for (std::size_t i=b; i<e; i++)
        rv.push_back( ('A'<=in[i] && in[i]<='Z')? (in[i]-'A'+'a'): in[i]);
    return rv;
}

// case for path_out: xxx.so / .lib may have same name with folder-name 
//                    in src folder, so we have to add '_' before path_out.
// case for path_out: cgn-out/.../ in same folder of current interpreter
//                    add '__' (two underline) before path_out.
// @param IN  file1   : filename in context.src after convert to working-root-rel
// @param IN  opt     : opt from interpreter
// @param OUT path_in : "input file relpath"
// @param OUT path_out: "output file relpath"
// @return type of src: A/+/C/0 (asm, c++, c, 0:igonre)
// TODO : in windows, ninja have bug that cannot mkdir end with '..'
static char src_path_convert(
    std::string file1, const cgn::CGNTargetMaker *mk,
    std::string *path_in, std::string *path_out, bool dot_obj = false
) {
    // get extension
    auto fd_slash = file1.rfind('/');
    auto fd = file1.rfind('.');
    if (fd == file1.npos || (fd_slash != file1.npos && fd < fd_slash))
        return 0;  // no extension, cpp header
    std::string left = file1.substr(0, fd);
    std::string ext = lower_substr(file1, fd+1);

    // file1 : abspath or relpath of WorkingRoot => path_out : filename starting with mk->out_prefix
    // under ${out_prefix} => ${out_prefix} + __ + file1
    // under ${src_prefix} => ${out_prefix} + _ + file1
    // othercase           => ${out_prefix} + mangle(file1) (mangle starting with 'A' or 'R')
    auto gen = [&]() {
        // using whole name 'file1' instead of 'left' to avoid name conflict
        // bad example : src["a.cpp", "a.c"] => dst["a.o", "a.o"]
        *path_in = file1; // api.rebase_path(file1, ".", opt.src_prefix);
        std::string src_dir = api.locale_path(mk->src_prefix);

        // since path_in and opt.out_prefix is not in same driver in windows,
        // using rebase_path() can only get the abspath of path_in and it's 
        // hard to check.
        bool start_with_outprefix = (path_in->size() > mk->out_prefix.size()
            && memcmp(path_in->c_str(), mk->out_prefix.c_str(), mk->out_prefix.size())==0);
        bool start_with_srcprefix = (path_in->size() > src_dir.size()
            && memcmp(path_in->c_str(), src_dir.c_str(), src_dir.size())==0);
        if (start_with_outprefix) {// path_in is inside out_prefix
            std::string probe1 = api.rebase_path(*path_in, mk->out_prefix);
            left = probe1.substr(0, probe1.rfind('.'));
            *path_out = cgn::Tools::locale_path(mk->out_prefix + "__" 
                      + file1.substr(mk->out_prefix.size()) + (dot_obj?".obj":".o"));
        }
        else if (start_with_srcprefix)
            *path_out = cgn::Tools::locale_path(mk->out_prefix + "_" 
                      + file1.substr(src_dir.size()) + (dot_obj?".obj":".o"));
        else
            *path_out = mk->out_prefix + api.mangle_path_to_relative(file1) + (dot_obj?".obj":".o");
    };
    
    // check current file is c/cpp source file
    if (ext == "def") {
        *path_in = file1; //cgn::Tools::locale_path(opt.src_prefix + file1);
        return 'D';
    }
    if (ext == "cc" || ext == "cpp" || ext == "cxx" || ext == "c++")
        return gen(), '+';
    if (ext == "c")
        return gen(), 'C';
    if (ext == "s" || ext == "asm")
        return gen(), 'A';
    return 0;
}


struct TargetWorker
{
    // win10==0x0A00; win7==0x0601;
    // win8.1/Server2012R2==0x0603;
    static constexpr const char *mimimum_winver = "0x0A00";

    // extra function
    // generate the mimimum cflags and ldflags for external build system like
    // pkg-config or cmake
    // @param in.include_dirs[] based on working-root-dir
    // @return CxxInfo::cflags and CxxInfo::ldflags
    static CxxInfo export_unix(cgn::Configuration &cfg, CxxInfo in, const std::string &libfile="");
    static CxxInfo export_win_msvc(cgn::Configuration &cfg, CxxInfo in, const std::string &libfile="");

    static CxxToolchainInfo test_param_unix_full(cgn::Configuration &cfg);

    // Step0: input
    CxxContext &x;
    TargetWorker(CxxContext &x) : x(x) {};

    // step1: CxxInfo by interperter
    static CxxToolchainInfo step1_linux_gcc(cgn::Configuration &cfg);
    static CxxToolchainInfo step1_linuxllvm_and_xcode(cgn::Configuration &cfg);
    static CxxToolchainInfo step1_win_msvc(cgn::Configuration &cfg);
    static CxxToolchainInfo step1_minimum(cgn::Configuration &cfg);

    // step2: generate compile argument carg
    // - Confirm the configuration using x.opt->confirm().
    // - Merge C++ flags from target(x), dependencies, and the interpreter in
    //   a later step. Escape arguments to Ninja format.
    //   Specifically, convert carg.include_dirs[] to carg_include_dirs.
    // - Generate opt.result[CxxInfo] from x.pub and target_deps.
    //
    // these variables are generated by step2_merge_selfarg() 
    std::vector<std::string> ninjavar_target_dep;
    cgn::CGNTargetMaker *mk = nullptr;
    cgn::CGNTarget no_pkgmode_alter_target;
    CxxToolchainInfo interp;
    CxxInfo &carg = interp.arg;
    std::vector<std::string> carg_include_dirs;
    cgn::LinkAndRunInfo &carg_link_file = x._lnr_to_self;
    bool step2_opt_confirm(const CxxToolchainInfo &interp);
    std::string two_escape(const std::string &in) {
        return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in, x.cfg["host_shell"]));
    }
    std::vector<std::string> two_escape(std::vector<std::string> in) {
        for (auto &it : in)
            it = two_escape(it);
        return in;
    }


    // step3: lock configuration and generate opt, make ninja file and return value
    //        these step would fill opt->result
    cgn::LinkAndRunInfo *rvlnr;  // point to entry inside 'opt->result'
    std::string escaped_ninja_entry;
    void step31_unix();
    void step31_win();
    void _entry_postprocess(const std::vector<std::string> &to);
};


CxxToolchainInfo TargetWorker::step1_win_msvc(cgn::Configuration &cfg)
{
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
    interp.extra_ldflags_so = {"/DLL"};
    interp.exe_xlink  = (exe_prefix + "link.exe");
    interp.extra_cflags_cpp = {"/std:c++17"};
    interp.extra_cflags_c   = {"/std:c17"};
    interp.is_compiler_controlled_link = false;

    interp.arg.defines += {
        //"UNICODE", "_UNICODE",   // default for NO unicode WidthType (encoding UTF-8 only)
        "_CONSOLE",                //"WIN32",
        "_CRT_SECURE_NO_WARNINGS", //for strcpy instead of strcpy_s
        "WINVER=" + std::string{mimimum_winver},        // win10==0x0A00; win7==0x0601;
        "_WIN32_WINNT=" + std::string{mimimum_winver},  // win8.1/Server2012R2==0x0603;
    };

    interp.arg.cflags += {
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

    interp.arg.ldflags += {
        "/NXCOMPAT",    // Compatible with Data Execution Prevention
        "/DYNAMICBASE",
        
        "kernel32.lib", "user32.lib", "gdi32.lib", "winspool.lib", "comdlg32.lib", 
        "advapi32.lib", "shell32.lib", "ole32.lib", "oleaut32.lib", "uuid.lib", 
        "odbc32.lib", "odbccp32.lib", "legacy_stdio_definitions.lib"
    };
    
    //["cpu"]
    // https://stackoverflow.com/questions/13545010/amd64-not-defined-in-vs2010
    if (cfg["cpu"] == "x86") {
        interp.arg.defines += {"WIN32", "_X86_"};
        // The /LARGEADDRESSAWARE option tells the linker that the application 
        // can handle addresses larger than 2 gigabytes.
        interp.arg.ldflags += {"/SAFESEH", "/MACHINE:X86", "/LARGEADDRESSAWARE"};
        interp.arg.arflags += {"/MACHINE:X86"};
    }
    if (cfg["cpu"] == "x86_64"){
        interp.arg.defines += {"_AMD64_"};  
        interp.arg.ldflags += {"/MACHINE:X64"};
        interp.arg.arflags += {"/MACHINE:X64"};
    }

    //["msvc_runtime"]
    if (cfg["msvc_runtime"] == "MDd") {
        interp.arg.defines += {"_DEBUG"};
        interp.arg.cflags  += {"/MDd"};
        interp.arg.ldflags += {"msvcrtd.lib"};
    }
    if (cfg["msvc_runtime"] == "MD") {
        interp.arg.defines += {"NDEBUG"};
        interp.arg.cflags  += {"/MD"};
        interp.arg.ldflags += {"msvcrt.lib"};
    }
    if (cfg["msvc_runtime"] == "MTd") {
        interp.arg.defines += {"_DEBUG"};
        interp.arg.cflags  += {"/MTd"};
        interp.arg.ldflags += {"libcmtd.lib"};
    }
    if (cfg["msvc_runtime"] == "MT") {
        interp.arg.defines += {"NDEBUG"};
        interp.arg.cflags  += {"/MT"};
        interp.arg.ldflags += {"libcmt.lib"};
    }

    //["optimization"]
    if (cfg["optimization"] == "debug") {
        interp.arg.cflags += {
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
        interp.arg.ldflags += {
            "/DEBUG:FULL", 
            "/INCREMENTAL"
        };
    }
    if (cfg["optimization"] == "release") {
        interp.arg.cflags += {
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
        interp.arg.ldflags += {
            "/OPT:ICF",
            "/DEBUG:FASTLINK",    // /Zi does imply /debug, the default is FASTLINK
            "/LTCG:incremental",  //TODO: using profile to guide optimization here (PGOptimize)
            "/RELEASE",           //sets the Checksum in the header of an .exe file.
        };
    }

    //["msvc_subsystem"]
    if (cfg["msvc_subsystem"] == "CONSOLE")
        interp.arg.ldflags += {"/SUBSYSTEM:CONSOLE"};
    if (cfg["msvc_subsystem"] == "WINDOW")
        interp.arg.ldflags += {"/SUBSYSTEM:WINDOW"};
    
    return interp;
} //TargetWorker::step1_win_msvc()

CxxToolchainInfo TargetWorker::step1_linux_gcc(cgn::Configuration &cfg)
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
    interp.extra_ldflags_so = {"-shared"};

    std::vector<std::string> cflags_1st, ldflags_1st;
    interp.arg.cflags += {
        "-I.",
        "-fno-common",
        "-fdiagnostics-color=always",
        "-fvisibility=hidden",
        "-Wl,--exclude-libs,ALL"
    };
    interp.extra_cflags_c = {"-std=c17"};
    interp.extra_cflags_cpp = {"-std=c++17"};

    // using .def file to guide symbol expose
    // only valid for current target
    // if (dyn_def_file.empty())
    //     interp.arg.cflags += {
    //         "-fvisibility=hidden",
    //     };
    interp.arg.ldflags += {
        "-L.",
        "-Wl,--warn-common", "-Wl,-z,origin", 
        "-Wl,--export-dynamic",  // force export from executable
        // "-Wl,--warn-section-align", 
        // "-Wl,-Bsymbolic", "-Wl,-Bsymbolic-functions",
    };

    //["os"]
    if (cfg["os"] == "linux") {
        interp.arg.cflags  += {"-fPIC","-pthread"};
        interp.arg.ldflags += {"-ldl", "-lrt", "-lpthread"};
    }

    //["optimization"]
    if (cfg["optimization"] == "debug") {
        interp.arg.defines += {"_DEBUG"};
        interp.arg.cflags += {
            "-Og", "-g", "-Wall", "-ggdb", "-O0",
            "-fno-eliminate-unused-debug-symbols", 
            "-fno-eliminate-unused-debug-types"};
        interp.extra_cflags_cpp.push_back("-ftemplate-backtrace-limit=0");
    }
    if (cfg["optimization"] == "release")
        interp.arg.cflags += {"-O2", "-flto", "-fwhole-program"};
    
    //["cxx_sysroot"]
    if (cfg["cxx_sysroot"] != "")
        interp.arg.cflags += {
            "--sysroot=" + (std::string)cfg["cxx_sysroot"]
        };

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
        interp.arg.cflags  += {"-fsanitize=" + sanitizer};
        interp.arg.ldflags += {"-fsanitize=" + sanitizer};
    }

    return interp;
} //TargetWorker::step1_linux_gcc()


CxxToolchainInfo TargetWorker::step1_linuxllvm_and_xcode(cgn::Configuration &cfg)
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
    interp.extra_ldflags_so = {"-shared"};
    if (cfg["os"] == "linux") {
        interp.exe_ar     = (prefix + "llvm-ar");
        interp.exe_solink = (prefix + "clang++");
        interp.extra_ldflags_so = {"-fuse-ld=lld", "-shared"};
        interp.exe_xlink = (prefix + "clang++");
        interp.extra_ldflags_x = {"-fuse-ld=lld"};
    }
    interp.is_compiler_controlled_link = true;

    interp.arg.cflags += {
        "-fvisibility=hidden",
        "-fno-common",
        "-fcolor-diagnostics", "-Wreturn-type", 
        "-I.", "-fPIC", "-pthread"};
    
    interp.arg.ldflags += {
        "-L.",
        "-lpthread"
    };
    if (cfg["os"] == "linux") {
        if (cfg["cxx_asan"] == "")
            interp.arg.ldflags += {"-Wl,--warn-common"};
        interp.arg.ldflags += {"-Wl,--warn-backrefs", "-lrt"};
    }
    if (cfg["os"] == "mac") //for macos : using warn-commons instead of warn-common
        interp.arg.ldflags += {"-fprofile-instr-generate", "-Wl,-warn_commons"};

    interp.arg.defines += {"_GNU_SOURCE"};
    interp.extra_cflags_c = {"-std=c17"};
    interp.extra_cflags_cpp = {"-std=c++17"};

    //["optimization"]
    if (cfg["optimization"] == "debug") {
        interp.arg.defines += {"_DEBUG"};
        interp.arg.cflags += {
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
        interp.arg.cflags += {"-O3", "-flto"};
        interp.arg.ldflags += {
            "-flto", "-Wl,--exclude-libs=ALL", "-Wl,--discard-all",
            // "-Wl,--thinlto-jobs=0", 
            // "-Wl,--thinlto-cache-dir=./thinlto_cache", 
            // "-Wl,--thinlto-cache-policy,cache_size_bytes=1g",
            "-Wl,--warn-unresolved-symbols"
        };
    }

    //["llvm_stl"]
    if (cfg["llvm_stl"] == "libc++")
        interp.arg.cflags += {"-stdlib=libc++"};

    //["cxx_sysroot"]
    if (cfg["cxx_sysroot"] != "")
        interp.arg.cflags += {
            "--sysroot=" + (std::string)(cfg["cxx_sysroot"])};

    //llvm cross compile argument
    if (cfg["os"] != cfg["host_os"] || cfg["cpu"] != cfg["host_cpu"]) {
        std::string cpu = cfg["cpu"];
        if (cpu == "x86_64")
            cpu = "amd64";
        if (cpu == "arm64")
            cpu = "aarch64";
        interp.arg.cflags += {"--target=" + cpu + "-pc-" + (std::string)cfg["os"]};
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
        interp.arg.cflags  += {"-fsanitize=" + sanitizer};
        if (cfg["optimization"] != "debug")
            interp.arg.cflags += {"-fno-omit-frame-pointer"};
        interp.arg.ldflags += {"-fsanitize=" + sanitizer};
    }

    return interp;
} //TargetWorker::step1_linuxllvm_and_xcode()

bool TargetWorker::step2_opt_confirm(const CxxToolchainInfo &_interp)
{
    // == pkg mode optimization ==
    if (x.cfg["pkg_mode"] != "") {
        auto cfg_without_pkg = x.cfg;
        cfg_without_pkg["pkg_mode"] = "";
        no_pkgmode_alter_target = x.quick_dep(mk->label, cfg_without_pkg, false);
    }

    // === opt confirm ===
    // for function two_escape()
    x.opt->cfg.visit_keys({"host_shell"});
    mk = x.opt->confirm();
    if (!mk)
        return true;

    // === merge args from target(x), deps, interpreter ===
    this->interp = _interp;
    interp.exe_cc     = two_escape(interp.exe_cc);
    interp.exe_cxx    = two_escape(interp.exe_cxx);
    interp.exe_asm    = two_escape(interp.exe_asm);
    interp.exe_solink = two_escape(interp.exe_solink);
    interp.exe_xlink  = two_escape(interp.exe_xlink);
    interp.exe_ar     = two_escape(interp.exe_ar);

    carg.cflags = two_escape(interp.arg.cflags) 
                + two_escape(x._cxx_to_self.cflags) 
                + two_escape(x.cflags);
    carg.ldflags = two_escape(interp.arg.ldflags) 
                 + two_escape(x._cxx_to_self.ldflags) 
                 + two_escape(x.ldflags);
    carg.defines = x.defines + x._cxx_to_self.defines + interp.arg.defines;
    [&](const std::vector<cgn::CGNPath> &ls) {
        for (auto &it : ls)
            carg_include_dirs.push_back(two_escape(
                api.rebase_path(it, ".", x.opt) ));
    } (x.include_dirs + x._cxx_to_self.include_dirs + interp.arg.include_dirs);

    // cpp define priority merge : TODO
    // auto def_priority_merge = [](StrSet priority, StrSet candidate) {};

    // auto def_to_cflag = [](CxxInfo &inf) {
    //     for (auto &def : inf.defines)
    //         inf.cflags += {"/D" + cgn::Tools::shell_escape(def)};
    //     for (auto &dir : inf.include_dirs)
    //         inf.cflags += {"/I" + cgn::Tools::shell_escape(dir)};
    // };
    // def_to_cflag(x);
    // def_to_cflag(x.pub); //x.pub don't need to modify anymore.

    // clear duplicate include folder, .obj files from dep
    cgn::Tools::remove_duplicate_inplace(carg_include_dirs);
    cgn::Tools::remove_duplicate_inplace(x._lnr_to_self.object_files);
    cgn::Tools::remove_duplicate_inplace(x._lnr_to_self.static_files);
    cgn::Tools::remove_duplicate_inplace(x._lnr_to_self.shared_files);

    // === generate result ===
    // Merge infos from Context::_pub_infos
    mk->merge_from(x._pub_infos);
    mk->ninja_dep_level = x._max_pub_ninja_level;

    // init CxxInfo and LinkAndRunInfo for return value
    auto rvcxx = mk->get<CxxInfo>(true);
    rvcxx->cflags  += x.pub.cflags;
    rvcxx->ldflags += x.pub.ldflags;
    rvcxx->defines += x.pub.defines;
    rvcxx->include_dirs = x.pub.include_dirs + rvcxx->include_dirs;
    api.convert_cgnpath_to_working_root_inplace(rvcxx->include_dirs, x.opt);
    // cgn::Tools::remove_duplicate_inplace(rvcxx->include_dirs);

    rvlnr = mk->get<cgn::LinkAndRunInfo>(true);
    cgn::Tools::remove_duplicate_inplace(rvlnr->object_files);
    
    escaped_ninja_entry = cgn::NinjaFile::escape_path(mk->ninja_entry);

    // include rule.ninja
    constexpr const char *rule_ninja = "@cgn.d//library/cxx/cxx_rule.ninja";
    static std::string rule_path = api.get_filepath(rule_ninja);
    mk->ninja->append_include(rule_path);

    // add common variables
    if (x.quickdep_ninja_target.size()) {
        mk->ninja->append_variable("target_dep", 
            list2str(cgn::NinjaFile::escape_path(x.quickdep_ninja_target)));
        ninjavar_target_dep = {"$target_dep"};
    }

    mk->ninja->append_variable("obj_cflags_c",
        list2str(interp.extra_cflags_c) 
        + list2str(carg.cflags)
        + list2str(carg_include_dirs, "/I")
        + list2str(carg.defines, "/D")
    );

    mk->ninja->append_variable("obj_cflags_cpp",
        list2str(interp.extra_cflags_cpp) 
        + list2str(carg.cflags)
        + list2str(carg_include_dirs, "/I")
        + list2str(carg.defines, "/D")
    );
    return false;

} // TargetWorker::step2_opt_confirm()

void TargetWorker::step31_win()
{
    std::string ccenv;
    // === add setenv batch dependency before cc.exe run ===
    if (interp.env_loader_script.size())
        ccenv = "cmd.exe /c " + two_escape(interp.env_loader_script) + " && ";
    if (interp.env_loader_script_anode)
        api.add_adep_edge(interp.env_loader_script_anode, mk->anode);
    auto add_ccenv_njdep = [&](cgn::NinjaFile::BuildSection *build_field){
        if (interp.env_loader_script.size())
            build_field->implicit_inputs += {cgn::NinjaFile::escape_path(
            interp.env_loader_script)};
    };

    // patch for .lib in windows : 
    //   if field->inputs empty, lib.exe would not generate any files,
    //   so here we feed a empty source file here.
    if (x.role == 'a' && x.srcs.empty() && x._lnr_to_self.object_files.empty())
        x.srcs = {cgn::make_path_base_working("@cgn.d//library/cxx/vsenv_loader/empty_file.c")};

    std::string def_file;

    // build.ninja : source file => .o
    std::string pdbfile = mk->out_prefix + "__vc.pdb";
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;
    for (auto &_file : x.srcs) {
        std::string file = api.rebase_path(_file, ".", mk);
        std::string path_in, path_out;
        auto file_type = src_path_convert(file, mk, &path_in, &path_out, true);
        if (file_type == 0)
            continue;
        if (file_type == 'D') {
            def_file = path_in;
            continue;
        }
        //field->input has been moved into cflags
        cgn::NinjaFile::BuildSection field;
        field.outputs = {cgn::NinjaFile::escape_path(path_out)};
        field.implicit_inputs = {cgn::NinjaFile::escape_path(path_in)};
        field.order_only = ninjavar_target_dep;
        if (file_type == 'A') {
            field.rule = "msvc_ml";
            field.variables["cc"] = ccenv + interp.exe_asm;
            field.variables["cflags"] = list2str(interp.extra_cflags_asm);
        }else {
            field.rule = "msvc_cl";
            field.variables["cc"] = ccenv + interp.exe_cxx;
            field.variables["cflags"] = (file_type=='+'? "$obj_cflags_c":"$obj_cflags_cpp");
            field.variables["pdb"] = cgn::NinjaFile::escape_path(pdbfile);
        }

        add_ccenv_njdep(&field);

        // NinjaBuild bug DirtyPatch:
        // add ./ prefix of src filepath to avoid string starting with '@'
        // if we add "./" in ninja input file, ninja.exe would auto remove it 
        // when writing down to .rspfile, then it cause cl.exe parse it as
        // another rspfile.
        if (path_in.at(0) == '@')
            path_in = "." + mk->PATH_SEPARATOR + path_in;
        field.variables["cflags"] += "/c " + two_escape(path_in);

        // if (x.cfg["pkg_mode"] == "") {
        //     mk->ninja->append_build(field);
        //     CxxInterpreter::ninja_dedup->obj_regalt(opt, path_out, field);
        // }
        // else if (!CxxInterpreter::ninja_dedup->obj_tryomit(no_pkgmode_alter_target, &path_out, &field))
        //     mk->ninja->append_build(field);
        mk->ninja->append_build(field);
        obj_out.push_back(path_out);
        obj_out_ninja_esc.push_back(field.outputs[0]);
    }

    // build.ninja : cxx_sources()
    // cxx_sources() cannot process any field of LinkAndRunInfo
    // so add the .obj file generated by itself then return
    if (x.role == 'o') {
        _entry_postprocess(obj_out_ninja_esc);
        rvlnr->object_files = obj_out + rvlnr->object_files;
        return ;
    }

    // build.ninja : cxx_static()
    //  deps.obj + self.srcs.o => rv[LRinfo].a
    //  deps.rt / deps.so / deps.a => rv[LRinfo]
    if (x.role == 'a') {
        std::string outfile = mk->out_prefix + x.name + ".lib";
        if (x.perferred_binary_name.size())
            outfile = mk->out_prefix + x.perferred_binary_name;
        std::string outfile_njesc = cgn::NinjaFile::escape_path(outfile);
        auto *field = mk->ninja->append_build();
        field->rule = "msvc_lib";
        field->inputs = obj_out_ninja_esc
                      + cgn::NinjaFile::escape_path(x._lnr_to_self.object_files);
        field->outputs = {outfile_njesc};
        field->variables["libexe"] = ccenv + interp.exe_ar;
        field->variables["arflags"] = list2str(carg.arflags);
        if (def_file.size()) {
            field->variables["arflags"] += "/DEF:" + def_file + " ";
            field->implicit_inputs += {cgn::NinjaFile::escape_path(def_file)};
        }
        add_ccenv_njdep(field);

        _entry_postprocess(field->outputs);
        rvlnr->static_files = std::vector<std::string>{outfile} + rvlnr->static_files;
        return ;
    }
    
    // build.ninja : cxx_shared() / cxx_executable()
    //   deps.object + deps.static + self.srcs.o => self.so / self.exe
    //   with carg.ldflags and -wholearchive:x._wholearchive_a
    //   self.so + {so from deps} => rv[LRinfo].so
    if (x.role == 's' || x.role == 'x') {
        std::string outfile_fname = x.name + (x.role=='s'? ".dll" :".exe");
        std::string outfile_implib = mk->out_prefix + x.name + ".lib";
        if (x.perferred_binary_name.size()) {
            outfile_fname = x.perferred_binary_name;
            outfile_implib = mk->out_prefix + outfile_fname + ".lib";
        }
        std::string outfile = mk->out_prefix + outfile_fname;
        
        //prepare rpath argument
        //  this is seen as target ldflags, so put on the tail of cargs.ldflags
        //TODO: manifest and .runtime

        //generate ninja section
        // --start-group   : {all.obj} {dep.static without whole} -l{dep.shared}
        // --whole-archive : {static_files from inherit dep}
        auto *field = mk->ninja->append_build();
        field->rule = "msvc_link";
        field->inputs = obj_out_ninja_esc 
                      + cgn::NinjaFile::escape_path(x._lnr_to_self.object_files)
                      + cgn::NinjaFile::escape_path(x._lnr_to_self.static_files) 
                      + cgn::NinjaFile::escape_path(x._lnr_to_self.shared_files)
                      + cgn::NinjaFile::escape_path(x._wholearchive_a);
        field->outputs = {cgn::NinjaFile::escape_path(outfile)};
        field->variables["restat"] = "1";
        if (x.role == 's') //only add .lib for .dll
            field->implicit_outputs = {cgn::NinjaFile::escape_path(outfile_implib)};
        field->variables["link"] = ccenv + (x.role=='s'? interp.exe_solink:interp.exe_xlink);
        field->variables["ldflags"] = list2str(carg.ldflags)
                + list2str(x.role=='s'? interp.extra_ldflags_so : interp.extra_ldflags_x)
                + list2str(two_escape(x._wholearchive_a), "/WHOLEARCHIVE:");
        if (def_file.size()) {
            field->variables["ldflags"] += "/DEF:" + def_file + " ";
            field->implicit_inputs += {cgn::NinjaFile::escape_path(def_file)};
        }
        add_ccenv_njdep(field);           
        
        // generate entry
        _entry_postprocess(field->outputs);

        // copy runtime when cxx_executable() and not pkg_mode
        if (x.role == 'x' && x.cfg["pkg_mode"] == "")
            for (auto &one_entry : x._lnr_to_self.runtime_files) {
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

        // put front
        rvlnr->shared_files = std::vector<std::string>{outfile_implib} 
                            + rvlnr->shared_files;
        
        rvlnr->runtime_files[cgn::make_path_base_out(outfile_fname)] = outfile;

        mk->outputs = {outfile, outfile_implib};
        // mk->ninja_dep_level = x._max_pub_ninja_level;
    } // if (role=='s' or 'x')

} //TargetWorker::step31_win()


void TargetWorker::step31_unix()
{
    //=== Section 3: make ninja file ===
    // common for both GCC and LLVM
    // build.ninja : source file => .o
    // field.input and field.output : need ninja escape, instead of shell esacpe
    // carg.cflags and carg.ldflags : already two escaped
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;
    std::string dyn_def_file;
    for (auto &_ss : x.srcs) {
        std::string ss = api.rebase_path(_ss, ".", mk);
        // if (ss[0] == '.' && ss[1] == '.')
        //     ss = api.abspath(ss);  // using abspath to present file outside project
        std::string path_in, path_out;
        auto file_type = src_path_convert(ss, mk, &path_in, &path_out);
        if (file_type == 0)
            continue;

        if (file_type == 'D') {
            dyn_def_file = path_in;
        }else {
            cgn::NinjaFile::BuildSection field;
            // auto *field = mk->ninja->append_build();
            field.rule = "gcc";
            field.inputs  = {cgn::NinjaFile::escape_path(path_in)};
            field.outputs = {cgn::NinjaFile::escape_path(path_out)};
            field.order_only = ninjavar_target_dep;
            field.variables["cflags"] = list2str(carg.cflags);
            if (file_type == '+') {
                field.variables["cc"] = interp.exe_cxx;
                field.variables["cflags"] += list2str(interp.extra_cflags_cpp);        
            }else if (file_type == 'A') {
                field.variables["cc"] = interp.exe_asm;
                field.variables["cflags"] += list2str(interp.extra_cflags_asm);
            }else {
                field.variables["cc"] = interp.exe_cc;
                field.variables["cflags"] += list2str(interp.extra_cflags_c);
            }
            field.variables["cflags"] += list2str(carg_include_dirs, "-I")
                                        + list2str(carg.defines, "-D");
            
            // if (x.cfg["pkg_mode"] == "") {
            //     mk->ninja->append_build(field);
            //     CxxInterpreter::ninja_dedup->obj_regalt(opt, path_out, field);
            // }
            // else if (!CxxInterpreter::ninja_dedup->obj_tryomit(no_pkgmode_alter_target, &path_out, &field))
            //     mk->ninja->append_build(field);
            mk->ninja->append_build(field);
            obj_out.push_back(path_out);
            obj_out_ninja_esc.push_back(field.outputs[0]);
        }
    }

    // build.ninja : cxx_sources()
    // cxx_sources() cannot process any field of LinkAndRunInfo
    // so add the .obj file generated by itself then return
    if (x.role == 'o') {
        _entry_postprocess(obj_out_ninja_esc);
        rvlnr->object_files = obj_out + rvlnr->object_files;
        return ;
    }

    // build.ninja : cxx_static()
    //  deps.obj + self.srcs.o => rv[LRinfo].a
    //  deps.rt / deps.so / deps.a => rv[LRinfo]
    if (x.role == 'a') {
        std::string outfile = mk->out_prefix + "lib" + x.name + ".a";
        if (x.perferred_binary_name.size())
            outfile = mk->out_prefix + x.perferred_binary_name;
        std::string outfile_njesc = cgn::NinjaFile::escape_path(outfile);
        cgn::NinjaFile::BuildSection field;
        // auto *field = mk->ninja->append_build();
        field.rule = "gcc_ar";
        field.inputs = obj_out_ninja_esc
                     + cgn::NinjaFile::escape_path(x._lnr_to_self.object_files);
        field.outputs = {outfile_njesc};
        field.variables["exe"] = interp.exe_ar;
        field.variables["arflags"] = list2str(carg.arflags);
        // if (x.cfg["pkg_mode"] == "") {
        //     mk->ninja->append_build(field);
        //     CxxInterpreter::ninja_dedup->obj_regalt(opt, outfile, field);
        // }
        // else if (!CxxInterpreter::ninja_dedup->obj_tryomit(no_pkgmode_alter_target, &outfile, &field))
        //     mk->ninja->append_build(field);
        mk->ninja->append_build(field);
        _entry_postprocess(field.outputs);
        rvlnr->static_files = std::vector<std::string>{outfile} + rvlnr->static_files;

        mk->outputs = {outfile};
        return ;
    }
    
    // currently we have 3 types of linker in unix-like world
    //  * linux: gnu-ld (binutils)
    //  * linux: llvm-ld
    //  * mac:   bsd-ld (os-internal)

    // build.ninja : cxx_shared() / cxx_executable()
    //   deps.object + deps.static + self.srcs.o => self.so / self.exe
    //   with carg.ldflags and -wholearchive:x._wholearchive_a
    //   self.so + {so from deps} => rv[LRinfo].so
    if (x.role == 's' || x.role == 'x') {
        std::string filename;
        std::string outfile;
        std::string outfile_njesc;
        if (x.role == 's')
            outfile = mk->out_prefix + (filename = "lib" + x.name + ".so");
        else
            outfile = mk->out_prefix + (filename = x.name);
        if (x.perferred_binary_name.size())
            outfile = mk->out_prefix + (filename = x.perferred_binary_name);
        outfile_njesc = cgn::NinjaFile::escape_path(outfile);

        //prepare rpath argument
        //  this is seen as target ldflags, so put on the tail of cargs.ldflags
        if (x.cfg["os"] == "linux") {
            if (x.cfg["pkg_mode"] != "") {
                carg.ldflags += {
                    "-Wl,--enable-new-dtags", 
                    two_escape("-Wl,-rpath=$ORIGIN")
                };
                rvlnr->runtime_files[
                    cgn::make_path_base_out(filename)
                ] = outfile;
            }
            else {
                carg.ldflags += {"-Wl,--enable-new-dtags"};
                for (auto &so : x._lnr_to_self.shared_files) {
                    auto path1    = cgn::Tools::parent_path(so);
                    auto path_rel = cgn::Tools::rebase_path(path1, mk->out_prefix);
                    carg.ldflags += {two_escape("-Wl,--rpath=$ORIGIN/" + path_rel)};
                }
            }

            if (dyn_def_file.size())
            //     carg.ldflags += {"-Wl,--version-script=" + dyn_def_file};
                carg.ldflags += {"-Wl,--export-dynamic-symbol-list=" + dyn_def_file};
                // carg.ldflags += {"-Wl,-exported_symbols_list,\"" + dyn_def_file + "\""};
        }

        //generate ninja section
        // --start-group   : {all.obj} {dep.static without whole} -l{dep.shared}
        // --whole-archive : {static_files from inherit dep}
        auto *field = mk->ninja->append_build();
        field->rule = "crun_rsp";
        field->inputs = obj_out_ninja_esc 
                      + cgn::NinjaFile::escape_path(x._lnr_to_self.object_files);
        field->implicit_inputs = cgn::NinjaFile::escape_path(x._lnr_to_self.static_files) 
                               + cgn::NinjaFile::escape_path(x._lnr_to_self.shared_files)
                               + cgn::NinjaFile::escape_path(x._wholearchive_a);
        field->outputs = {outfile_njesc};
        field->variables["exe"] = cgn::NinjaFile::escape_path(
                                    x.role=='s'? interp.exe_solink:interp.exe_xlink);
        
        std::string buildstr_a, buildstr_start_group, buildstr_end_group;
        if (x.cfg["cxx_toolchain"] == "xcode")
            buildstr_a = " " + list2str(two_escape(x._wholearchive_a), "-Wl,-force_load ");
        else{
            if (x._wholearchive_a.size())
                buildstr_a = " -Wl,--whole-archive " 
                           + list2str(two_escape(x._wholearchive_a))
                           + "-Wl,--no-whole-archive";
            buildstr_start_group = "-Wl,--start-group";
            buildstr_end_group   = "-Wl,--end-group";
        }
        field->variables["args"] = list2str(carg.ldflags)
            + list2str(x.role=='s'? interp.extra_ldflags_so : interp.extra_ldflags_x)
            + "-o " + api.shell_escape(field->outputs[0], x.cfg["host_shell"])
            + buildstr_a + " "
            + buildstr_start_group + " "
            + list2str(two_escape(obj_out))
            + list2str(two_escape(x._lnr_to_self.object_files))
            + list2str(two_escape(x._lnr_to_self.static_files))
            + list2str(two_escape(x._lnr_to_self.shared_files), "-l:")
            + buildstr_end_group;
        field->variables["desc"] = "LINK " + outfile_njesc;
        
        // generate entry
        _entry_postprocess(field->outputs);

        // copy runtime when cxx_executable()
        if (x.role == 'x')
            for (auto &one_entry : x._lnr_to_self.runtime_files) {
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

        // put front
        rvlnr->shared_files = std::vector<std::string>{outfile} + rvlnr->shared_files;

        mk->outputs = {outfile};
        // mk->ninja_dep_level = x._max_pub_ninja_level;
        return ;
    } // if (role=='s' or 'x')
} //TargetWorker::step31_unix()


void TargetWorker::_entry_postprocess(const std::vector<std::string> &to)
{
    auto *entry = mk->ninja->append_build();
    entry->rule = "phony";
    entry->inputs = to;
    entry->outputs = {escaped_ninja_entry};
    entry->order_only = ninjavar_target_dep;
}

void CxxInterpreter::interpret(context_type &x)
{
    // preprocess "x.srcs = file_glob(*)"
    // if the source file is not at the same/sub folder of BUILD.cgn.cc
    // using absolutely path to locate.
    // BUG HERE: file_glob(*) cannot found file newly added (in ninja cache)
    //     TODO: target with file_glob() would re-analyse each time.
    //           so we should use external executable to generate ninja dyndep.
    //           @cgn.d//library/advtools
    std::vector<cgn::CGNPath> real_srcs;
    for (auto &ss : x.srcs) {
        if (ss.type == ss.BASE_ON_OUTPUT)
            return x.opt->set_fail("Wrong path in x.src[] " + ss.to_string());
        if (ss.rpath.find('*') == ss.rpath.npos) //if not file_glob
            real_srcs += {ss};
        else {
            std::string path2 = api.rebase_path(ss, ".", x.opt);
            for (const auto &it : api.file_glob(path2, ".")) {
                real_srcs.push_back(cgn::make_path_base_working(it));
            }
        }
    }
    std::swap(x.srcs, real_srcs);

    // start interpret
    TargetWorker w(x);
    if (x.cfg["cxx_toolchain"] == "msvc") {
        assert(x.cfg["os"] == "win");
        auto interp = w.step1_win_msvc(x.cfg);
        if (w.step2_opt_confirm(interp))
            return ;
        w.step31_win();
    }
    else if (x.cfg["cxx_toolchain"] == "gcc" && x.cfg["os"] == "linux") {
        auto interp = w.step1_linux_gcc(x.cfg);
        if (w.step2_opt_confirm(interp))
            return ;
        w.step31_unix();
    }
    else if (
        (x.cfg["cxx_toolchain"] == "xcode" && x.cfg["os"] == "mac") ||
        (x.cfg["cxx_toolchain"] == "llvm"  && x.cfg["os"] == "linux")
    ) {
        auto interp = w.step1_linuxllvm_and_xcode(x.cfg);
        if (w.step2_opt_confirm(interp))
            return ;
        w.step31_unix();
    }
    else
        x.opt->set_fail("Unsupported cxx_toolchain " + (std::string)x.cfg["cxx_toolchain"]);
}

// std::unique_ptr<NinjaDedup> CxxInterpreter::ninja_dedup = std::unique_ptr<NinjaDedup>(new NinjaDedup);

CxxContext::CxxContext(char role, cgn::CGNTargetOpt *opt)
: cgn::QuickDepContext(opt), role(role), name(opt->name), cfg(opt->cfg) {}

cgn::CGNTarget CxxContext::add_dep(
    const std::string &label, cgn::Configuration new_cfg, DepType flag
) {
    // merge TargetInfos[] manually
    cgn::CGNTarget early = quick_dep(label, cfg, false);
    if (early.errmsg.size()) // return if error occured.
        return early;

    // cxx::order_dep
    // Ignore remote CGNTarget value and return.
    if (flag == cxx::order_dep)
        return early;

    // if (flag & cxx::inherit)
    //     _max_pub_ninja_level = std::max(_max_pub_ninja_level, early.ninja_dep_level);

    // call merge() for unused field
    for (auto &rhs : early.data())
        if (rhs.first != "CxxInfo" && rhs.first != "LinkAndRunInfo")
            _pub_infos.merge_entry(rhs.first, rhs.second.get());

    // rhs[CxxInfo]
    //   cxx::inherit : append to interpreter_rv[CxxInfo] as is, 
    //                  and also apply on current target.
    //   cxx::private : save to _cxx_to_self to use for current target only.
    if ((flag & cxx::inherit))
        _pub_infos.merge_entry(early.get<CxxInfo>(false));

    _cxx_to_self.merge_entry(early.get<CxxInfo>(false));

    // rhs[LinkAndRunInfo]
    auto *r_lnr = early.get<cgn::LinkAndRunInfo>(false);

    // for both msvc and GNU
    // cxx_sources() : keep as is (do not consume anyone)
    // cxx_static()  : move(r_lnr.obj) to cmd "ar rcs" later in interpreter, 
    //                 keep others as-is.
    if (role == 'o')
        _pub_infos.get<cgn::LinkAndRunInfo>(true)->merge_entry(r_lnr);
    if (role == 'a') {
        if (flag & pack_obj)
            _lnr_to_self.object_files += std::move(r_lnr->object_files);
        _pub_infos.get<cgn::LinkAndRunInfo>(true)->merge_entry(r_lnr);
    }
    
    // cxx_executable() and cxx_shared() for both msvc and GNU
    //   r_lnk.obj and r_lnk.a would use by current target interpreter for (cxx::private_dep)
    //   r_lnk.obj and r_lnk.a would both export since visiblity(hidden) when (cxx::inherit)
    //   r_lnk.a with wholearchive (cxx::pack_obj) do not storage into _lnr_to_self
    //   r_lnk.so link to current one
    //   r_lnk.rt processed later in interpreter
    if (r_lnr && (role == 's' || role == 'x')) {
        // Record 'wholearchive' in the separator along with 'dep.static'
        if (flag & cxx::pack_obj)
            _wholearchive_a += std::move(r_lnr->static_files);
        _lnr_to_self.merge_entry(r_lnr);

        // If the pack_obj special flag is set, the current object files are 
        // not exposed, even if the inherit flag is set. In this case, inherit 
        // only affects the CxxInfo field.
        if (flag & cxx::pack_obj)
            r_lnr->object_files.clear();

        // special for cxx_executable(), consume all runtime_files from deps
        // transferr the runtime_files[] when pkg_mode flag assigned
        if (role == 'x' && cfg["pkg_mode"] == "")
            r_lnr->runtime_files.clear();
        else{
            auto &pubrt = _pub_infos.get<cgn::LinkAndRunInfo>(true)->runtime_files;
            pubrt.insert(std::make_move_iterator(r_lnr->runtime_files.begin()), 
                         std::make_move_iterator(r_lnr->runtime_files.end()));
            r_lnr->runtime_files.clear();
        }
            
        if (flag & cxx::inherit)
            _pub_infos.get<cgn::LinkAndRunInfo>(true)->merge_entry(r_lnr);

    }

    return early;
} //CxxContext::add_dep

CxxToolchainInfo TargetWorker::step1_minimum(cgn::Configuration &cfg)
{
    CxxToolchainInfo rv;
    std::string prefix = cfg["cxx_prefix"];
    if (cfg["cxx_toolchain"] == "gcc") {
        rv.exe_cc = rv.exe_asm = rv.exe_solink = rv.exe_xlink = prefix + "gcc";
        rv.exe_cxx = prefix + "g++";
        rv.exe_ar  = prefix + "ar";
        rv.is_compiler_controlled_link = true;
    }
    if (cfg["cxx_toolchain"] == "llvm") {
        rv.exe_cc = rv.exe_asm = rv.exe_solink = rv.exe_xlink = prefix + "clang";
        rv.exe_cxx = prefix + "clang++";
        rv.exe_ar  = prefix + "ar";
        rv.extra_ldflags_so = {"-shared"};
        rv.is_compiler_controlled_link = true;
    }
    if (cfg["cxx_toolchain"] == "xcode") {
        rv.exe_cc = rv.exe_asm = rv.exe_solink = rv.exe_xlink = prefix + "clang";
        rv.exe_cxx = prefix + "clang++";
        rv.exe_ar  = prefix + "ar";
        rv.extra_ldflags_so = {"-shared"};
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
        rv.extra_ldflags_so = {"/DLL"};
        rv.exe_asm = prefix + (cfg["host_cpu"]=="x86"? "ml.exe":"ml64.exe");
        rv.is_compiler_controlled_link = false;
        // rv.arg.defines += {
        //     "WINVER=" + std::string{mimimum_winver},
        //     "_WIN32_WINNT=" + std::string{mimimum_winver},
        //     (cfg["msvc_runtime"] == "MDd"? "_DEBUG" : "NDEBUG")
        // };
        // rv.arg.cflags = {
        //     "/utf-8", "/wd4828",   // illegal character in UTF-8
        //     "/EHsc",               // Enables standard C++ stack unwinding
        //     (cfg["msvc_runtime"] == "MDd"? "/MDd" : (
        //      cfg["msvc_runtime"] == "MD"?  "/MD" : (
        //      cfg["msvc_runtime"] == "MTd"? "/MT": "/MTd")))
        // };
    }

    if (cfg["os"] == "linux")
        rv.arg.cflags = {"-fPIC","-pthread"};

    return rv;
}

CxxToolchainInfo CxxInterpreter::test_param(
    cgn::Configuration &cfg, const std::string &type
) {
    if (type == "default" || type == "makefile") {
        if (cfg["cxx_toolchain"] == "msvc" && cfg["host_os"] == "win")
            return TargetWorker::step1_win_msvc(cfg);
        if (cfg["cxx_toolchain"] == "gcc" && cfg["host_os"] == "linux")
            return TargetWorker::step1_linux_gcc(cfg);
        if (
            (cfg["cxx_toolchain"] == "xcode" && cfg["os"] == "mac") ||
            (cfg["cxx_toolchain"] == "llvm"  && cfg["os"] == "linux")
        )   return TargetWorker::step1_linuxllvm_and_xcode(cfg);
    }
    else if (type == "minimum" || type == "cmake")
        return TargetWorker::step1_minimum(cfg);
    else
        throw std::runtime_error{"CxxInterpreter::test_param() unsupported type " + type};

    return {};
}


// CxxPrebuiltInterpreter
// ----------------------

void CxxPrebuiltInterpreter::interpret(context_type &x)
{
    // std::string dllbase = mk->out_prefix;
    // if (x.runtime_dir.size() && (x.cfg["pkg_mode"] == "T" || x.cfg["os"] == "win"))
    //     dllbase += x.runtime_dir + opt.path_separator;

    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;

    // result[CxxInfo]
    for (auto &it : x.pub.include_dirs)
        it = api.convert_cgnpath_to_working_root(it, x.opt);
    mk->merge_entry(&x.pub);

    // TargetInfos[LinkAndRunInfo]
    cgn::LinkAndRunInfo *lrinfo = mk->get<cgn::LinkAndRunInfo>(true);
    bool have_sofile = false;
    std::unordered_set<std::string> dllstem;
    std::vector<std::pair<std::string,std::string>> dotlib;
    for (auto file : x.files) {
        auto fd1   = file.rpath.rfind('/');
        auto fddot = file.rpath.rfind('.');
        fd1 = (fd1 == file.rpath.npos? 0: fd1+1);
        if (fddot == file.rpath.npos || fddot < fd1)
            continue;
        std::string stem = file.rpath.substr(fd1, fddot-fd1);
        std::string ext  = file.rpath.substr(fddot);
        // std::string fullp = api.locale_path(opt->src_prefix + file);
        std::string fullp = api.rebase_path(file, ".", mk);
        if (ext == ".so")
            lrinfo->shared_files.push_back(fullp);
        else if (ext == ".a")
            lrinfo->static_files.push_back(fullp);
        else if (ext == ".dll") {
            lrinfo->runtime_files[cgn::make_path_base_out(stem + ".dll")] = fullp;
            dllstem.insert(stem);
        }
        else if (ext == ".lib")
            dotlib.push_back({stem, fullp});
        else
            lrinfo->runtime_files[cgn::make_path_base_out(stem + "." + ext)] = fullp;
    }
    for (auto item : dotlib)
        if (dllstem.count(item.first) != 0)
            lrinfo->shared_files.push_back(item.second);
        else
            lrinfo->static_files.push_back(item.second);

    // build.ninja
    if (mk->ninja) {
        auto *entry = mk->ninja->append_build();
        entry->rule = "phony";
        entry->order_only = mk->ninja->escape_path(x.quickdep_ninja_target);
        entry->outputs = {mk->ninja->escape_path(mk->ninja_entry)};
    }
}

} //namespace cxx