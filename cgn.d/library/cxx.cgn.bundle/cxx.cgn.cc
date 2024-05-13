//
//LLVM cross compile
// clang++ ./hello.cpp -o hello.fbsd --target=amd64-pc-freebsd 
//      --sysroot=/project/freebsd-14.0-amd64 -fuse-ld=lld
// clang++ ./hello.cpp -o hello.fbsd --target=aarch64-pc-freebsd 
//      --sysroot=/project/freebsd-14.0-arm64 -fuse-ld=lld

//
#include "cxx.cgn.h"

//start vector_set_calculator
#include <vector>
#include <string>
#include <unordered_set>
using StrList = std::vector<std::string>;
using StrSet  = std::unordered_set<std::string>;
StrList operator+(const StrList &lhs, const StrList &rhs) {
    StrList rv{lhs};
    rv.insert(rv.end(), rhs.begin(), rhs.end());
    return rv;
}
StrList operator+(const StrList &lhs, StrList &&rhs) {
    StrList rv{lhs};
    rv.insert(rv.end(), 
        std::make_move_iterator(rhs.begin()), 
        std::make_move_iterator(rhs.end()));
    rhs.clear();
    return rv;
}
StrList &operator+=(StrList &lhs, const StrList &rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}
StrList &operator+=(StrList &lhs, StrList &&rhs) {
    lhs.insert(lhs.end(), 
        std::make_move_iterator(rhs.begin()), 
        std::make_move_iterator(rhs.end()));
    rhs.clear();
    return lhs;
}

StrSet operator+(const StrSet &lhs, const StrSet &rhs) {
    StrSet rv{lhs};
    rv.insert(rhs.begin(), rhs.end());
    return rv;
}
StrSet operator+(const StrSet &lhs, StrSet &&rhs) {
    StrSet rv{lhs};
    rv.insert(std::make_move_iterator(rhs.begin()), 
              std::make_move_iterator(rhs.end()));
    rhs.clear();
    return rv;
}

StrSet &operator+=(StrSet &lhs, const StrSet &rhs) {
    lhs.insert(rhs.begin(), rhs.end());
    return lhs;
}
StrSet &operator+=(StrSet &lhs, StrSet &&rhs) {
    lhs.insert(std::make_move_iterator(rhs.begin()), 
               std::make_move_iterator(rhs.end()));
    rhs.clear();
    return lhs;
}

// template<typename T> StrList 
// operator+(const StrList &lhs, T&& rhs) {
//     StrList rv{lhs};
//     rv.insert(rv.end(), rhs.begin(), rhs.end());
//     return rv;
// }
template<typename T> StrList&
operator+=(StrList &lhs, std::initializer_list<T> &&rhs) {
    lhs.insert(lhs.end(), 
        std::make_move_iterator(rhs.begin()), 
        std::make_move_iterator(rhs.end()));
    return lhs;
}
template<typename T> StrSet&
operator+=(StrSet &lhs, std::initializer_list<T> &&rhs) {
    lhs.insert(std::make_move_iterator(rhs.begin()), 
               std::make_move_iterator(rhs.end()));
    return lhs;
}
//end vector_set_calculator


