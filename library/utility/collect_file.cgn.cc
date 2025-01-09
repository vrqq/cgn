#define CGN_UTILITY_IMPL
#include "collect_file.cgn.h"

CopyInterpreter::CopyResult 
CopyInterpreter::CopyContext::copy_wrbase_to_output(
    const std::vector<std::string> rel_srcs,
    const std::string &src_base_prefix_of_working_root, 
    const std::string &rel_in_output
) {
    for (auto src: rel_srcs){
        Record rec;
        rec.src_path = src_base_prefix_of_working_root + src;
        rec.out_dir  = rel_in_output + api.parent_path(src);
        rec.out_filename = api.filename_of_path(src);
        records.push_back(rec);
    }
    return CopyResult{CopyResult::BASE_ON_OUTPUT, rel_in_output};
}

std::vector<CopyInterpreter::CopyResult> 
CopyInterpreter::CopyContext::flat_copy_thisbase_to_output(
    const std::vector<std::string> rel_srcs,
    const std::string &rel_in_output
) {
    std::string sep = (rel_in_output.size() && rel_in_output.back() != '/')?"/":"";
    
    std::vector<CopyInterpreter::CopyResult> rv;
    for (auto src : rel_srcs){
        Record rec;
        rec.src_path = opt->src_prefix + src;
        rec.out_dir  = rel_in_output;
        rec.out_filename = api.filename_of_path(rec.src_path);
        records.push_back(rec);
        rv.push_back({CopyResult::BASE_ON_OUTPUT, rel_in_output + sep + rec.out_filename});
    }
    return rv;
}

void CopyInterpreter::interpret(context_type &x)
{
    std::string cprule = (x.cfg["host_os"] == "win"? "win_cp_to_dir" : "unix_cp_to_dir");
    cgn::CGNTargetOpt *opt = x.opt->confirm();

    std::string rulepath = api.get_filepath("@cgn.d//library/general.cgn.bundle/rule.ninja");
    if (!opt->ninja->is_file_included(rulepath))
        opt->ninja->append_include(rulepath);

    auto esc = [&](const std::string &in) {
        return opt->ninja->escape_path(api.shell_escape(api.locale_path(in)));
    };

    auto *phony = opt->ninja->append_build();
    phony->rule = "phony";
    phony->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
    for (auto rec : x.records) {
        auto *field = opt->ninja->append_build();
        field->rule = cprule;
        field->variables["out_dir"] = esc(opt->out_prefix + rec.out_dir + "/");
        field->inputs  = {esc(rec.src_path)};
        field->outputs = {esc(opt->out_prefix + rec.out_dir + "/" + rec.out_filename)};
        field->implicit_inputs = opt->quickdep_ninja_full;
        field->order_only      = opt->quickdep_ninja_dynhdr;
        phony->inputs += field->outputs;
    }

    for (auto item : x.target_results) {
        if (item.type == item.BASE_ON_OUTPUT)
            opt->result.outputs += {api.locale_path(opt->out_prefix + item.relpath)};
        else
            opt->result.outputs += {api.locale_path(item.relpath)};
    }
}
