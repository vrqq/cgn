//
// example of build.ninja
// 
#include "../cgn.h"
#include "shell.cgn.h"

namespace shell {

cgn::TargetInfos ShellBinary::interpret(context_type &x, cgn::CGNTargetOpt opt) {
    //.build_stamp
    // static std::string shell_rule_path = cgn.get_filepath(shell_rule);
    // opt.ninja->append_include(shell_rule_path);

    auto *rule = opt.ninja->append_rule();
    rule->name = "exec";
    rule->command = x.main;
    for (auto &ss : x.args)
        rule->command+= " " + cgn::Tools::shell_escape(ss);

    auto *field = opt.ninja->append_build();
    field->rule = "exec";
    field->implicit_inputs = {x.inputs.begin(), x.inputs.end()};

    field->outputs.push_back(opt.out_prefix + opt.BUILD_ENTRY);
    for (auto &fname : x.outputs)
        field->implicit_outputs.push_back(opt.out_prefix 
                        + cgn::Tools::locale_path(fname));

    cgn::TargetInfos rv;
    cgn::DefaultInfo *oinf = rv.get<cgn::DefaultInfo>(true);
    oinf->target_label = opt.factory_ulabel;
    oinf->outputs = x.outputs;
    return rv;
}

} //namespace