// CxxInterpreter
// ------------- -------------
namespace cxx {

const cgn::BaseInfo::VTable &CxxInfo::_glb_cxx_vtable()
{
    const static cgn::BaseInfo::VTable v = {
        []() -> std::shared_ptr<cgn::BaseInfo> {
            return std::make_shared<CxxInfo>();
        },
        [](void *ecx, const void *rhs) {
            if (rhs == nullptr)
                return ;
            CxxInfo *self = (CxxInfo*)ecx, *r = (CxxInfo*)rhs;
            self->include_dirs += r->include_dirs;
            self->defines += r->defines;
            self->ldflags += r->ldflags;
            self->cflags  += r->cflags;
        }, 
        [](const void *ecx) -> std::string { 
            auto *self = (CxxInfo *)ecx;
            return std::string{"{\n"}
                + "  cflags: [..]size=" + std::to_string(self->cflags.size()) + "\n"
                + " ldflags: [..]size=" + std::to_string(self->ldflags.size()) + "\n"
                + " incdirs: [..]size=" + std::to_string(self->include_dirs.size()) + "\n"
                + " defines: [..]size=" + std::to_string(self->defines.size()) + "\n"
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
    return {"", 0};
}

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

static std::vector<std::string> two_escape(std::vector<std::string> &in) {
    for (auto &it : in)
        two_escape(it);
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

// keep element first seen and remove duplicate in 'ls'
static void remove_dup(std::vector<std::string> &ls)
{
    std::unordered_set<std::string> visited;
    std::size_t i=0;
    for (std::size_t j=0; j<ls.size(); j++)
        if (visited.insert(ls[j]).second == true)
            std::swap(ls[i++], ls[j]);
    ls.resize(i);
}

cgn::TargetInfos CxxInterpreter::msvc_interpret(
    CxxInterpreter::context_type &x, cgn::CGNTargetOpt opt
) {
    std::vector<std::string> cflags_cc{"/std:c++17"}, cflags_c{"/std:c17"};

    CxxInfo &arg = x;
    arg.defines += {
        //"UNICODE", "_UNICODE",   // default for NO unicode WidthType (encoding UTF-8 only)
        "_CONSOLE",                //"WIN32",
        "_CRT_SECURE_NO_WARNINGS", //for strcpy instead of strcpy_s
        "WINVER=${mimimum_winver}",        // win10==0x0A00; win7==0x0601;
        "_WIN32_WINNT=${mimimum_winver}",  // win8.1/Server2012R2==0x0603;
    };
    arg.cflags += {
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
        "/utf-8", "/wd4828",   // illegal character in UTF-8
        // "/nologo",      // /nologo is in ninja file
        "/diagnostics:column",
        "/EHs",        // Enables standard C++ stack unwinding
        "/FS",         // force synchoronous PDB write for parallel to serializes.
        // "/await",
    };
    arg.ldflags += {
        "/NXCOMPAT",    // Compatible with Data Execution Prevention
        "/DYNAMICBASE",
        
        "kernel32.lib", "user32.lib", "gdi32.lib", "winspool.lib", "comdlg32.lib", 
        "advapi32.lib", "shell32.lib", "ole32.lib", "oleaut32.lib", "uuid.lib", 
        "odbc32.lib", "odbccp32.lib"
    };
    
    //["cpu"]
    // https://stackoverflow.com/questions/13545010/amd64-not-defined-in-vs2010
    if (x.cfg["cpu"] == "x86") {
        arg.defines += {"WIN32", "_X86_"};
        // The /LARGEADDRESSAWARE option tells the linker that the application 
        // can handle addresses larger than 2 gigabytes.
        arg.ldflags += {"/SAFESEH", "/MACHINE:X86", "/LARGEADDRESSAWARE"};
    }
    if (x.cfg["cpu"] == "x86_64")
        arg.defines += {"_AMD64_"};  
        arg.ldflags += {"/MACHINE:X64"};

    //["msvc_runtime"]
    if (x.cfg["msvc_runtime"] == "MDd") {
        arg.defines += {"_DEBUG"};
        arg.cflags  += {"/MDd"};
    }
    if (x.cfg["msvc_runtime"] == "MD") {
        arg.defines += {"NDEBUG"};
        arg.cflags  += {"/MD"};
    }

    //["optimization"]
    if (x.cfg["optimization"] == "debug") {
        arg.cflags += {
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
        arg.ldflags += {
            "/DEBUG:FULL", 
            "/INCREMENTAL"
        };
    }
    if (x.cfg["optimization"] == "release") {
        arg.cflags += {
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
        arg.ldflags += {
            "/OPT:ICF",
            "/DEBUG:FASTLINK",    // /Zi does imply /debug, the default is FASTLINK
            "/LTCG:incremental",  //TODO: using profile to guide optimization here (PGOptimize)
            "/RELEASE",           //sets the Checksum in the header of an .exe file.
        };
    }

    //["msvc_subsystem"]
    if (x.cfg["msvc_subsystem"] == "CONSOLE")
        arg.ldflags += {"/SUBSYSTEM:CONSOLE"};
    if (x.cfg["msvc_subsystem"] == "WINDOW")
        arg.ldflags += {"/SUBSYSTEM:WINDOW"};
    
    
    //=== Section 2: make ninja file ===
    // for msvc only
    // TODO: implement after llvm-toolchain tested.

    //(2) args.define => .cflags
    //    arg.include_dirs => .cflags
    auto def_to_cflag = [](CxxInfo &inf) {
        for (auto &def : inf.defines)
            inf.cflags += {"/D" + cgn::Tools::shell_escape(def)};
        for (auto &dir : inf.include_dirs)
            inf.cflags += {"/I" + cgn::Tools::shell_escape(dir)};
    };
    def_to_cflag(x);
    // def_to_cflag(x.pub); //x.pub don't need to modify anymore.

    // build.ninja : source file => .o
    std::vector<std::string> obj_out;
    for (auto &file : x.srcs) {
        auto chk = file_check(file);
        std::string path_in  = opt.src_prefix + cgn::Tools::locale_path(file);
        std::string path_out = opt.src_prefix + cgn::Tools::locale_path(chk.first) + ".obj";
        if (chk.second == '+' || chk.second == 'c' || chk.second == 'a') { // .c .cpp .S
            auto *field = opt.ninja->append_build();
            field->rule = "msvc";
            field->inputs  = {path_in};
            field->outputs = {path_out};
            field->variables["cflags"] = list2str(arg.cflags) 
                + list2str(chk.second=='+'? cflags_cc:cflags_c);
            obj_out.push_back(path_out);
        }
    }

    // init CxxInfo for return value
    cgn::TargetInfos rv;
    rv.get<cgn::DefaultInfo>()->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;
    rv.set(x.pub); // rv[CxxInfo] = x.pub
    
    return rv;
}

cgn::TargetInfos CxxInterpreter::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    constexpr const char *rule_ninja = "@cgn.d//library/cxx.cgn.bundle/cxx_rule.ninja";

    // convert file path to relpath-of-working-dir
    for (std::string &dir : x.include_dirs)
        dir = cgn::Tools::locale_path(opt.src_prefix + dir);
    for (std::string &dir : x.pub.include_dirs)
        dir = cgn::Tools::locale_path(opt.src_prefix + dir);

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
            real_srcs += api.file_glob(opt.src_prefix + ss, opt.src_prefix);
    }
    std::swap(x.srcs, real_srcs);
    
    static std::string rule_path = api.get_filepath(rule_ninja);
    opt.ninja->append_include(rule_path);

    if (x.cfg["toolchain"] == "msvc")
        return msvc_interpret(x, opt);

    // For toolchain gcc, the cross compiler is present by compiler filename
    // like /toolchain_X/arm-none-linux-gnueabi-gcc, and the kernel path 
    // (--sysroot) usually hard-coding inside compiler.
    //
    // For toolchain llvm, user should assign the target os/cpu and sysroot for 
    // cross compile.
    //      
    
    // interp_arg: data has been processed by two_escape()
    // interp_arg 内数据均经 两次转义过(two_escape) 或用户保证无需转义
    //            参数前半段为编译器参数 通常不需转义 (例如 --sysroot= ) 
    //            一般仅后段需转义 (例如 $ORIGIN => \$ORIGIN )
    CxxInfo interp_arg;
    std::vector<std::string> interp_cflags_cc{"-std=c++17"}, interp_cflags_c{"-std=gnu17"};
    std::string prefix = x.cfg["cxx_prefix"];
    std::string exe_cc, exe_cxx, exe_solink, exe_xlink, exe_ar;
    if (x.cfg["toolchain"] == "gcc") {
        exe_cc  = two_escape(prefix + "gcc");
        exe_cxx = two_escape(prefix + "g++");
        exe_ar  = two_escape(prefix + "gcc-ar");
        exe_solink = two_escape(prefix + "g++") + " -shared";
        exe_xlink  = two_escape(prefix + "g++");

        std::vector<std::string> cflags_1st, ldflags_1st;
        interp_arg.cflags += {
            "-I.",
            "-fdiagnostics-color=always",
            "-fvisibility=hidden",
            "-Wl,--exclude-libs,ALL"
        };
        interp_arg.ldflags += {
            "-L.",
            "-Wl,--warn-common", "-Wl,-z,origin", 
            "-Wl,--export-dynamic",  // force export from executable
            // "-Wl,--warn-section-align", 
            // "-Wl,-Bsymbolic", "-Wl,-Bsymbolic-functions",
        };

        //["os"]
        if (x.cfg["os"] == "linux") {
            interp_arg.cflags  += {"-fPIC","-pthread"};
            interp_arg.ldflags += {"-ldl", "-lrt", "-lpthread"};
        }

        //["optimization"]
        if (x.cfg["optimization"] == "debug") {
            interp_arg.defines += {"_DEBUG"};
            interp_arg.cflags += {
                "-Og", "-g", "-Wall", "-ggdb", "-O0",
                "-fno-eliminate-unused-debug-symbols", 
                "-fno-eliminate-unused-debug-types"};
            interp_cflags_cc.push_back("-ftemplate-backtrace-limit=0");
        }
        if (x.cfg["optimization"] == "release")
            interp_arg.cflags += {"-O2", "-flto", "-fwhole-program"};
        
        //["cxx_sysroot"]
        if (x.cfg["cxx_sysroot"] != "")
            interp_arg.cflags += {
                "--sysroot=" + two_escape(x.cfg["cxx_sysroot"])
            };
    } //toolchain == "gcc"
    else if (x.cfg["toolchain"] == "llvm") {
        exe_cc  = two_escape(prefix + "clang");
        exe_cxx = two_escape(prefix + "clang++");
        exe_ar  = two_escape(prefix + "llvm-ar");
        exe_solink = two_escape(prefix + "clang++") + " -fuse-ld=lld -shared";
        exe_xlink  = two_escape(prefix + "clang++") + " -fuse-ld=lld";

        interp_arg.cflags += {
            "-fvisibility=hidden",
            "-fcolor-diagnostics", "-Wreturn-type", 
            "-I.", "-fPIC", "-pthread"};
        
        interp_arg.ldflags += {
            "-L.",
            "-Wl,--warn-common", "-Wl,--warn-backrefs",
            "-lpthread", "-lrt"
        };

        interp_arg.defines += {"_GNU_SOURCE"};
        interp_cflags_c = {"-std=c17"};

        //["optimization"]
        if (x.cfg["optimization"] == "debug") {
            interp_arg.defines += {"_DEBUG"};
            interp_arg.cflags += {
                "-g", "-Wall", "-Wextra", "-Wno-unused-parameter",
                "-fno-omit-frame-pointer", "-fno-optimize-sibling-calls",
                "-ftemplate-backtrace-limit=0", "-fno-limit-debug-info",
                "-fstandalone-debug", "-fdebug-macro", "-glldb", //"-march=native",
                "-fcoverage-mapping", "-fprofile-instr-generate", "-ftime-trace"
                // "-flto=thin"
            };
        }
        if (x.cfg["optimization"] == "release") {
            interp_arg.cflags += {"-O3", "-flto"};
            interp_arg.ldflags += {
                "-flto", "-Wl,--exclude-libs=ALL", "-Wl,--discard-all",
                // "-Wl,--thinlto-jobs=0", 
                // "-Wl,--thinlto-cache-dir=./thinlto_cache", 
                // "-Wl,--thinlto-cache-policy,cache_size_bytes=1g"
            };
        }

        //["llvm_stl"]
        if (x.cfg["llvm_stl"] == "libc++")
            interp_arg.cflags += {"-stdlib=libc++"};

        //["cxx_sysroot"]
        if (x.cfg["cxx_sysroot"] != "")
            interp_arg.cflags += {
                "--sysroot=" + two_escape(x.cfg["cxx_sysroot"])};

        //llvm cross compile argument
        auto host = api.get_host_info();
        if (x.cfg["os"] != host.os || x.cfg["cpu"] != host.cpu) {
            std::string cpu = x.cfg["cpu"];
            if (cpu == "x86_64")
                cpu = "amd64";
            if (cpu == "arm64")
                cpu = "aarch64";
            interp_arg.cflags += {"--target=" + cpu + "-pc-" + (std::string)x.cfg["os"]};
        }
    } //toolchain == "llvm"
    else {
        throw std::runtime_error{
            "Unsupported toolchain " + (std::string)x.cfg["toolchain"]};
    }

    //=== Section 2: merge args from target(x), deps, interpreter ===
    CxxInfo carg; //compile arg
    carg.cflags = interp_arg.cflags + two_escape(x._cxx_to_self.cflags) 
                + two_escape(x.cflags);
    carg.ldflags = interp_arg.ldflags + two_escape(x._cxx_to_self.ldflags) 
                + two_escape(x.ldflags);
    carg.include_dirs = two_escape(x.include_dirs)
                      + two_escape(x._cxx_to_self.include_dirs)
                      + interp_arg.include_dirs;
    carg.defines = x.defines + x._cxx_to_self.defines + interp_arg.defines;

    // clear duplicate include folder, .obj files from dep
    remove_dup(carg.include_dirs);
    remove_dup(x._lnr_to_self.object_files);
    remove_dup(x._lnr_to_self.static_files);
    remove_dup(x._lnr_to_self.shared_files);

    //=== Section 3: make ninja file ===
    // common for both GCC and LLVM
    // build.ninja : source file => .o
    // field.input and field.output : need ninja escape, instead of shell esacpe
    // carg.cflags and carg.ldflags : already two escaped
    //
    // case for path_out: xxx.so / .lib may have same name with folder-name 
    //                    in src folder, so we have to add '_' before path_out.
    // case for path_out: cgn-out/.../ in same folder of current interpreter
    //                    add '__' (two underline) before path_out.
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;
    for (auto &ss : x.srcs) {
        // if (ss[0] == '.' && ss[1] == '.')
        //     ss = api.abspath(ss);  // using abspath to present file outside project
        auto chk = file_check(ss);
        std::string path_in  = cgn::Tools::locale_path(opt.src_prefix + ss);
        std::string path_out;
        std::string probe1 = api.rebase_path(path_in, opt.out_prefix);
        if (probe1[0] != '.' && probe1[1] != '.') {// path_in is inside out_prefix
            chk = file_check(probe1);
            path_out = opt.out_prefix + "__" + chk.first + ".o";
        }
        else
            path_out = cgn::Tools::locale_path(opt.out_prefix + "_" + chk.first + ".o");
        if (chk.second) {
            auto *field = opt.ninja->append_build();
            field->rule = "gcc";
            field->inputs  = {cgn::NinjaFile::escape_path(path_in)};
            field->outputs = {cgn::NinjaFile::escape_path(path_out)};
            field->order_only = x.phony_order_only;
            field->variables["cc"] = (chk.second=='+'?exe_cxx:exe_cc);
            field->variables["cflags"] = 
                list2str(chk.second=='+'? interp_cflags_cc:interp_cflags_c) 
                + list2str(carg.cflags)
                + list2str(carg.include_dirs, "-I")
                + list2str(carg.defines, "-D");
            obj_out.push_back(path_out);
            obj_out_ninja_esc.push_back(field->outputs[0]);
        }
    }

    // init CxxInfo for return value
    cgn::TargetInfos &rv = x._pub_infos_fromdep;
    
    CxxInfo *rvcxx = rv.get<CxxInfo>();
    rvcxx->cflags  += x.pub.cflags;
    rvcxx->ldflags += x.pub.ldflags;
    rvcxx->defines += x.pub.defines;
    rvcxx->include_dirs = x.pub.include_dirs + rvcxx->include_dirs;
    
    cgn::DefaultInfo *rvdef = rv.get<cgn::DefaultInfo>(true);
    rvdef->target_label = opt.factory_ulabel;
    rvdef->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;

    cgn::LinkAndRunInfo *rvlnr = rv.get<cgn::LinkAndRunInfo>(true);
    remove_dup(rvlnr->object_files);

    // build.ninja : cxx_sources()
    // cxx_sources() cannot process any field of LinkAndRunInfo
    // so add the .obj file generated by itself then return
    std::string escaped_ninja_entry = cgn::NinjaFile::escape_path(
                                    opt.out_prefix + opt.BUILD_ENTRY);
    if (x.role == 'o') {
        auto *stamp = opt.ninja->append_build();
        stamp->rule = "phony";
        stamp->outputs = {escaped_ninja_entry};
        stamp->inputs  = obj_out_ninja_esc;
        stamp->order_only = x.phony_order_only;

        rvlnr->object_files = obj_out + rvlnr->object_files;
        return rv;
    }

    // build.ninja : cxx_static()
    //  deps.obj + self.srcs.o => rv[LRinfo].a
    //  deps.rt / deps.so / deps.a => rv[LRinfo]
    if (x.role == 'a') {
        std::string outfile = opt.out_prefix + x.name + ".a";
        std::string outfile_njesc = cgn::NinjaFile::escape_path(outfile);
        auto *field = opt.ninja->append_build();
        field->rule = "gcc_ar";
        field->inputs = obj_out_ninja_esc
                      + cgn::NinjaFile::escape_path(x._lnr_to_self.object_files);
        field->outputs = {outfile_njesc};
        field->variables["exe"] = exe_ar;

        auto *entry = opt.ninja->append_build();
        entry->rule = "phony";
        entry->inputs = field->outputs;
        entry->outputs = {escaped_ninja_entry};
        entry->order_only = x.phony_order_only;

        rvlnr->static_files = std::vector<std::string>{outfile} + rvlnr->static_files;
        return rv;
    }

    // build.ninja : cxx_shared() / cxx_executable()
    //   deps.object + deps.static + self.srcs.o => self.so / self.exe
    //   with carg.ldflags and -wholearchive:x._wholearchive_a
    //   self.so + {so from deps} => rv[LRinfo].so
    if (x.role == 's' || x.role == 'x') {
        std::string outfile;
        std::string outfile_njesc;
        if (x.role == 's')
            outfile = opt.out_prefix + "lib" + x.name + ".so";
        else
            outfile = opt.out_prefix + x.name;
        outfile_njesc = opt.ninja->escape_path(outfile);

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
                    auto path_rel = cgn::Tools::rebase_path(path1, opt.out_prefix);
                    carg.ldflags += {two_escape("-Wl,--rpath=$ORIGIN/" + path_rel)};
                }
            }
        }
        if (x.cfg["os"] == "win") {
            //TODO: manifest and .runtime
        }

        //generate ninja section
        // --start-group   : {all.obj} {dep.static without whole} -l{dep.shared}
        // --whole-archive : {static_files from inherit dep}
        auto *field = opt.ninja->append_build();
        field->rule = "crun_rsp";
        field->inputs = obj_out_ninja_esc 
                      + opt.ninja->escape_path(x._lnr_to_self.object_files);
        field->implicit_inputs = opt.ninja->escape_path(x._lnr_to_self.static_files) 
                               + opt.ninja->escape_path(x._lnr_to_self.shared_files);
        field->outputs = {outfile_njesc};
        field->variables["exe"] = opt.ninja->escape_path(
                                    x.role=='s'? exe_solink:exe_xlink);
        field->variables["args"] = list2str(carg.ldflags) 
            + "-o " + api.shell_escape(field->outputs[0])
            + " -Wl,--whole-archive " + list2str(two_escape(x._wholearchive_a))
            + "-Wl,--no-whole-archive "
            + "-Wl,--start-group "
            + list2str(obj_out_ninja_esc)
            + list2str(two_escape(x._lnr_to_self.object_files))
            + list2str(two_escape(x._lnr_to_self.static_files))
            + list2str(two_escape(x._lnr_to_self.shared_files), "-l:")
            + "-Wl,--end-group";
        field->variables["desc"] = "LINK " + outfile_njesc;
        
        // generate entry
        auto *entry = opt.ninja->append_build();
        entry->rule = "phony";
        entry->outputs = {escaped_ninja_entry};
        entry->inputs += field->outputs;
        entry->order_only = x.phony_order_only;

        // copy runtime when cxx_executable()
        if (x.role == 'x')
            for (auto &one_entry : x._lnr_to_self.runtime_files) {
                auto dst  = opt.out_prefix + one_entry.first;
                auto &src = one_entry.second;
                auto *cpfield = opt.ninja->append_build();
                //TODO: copy runtime by custom command (like symbolic-link)
                cpfield->rule    = "unix_cp";
                cpfield->inputs  = {cgn::NinjaFile::escape_path(src)};
                cpfield->outputs = {cgn::NinjaFile::escape_path(dst)};
                field->order_only += cpfield->outputs;
                // entry->order_only += cpfield->outputs;
            }

        // put front
        rvlnr->shared_files = std::vector<std::string>{outfile} + rvlnr->shared_files;

        rvdef->outputs = {outfile};
        return rv;
    } // if (role=='s' or 'x')

    return {};
}

// struct CxxInfo

CxxContext::CxxContext(char role, const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
: role(role), name(opt.factory_name), cfg(cfg), opt(opt) {}

cgn::TargetInfos CxxContext::add_dep(
    const std::string &label, cgn::Configuration new_cfg, DepType flag
) {
    auto rhs = api.analyse_target(
        api.absolute_label(label, opt.src_prefix), new_cfg);
    if (rhs.infos.empty())
        return {};
    api.add_adep_edge(rhs.adep, opt.adep);
    auto *r_def = rhs.infos.get<cgn::DefaultInfo>();
    if ((flag & cxx::order_dep) || r_def->enforce_keep_order) {
        const cgn::DefaultInfo *inf = rhs.infos.get<cgn::DefaultInfo>();
        phony_order_only.push_back(inf->build_entry_name);
        if (flag == cxx::order_dep)
            return rhs.infos;
    }

    // call merge() for unused field
    for (auto &rhs : rhs.infos.data())
        if (rhs.first != "CxxInfo" && rhs.first != "LinkAndRunInfo")
            _pub_infos_fromdep.merge_entry(rhs.first, rhs.second);

    // rhs[CxxInfo]
    //   cxx::inherit : append to interpreter_rv[CxxInfo] as is, 
    //                  and also apply on current target.
    //   cxx::private : save to _cxx_to_self to use for current target only.
    if ((flag & cxx::inherit))
        _pub_infos_fromdep.get<CxxInfo>(true)->merge_from(rhs.infos.get<CxxInfo>());
    _cxx_to_self.merge_from(rhs.infos.get<CxxInfo>());

    // rhs[LinkAndRunInfo]
    auto *r_lnr = rhs.infos.get<cgn::LinkAndRunInfo>();

    // for both msvc and GNU
    // cxx_sources() : keep as is (do not consume anyone)
    // cxx_static()  : move(r_lnr.obj) to cmd "ar rcs" later in interpreter, 
    //                 keep others as-is.
    if (role == 'o')
        _pub_infos_fromdep.get<cgn::LinkAndRunInfo>(true)->merge_from(r_lnr);
    if (role == 'a') {
        if (flag & pack_obj)
            _lnr_to_self.object_files += std::move(r_lnr->object_files);
        _pub_infos_fromdep.get<cgn::LinkAndRunInfo>(true)->merge_from(r_lnr);
    }
    
    // cxx_executable() and cxx_shared() for both msvc and GNU
    //   r_lnk.obj and r_lnk.a would consume by current target interpreter
    //   r_lnk.a with wholearchive (inherit dep) do not storage into _lnr_to_self
    //   r_lnk.so link to current one
    //   r_lnk.rt processed later in interpreter
    if (role == 's' || role == 'x') {
        if ((flag & cxx::inherit)) {
            _wholearchive_a += std::move(r_lnr->static_files);  //move and clear this entry
            auto *pub_lr = _pub_infos_fromdep.get<cgn::LinkAndRunInfo>(true);
            pub_lr->shared_files += r_lnr->shared_files;
            pub_lr->runtime_files.insert(r_lnr->runtime_files.begin(), r_lnr->runtime_files.end());
        }
        _lnr_to_self.merge_from(r_lnr);
        
    }

    return rhs.infos;
} //CxxContext::add_dep

CxxToolchainInfo CxxInterpreter::test_param(const cgn::Configuration &cfg)
{
    CxxToolchainInfo rv;
    std::string prefix = cfg["cxx_prefix"];
    if (cfg["toolchain"] == "gcc") {
        rv.c_exe = prefix + "gcc";
        rv.cxx_exe = prefix + "g++";
    }
    if (cfg["toolchain"] == "llvm") {
        rv.c_exe = prefix + "clang";
        rv.cxx_exe = prefix + "clang++";
    }

    return rv;
}

// CxxPrebuiltInterpreter
// ----------------------

cgn::TargetInfos CxxPrebuiltInterpreter::interpret(
    context_type &x, cgn::CGNTargetOpt opt
) {
    cgn::TargetInfos &rv = x.merged_info;

    // TargetInfos[CxxInfo]
    for (auto &it : x.pub.include_dirs)
        it = api.locale_path(opt.src_prefix + it);
    rv.set(x.pub);

    std::string dllbase = opt.out_prefix;
    if (x.runtime_dir.size() && (x.cfg["pkg_mode"] == "T" || x.cfg["os"] == "win"))
        dllbase += x.runtime_dir + opt.path_separator;

    // TargetInfos[LinkAndRunInfo]
    auto *lrinfo = rv.get<cgn::LinkAndRunInfo>(true);
    bool have_sofile = false;
    std::unordered_set<std::string> dllstem;
    std::vector<std::pair<std::string,std::string>> dotlib;
    for (auto file : x.files) {
        auto fd1   = file.rfind('/');
        auto fddot = file.rfind('.');
        fd1 = (fd1 == file.npos? 0: fd1+1);
        if (fddot == file.npos || fddot < fd1)
            continue;
        std::string fullp = api.locale_path(opt.out_prefix + file);
        std::string stem = file.substr(fd1, fddot-fd1);
        std::string ext  = file.substr(fddot);
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
            lrinfo->runtime_files[file] = fullp;
    }
    for (auto item : dotlib)
        if (dllstem.count(item.first) != 0)
            lrinfo->shared_files.push_back(item.second);
        else
            lrinfo->static_files.push_back(item.second);

    auto *entry = opt.ninja->append_build();
    entry->rule = "phony";
    entry->order_only = x.ninja_target_dep;
    entry->outputs = {opt.out_prefix + opt.BUILD_ENTRY};

    return rv;
}

} //namespace cxx