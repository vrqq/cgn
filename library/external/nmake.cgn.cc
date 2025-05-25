#define NMAKE_CGN_IMPL
#include <fstream>
#include <cassert>
#include "nmake.cgn.h"
#include "../utility/copy.cgn.h"

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
        return x.opt->confirm_with_error(x.opt->factory_label + " OUTPUT required.");
    if (x.makefile.empty())
        return x.opt->confirm_with_error(x.opt->factory_label + " makefile required.");

    bool need_copy_src = x.build_dir_varname.empty();
    CopyWorker copy_worker;
    if (need_copy_src)
        copy_worker.preconfig(x.opt);
    
    cxx::CxxToolchainInfo cxx = cxx::CxxInterpreter::test_param(x.cfg, 
                                (x.auto_cflags_rel?"default":"minimum"));
    
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // the real source code path
    std::string src_dir = api.rebase_path(x.src_base, ".", opt);

    // if copy source code to ${output} required
    cgn::NinjaFile::BuildSection *ninja_copy_target = nullptr;
    if (need_copy_src) {
        std::string src = src_dir;
        src_dir = opt->out_prefix + "src";
        std::vector<std::string> src_exclude;
        for (auto it : x.inputs_exclude_rpath)
            src_exclude.push_back(api.locale_path(src + "/" + it));
        ninja_copy_target = copy_worker.postgen_flat_copy(opt, 
                {src + opt->path_separator + "*"}, src_exclude, src_dir);

    }

    // prepare override_vars
    x.override_vars["CC"]  = cxx.exe_cc;
    x.override_vars["CPP"] = cxx.exe_cxx;
    x.override_vars["CXX"] = cxx.exe_cxx;
    x.override_vars["AS"]  = cxx.exe_asm;    
    if (x.auto_cflags_rel) {
        auto append = [](std::string &tgt, const auto &ls) {
            if (tgt.size())
                tgt += " ";
            tgt += api.convert_list_to_string(ls, api.shell_escape);
        };
        append(x.override_vars["CFLAGS"],   cxx.arg.cflags + cxx.extra_cflags_c);
        append(x.override_vars["CPPFLAGS"], cxx.arg.cflags + cxx.extra_cflags_cpp);
        append(x.override_vars["CXXFLAGS"], cxx.arg.cflags + cxx.extra_cflags_cpp);
    }

    if (x.build_dir_varname.size()) {
        api.mkdir(opt->out_prefix + "build");
        x.override_vars[x.build_dir_varname] = api.rebase_path(opt->out_prefix + "build", "");
    }

    api.mkdir(opt->out_prefix + "install");
    std::string dir_install = opt->out_prefix + "install";
    x.override_vars[x.install_prefix_varname] = api.rebase_path(dir_install, "");

    // generate build helper bat file
    if (!opt->file_unchanged) {
        std::string pushd_cwd = api.rebase_path(x.nmake_run_dir, ".", src_dir);
        std::string log_file  = opt->out_prefix + "install_log.log";

        // generate build args
        std::string argstr_shesc;
        for (auto it : x.override_vars)
            argstr_shesc += api.shell_escape(it.first) + "=" + api.shell_escape(it.second) + " ";

        std::ofstream fout(opt->out_prefix + "nmake_build.bat");
        if (!fout)
            throw std::runtime_error{"nmake_interpret : cannot create " 
                                    + opt->out_prefix + "nmake_build.bat"};
        
        std::string nmake_install_cmd = 
            "nmake.exe /NOLOGO /f " + api.shell_escape(api.locale_path(x.makefile)) + " " 
            + argstr_shesc + api.shell_escape(x.install_target_name) 
            + " > " + api.shell_escape(log_file) + "\n";
        std::string nmake_clear_cmd =
            "nmake.exe /NOLOGO /f " + api.shell_escape(api.locale_path(x.makefile)) + " " 
            + argstr_shesc + api.shell_escape(x.clean_target_name)
            + " > " + api.shell_escape(log_file) + "\n";

        fout<<"@echo off\n"
            <<"call " + cxx.env_loader_script + "\n\n"
            <<"pushd " + pushd_cwd + "\n"
            <<nmake_install_cmd  //build first time
            <<"if %ERRORLEVEL% == 0 ( popd & exit /B 0 )\n\n"
            <<"echo\n"
            <<"echo --- Build Failed, try to clear and rebuild. ---\n" // >&2
            <<nmake_clear_cmd    //build failed, clear
            <<"if %ERRORLEVEL% NEQ 0 ( popd & exit /B %ERRORLEVEL% )\n\n"
            <<"echo\n"
            <<"echo --- REBUILD ---\n"
            <<nmake_install_cmd  //rebuild after clear
            <<"popd\n"
            <<"exit /B %ERRORLEVEL%\n\n";
        fout.close();
    }

    // the output files (return value)
    std::vector<std::string> aout_paths;
    for (auto it : x.outputs)
        aout_paths.push_back(dir_install + opt->path_separator + it);

    // build.ninja : import general rule. (require 'run' rule)
    constexpr const char *rule = "@cgn.d//library/utility/quick_run.ninja";
    static std::string rule_path = api.get_filepath(rule);
    opt->ninja->append_include(rule_path);

    // build.ninja : the entrypoint
    //  var["exe"] ${in} var["args"]
    auto *build = opt->ninja->append_build();
    build->rule = "run";
    build->variables["exe"] = "cmd.exe /c "; 
    build->inputs = {opt->ninja->escape_path(opt->out_prefix + "nmake_build.bat")};

    std::string makefile_path = api.locale_path(
        src_dir + "/" + x.nmake_run_dir + "/" + x.makefile);        
    if (ninja_copy_target) {
        build->implicit_inputs = ninja_copy_target->outputs;
        ninja_copy_target->implicit_outputs += {opt->ninja->escape_path(makefile_path)};
    }
    build->implicit_inputs += {opt->ninja->escape_path(makefile_path)};
    build->implicit_inputs += rebase_and_njesc(src_dir, x.inputs_rpath);
    build->implicit_inputs += opt->ninja->escape_path(opt->quickdep_ninja_full);
    build->order_only       = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
    build->outputs          = opt->ninja->escape_path(aout_paths);
    build->variables["desc"] = "NMAKE " + opt->ninja->escape_path(src_dir);
    if (need_copy_src)
        build->variables["restat"] = "1";

    // build.ninja : phony .ENTRY
    auto *phony = opt->ninja->append_build();
    phony->rule = "phony";
    phony->inputs  = build->outputs;
    phony->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};

    // rebase output files
    opt->result.outputs = aout_paths;
}
