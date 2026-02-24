#include <cgn>
#include "@cgn.d/library/perl/test_module.cgn.h"

// Openssl 3.6.1
git("openssl.git", x) {
    x.repo = "https://github.com/openssl/openssl.git";
    x.commit_id = "c9a9e5b10105ad850b6e4d1122c645c67767c341";
    x.dest_dir = "repo";
}

// cxx_sources("openssl", x) {
//     if (x.cfg["os"] == "linux")
//         x.pub.ldflags = {"-lssl", "-lcrypto"};
// }

// here we use perl.exe and nasm.exe not in form of ninja ${in}
// but "export perl.exe" in command line, so we have to ref it
// in ninja->order_only[] form.
//
// Openssl is using perl to generate Makefile for all platforms.
//
// for windows https://github.com/openssl/openssl/blob/master/NOTES-WINDOWS.md
//
// Perl module dependency: perl-FindBin and others (dnf install perl-FindBin perl-IPC-Cmd ...) 
//
perl_test_module_existed("perl_test", x) {
    x.module_names = {"FindBin", "IPC::Cmd", "File::Compare", "File::Copy", "Pod::Html"};
}

shell_script("openssl3_build", x) {
    auto perl_target = x.quick_dep_namedcfg("@cgn.d//library/perl:host_exe", "host_release", false);
    auto perl_modtest = x.quick_dep(":perl_test", x.cfg, false);
    if (perl_target.outputs.size() == 0)
        return x.opt->set_fail("Perl exe not found");
    std::string perl_dep = perl_target.ninja_entry;
    std::string perl_exe = perl_target.outputs[0];
    x.extra_watch_files += {cgn::make_path_base_working(perl_dep)};

    auto nasm_target = x.quick_dep_namedcfg("@third_party//nasm", "host_release", false);
    if (nasm_target.outputs.size() == 0)
        return x.opt->set_fail("NASM exe not found");
    std::string nasm_exe = nasm_target.outputs[0];
    std::string nasm_dir = api.parent_path(nasm_exe);
    x.extra_watch_files += {cgn::make_path_base_working(nasm_exe)};

    std::string cfg_arg;

    // perl Configure [cfg_arg]
    bool msvc_msvcrt = (x.cfg["cxx_toolchain"]=="msvc" && (x.cfg["msvc_runtime"]=="MD" || x.cfg["msvc_runtime"]=="MDd"));
    bool msvc_libcmt = (x.cfg["cxx_toolchain"]=="msvc" && (x.cfg["msvc_runtime"]=="MT" || x.cfg["msvc_runtime"]=="MTd"));
    bool linux_llvm = (x.cfg["cxx_toolchain"]=="llvm");
    if (x.cfg["os"]=="win" && x.cfg["cpu"]=="x86" && msvc_msvcrt)
        cfg_arg = "VC-WIN32";
    else if (x.cfg["os"]=="win" && x.cfg["cpu"]=="x86_64" && msvc_msvcrt)
        cfg_arg = "VC-WIN64A";
    else if (x.cfg["os"]=="win" && x.cfg["cpu"]=="x86" && msvc_libcmt)
        cfg_arg = "VC-WIN32-HYBRIDCRT";
    else if (x.cfg["os"]=="win" && x.cfg["cpu"]=="x86_64" && msvc_libcmt)
        cfg_arg = "VC-WIN64A-HYBRIDCRT";
    else if (x.cfg["os"]=="linux" && x.cfg["cpu"]=="x86" && !linux_llvm)
        cfg_arg = "linux-x86";
    else if (x.cfg["os"]=="linux" && x.cfg["cpu"]=="x86_64" && !linux_llvm)
        cfg_arg = "linux-x86_64";
    else if (x.cfg["os"]=="linux" && x.cfg["cpu"]=="x86" && linux_llvm)
        cfg_arg = "linux-x86-clang";
    else if (x.cfg["os"]=="linux" && x.cfg["cpu"]=="x86_64" && linux_llvm)
        cfg_arg = "linux-x86_64-clang";
    else {
        x.opt->set_fail("Unsupported platform.");
        return ;
    }

    // --debug --release
    std::string arg_build_type = (x.cfg["optimization"] == "debug"?"--debug":"--release");

    // CC and CFLAGS
    if (x.cfg["os"] == "linux") {
        cxx::CxxToolchainInfo cxx_toolchain = cxx::CxxInterpreter::test_param(x.cfg, "minimum");
        std::string cflags_str = api.convert_list_to_string(cxx_toolchain.c_arg.cflags);
        x.worker.append_setenv("CC", cxx_toolchain.exe_cc);
        x.worker.append_setenv("CFLAGS", api.shell_escape(cflags_str, x.cfg["host_shell"]));
    }

    // --with-zlib-lib, --with-zlib-include. (TODO)
    // cgn::CGNTarget zlib = x.add_dep("@third_party//zlib", x.cfg);
    // std::string arg_zlib = zlib.get<BinDevelInfo>()->base;

    // --with-zstd-lib, --with-zstd-include. (TODO)
    // cgn::CGNTarget zstd = x.add_dep("@third_party//zstd", x.cfg);
    // std::string arg_zstd = zlib.get<BinDevelInfo>()->base;

    // opt confirm
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;
    
    cgn::CGNPath build_dir  = cgn::make_path_base_out("build");
    std::string install_dir = api.rebase_path(cgn::make_path_base_out("install"), "", mk);
    std::string etc_dir     = api.rebase_path(cgn::make_path_base_out("etc"), "", mk);
    std::string build_log   = api.rebase_path(cgn::make_path_base_out("build.log"), "", mk);
    std::string instl_log   = api.rebase_path(cgn::make_path_base_out("install.log"), "", mk);
    std::string config_log  = api.rebase_path(cgn::make_path_base_out("config.log"), "", mk);
    api.mkdir(api.rebase_path(build_dir, "", mk));
    api.mkdir(install_dir);
    api.mkdir(etc_dir);

    // compile output
    if (x.cfg["os"] == "win"){
        x.script_outputs = {
            cgn::make_path_base_out("install/bin/openssl.exe"),
            cgn::make_path_base_out("install/lib/libssl.lib"),
            cgn::make_path_base_out("install/bin/libssl-3-x64.dll"),
            cgn::make_path_base_out("install/lib/libcrypto.lib"),
            cgn::make_path_base_out("install/bin/libcrypto-3-x64.dll"),
        };
        if (x.cfg["optimization"] == "debug")
            x.script_outputs += {
                cgn::make_path_base_out("install/bin/libssl-3-x64.pdb"),
                cgn::make_path_base_out("install/bin/libcrypto-3-x64.pdb")
            };
    }
    else 
        x.script_outputs = {
            cgn::make_path_base_out("install/bin/openssl"),
            cgn::make_path_base_out("install/lib64/libssl.a"),
            cgn::make_path_base_out("install/lib64/libcrypto.a")
        };

    // prepare enviromnent for make / nmake
    if (x.cfg["os"] == "win")
        x.worker.append_setenv("PATH", nasm_dir + ";%PATH%");
    else
        x.worker.append_setenv("PATH", nasm_dir + ":$PATH");
    x.worker.append_setenv("PERL", perl_exe);

    // https://github.com/openssl/openssl/blob/master/INSTALL.md#out-of-tree-builds
    x.worker.append_pushd(build_dir, mk);
    std::string src_cfgdir = api.rebase_path("repo/Configure", "", x.opt->src_prefix);
    x.worker.append_escaped_cmd(
        api.shell_escape(perl_exe, x.cfg["host_shell"]) 
        + " " + api.shell_escape(src_cfgdir, x.cfg["host_shell"])
        + " " + cfg_arg + " " + arg_build_type
        + " --prefix=" + api.shell_escape(install_dir, x.cfg["host_shell"])
        + " --openssldir=" + api.shell_escape(etc_dir, x.cfg["host_shell"])
        + " > " + api.shell_escape(config_log, x.cfg["host_shell"])
    );

    if (x.cfg["os"] == "win") {
        x.worker.append_escaped_cmd({"nmake > " + api.shell_escape(build_log, x.cfg["host_shell"])});
        x.worker.append_escaped_cmd({"nmake install > " + api.shell_escape(instl_log, x.cfg["host_shell"])});
    }else{
        x.worker.append_escaped_cmd({"make -j > " + api.shell_escape(build_log, x.cfg["host_shell"])});
        x.worker.append_escaped_cmd("make install > " + api.shell_escape(instl_log, x.cfg["host_shell"]));
    }

    x.worker.append_popd(); // for touch .stamp in interpreter

    // result output
    x.analysis_outputs = {
        cgn::make_path_base_out("install"),
        cgn::make_path_base_out("etc")
    };

    // TODO: the form of bin_devel hasn't been determined.
    // auto *bin_devel = x.analysis_infos.get<BinDevelInfo>(true);
    // bin_devel->base = x.rebase_path(cgn::make_path_base_out("install"));
}

