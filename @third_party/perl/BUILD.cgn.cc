#include <cgn>

git("perl.git", x) {
    x.repo = "https://github.com/Perl/perl5.git";
    x.commit_id = "3a3761b09845d0d6d68f91eb45c52ae70de89489"; //5.41.7
    x.dest_dir = "repo";
}

sh_binary("perl.curl", x) {
    x.cwd = ".";
    x.cmd_build = {"curl", "-O", "https://www.cpan.org/src/5.0/perl-5.40.0.tar.gz"};
    x.outputs   = {"perl-5.40.0.tar.gz"};
}

// Using nmake for compilation will generate temporary files in the source code 
// directory. If Linux and Windows share the same monorepo, Windows may 
// incorrectly collect Linux-specific configuration files during copy_to_output. 
// Therefore, regardless of the host platform, copy_to_output should always be 
// executed to generate a separate copy of the source code.
//
// copy("copy_to_output", x) {
//     x.target_results = {
//         x.flat_copy_thisbase_to_output({"repo"})
//     };
// }
file_utility("copy_to_output", x) {
    x.cfg.visit_keys({"host_os", "host_cpu"});
    x.flat_copy_on_build({cgn::make_path_base_script("repo")}, cgn::make_path_base_out());
    x.ninja_outputs    = {cgn::make_path_base_out("repo"), cgn::make_path_base_out("repo/win32/Makefile")};
    x.analysis_outputs = {cgn::make_path_base_out("repo")};
}

// There has bug in perl build script 'win32/Makefile'
// It cannot be built in case-sensitive partition
nmake("perl_win", x) {
    auto srcdir = x.add_dep(":copy_to_output", x.cfg).outputs[0];
    x.cwd = api.rebase_path(srcdir + "/win32", api.get_runtime().src_prefix);

    x.makefile = "Makefile";
    x.outputs  = {"bin/perl.exe"};
    x.install_prefix_varname = "INST_TOP";
    x.override_vars["CCTYPE"] = "MSVC143"; //VS2022 (TODO: fetch from cxx interpreter)
}

// for windows os: using ":perl_win"
// otherwise: using os internal "perl"
custom_command("perl_host_exe", x) {
    std::string perl_exe = "perl";
    if (x.cfg["os"] == "win") {
        cgn::CGNTarget perlwin = x.add_dep(":perl_win", "host_release");
        x.opt->quickdep_ninja_full = {perlwin.ninja_entry};
        perl_exe = perlwin.outputs[0];
    }
    x.phase2_fn = [perl_exe](CustomCommand &x, cgn::CGNTargetOpt *opt) {
        opt->result.outputs = {perl_exe};
    };
}