#pragma once
#include "../../../cgn.h"

namespace cxx {

class VSEnvLoader
{
public:
    struct context_type {
        context_type(cgn::CGNTargetOpt *opt) : cfg(opt->cfg), opt(opt) {}

        cgn::Configuration &cfg;
        cgn::CGNTargetOpt *opt;
    };

    constexpr static cgn::ConstLabelGroup<0> preload_labels() { return {}; }

    static void interpret(context_type &x) {
        if (x.cfg["cxx_toolchain"] != "msvc" || x.cfg["host_os"] != "win")
            return x.opt->set_fail("CxxInterpreter internal error, "
                "only windows platform supported by current target");

        // https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170&redirectedfrom=MSDN#vcvarsall-syntax
        auto cnv = [](const std::string &in) -> std::string {
            if (in == "x86_64") return "x64";
            else return in;
        };
        std::string arg1;
        if (x.cfg["host_cpu"] == x.cfg["cpu"])
            arg1 = cnv(x.cfg["host_cpu"]);
        else
            arg1 = cnv(x.cfg["host_cpu"]) + "_" + cnv(x.cfg["cpu"]);

        cgn::CGNTargetMaker *maker = x.opt->confirm();
        if (!maker)
            return ;
        
        std::string script_gen = api.get_filepath("@cgn.d//library/cxx/vsenv_loader/msvc_quick_env.bat");
        script_gen = api.locale_path(script_gen);

        std::string arg0 = api.shell_escape(script_gen);
        if (arg0[0] == '@')
            arg0 = ".\\" + arg0;
        auto *rule = maker->ninja->append_rule();
        rule->name = "generate_msvc_env_bat";
        rule->command = "cmd.exe /c " + maker->ninja->escape_path(arg0) + " " + arg1 + " ${out}";
        rule->variables["depfile"] = "${out}.d";
        rule->variables["deps"]    = "gcc";
        
        auto *field = maker->ninja->append_build();
        field->inputs  = {maker->ninja->escape_path(script_gen)};
        field->outputs = {maker->ninja->escape_path(maker->out_prefix + "msvcenv.bat")};
        field->rule    = "generate_msvc_env_bat";

        auto *phony = maker->ninja->append_build();
        phony->rule = "phony";
        phony->inputs = field->outputs;
        phony->outputs = {maker->ninja->escape_path(maker->out_prefix + maker->NINJA_ENTRY_TARGET)};

        maker->outputs = {maker->out_prefix + "msvcenv.bat"};
    }
};

} //namespace

// #define cxx_msvc_prepare_env(name, x) CGN_RULE_DEFINE(cxx::VSEnvLoader, name, x)
// cxx_msvc_prepare_env("vsenv_loader", x) {}

CGN_RULE_DEFINE(cxx::VSEnvLoader, "vsenv_loader", x) {}