// custom_command("openssl3_exe", x) {
//     cgn::CGNTarget buildt = x.add_dep(":openssl3_build", x.cfg);
//     std::string inst_dir = buildt.outputs[0];
//     if (x.cfg["os"] == "win")
//         x.analysis_outputs = {
//             cgn::make_path_base_working(inst_dir + "/bin/openssl.exe")
//         };
//     else
//         x.analysis_outputs = {
//             cgn::make_path_base_working(inst_dir + "/bin/openssl")
//         };
// }

// dlopen are required by libcrypto-lib-dso_dlfcn.o in libcrypto.a
cxx_prebuilt("openssl3_static" , x) {
    cgn::CGNTarget buildt = x.add_dep(":openssl3_build");
    auto instdir = buildt.outputs[0];

    x.pub.include_dirs = {cgn::make_path_base_working(instdir + "/include")};
    if (x.cfg["cxx_toolchain"] == "msvc") {
        return ;
        // x.opt->confirm_with_error("In windows MSVC, no static library supported, using shared library instead.");
        // return ;
    }
    else {
        x.files = {
            cgn::make_path_base_working(instdir + "/lib64/libssl.a"),
            cgn::make_path_base_working(instdir + "/lib64/libcrypto.a"),
        };
        x.system_libs = {"dl"};
    }
}

