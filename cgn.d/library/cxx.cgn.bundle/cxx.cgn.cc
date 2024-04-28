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
StrList &operator+=(StrList &lhs, const StrList &rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}
StrSet operator+(const StrSet &lhs, const StrSet &rhs) {
    StrSet rv{lhs};
    rv.insert(rhs.begin(), rhs.end());
    return rv;
}
StrSet &operator+=(StrSet &lhs, const StrSet &rhs) {
    lhs.insert(rhs.begin(), rhs.end());
    return lhs;
}

// template<typename T> StrList 
// operator+(const StrList &lhs, T&& rhs) {
//     StrList rv{lhs};
//     rv.insert(rv.end(), rhs.begin(), rhs.end());
//     return rv;
// }
template<typename T> StrList&
operator+=(StrList &lhs, std::initializer_list<T> rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}
template<typename T> StrSet&
operator+=(StrSet &lhs, std::initializer_list<T> rhs) {
    lhs.insert(rhs);
    return lhs;
}
//end vector_set_calculator

namespace cxx {

// struct CxxInfo

CxxContext::CxxContext(char role, const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
: role(role), name(opt.factory_name), cfg(cfg), opt(opt) {}

const cgn::BaseInfo::VTable CxxInfo::v = {
    [](void *ecx, const void *rhs) {
        CxxInfo *self = (CxxInfo*)ecx, *r = (CxxInfo*)rhs;
        self->include_dirs += r->include_dirs;
        self->defines += r->defines;
        self->ldflags += r->ldflags;
        self->cflags  += r->cflags;
    }, 
    [](const void *ecx) -> std::string { 
        return "CxxInfo{...}";
    }
};

// @return {stem, type}:
//      type: '\0' to skip, '+' for cpp, 'c' for c, 'a' for asm
std::pair<std::string, char> file_check(const std::string& filename) {
    auto fd = filename.rfind('.');
    if (fd == filename.npos)
        return {filename, '+'};
    
    std::string ext = filename.substr(fd+1);
    // to lower case
    for (char &c : ext)
        if ('A' <= c && c <= 'Z')
            c = c - 'A' + 'a';
    
    // check current file is c/cpp source file
    if (ext == "cc" || ext == "cpp" || ext == "cxx" || ext == "c++")
        return {filename.substr(0, fd), '+'};
    if (ext == "c")
        return {filename.substr(0, fd), 'c'};
    if (ext == "s")
        return {filename.substr(0, fd), 'a'};
    return {"", 0};
}

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

template<bool NeedEscape = false>
std::string list2str(
    const std::vector<std::string> &in, const std::string prefix="",
    bool erase_dup = false
) {
    std::unordered_set<std::string> visited;
    std::string rv;
    for (auto &it : in)
        if (!erase_dup || visited.insert(it).second){
            std::string tmp = prefix + it;
            rv += (NeedEscape? two_escape(tmp):tmp) + " ";
        }
    return rv;
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
    def_to_cflag(x.pub);

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
    // merge variable and convert file path to relpath-of-working-dir
    // add cflags, ldflags from current target
    ((CxxInfo*)&x)->merge_from(&x.dep_cxx_self);
    ((CxxInfo*)&x)->merge_from(&x.dep_cxx_pub);
    x.pub.merge_from(&x.dep_cxx_pub);
    for (std::string &dir : x.include_dirs)
        dir = opt.src_prefix + cgn::Tools::locale_path(dir);
    for (std::string &dir : x.pub.include_dirs)
        dir = opt.src_prefix + cgn::Tools::locale_path(dir);
    // for (std::string &ss : x.srcs)
    //     ss = opt.src_prefix + cgn::Tools::locale_path(ss);
    // NOW: x.dep_cxx_self and x.dep_cxx_pub are invalid

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
    // arg: 作用于当前target的参数 (shell escaped)
    //      参数前半段为编译器参数 通常不需转义 (例如 --sysroot= ) 
    //      一般仅后段需转义 (例如 $ORIGIN => \$ORIGIN )
    // 1. move content from 'x' to 'arg'
    // 2. move 'arg.defines' and 'arg.include_dirs' to 'arg.cflags'
    CxxInfo &arg = x;
    std::vector<std::string> cflags_cc{"-std=c++17"}, cflags_c{"-std=gnu17"};

    std::string prefix = x.cfg["cxx_prefix"];
    std::string exe_cc, exe_cxx, exe_solink, exe_xlink, exe_ar;
    if (x.cfg["toolchain"] == "gcc") {
        exe_cc  = two_escape(prefix + "gcc");
        exe_cxx = two_escape(prefix + "g++");
        exe_ar  = two_escape(prefix + "gcc-ar");
        exe_solink = two_escape(prefix + "g++") + " -shared";
        exe_xlink  = two_escape(prefix + "g++");

        arg.cflags.insert(arg.cflags.end(), {
            "-I.",
            "-fdiagnostics-color=always",
            "-fvisibility=hidden",
            "-Wl,--exclude-libs,ALL"
        });
        arg.ldflags += {
            "-L.",
            "-Wl,--warn-common", "-Wl,-z,origin", 
            "-Wl,--export-dynamic",  // force export from executable
            // "-Wl,--warn-section-align", 
            // "-Wl,-Bsymbolic", "-Wl,-Bsymbolic-functions",
        };

        //["os"]
        if (x.cfg["os"] == "linux") {
            arg.cflags  += {"-fPIC","-pthread"};
            arg.ldflags += {"-ldl", "-lrt", "-lpthread"};
        }

        //["optimization"]
        if (x.cfg["optimization"] == "debug") {
            arg.defines.push_back("_DEBUG");
            arg.cflags.insert(arg.cflags.end(), {
                "-Og", "-g", "-Wall", "-ggdb", "-O0",
                "-fno-eliminate-unused-debug-symbols", 
                "-fno-eliminate-unused-debug-types"});
            cflags_cc.push_back("-ftemplate-backtrace-limit=0");
        }
        if (x.cfg["optimization"] == "release")
            arg.cflags.insert(arg.cflags.end(), {
                "-O2", "-flto", "-fwhole-program"});
        
        //["cxx_sysroot"]
        if (x.cfg["cxx_sysroot"] != "")
            arg.cflags += {"--sysroot=" + cgn::Tools::shell_escape(x.cfg["cxx_sysroot"])};
    } //toolchain == "gcc"
    else if (x.cfg["toolchain"] == "llvm") {
        exe_cc  = two_escape(prefix + "clang");
        exe_cxx = two_escape(prefix + "clang++");
        exe_ar  = two_escape(prefix + "llvm-ar");
        exe_solink = two_escape(prefix + "clang++") + " -fuse-ld=lld -shared";
        exe_xlink  = two_escape(prefix + "clang++") + " -fuse-ld=lld";

        arg.cflags += {
            "-fvisibility=hidden",
            "-fcolor-diagnostics", "-Wreturn-type", 
            "-I.", "-fPIC", "-pthread"};
        arg.ldflags += {
            "-L.",
            "-Wl,--warn-common", "-Wl,--warn-backrefs",
            "-lpthread", "-lrt"
        };
        arg.defines += {"_GNU_SOURCE"};
        cflags_c = {"-std=c17"};

        //["optimization"]
        if (x.cfg["optimization"] == "debug") {
            arg.defines += {"_DEBUG"};
            arg.cflags += {"-g", "-Wall", "-Wextra", "-Wno-unused-parameter",
                "-fno-omit-frame-pointer", "-fno-optimize-sibling-calls",
                "-ftemplate-backtrace-limit=0",
                "-fstandalone-debug", "-fdebug-macro", "-glldb", //"-march=native",
                "-fcoverage-mapping", "-fprofile-instr-generate", "-ftime-trace"
                // "-flto=thin"
            };
        }
        if (x.cfg["optimization"] == "release") {
            arg.cflags += {"-O3", "-flto"};
            arg.ldflags += {
                "-flto", "-Wl,--exclude-libs=ALL", "-Wl,--discard-all",
                // "-Wl,--thinlto-jobs=0", 
                // "-Wl,--thinlto-cache-dir=./thinlto_cache", 
                // "-Wl,--thinlto-cache-policy,cache_size_bytes=1g"
            };
        }

        //["llvm_stl"]
        if (x.cfg["llvm_stl"] == "libc++")
            arg.cflags += {"-stdlib=libc++"};

        //["cxx_sysroot"]
        if (x.cfg["cxx_sysroot"] != "")
            arg.cflags += {"--sysroot=" + cgn::Tools::shell_escape(x.cfg["cxx_sysroot"])};

        //llvm cross compile argument
        auto host = api.get_host_info();
        if (x.cfg["os"] != host.os || x.cfg["cpu"] != host.cpu) {
            std::string cpu = x.cfg["cpu"];
            if (cpu == "x86_64")
                cpu = "amd64";
            if (cpu == "arm64")
                cpu = "aarch64";
            arg.cflags += {"--target=" + cpu + "-pc-" + (std::string)x.cfg["os"]};
        }
    } //toolchain == "llvm"
    else {
        throw std::runtime_error{
            "Unsupported toolchain " + (std::string)x.cfg["toolchain"]};
    }

    //=== Section 2: make ninja file ===
    // common for both GCC and LLVM

    //(2) args.define => .cflags
    //    arg.include_dirs => .cflags
    // now inf.cflags and .ldflags can written to ninja file directly
    auto def_to_cflag = [](CxxInfo &inf) {
        for (auto &def : inf.defines)
            inf.cflags += {"-D" 
                + cgn::NinjaFile::escape_path(cgn::Tools::shell_escape(def))};
        for (auto &dir : inf.include_dirs)
            inf.cflags += {"-I"
                + cgn::NinjaFile::escape_path(cgn::Tools::shell_escape(dir))};
    };
    def_to_cflag(x);
    def_to_cflag(x.pub);
    
    // build.ninja : source file => .o
    // field.input and .output : need ninja escape, instead of shell esacpe
    // arg.cflags : already escaped
    std::vector<std::string> obj_out;
    std::vector<std::string> obj_out_ninja_esc;
    for (auto &file : x.srcs) {
        auto chk = file_check(file);
        std::string path_in  = opt.src_prefix + cgn::Tools::locale_path(file);
        std::string path_out = opt.out_prefix + cgn::Tools::locale_path(chk.first) + ".o";
        if (chk.second == '+' || chk.second == 'c' || chk.second == 'a') { // .c .cpp .S
            auto *field = opt.ninja->append_build();
            field->rule = "gcc";
            field->inputs  = {cgn::NinjaFile::escape_path(path_in)};
            field->outputs = {cgn::NinjaFile::escape_path(path_out)};
            field->variables["cc"] = (chk.second=='+'?exe_cxx:exe_cc);
            field->variables["cflags"] = list2str(arg.cflags) 
                + list2str(chk.second=='+'? cflags_cc:cflags_c);
            obj_out.push_back(path_out);
            obj_out_ninja_esc.push_back(field->outputs[0]);
        }
    }

    // init CxxInfo for return value
    cgn::TargetInfos rv;
    rv.set(x.pub); // rv[CxxInfo] = x.pub

    rv.get<cgn::DefaultInfo>()->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;

    // build.ninja : for cxx_sources() only
    // cxx_sources() cannot process any field of LinkAndRunInfo
    // so append the .obj file generated by itself then return
    std::string escaped_ninja_stamp = cgn::NinjaFile::escape_path(
                          opt.out_prefix + opt.BUILD_ENTRY);
    if (x.role == 'o') {
        // .build_stamp
        auto *stamp = opt.ninja->append_build();
        stamp->rule = "unix_stamp";
        stamp->outputs = {escaped_ninja_stamp};
        stamp->inputs  = obj_out_ninja_esc;

        cgn::LinkAndRunInfo rvbr;
        rvbr.object_files  = x.dep_lr_self.object_files + obj_out;
        rvbr.runtime_files = x.dep_lr_self.runtime_files;
        rvbr.shared_files  = x.dep_lr_self.shared_files;
        rvbr.static_files  = x.dep_lr_self.static_files;

        rv.set(rvbr);
        return rv;
    }

    // build.ninja : for cxx_static() only
    //  deps.obj + self.srcs.o => rv[LRinfo].a
    //  deps.rt / deps.so / deps.a => rv[LRinfo]
    if (x.role == 'a') {
        std::string outfile = opt.out_prefix + x.name + ".a";
        std::string outfile_njesc = cgn::NinjaFile::escape_path(outfile);
        auto *field = opt.ninja->append_build();
        field->rule = "gcc_ar";
        field->inputs = cgn::NinjaFile::escape_path(x.dep_lr_self.object_files) 
                      + obj_out_ninja_esc;
        field->outputs = {outfile_njesc};
        field->variables["exe"] = exe_ar;

        auto *entry = opt.ninja->append_build();
        entry->rule = "phony";
        entry->inputs = field->outputs;
        entry->outputs = {escaped_ninja_stamp};

        cgn::LinkAndRunInfo rvbr;
        rvbr.runtime_files = x.dep_lr_self.runtime_files;
        rvbr.shared_files  = x.dep_lr_self.shared_files;
        rvbr.static_files  = StrList{outfile} + x.dep_lr_self.static_files;

        rv.set(rvbr);
        return rv;
    }

    // build.ninja : for cxx_shared() / cxx_executable() only
    //   deps.object + deps.static + self.srcs.o => self.so
    //   with -l:deps.shared, -wholearchive:x.pub_a
    //   x.pub_so + self.so => rv[LRinfo].so
    if (x.role == 's' || x.role == 'x') {
        cgn::LinkAndRunInfo rvbr;
        std::string tgt_out;
        std::string tgtout_njesc;
        if (x.role == 's')
            tgt_out = opt.out_prefix + "lib" + x.name + ".so";
        else
            tgt_out = opt.out_prefix + x.name;
        tgtout_njesc = opt.ninja->escape_path(tgt_out);

        //prepare rpath argument
        if (x.cfg["os"] == "linux") {
            if (x.cfg["pkg_mode"] == "T") {
                arg.ldflags += {
                    "-Wl,--enable-new-dtags", 
                    two_escape("-Wl,-rpath=$ORIGIN")
                };
                rvbr.runtime_files["lib" + x.name + ".so"] = tgt_out;
            }
            else {
                arg.ldflags += {"-Wl,--enable-new-dtags"};
                for (auto &so : x.dep_lr_self.shared_files + x.pub_so) {
                    auto path1    = cgn::Tools::parent_path(so);
                    auto path_rel = cgn::Tools::rebase_path(path1, opt.out_prefix);
                    arg.ldflags += {two_escape("-Wl,--rpath=$ORIGIN/" + path_rel)};
                }
            }
        }
        if (x.cfg["os"] == "win") {
            //TODO: manifest and .runtime
        }

        auto *phony = opt.ninja->append_build();
        phony->rule = "phony";
        phony->outputs = {escaped_ninja_stamp};

        //copy runtime when cxx_executable()
        if (x.role == 'x')
            for (auto &one_entry : x.dep_lr_self.runtime_files) {
                auto &dst = one_entry.first;
                auto &src = one_entry.second;
                auto *field = opt.ninja->append_build();
                //TODO: copy runtime by custom command (like symbolic-link)
                field->rule   = "unix_cp";
                field->inputs = {cgn::NinjaFile::escape_path(src)};
                field->outputs = {cgn::NinjaFile::escape_path(opt.out_prefix + dst)};
                phony->inputs += field->outputs;
            }

        //generate ninja section
        // --start-group   : {all.obj} {dep.static without whole} -l{dep.shared}
        // --whole-archive : {dep.static with whole}
        auto *field = opt.ninja->append_build();
        field->rule = "crun_rsp";
        field->inputs = obj_out_ninja_esc 
                      + opt.ninja->escape_path(x.dep_lr_self.object_files);
        field->implicit_inputs = opt.ninja->escape_path(x.dep_lr_self.static_files) 
                               + opt.ninja->escape_path(x.dep_lr_self.shared_files)
                               + opt.ninja->escape_path(x.pub_a)
                               + opt.ninja->escape_path(x.pub_so);
        field->outputs = {tgtout_njesc};
        phony->inputs += field->outputs;
        field->variables["exe"] = opt.ninja->escape_path(
                                    x.role=='s'? exe_solink:exe_xlink);
        field->variables["args"] = list2str(arg.ldflags) 
            + "-o " + api.shell_escape(field->outputs[0])
            + " -Wl,--whole-archive " + list2str<true>(x.pub_a)
            + "-Wl,--no-whole-archive "
            + "-Wl,--start-group "
            + list2str<true>(field->inputs)
            + list2str<true>(x.dep_lr_self.static_files)
            + list2str<true>(x.dep_lr_self.shared_files, "-l:")
            + list2str<true>(x.pub_so, "-l:")
            + "-Wl,--end-group";
        field->variables["desc"] = "LINK " + tgtout_njesc;
        
        rvbr.shared_files = x.pub_so + field->outputs;

        rv.set(rvbr);
        return rv;
    } // if (role=='s' or 'x')

    return {};
}

cgn::TargetInfos CxxContext::add_dep(
    const std::string &label, cgn::Configuration cfg, DepType flag
) {
    auto rv = api.analyse_target(
        api.absolute_label(label, opt.src_prefix), this->cfg);
    api.add_adep_edge(opt.adep, rv.adep);
    if (flag == DepType::_order_dep) {
        const cgn::DefaultInfo *inf = rv.infos.get<cgn::DefaultInfo>();
        phony_order_only.push_back(inf->build_entry_name);
        return rv.infos;
    }

    // inherit CxxInfo from dependencies
    auto *rcxx = rv.infos.get<CxxInfo>();
    auto write_arg = [&](CxxInfo *to) {
        to->cflags  += rcxx->cflags;
        to->defines += rcxx->defines;
        to->ldflags += rcxx->ldflags;
        to->include_dirs += rcxx->include_dirs;
    };
    if (flag == DepType::_inherit)
        write_arg(&dep_cxx_self);
    else
        write_arg(&dep_cxx_pub);

    // process LinkAndRunInfo from dep
    auto *rbr = rv.infos.get<cgn::LinkAndRunInfo>();

    //cxx_sources() and cxx_static() for both msvc and GNU
    //for static: move(brinfo.obj) to cmd "ar rcs" later in interpreter
    if (role == 'o' || role == 'a')
        dep_lr_self.merge_from(rbr);
    
    //cxx_executable() and cxx_shared() for both msvc and GNU
    // brin.obj and brin.a would consume by current target interpreter
    // brin.so and brin.rt processed later in interpreter
    // pub_a and pub_so would join as return value of current target
    if (role == 's' || role == 'x') {
        if (flag == DepType::_inherit) {
            pub_a  += rbr->static_files;
            pub_so += rbr->shared_files;
            dep_lr_self.object_files += rbr->object_files;
            dep_lr_self.runtime_files.insert(
                rbr->runtime_files.begin(), rbr->runtime_files.end());
        }
        else
            dep_lr_self.merge_from(rbr);
    }

    return rv.infos;
} //CxxContext::add_dep

} //namespace