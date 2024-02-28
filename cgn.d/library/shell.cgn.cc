#include "shell.cgn.h"

namespace shell {

TargetInfos ShellBinary::interpret(context_type &x, TargetOpt opt) {
    std::string shell_rule_file = glb.get_path(shell_rule)[0];
    NinjaFile fout(opt.fout_prefix + opt.BUILD_NINJA);
    
    //.build_stamp
    fout.append_include(shell_rule_file);
    auto *rule = fout.append_rule();
    rule->name = "exec";
    rule->command = x.main;
    for (auto &ss : x.args)
        rule->command+= " " + shell_escape(ss);

    auto *field = fout.append_build();
    field->rule = "exec";
    field->implicit_inputs = {x.inputs.begin(), x.inputs.end()};
    field->outputs.push_back(opt.fout_prefix + opt.BUILD_STAMP);
    for (auto &fname : x.outputs)
        field->implicit_outputs.push_back(opt.fout_prefix + fname);

    //.analysis_stamp
    auto *stamp = fout.append_build();
    stamp->rule = "stamp";
    stamp->inputs = glb.get_path(script_label);
    stamp->inputs.push_back(shell_rule_file);
    stamp->inputs.push_back(opt.fin_ulabel.substr(2));
    stamp->outputs.push_back(opt.fout_prefix + opt.ANALYSIS_STAMP);

    glb.register_ninjafile(opt.fout_prefix + opt.BUILD_NINJA);

    TargetInfos rv;
    DefaultInfo *oinf = rv.get<DefaultInfo>();
    oinf->target_label = opt.factory_ulabel;
    oinf->outputs = x.outputs;
    oinf->dep_scripts = {stamp->inputs.begin(), stamp->inputs.end()};
    return rv;
}

} //namespace
