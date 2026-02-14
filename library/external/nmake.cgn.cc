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
    
    cxx::CxxToolchainInfo cxx = cxx::CxxInterpreter::test_param(x.cfg, 
                                (x.autovar_cflags?"default":"minimum"));
    
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;
    
    // the real source code path
    std::string src_base = api.rebase_path(x.project_dir, ".", mk);
    
    // relocate 'src_base' if copy source code to ${output} required.
    cgn::NinjaFile::BuildSection *ninja_copy_target = nullptr;
    if (x.need_copy_src && mk->ninja) {
        std::string exclude_file_path;
        if (x.copy_exclude.size()) {
            //generate exclude.txt
            exclude_file_path = mk->out_prefix + "xcopy_exclude.txt";
            std::ofstream f3(exclude_file_path, std::ios::out);
            for (auto it : x.copy_exclude)
                f3<<api.rebase_path(it, ".", src_base)<<"\n";
            mk->ninja_file_appendix += {exclude_file_path};
        }

        cgn::NinjaFile::RuleSection *xcopy = mk->ninja->append_rule();
        xcopy->name = "nmake_copy";
        xcopy->command = "xcopy.exe /E /I /Y /h ${in} ${out} ${args}";
        xcopy->variables["description"] = "XCOPY ${in} -> ${out}";

        ninja_copy_target = mk->ninja->append_build();
        ninja_copy_target->rule = "nmake_copy";
        ninja_copy_target->inputs  = {cgn::NinjaFile::escape_path(src_base)};
        ninja_copy_target->outputs = {cgn::NinjaFile::escape_path(
                                      src_base = mk->out_prefix + "src")};
        
        // add copied Makefile to target output
        ninja_copy_target->implicit_outputs += {cgn::NinjaFile::escape_path(
            api.locale_path(src_base + "/" + x.nmake_run_dir + "/" + x.makefile))};
        if (exclude_file_path.size())
            ninja_copy_target->variables["args"] = "/EXCLUDE:" + exclude_file_path;
    }

    // prepare override_vars
    x.override_vars["CC"]  = cxx.exe_cc;
    x.override_vars["CPP"] = cxx.exe_cxx;
    x.override_vars["CXX"] = cxx.exe_cxx;
    x.override_vars["AS"]  = cxx.exe_asm;
    if (x.autovar_cflags) {
        auto append = [](std::string &tgt, const std::vector<std::string> &ls) {
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

    // the output files (return value)
    std::vector<std::string> nmake_outputs;
    for (auto it : x.outputs)
        nmake_outputs.push_back(api.locale_path(dir_install + "/" + it));
    mk->outputs = nmake_outputs;

    // generate build helper bat file
    if (!mk->ninja)
        return ;

    std::string pushd_cwd = api.rebase_path(x.nmake_run_dir, ".", src_base);
    std::string log_file  = mk->out_prefix + "install_log.log";

    // convert vars to string for nmake.exe argument
    std::string marcos_str;
    for (auto it : x.override_vars)
        marcos_str += api.shell_escape(it.first, "cmd") + "=" 
                    + api.shell_escape(it.second, "cmd") + " ";

    // write "nmake_build.bat"
    std::ofstream fout(mk->out_prefix + "nmake_build.bat");
    if (!fout)
        throw std::runtime_error{"nmake_interpret : cannot create " 
                                + mk->out_prefix + "nmake_build.bat"};
    
    std::string escaped_targets_str;
    for (auto &cmd_instl : x.target_installs)
        escaped_targets_str += api.shell_escape(cmd_instl, "cmd") + " ";
    std::string nmake_install_cmd = 
        "nmake.exe /NOLOGO /f " + api.shell_escape(api.locale_path(x.makefile), "cmd") + " " 
        + marcos_str + escaped_targets_str
        + " > " + api.shell_escape(log_file, "cmd") + "\n";
    std::string nmake_clear_cmd =
        "nmake.exe /NOLOGO /f " + api.shell_escape(api.locale_path(x.makefile), "cmd") + " " 
        + marcos_str + api.shell_escape(x.target_clean, "cmd")
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

    mk->ninja_file_appendix += {mk->out_prefix + "nmake_build.bat"};

    // build.ninja : import general rule. (require 'run' rule)
    constexpr const char *rule = "@cgn.d//library/utility/quick_run.ninja";
    static std::string rule_path = api.get_filepath(rule);
    mk->ninja->append_include(rule_path);

    // build.ninja : the entrypoint
    //  var["exe"] ${in} var["args"]
    auto *build = mk->ninja->append_build();
    build->rule = "run";
    build->variables["exe"] = "cmd.exe /c "; 
    build->variables["desc"] = "NMAKE " + mk->ninja->escape_path(mk->label);
    build->variables["restat"] = "1";
    build->inputs = {mk->ninja->escape_path(mk->out_prefix + "nmake_build.bat")};
    build->order_only = mk->ninja->escape_path(x.quickdep_ninja_target);
    build->outputs    = mk->ninja->escape_path(nmake_outputs);
    
    // add x.makefile to extra_watch_list
    auto makefile_before_copy = x.project_dir;
    makefile_before_copy.rpath += "/" + x.nmake_run_dir + "/" + x.makefile;
    x.extra_watch_files += {makefile_before_copy};

    // If copy_target existed, attach extra_watch to copy_target,
    // otherwise to current 'build'.
    if (ninja_copy_target)
        build->implicit_inputs = ninja_copy_target->outputs;
    for (auto &pth : x.extra_watch_files)
        (ninja_copy_target? ninja_copy_target:build)->implicit_inputs 
            += {cgn::NinjaFile::escape_path(api.rebase_path(pth, ".", mk))};

    // build.ninja : phony .ENTRY
    auto *phony = mk->ninja->append_build();
    phony->rule = "phony";
    phony->inputs  = build->outputs;
    phony->outputs = {mk->ninja->escape_path(mk->ninja_entry)};
} //NMakeInterpreter::interpret()
