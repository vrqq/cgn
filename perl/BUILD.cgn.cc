#include <cgn>

git("perl.git", x) {
    x.repo = "https://github.com/Perl/perl5.git";
    x.commit_id = "bc4cd0b708bd15e68ef7f85105f7edb5be1f0f58"; //5.41.12
    x.dest_dir = "repo";
}

run_exec("perl.curl", x) {
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
copy_files("copy_to_output", x) {
    // x.cfg.visit_keys({"host_os", "host_cpu"});
    x.flat_copy_on_build(
        {cgn::make_path_base_script("repo")}, 
        {cgn::make_path_base_script("repo/.git"), 
         cgn::make_path_base_script("repo/.github"), 
         cgn::make_path_base_script("repo/.gitattributes")
        }, 
        cgn::make_path_base_out()
    );

    x.ninja_build_trigger = {cgn::make_path_base_out("repo"), cgn::make_path_base_out("repo/win32/Makefile")};
    x.analysis_outputs    = {cgn::make_path_base_out("repo")};
}

// There has bug in perl build script 'win32/Makefile'
// It cannot be built in case-sensitive partition
nmake("perl_win", x) {
    x.project_dir = "repo";
    x.nmake_run_dir = "win32";
    x.makefile = "Makefile";

    x.outputs  = {"bin/perl.exe", "bin/wperl.exe"};
    x.install_prefix_varname = "INST_TOP";
    x.override_vars["CCTYPE"] = "MSVC143";  //VS2022 (TODO: fetch from cxx interpreter)

    if (x.cfg["optimization"] == "debug"){
        if (x.cfg["msvc_runtime"] == "MD")
            x.override_vars["CFG"] = "Debug";
        else if (x.cfg["msvc_rutime"] == "MDd")
            x.override_vars["CFG"] = "DebugFull";
        else
            x.set_fail("Unsupported msvc_runtime " + x.cfg["msvc_runtime"].string());
    }

    x.need_copy_src = true;
    x.copy_exclude = {".git"};
    x.extra_watch_files = {"repo/.git/HEAD"};
}

// for windows os: using ":perl_win"
// otherwise: using os internal "perl"
// custom_command("perl_host_exe", x) {
//     cgn::CGNPath perl_exe = cgn::make_path_base_working("perl");
//     if (x.cfg["host_os"] == "win") {
//         cgn::CGNTarget perlwin = x.add_dep(":perl_win", "host_release");
//         x.watch_inputs = {perl_exe = cgn::make_path_base_working(perlwin.outputs[0])};
//     }
//     x.analysis_outputs = {perl_exe};
// }