cxx_prebuilt("openssl3_shared", x) {
    cgn::CGNTarget buildt = x.add_dep(":openssl3_build");
    auto instdir = buildt.outputs[0];

    x.pub.include_dirs = {instdir + "/include"};
    if (x.cfg["cxx_toolchain"] == "msvc"){
        x.files = {
            cgn::make_path_base_working(instdir + "/lib/libssl.lib"),
            cgn::make_path_base_working(instdir + "/bin/libssl-3-x64.dll"),
            cgn::make_path_base_working(instdir + "/lib/libcrypto.lib"),
            cgn::make_path_base_working(instdir + "/bin/libcrypto-3-x64.dll"),
        };
        if (x.cfg["optimization"] == "debug")
            x.files += {
                cgn::make_path_base_working(instdir + "/bin/libssl-3-x64.pdb"),
                cgn::make_path_base_working(instdir + "/bin/libcrypto-3-x64.pdb")
            };
    }
    else
        x.files = {
            cgn::make_path_base_working(instdir + "/lib64/libssl.so"),
            cgn::make_path_base_working(instdir + "/lib64/libssl.so.3"),
            cgn::make_path_base_working(instdir + "/lib64/libcrypto.so"),
            cgn::make_path_base_working(instdir + "/lib64/libcrypto.so.3")
        };
}

cxx_sources("system_prebuilt", x) {
    if (x.cfg["os"] != "win")
        x.pub.ldflags = {"-lssl", "-lcrypto"};
    else
        x.pub.ldflags = {"/Lssl", "/Lcrypto"};
}

alias("openssl", x) {
    if (x.cfg["cxx_toolchain"] == "msvc")
        x.actual_label = ":openssl3_shared";
    else
        x.actual_label = ":openssl3_static";

    // x.actual_label = ":system_prebuilt";
}

alias("host_openssl3_exe", x) {
    x.actual_label = ":openssl3_exe";
    x.load_named_config("host_release");
}
