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

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

static std::vector<std::string> two_escape(std::vector<std::string> in) {
    for (auto &it : in)
        it = two_escape(it);
    return in;
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
// @param IN  file1   : filename in context.src
// @param IN  opt     : opt from interpreter
// @param OUT path_in : "input file relpath"
// @param OUT path_out: "output file relpath"
// @return type of src: A/+/C/0 (asm, c++, c, 0:igonre)
static char src_path_convert(
    std::string file1, const cgn::CGNTargetOpt &opt,
    std::string *path_in, std::string *path_out, bool dot_obj = false
) {
    // get extension
    auto fd_slash = file1.rfind('/');
    auto fd = file1.rfind('.');
    if (fd == file1.npos || (fd_slash != file1.npos && fd < fd_slash))
        return 0;  // no extension, cpp header
    std::string left = file1.substr(0, fd);
    std::string ext = lower_substr(file1, fd+1);

    auto gen = [&]() {
        *path_in = api.rebase_path(file1, ".", opt.src_prefix);

        // since path_in and opt.out_prefix is not in same driver in windows,
        // using rebase_path() can only get the abspath of path_in and it's 
        // hard to check.
        bool start_with_outprefix = (path_in->size() > opt.out_prefix.size()
            && memcmp(path_in->c_str(), opt.out_prefix.c_str(), opt.out_prefix.size())==0);
        if (start_with_outprefix) {// path_in is inside out_prefix
            std::string probe1 = api.rebase_path(*path_in, opt.out_prefix);
            left = probe1.substr(0, probe1.rfind('.'));
            *path_out = opt.out_prefix + "__" + left + (dot_obj?".obj":".o");
        }
        else
            *path_out = cgn::Tools::locale_path(opt.out_prefix + "_" + left + (dot_obj?".obj":".o"));
    };
    
    // check current file is c/cpp source file
    if (ext == "def") {
        *path_in = cgn::Tools::locale_path(opt.src_prefix + file1);
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
    static constexpr const char *mimimum_winver = "0x0603";

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
    cgn::CGNTargetOpt *opt = nullptr;
    CxxToolchainInfo interp;
    CxxInfo &carg = interp.arg;
    std::vector<std::string> carg_include_dirs;
    cgn::LinkAndRunInfo &carg_link_file = x._lnr_to_self;
    bool step2_opt_confirm(const CxxToolchainInfo &interp);

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
    }
    if (cfg["cpu"] == "x86_64")
        interp.arg.defines += {"_AMD64_"};  
        interp.arg.ldflags += {"/MACHINE:X64"};

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
    interp.exe_ar  = (prefix + "gcc-ar");
    interp.exe_solink = (prefix + "g++") + " -shared";
    interp.exe_xlink  = (prefix + "g++");
    interp.is_compiler_controlled_link = true;

    std::vector<std::string> cflags_1st, ldflags_1st;
    interp.arg.cflags += {
        "-I.",
        "-fdiagnostics-color=always",
        "-fvisibility=hidden",
        "-Wl,--exclude-libs,ALL"
    };
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
    interp.exe_ar     = (prefix + "ar");
    interp.exe_solink = (prefix + "clang++");
    interp.extra_ldflags_so = {"-shared"};
    interp.exe_xlink  = (prefix + "clang++");
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
        "-fcolor-diagnostics", "-Wreturn-type", 
        "-I.", "-fPIC", "-pthread"};
    
    interp.arg.ldflags += {
        "-L.",
        "-lpthread"
    };
    if (cfg["os"] == "linux")
        interp.arg.ldflags += {"-Wl,--warn-common", "-Wl,--warn-backrefs", "-lrt"};
    if (cfg["os"] == "mac") //for macos : using warn-commons instead of warn-common
        interp.arg.ldflags += {"-fprofile-instr-generate", "-Wl,-warn_commons"};

    interp.arg.defines += {"_GNU_SOURCE"};
    interp.extra_cflags_c = {"-std=c17"};

    //["optimization"]
    if (cfg["optimization"] == "debug") {
        interp.arg.defines += {"_DEBUG"};
        interp.arg.cflags += {
            "-g", "-Wall", "-Wextra", "-Wno-unused-parameter",
            "-fno-omit-frame-pointer", "-fno-optimize-sibling-calls",
            "-ftemplate-backtrace-limit=0", "-fno-limit-debug-info",
            "-fstandalone-debug", "-fdebug-macro", "-glldb", //"-march=native",
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
            // "-Wl,--thinlto-cache-policy,cache_size_bytes=1g"
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
    interp.arg.cflags  += {"-fsanitize=" + sanitizer};
    interp.arg.ldflags += {"-fsanitize=" + sanitizer};

    return interp;
} //TargetWorker::step1_linuxllvm_and_xcode()

bool TargetWorker::step2_opt_confirm(const CxxToolchainInfo &_interp)
{
    // === opt confirm ===
    opt = x.opt->confirm();
    if (opt->cache_result_found)
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
    opt->result.merge_from(x._pub_infos);

    // init CxxInfo and LinkAndRunInfo for return value
    auto rvcxx = opt->result.get<CxxInfo>(true);
    rvcxx->cflags  += x.pub.cflags;
    rvcxx->ldflags += x.pub.ldflags;
    rvcxx->defines += x.pub.defines;
    rvcxx->include_dirs = x.pub.include_dirs + rvcxx->include_dirs;
    api.convert_cgnpath_to_working_root_inplace(rvcxx->include_dirs, x.opt);
    // cgn::Tools::remove_duplicate_inplace(rvcxx->include_dirs);

    rvlnr = opt->result.get<cgn::LinkAndRunInfo>(true);
    cgn::Tools::remove_duplicate_inplace(rvlnr->object_files);
    
    escaped_ninja_entry = cgn::NinjaFile::escape_path(
                          opt->out_prefix + opt->BUILD_ENTRY);

    // include rule.ninja
    constexpr const char *rule_ninja = "@cgn.d//library/cxx.cgn.bundle/cxx_rule.ninja";
    static std::string rule_path = api.get_filepath(rule_ninja);
    opt->ninja->append_include(rule_path);

    return false;

} // TargetWorker::step2_opt_confirm()


void TargetWorker::step31_win()
{
    std::string dyn_def_file;

    // build.ninja : source file => .o
    std::string pdbfile = opt->out_prefix + "__vc.pdb";
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;
    for (auto &file : x.srcs) {
        std::string path_in, path_out;
        auto file_type = src_path_convert(file, *opt, &path_in, &path_out, true);
        if (file_type == 0)
            continue;
        //field->input has been moved into cflags
        auto *field = opt->ninja->append_build();
        field->outputs = {cgn::NinjaFile::escape_path(path_out)};
        field->implicit_inputs = {cgn::NinjaFile::escape_path(path_in)};
        field->implicit_inputs += opt->quickdep_ninja_full;
        field->order_only      = opt->quickdep_ninja_dynhdr;
        if (file_type == 'D') {
            dyn_def_file = path_in;
        }else if (file_type == 'A') {
            field->rule = "msvc_ml";
            field->variables["cc"] = interp.exe_asm;
            field->variables["cflags"] = list2str(interp.extra_cflags_asm);
        }else {
            field->rule = "msvc_cl";
            field->variables["cc"] = interp.exe_cxx;
            field->variables["cflags"] = 
                list2str(file_type=='+'? interp.extra_cflags_cpp:interp.extra_cflags_c) 
                + list2str(carg.cflags)
                + list2str(carg_include_dirs, "/I")
                + list2str(carg.defines, "/D");
            field->variables["pdb"] = opt->ninja->escape_path(pdbfile);
        }

        // NinjaBuild bug DirtyPatch:
        // add ./ prefix of src filepath to avoid string starting with '@'
        // if we add "./" in ninja input file, ninja.exe would auto remove it 
        // when writing down to .rspfile, then it cause cl.exe parse it as
        // another rspfile.
        if (path_in.at(0) == '@')
            path_in = "." + opt->path_separator + path_in;
        field->variables["cflags"] += "/c " + two_escape(path_in);

        obj_out.push_back(path_out);
        obj_out_ninja_esc.push_back(field->outputs[0]);
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
        std::string outfile = opt->out_prefix + x.name + ".lib";
        if (x.perferred_binary_name.size())
            outfile = opt->out_prefix + x.perferred_binary_name;
        std::string outfile_njesc = cgn::NinjaFile::escape_path(outfile);
        auto *field = opt->ninja->append_build();
        field->rule = "msvc_lib";
        field->inputs = obj_out_ninja_esc
                      + cgn::NinjaFile::escape_path(x._lnr_to_self.object_files);
        field->outputs = {outfile_njesc};
        field->variables["libexe"] = interp.exe_ar;

        _entry_postprocess(field->outputs);
        rvlnr->static_files = std::vector<std::string>{outfile} + rvlnr->static_files;
        return ;
    }
    
    // build.ninja : cxx_shared() / cxx_executable()
    //   deps.object + deps.static + self.srcs.o => self.so / self.exe
    //   with carg.ldflags and -wholearchive:x._wholearchive_a
    //   self.so + {so from deps} => rv[LRinfo].so
    if (x.role == 's' || x.role == 'x') {
        std::string outfile;
        std::string outfile_implib;
        if (x.role == 's')
            outfile = opt->out_prefix + x.name + ".dll";
        else
            outfile = opt->out_prefix + x.name + ".exe";
        outfile_implib = opt->out_prefix + x.name + ".lib";
        if (x.perferred_binary_name.size()) {
            outfile = opt->out_prefix + x.perferred_binary_name;
            outfile_implib = opt->out_prefix + x.perferred_binary_name + ".lib";
        }

        //prepare rpath argument
        //  this is seen as target ldflags, so put on the tail of cargs.ldflags
        //TODO: manifest and .runtime

        //generate ninja section
        // --start-group   : {all.obj} {dep.static without whole} -l{dep.shared}
        // --whole-archive : {static_files from inherit dep}
        auto *field = opt->ninja->append_build();
        field->rule = "msvc_link";
        field->inputs = obj_out_ninja_esc 
                      + opt->ninja->escape_path(x._lnr_to_self.object_files)
                      + opt->ninja->escape_path(x._lnr_to_self.static_files) 
                      + opt->ninja->escape_path(x._lnr_to_self.shared_files)
                      + opt->ninja->escape_path(x._wholearchive_a);
        field->outputs = {opt->ninja->escape_path(outfile)};
        if (x.role == 's') //only add .lib for .dll
            field->implicit_outputs = {opt->ninja->escape_path(outfile_implib)};
        field->variables["link"] = (x.role=='s'? interp.exe_solink:interp.exe_xlink);
        field->variables["ldflags"] = list2str(carg.ldflags)
                + list2str(x.role=='s'? interp.extra_ldflags_so : interp.extra_ldflags_x)
                + list2str(two_escape(x._wholearchive_a), "/WHOLEARCHIVE:");
                   
        // generate entry
        _entry_postprocess(field->outputs);

        // copy runtime when cxx_executable()
        if (x.role == 'x')
            for (auto &one_entry : x._lnr_to_self.runtime_files) {
                auto dst  = opt->out_prefix + one_entry.first;
                auto &src = one_entry.second;
                auto *cpfield = opt->ninja->append_build();
                //TODO: copy runtime by custom command (like symbolic-link)
                cpfield->rule    = "win_cp";
                cpfield->inputs  = {cgn::NinjaFile::escape_path(src)};
                cpfield->outputs = {cgn::NinjaFile::escape_path(dst)};
                field->order_only += cpfield->outputs;
                // entry->order_only += cpfield->outputs;
            }

        // put front
        rvlnr->shared_files = std::vector<std::string>{outfile_implib} 
                            + rvlnr->shared_files;

        opt->result.outputs = {outfile, outfile_implib};
        opt->result.ninja_dep_level = x._max_pub_ninja_level;
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
    for (auto &ss : x.srcs) {
        // if (ss[0] == '.' && ss[1] == '.')
        //     ss = api.abspath(ss);  // using abspath to present file outside project
        std::string path_in, path_out;
        auto file_type = src_path_convert(ss, *opt, &path_in, &path_out);
        if (file_type == 0)
            continue;

        if (file_type == 'D') {
            dyn_def_file = path_in;
        }else {
            auto *field = opt->ninja->append_build();
            field->rule = "gcc";
            field->inputs  = {cgn::NinjaFile::escape_path(path_in)};
            field->outputs = {cgn::NinjaFile::escape_path(path_out)};
            field->implicit_inputs = opt->quickdep_ninja_full;
            field->order_only      = opt->quickdep_ninja_dynhdr;
            field->variables["cflags"] = list2str(carg.cflags);
            if (file_type == '+') {
                field->variables["cc"] = interp.exe_cxx;
                field->variables["cflags"] += list2str(interp.extra_cflags_cpp);        
            }else if (file_type == 'A') {
                field->variables["cc"] = interp.exe_asm;
                field->variables["cflags"] += list2str(interp.extra_cflags_asm);
            }else {
                field->variables["cc"] = interp.exe_cc;
                field->variables["cflags"] += list2str(interp.extra_cflags_c);
            }
            field->variables["cflags"] += list2str(carg.cflags)
                                        + list2str(carg_include_dirs, "-I")
                                        + list2str(carg.defines, "-D");
            obj_out.push_back(path_out);
            obj_out_ninja_esc.push_back(field->outputs[0]);
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
        std::string outfile = opt->out_prefix + "lib" + x.name + ".a";
        if (x.perferred_binary_name.size())
            outfile = opt->out_prefix + x.perferred_binary_name;
        std::string outfile_njesc = cgn::NinjaFile::escape_path(outfile);
        auto *field = opt->ninja->append_build();
        field->rule = "gcc_ar";
        field->inputs = obj_out_ninja_esc
                      + cgn::NinjaFile::escape_path(x._lnr_to_self.object_files);
        field->outputs = {outfile_njesc};
        field->variables["exe"] = interp.exe_ar;

        _entry_postprocess(field->outputs);
        rvlnr->static_files = std::vector<std::string>{outfile} + rvlnr->static_files;

        opt->result.outputs = {outfile};
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
        std::string outfile;
        std::string outfile_njesc;
        if (x.role == 's')
            outfile = opt->out_prefix + "lib" + x.name + ".so";
        else
            outfile = opt->out_prefix + x.name;
        if (x.perferred_binary_name.size())
            outfile = opt->out_prefix + x.perferred_binary_name;
        outfile_njesc = opt->ninja->escape_path(outfile);

        //prepare rpath argument
        //  this is seen as target ldflags, so put on the tail of cargs.ldflags
        if (x.cfg["os"] == "linux") {
            if (x.cfg["pkg_mode"] == "T") {
                carg.ldflags += {
                    "-Wl,--enable-new-dtags", 
                    two_escape("-Wl,-rpath=$ORIGIN")
                };
                rvlnr->runtime_files["lib" + x.name + ".so"] = outfile;
            }
            else {
                carg.ldflags += {"-Wl,--enable-new-dtags"};
                for (auto &so : x._lnr_to_self.shared_files) {
                    auto path1    = cgn::Tools::parent_path(so);
                    auto path_rel = cgn::Tools::rebase_path(path1, opt->out_prefix);
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
        auto *field = opt->ninja->append_build();
        field->rule = "crun_rsp";
        field->inputs = obj_out_ninja_esc 
                      + opt->ninja->escape_path(x._lnr_to_self.object_files);
        field->implicit_inputs = opt->ninja->escape_path(x._lnr_to_self.static_files) 
                               + opt->ninja->escape_path(x._lnr_to_self.shared_files)
                               + opt->ninja->escape_path(x._wholearchive_a);
        field->outputs = {outfile_njesc};
        field->variables["exe"] = opt->ninja->escape_path(
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
            + "-o " + api.shell_escape(field->outputs[0])
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
                auto dst  = opt->out_prefix + one_entry.first;
                auto &src = one_entry.second;
                auto *cpfield = opt->ninja->append_build();
                //TODO: copy runtime by custom command (like symbolic-link)
                cpfield->rule    = "unix_cp";
                cpfield->inputs  = {cgn::NinjaFile::escape_path(src)};
                cpfield->outputs = {cgn::NinjaFile::escape_path(dst)};
                field->order_only += cpfield->outputs;
                // entry->order_only += cpfield->outputs;
            }

        // put front
        rvlnr->shared_files = std::vector<std::string>{outfile} + rvlnr->shared_files;

        opt->result.outputs = {outfile};
        opt->result.ninja_dep_level = x._max_pub_ninja_level;
        return ;
    } // if (role=='s' or 'x')
} //TargetWorker::step31_unix()


void TargetWorker::_entry_postprocess(const std::vector<std::string> &to)
{
    auto *entry = opt->ninja->append_build();
    entry->rule = "phony";
    entry->inputs = to;
    entry->outputs = {escaped_ninja_entry};
    entry->implicit_inputs = opt->quickdep_ninja_full;    //TODO: really need?
    entry->order_only      = opt->quickdep_ninja_dynhdr;  //TODO: really need?
}

void CxxInterpreter::interpret(context_type &x)
{
    // preprocess "x.srcs = file_glob(*)"
    // if the source file is not at the same/sub folder of BUILD.cgn.cc
    // using absolutely path to locate.
    // BUG HERE: file_glob(*) cannot found file newly added (in ninja cache)
    //     TODO: target with file_glob() would re-analyse each time.
    std::vector<std::string> real_srcs;
    for (auto &ss : x.srcs) {
        if (ss.find('*') == ss.npos) //if not file_glob
            real_srcs += {ss};
        else
            real_srcs += api.file_glob(x.opt->src_prefix + ss, x.opt->src_prefix);
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
        x.opt->confirm_with_error("Unsupported cxx_toolchain " + (std::string)x.cfg["cxx_toolchain"]);
}


CxxContext::CxxContext(char role, cgn::CGNTargetOptIn *opt)
: role(role), name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

cgn::CGNTarget CxxContext::add_dep(
    const std::string &label, cgn::Configuration new_cfg, DepType flag
) {
    // merge TargetInfos[] manually
    auto early = opt->quick_dep(label, cfg, false);
    if (early.errmsg.size()) // return if error occured.
        return early;

    // cxx::order_dep
    // Ignore remote CGNTarget value and return.
    if (flag == cxx::order_dep)
        return early;

    if (flag & cxx::inherit)
        _max_pub_ninja_level = std::max(_max_pub_ninja_level, early.ninja_dep_level);

    // call merge() for unused field
    for (auto &rhs : early.data())
        if (rhs.first != "CxxInfo" && rhs.first != "LinkAndRunInfo")
            _pub_infos.merge_entry(rhs.first, rhs.second.get());

    // rhs[CxxInfo]
    //   cxx::inherit : append to interpreter_rv[CxxInfo] as is, 
    //                  and also apply on current target.
    //   cxx::private : save to _cxx_to_self to use for current target only.
    if ((flag & cxx::inherit))
        _pub_infos.get<CxxInfo>(true)->merge_entry(early.get<CxxInfo>(false));

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
    //   r_lnk.obj and r_lnk.a would consume by current target interpreter
    //   r_lnk.a with wholearchive (inherit dep) do not storage into _lnr_to_self
    //   r_lnk.so link to current one
    //   r_lnk.rt processed later in interpreter
    if (role == 's' || role == 'x') {
        if ((flag & cxx::inherit) && r_lnr) {
            _wholearchive_a += std::move(r_lnr->static_files);  //move and clear this entry
            auto *pub_lr = _pub_infos.get<cgn::LinkAndRunInfo>(true);
            pub_lr->shared_files += r_lnr->shared_files;
            pub_lr->runtime_files.insert(r_lnr->runtime_files.begin(), r_lnr->runtime_files.end());
        }
        _lnr_to_self.merge_entry(r_lnr);
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
        rv.exe_cc = prefix + "cl.exe";
        rv.exe_cxx = prefix + "cl.exe";
        rv.exe_ar = prefix + "lib.exe";
        rv.exe_solink = rv.exe_xlink = prefix + "link.exe";
        rv.extra_ldflags_so = {"/DLL"};
        rv.exe_asm = prefix + (cfg["host_cpu"]=="x86"? "ml.exe":"ml64.exe");
        rv.is_compiler_controlled_link = false;
        rv.arg.defines += {
            "WINVER=" + std::string{mimimum_winver},
            "_WIN32_WINNT=" + std::string{mimimum_winver},
            (cfg["msvc_runtime"] == "MDd"? "_DEBUG" : "NDEBUG")
        };
        rv.arg.cflags = {
            "/utf-8", "/wd4828",   // illegal character in UTF-8
            "/EHsc",               // Enables standard C++ stack unwinding
            (cfg["msvc_runtime"] == "MDd"? "/MDd" : (
             cfg["msvc_runtime"] == "MD"?  "/MD" : (
             cfg["msvc_runtime"] == "MTd"? "/MT": "/MTd")))
        };
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
    // std::string dllbase = opt->out_prefix;
    // if (x.runtime_dir.size() && (x.cfg["pkg_mode"] == "T" || x.cfg["os"] == "win"))
    //     dllbase += x.runtime_dir + opt.path_separator;

    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // ninja_dep_level
    opt->result.ninja_dep_level = x._max_pub_ninja_level;

    // result[CxxInfo]
    for (auto &it : x.pub.include_dirs)
        it = api.convert_cgnpath_to_working_root(it, x.opt);
    opt->result.merge_entry(x.pub.name(), &(x.pub));

    // TargetInfos[LinkAndRunInfo]
    auto *lrinfo = opt->result.get<cgn::LinkAndRunInfo>(true);
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
        std::string fullp = api.rebase_path(file, ".", opt);
        if (ext == ".so")
            lrinfo->shared_files.push_back(fullp);
        else if (ext == ".a")
            lrinfo->static_files.push_back(fullp);
        else if (ext == ".dll") {
            lrinfo->runtime_files[stem + ".dll"] = fullp;
            dllstem.insert(stem);
        }
        else if (ext == ".lib")
            dotlib.push_back({stem, fullp});
        else
            lrinfo->runtime_files[stem + "." + ext] = fullp;
    }
    for (auto item : dotlib)
        if (dllstem.count(item.first) != 0)
            lrinfo->shared_files.push_back(item.second);
        else
            lrinfo->static_files.push_back(item.second);

    // build.ninja
    auto *entry = opt->ninja->append_build();
    entry->rule = "phony";
    entry->implicit_inputs = opt->quickdep_ninja_full;
    entry->order_only      = opt->quickdep_ninja_dynhdr;
    entry->outputs = {opt->out_prefix + opt->BUILD_ENTRY};
}

} //namespace cxx