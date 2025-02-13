#define NMAKE_CGN_IMPL
#include <fstream>
#include <cassert>
#include "nmake.cgn.h"

// static std::string two_escape(const std::string &in) {
//     return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
// }

static std::vector<std::string> rebase_and_njesc(
    const std::string base, const std::vector<std::string> &ls
) {
    std::vector<std::string> rv;
    for (auto &it : ls)
        rv.push_back(cgn::NinjaFile::escape_path( api.locale_path(base + "/" + it) ));
    return rv;
}

// Setting NMake Makefile directory: https://stackoverflow.com/a/59824258/12529885
// we use implicit_input and output to mark the running result rel on working-root
// and use cwd_xxx to mark the path rel to x.cwd
void NMakeInterpreter::interpret(context_type &x)
{
    assert(api.get_host_info().os == "win");
    if (x.outputs.empty())
        x.opt->confirm_with_error(x.opt->factory_label + " OUTPUT required.");
    if (x.cwd.empty())
        x.opt->confirm_with_error(x.opt->factory_label + " cwd required.");
    
    cxx::CxxToolchainInfo cxx = cxx::CxxInterpreter::test_param(x.cfg);
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    std::string wr_cwd    = api.locale_path(api.rebase_path(x.cwd, ".", opt->src_prefix));
    std::string wr_mkfile = api.locale_path(api.rebase_path(x.cwd, ".", opt->src_prefix) + "/" + x.makefile);
    std::string wr_instl  = opt->out_prefix + "install";
    // std::string cwd_build  = api.rebase_path(opt->out_prefix + "build", wr_cwd);
    std::string cwd_instl  = api.rebase_path(opt->out_prefix + "install", wr_cwd);
    std::string cwd_mkfile = api.locale_path(x.makefile);

    api.mkdir(opt->out_prefix + "install");
    // api.mkdir(opt->out_prefix + "build");

    x.override_vars["CC"]  = cxx.c_exe;
    x.override_vars["CPP"] = cxx.cxx_exe;
    x.override_vars["CXX"] = cxx.cxx_exe;
    // x.override_vars["MAKEDIR"] = wr_cwd;
    x.override_vars[x.install_prefix_varname] = cwd_instl;

    // import general rule.
    constexpr const char *rule = "@cgn.d//library/general.cgn.bundle/rule.ninja";
    static std::string rule_path = api.get_filepath(rule);
    opt->ninja->append_include(rule_path);

    // generate build args
    std::string argstr_shesc;
    for (auto it : x.override_vars)
        argstr_shesc += api.shell_escape(it.first) + "=" + api.shell_escape(it.second) + " ";

    // generate build helper bat file
    if (!opt->file_unchanged) {
        std::ofstream fout(opt->out_prefix + "nmake_build.bat");
        if (!fout)
        throw std::runtime_error{"nmake_interpret : cannot create " 
                                + opt->out_prefix + "nmake_build.bat"};
        
        std::string nmake_install_cmd = 
            "nmake.exe /NOLOGO /f " + api.shell_escape(cwd_mkfile) + " " 
            + argstr_shesc + api.shell_escape(x.install_target_name) + "\n";
        std::string nmake_clear_cmd =
            "nmake.exe /NOLOGO /f " + api.shell_escape(cwd_mkfile) + " " 
            + argstr_shesc + api.shell_escape(x.clean_target_name) + "\n";

        fout<<"@echo off\n"
            <<"pushd " + wr_cwd + "\n"
            <<nmake_install_cmd
            <<"if %ERRORLEVEL% == 0 ( popd & exit /B 0 )\n\n"
            <<"echo Build Failed, try to clear and rebuild. >&2\n"
            <<nmake_clear_cmd
            <<"if %ERRORLEVEL% NEQ 0 ( popd & exit /B %ERRORLEVEL% )\n\n"
            <<nmake_install_cmd
            <<"popd\n"
            <<"exit /B %ERRORLEVEL%\n\n";
        fout.close();
    }
    
    // generate build.ninja
    //  var["exe"] ${in} var["args"]
    auto *build = opt->ninja->append_build();
    std::string mkfile1 = api.locale_path(opt->src_prefix + x.cwd + "/" + x.makefile);
    build->rule = "run";
    build->variables["exe"] = "cmd.exe /c "; 
    build->inputs = {opt->ninja->escape_path(opt->out_prefix + "nmake_build.bat")};

    build->implicit_inputs  = {opt->ninja->escape_path(wr_mkfile)};
    build->implicit_inputs += rebase_and_njesc(opt->src_prefix, x.inputs);
    build->implicit_inputs += opt->ninja->escape_path(opt->quickdep_ninja_full);
    build->order_only       = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
    build->outputs          = rebase_and_njesc(wr_instl, x.outputs);
    build->variables["desc"] = "NMAKE " + opt->ninja->escape_path(wr_mkfile);

    // phony .ENTRY
    auto *phony = opt->ninja->append_build();
    phony->rule = "phony";
    phony->inputs  = build->outputs;
    phony->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};

    // rebase output files
    for (auto &it : x.outputs)
        opt->result.outputs += {api.locale_path(wr_instl + "/" + it)};
}
