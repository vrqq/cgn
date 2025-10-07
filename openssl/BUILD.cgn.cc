#include <cgn>
#include "@cgn.d/library/perl/test_module.cgn.h"

// Openssl 3.4.0
git("openssl.git", x) {
    x.repo = "https://github.com/openssl/openssl.git";
    x.commit_id = "98acb6b02839c609ef5b837794e08d906d965335";
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
custom_command("openssl3_build", x) {
    auto perl_target = x.add_dep("@cgn.d//library/perl:host_exe", "host_release");
    auto perl_modtest = x.add_dep(":perl_test", x.cfg);
    if (perl_target.outputs.size() == 0)
        return x.opt_confirm_error("Perl exe not found");
    std::string perl_dep = perl_target.ninja_entry;
    std::string perl_exe = perl_target.outputs[0];
    x.watch_inputs += {cgn::make_path_base_working(perl_dep)};
    x.watch_orderonly += {cgn::make_path_base_working(perl_modtest.ninja_entry)};

    auto nasm_target = x.add_dep("@third_party//nasm", "host_release");
    if (nasm_target.outputs.size() == 0)
        return x.opt_confirm_error("NASM exe not found");
    std::string nasm_exe = nasm_target.outputs[0];
    std::string nasm_dir = api.parent_path(nasm_exe);
    x.watch_inputs += {cgn::make_path_base_working(nasm_exe)};

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
        x.opt_confirm_error("Unsupported platform.");
        return ;
    }

    // --debug --release
    std::string arg_build_type = (x.cfg["optimization"] == "debug"?"--debug":"--release");

    // --with-zlib-lib, --with-zlib-include. (TODO)
    // cgn::CGNTarget zlib = x.add_dep("@third_party//zlib", x.cfg);
    // std::string arg_zlib = zlib.get<BinDevelInfo>()->base;

    // --with-zstd-lib, --with-zstd-include. (TODO)
    // cgn::CGNTarget zstd = x.add_dep("@third_party//zstd", x.cfg);
    // std::string arg_zstd = zlib.get<BinDevelInfo>()->base;

    // opt confirm
    if (x.opt_confirm_cached())
        return ;
    
    cgn::CGNPath build_dir  = cgn::make_path_base_out("build");
    std::string install_dir = x.rebase_path(cgn::make_path_base_out("install"), "");
    std::string etc_dir     = x.rebase_path(cgn::make_path_base_out("etc"), "");

    std::string build_log   = x.rebase_path(cgn::make_path_base_out("build.log"), "");
    std::string instl_log   = x.rebase_path(cgn::make_path_base_out("install.log"), "");
    std::string config_log  = x.rebase_path(cgn::make_path_base_out("config.log"), "");
    api.mkdir(x.rebase_path(build_dir));
    api.mkdir(install_dir);
    api.mkdir(etc_dir);

    // compile output
    if (x.cfg["os"] == "win"){
        x.watch_outputs = {
            cgn::make_path_base_out("install/bin/openssl.exe"),
            cgn::make_path_base_out("install/lib/libssl.lib"),
            cgn::make_path_base_out("install/bin/libssl-3-x64.dll"),
            cgn::make_path_base_out("install/lib/libcrypto.lib"),
            cgn::make_path_base_out("install/bin/libcrypto-3-x64.dll"),
        };
        if (x.cfg["optimization"] == "debug")
            x.watch_outputs += {
                cgn::make_path_base_out("install/bin/libssl-3-x64.pdb"),
                cgn::make_path_base_out("install/bin/libcrypto-3-x64.pdb")
            };
    }
    else 
        x.watch_outputs = {
            cgn::make_path_base_out("install/bin/openssl"),
            cgn::make_path_base_out("install/lib64/libssl.a"),
            cgn::make_path_base_out("install/lib64/libcrypto.a")
        };

    // prepare enviromnent for make / nmake
    if (x.cfg["os"] == "win")
        x.append_setenv("PATH", nasm_dir + ";%PATH%");
    else
        x.append_setenv("PATH", nasm_dir + ":$PATH");
    x.append_setenv("PERL", perl_exe);

    // https://github.com/openssl/openssl/blob/master/INSTALL.md#out-of-tree-builds
    x.append_pushd(build_dir);
    std::string src_cfgdir = x.rebase_path("repo/Configure", "");
    x.append_escaped_cmd(
        api.shell_escape(perl_exe) 
        + " " + api.shell_escape(src_cfgdir)
        + " " + cfg_arg + " " + arg_build_type
        + " --prefix=" + api.shell_escape(install_dir)
        + " --openssldir=" + api.shell_escape(etc_dir)
        + " > " + api.shell_escape(config_log)
    );

    if (x.cfg["os"] == "win") {
        x.append_escaped_cmd({"nmake > " + api.shell_escape(build_log)});
        x.append_escaped_cmd({"nmake install > " + api.shell_escape(instl_log)});
    }else{
        x.append_escaped_cmd({"make -j > " + api.shell_escape(build_log)});
        x.append_escaped_cmd("make install > " + api.shell_escape(instl_log));
    }

    x.append_popd(); // for touch .stamp in interpreter

    // result output
    x.analysis_outputs = {
        cgn::make_path_base_out("install"),
        cgn::make_path_base_out("etc")
    };

    // TODO: the form of bin_devel hasn't been determined.
    // auto *bin_devel = x.analysis_infos.get<BinDevelInfo>(true);
    // bin_devel->base = x.rebase_path(cgn::make_path_base_out("install"));
}

custom_command("openssl3_exe", x) {
    cgn::CGNTarget buildt = x.add_dep(":openssl3_build", x.cfg);
    std::string inst_dir = buildt.outputs[0];
    if (x.cfg["os"] == "win")
        x.analysis_outputs = {
            cgn::make_path_base_working(inst_dir + "/bin/openssl.exe")
        };
    else
        x.analysis_outputs = {
            cgn::make_path_base_working(inst_dir + "/bin/openssl")
        };
}

cxx_prebuilt("openssl3_static" , x) {
    cgn::CGNTarget buildt = x.add_dep(":openssl3_build");
    auto instdir = buildt.outputs[0];

    x.pub.include_dirs = {cgn::make_path_base_working(instdir + "/include")};
    if (x.cfg["cxx_toolchain"] == "msvc") {
        return ;
        // x.opt->confirm_with_error("In windows MSVC, no static library supported, using shared library instead.");
        // return ;
    }
    else
        x.files = {
            cgn::make_path_base_working(instdir + "/lib64/libssl.a"),
            cgn::make_path_base_working(instdir + "/lib64/libcrypto.a")
        };
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

cxx_sources("host_prebuilt", x) {
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

    // x.actual_label = ":host_prebuilt";
}

alias("host_exe", x) {
    x.actual_label = ":openssl3_exe";
    x.load_named_config("host_release");
}
