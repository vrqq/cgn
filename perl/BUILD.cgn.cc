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

copy("copy_to_output", x) {
    x.target_results = {
        x.flat_copy_thisbase_to_output({"repo"})
    };
}


// There has bug in perl build script 'win32/Makefile'
// It cannot be built in case-sensitive partition
nmake("perl_win", x) {
    x.makefile = "Makefile";
    x.cwd      = "repo/win32";
    x.outputs  = {"bin/perl.exe"};
    x.install_prefix_varname = "INST_TOP";
    x.override_vars["CCTYPE"] = "MSVC143"; //VS2022 (TODO: fetch from cxx interpreter)

    // win32 bug fixed
    if (x.cfg["FIX_src_case_sensitive"] != "") {
        auto srcdir = x.add_dep(":copy_to_output", x.cfg).outputs[0];
        x.cwd = api.rebase_path(srcdir, api.get_runtime().src_prefix);
    }
    // if (api.is_directory_case_sensitive(x.opt->src_prefix))
    //     x.opt->confirm_with_error("Perl on windows can only be compiled on case insensitive directory.");
}
