#include <cgn>

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
// for windows https://github.com/openssl/openssl/blob/master/NOTES-WINDOWS.md
custom_command("openssl3_build", x) {
    auto perl_target = x.add_dep("@third_party//perl:perl_host_exe", "host_release");
    if (perl_target.outputs.size() == 0)
        return x.opt_confirm_error("Perl exe not found");
    std::string perl_dep = perl_target.ninja_entry;
    std::string perl_exe = perl_target.outputs[0];
    x.watch_inputs += {cgn::make_path_base_working(perl_dep)};

    auto nasm_target = x.add_dep("@third_party//nasm", "host_release");
    if (nasm_target.outputs.size() == 0)
        return x.opt_confirm_error("NASM exe not found");
    std::string nasm_exe = nasm_target.outputs[0];
    std::string nasm_dir = api.parent_path(nasm_exe);
    x.watch_inputs += {cgn::make_path_base_working(nasm_exe)};

    // auto zstd_target = x.add_dep("@third_party//zstd", x.cfg);
    // std::string zstd_include = zstd_target.get<cxx::CxxInfo>(false)->include_dirs[0];
    // std::string zstd_lib     = 

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

    // opt confirm
    if (x.opt_confirm_cached())
        return ;
    
    cgn::CGNPath build_dir  = cgn::make_path_base_out("build");
    std::string install_dir = x.rebase_path(cgn::make_path_base_out("install"), "");
    std::string etc_dir     = x.rebase_path(cgn::make_path_base_out("etc"), "");
    api.mkdir(x.rebase_path(build_dir));
    api.mkdir(install_dir);
    api.mkdir(etc_dir);

    // compile output
    if (x.cfg["os"] == "win")
        x.watch_outputs = {
            cgn::make_path_base_out("install/bin/openssl.exe"),
            cgn::make_path_base_out("install/lib/openssl.lib")
        };
    else 
        x.watch_outputs = {
            cgn::make_path_base_out("install/bin/openssl"),
            cgn::make_path_base_out("install/lib/libopenssl.a")
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
    x.append_cmd({perl_exe, src_cfgdir, cfg_arg, 
        "--prefix=" + install_dir,
        "--openssldir=" + etc_dir
    });

    if (x.cfg["os"] == "win")
        x.append_cmd({"nmake"}), x.append_cmd({"nmake", "install"});
    else
        x.append_cmd({"make", "-j"}), x.append_cmd({"make", "install"});

    x.append_popd(); // for touch .stamp in interpreter

    // result output
    x.analysis_outputs = {
        cgn::make_path_base_out("install"),
        cgn::make_path_base_out("etc")
    };
}

cxx_prebuilt("openssl3_static" , x) {
    cgn::CGNTarget buildt = x.add_dep(":openssl3_build");
    auto instdir = buildt.outputs[0];

    x.pub.include_dirs = {instdir + "/include"};
    if (x.cfg["cxx_toolchain"] == "msvc")
        x.files = {
            cgn::make_path_base_working(instdir + "/lib/openssl.lib")
        };
    else
        x.files = {cgn::make_path_base_working(instdir + "/lib/libopenssl.a")};
}

alias("openssl", x) {
    x.actual_label = ":openssl3_static";
}
