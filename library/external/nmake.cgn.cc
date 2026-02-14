#define NMAKE_CGN_IMPL
#include <fstream>
#include <functional>
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
    if (x.cfg["host_os"] != "win")
        return x.opt->set_fail("NMake can only run on windows.");
    if (x.outputs.empty())
        return x.opt->set_fail("field OUTPUT required.");
    if (x.makefile.empty())
        return x.opt->set_fail("field makefile required.");

    bool need_copy_src = x.build_dir_varname.empty();
    CopyWorker copy_worker;
    if (need_copy_src)
        copy_worker.preconfig(x.opt);
    
    cxx::CxxToolchainInfo cxx = cxx::CxxInterpreter::test_param(x.cfg, 
                                (x.auto_cflags_rel?"default":"minimum"));
    
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;

    // the real source code path
    std::string src_dir = api.rebase_path(x.src_base, ".", mk);

    // if copy source code to ${output} required
    cgn::NinjaFile::BuildSection *ninja_copy_target = nullptr;
    if (need_copy_src) {
        std::string src = src_dir;
        src_dir = mk->out_prefix + "src";
        std::vector<std::string> src_exclude;
        for (auto it : x.inputs_exclude_rpath)
            src_exclude.push_back(api.locale_path(src + "/" + it));
        ninja_copy_target = copy_worker.postgen_flat_copy(mk, 
                {src + mk->PATH_SEPARATOR + "*"}, src_exclude, src_dir);
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
            tgt += api.convert_list_to_string(ls, std::bind(api.shell_escape, std::placeholders::_1, "cmd"));
        };
        append(x.override_vars["CFLAGS"],   cxx.c_arg.cflags);
        append(x.override_vars["CPPFLAGS"], cxx.cpp_arg.cflags);
        append(x.override_vars["CXXFLAGS"], cxx.cpp_arg.cflags);
    }

    if (x.build_dir_varname.size()) {
        api.mkdir(mk->out_prefix + "build");
        x.override_vars[x.build_dir_varname] = api.rebase_path(mk->out_prefix + "build", "");
    }

    api.mkdir(mk->out_prefix + "install");
    std::string dir_install = mk->out_prefix + "install";
    x.override_vars[x.install_prefix_varname] = api.rebase_path(dir_install, "");

    // generate build helper bat file
    if (!mk->file_unchanged) {
        std::string pushd_cwd = api.rebase_path(x.nmake_run_dir, ".", src_dir);
        std::string log_file  = mk->out_prefix + "install_log.log";

        // generate build args
        std::string argstr_shesc;
        for (auto it : x.override_vars)
            argstr_shesc += api.shell_escape(it.first, "cmd") + "=" 
                          + api.shell_escape(it.second, "cmd") + " ";

        std::ofstream fout(mk->out_prefix + "nmake_build.bat");
        if (!fout)
            throw std::runtime_error{"nmake_interpret : cannot create " 
                                    + mk->out_prefix + "nmake_build.bat"};
        
        std::string nmake_install_cmd = 
            "nmake.exe /NOLOGO /f " + api.shell_escape(api.locale_path(x.makefile), "cmd") + " " 
            + argstr_shesc + api.shell_escape(x.install_target_name, "cmd") 
            + " > " + api.shell_escape(log_file, "cmd") + "\n";
        std::string nmake_clear_cmd =
            "nmake.exe /NOLOGO /f " + api.shell_escape(api.locale_path(x.makefile), "cmd") + " " 
            + argstr_shesc + api.shell_escape(x.clean_target_name, "cmd")
            + " > " + api.shell_escape(log_file, "cmd") + "\n";

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
        aout_paths.push_back(dir_install + mk->PATH_SEPARATOR + it);

    // build.ninja : import general rule. (require 'run' rule)
    constexpr const char *rule = "@cgn.d//library/utility/quick_run.ninja";
    static std::string rule_path = api.get_filepath(rule);
    mk->ninja->append_include(rule_path);

    // build.ninja : the entrypoint
    //  var["exe"] ${in} var["args"]
    auto *build = mk->ninja->append_build();
    build->rule = "run";
    build->variables["exe"] = "cmd.exe /c "; 
    build->inputs = {mk->ninja->escape_path(mk->out_prefix + "nmake_build.bat")};

    std::string makefile_path = api.locale_path(
        src_dir + "/" + x.nmake_run_dir + "/" + x.makefile);        
    if (ninja_copy_target) {
        build->implicit_inputs = ninja_copy_target->outputs;
        ninja_copy_target->implicit_outputs += {mk->ninja->escape_path(makefile_path)};
    }
    build->implicit_inputs += {mk->ninja->escape_path(makefile_path)};
    build->implicit_inputs += rebase_and_njesc(src_dir, x.inputs_rpath);
    build->implicit_inputs += mk->ninja->escape_path(x.ninja_fulldeps);
    build->order_only       = mk->ninja->escape_path(x.quickdep_ninja_target);
    build->outputs          = mk->ninja->escape_path(aout_paths);
    build->variables["desc"] = "NMAKE " + mk->ninja->escape_path(src_dir);
    if (need_copy_src)
        build->variables["restat"] = "1";

    // build.ninja : phony .ENTRY
    auto *phony = mk->ninja->append_build();
    phony->rule = "phony";
    phony->inputs  = build->outputs;
    phony->outputs = {mk->ninja->escape_path(mk->ninja_entry)};

    // rebase output files
    mk->outputs = aout_paths;
}
