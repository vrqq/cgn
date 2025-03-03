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
custom_command("openssl", x) {
    auto perl_target = x.add_dep("@third_party//perl:perl_host_exe", "host_release");
    if (perl_target.outputs.size() == 0)
        return x.opt->confirm_with_error("Perl exe not found");
    std::string perl_exe = perl_target.outputs[0];
    x.opt->quickdep_ninja_dynhdr += {perl_target.ninja_entry};

    auto nasm_target = x.add_dep("@third_party//nasm", "host_release");
    if (nasm_target.outputs.size() == 0)
        return x.opt->confirm_with_error("NASM exe not found");
    std::string nasm_exe = nasm_target.outputs[0];
    std::string nasm_dir = api.parent_path(nasm_exe);
    x.opt->quickdep_ninja_dynhdr += {nasm_target.ninja_entry};

    // auto zstd_target = x.add_dep("@third_party//zstd", x.cfg);
    // std::string zstd_include = zstd_target.get<cxx::CxxInfo>(false)->include_dirs[0];
    // std::string zstd_lib     = 

    std::string openssl_src = x.opt->src_prefix + "repo";
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
        x.opt->confirm_with_error("Unsupported platform.");
        return ;
    }

    // --debug --release
    std::string arg_build_type = (x.cfg["optimization"] == "debug"?"--debug":"--release");

    x.phase2_fn = [=](CustomCommand &x, cgn::CGNTargetOpt *opt) {
        std::string build_dir   = opt->out_prefix + "build";
        std::string install_dir_abs = api.rebase_path(opt->out_prefix + "install", "");
        std::string etc_dir_abs     = api.rebase_path(opt->out_prefix + "etc", "");

        api.mkdir(build_dir);
        api.mkdir(install_dir_abs);
        api.mkdir(etc_dir_abs);

        if (x.cfg["os"] == "win")
            x.append_setenv("PATH", nasm_dir + ";%PATH%");
        else
            x.append_setenv("PATH", nasm_dir + ":$PATH");
        x.append_setenv("PERL", perl_exe);

        // https://github.com/openssl/openssl/blob/master/INSTALL.md#out-of-tree-builds
        x.append_cmd({"cd", build_dir});
        std::string cfg_absdir = api.rebase_path(openssl_src + "/Configure", "");
        x.append_cmd({perl_exe, cfg_absdir, cfg_arg, 
            "--prefix", install_dir_abs,
            "--openssldir", etc_dir_abs
        });
        if (x.cfg["os"] == "win")
            x.append_cmd({"nmake"}), x.append_cmd({"nmake", "install"});
        else
            x.append_cmd({"make"}), x.append_cmd({"make", "install"});
    };
}