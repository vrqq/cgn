#define CGN_LIBRARY_COPY_IMPL

#include <sstream>
#include <fstream>
#include "copy.cgn.h"

// Class CopyWorker
// ================

std::string CopyWorker::preconfig(cgn::CGNTargetOpt *opt, const std::string &argfile_prefix)
{
    this->argfile_prefix = argfile_prefix;
    
    // copy rule configuration
    // advcopy.exe varies by host_os and host_cpu.
    cgn::QuickDepContext qdep{opt};
    cgn::CGNTarget advcopy = qdep.quick_dep_namedcfg("@cgn.d//advcopy", "host_release", false);
    // opt->cfg.visit_keys({"host_os", "host_cpu"});
    if (advcopy.errmsg.size() || advcopy.outputs.empty())
        return "Cannot load advcopy: " + advcopy.errmsg;
    advcopy_exe_2esc = cgn::NinjaFile::escape_path(api.shell_escape(advcopy.outputs[0], opt->cfg["host_shell"]));
    return "";
}

cgn::NinjaFile::BuildSection* CopyWorker::mkninja(
    cgn::CGNTargetMaker *mk, const std::string &command,
    const std::vector<std::string> &arg_content,
    const std::vector<std::string> &njtargets_orderdep
) {
    // if (mk->file_unchanged)
    //     return target_n++, nullptr;
    
    if (target_n == 0) {
        std::string rulepath = api.get_filepath("@cgn.d//library/utility/advcopy.ninja");
        mk->ninja->append_include(rulepath);
    }
    
    // generate copy_<i>.rsp
    std::string path_stub = mk->out_prefix + this->argfile_prefix 
                             + std::to_string(target_n++);
    if (mk->file_unchanged == false) {
        std::stringstream argout;
        argout<<"@MF " + path_stub + ".stamp.d\n"
              <<"@stamp " + path_stub + ".stamp\n";
        for (const auto &arg : arg_content)
            argout << arg << "\n";
        api.write_file_content_if_changed(path_stub + ".rsp", argout.str());
    }

    auto *field = mk->ninja->append_build();
    field->rule = "advcopy";
    field->variables["subcmd"] = command;
    field->variables["desc"] = command + " " + *(++arg_content.rbegin()) 
                             + " -> " + arg_content.back();
    field->variables["exe"] = advcopy_exe_2esc;
    field->implicit_inputs  = {advcopy_exe_2esc};
    field->order_only       = cgn::NinjaFile::escape_path(njtargets_orderdep);
    field->inputs  = {mk->ninja->escape_path(path_stub + ".rsp")};
    field->outputs = {mk->ninja->escape_path(path_stub + ".stamp")};
    return field;
}

cgn::NinjaFile::BuildSection* CopyWorker::postgen_copy_rename(
    cgn::CGNTargetMaker *confirmed_mk,
    const std::string &src_file, const std::string &dst_file,
    const std::vector<std::string> &njtargets_orderdep
) {
    return this->mkninja(
        confirmed_mk, "copy_rename", 
        {"@src " + src_file, "@dst " + dst_file}, njtargets_orderdep
    );
}

cgn::NinjaFile::BuildSection* CopyWorker::postgen_flat_copy(
    cgn::CGNTargetMaker *confirmed_mk,
    const std::vector<std::string> &src_patterns, 
    const std::vector<std::string> &src_exclude_patterns,
    const std::string &dst_dir,
    const std::vector<std::string> &njtargets_orderdep
) {
    std::vector<std::string> exe_arg;
    for (auto ss : src_exclude_patterns)
        exe_arg.push_back("@srcex " + ss);
    for (auto ss : src_patterns)
        exe_arg.push_back("@src " + ss);
    exe_arg.push_back("@dst " + dst_dir);

    return this->mkninja(
        confirmed_mk, "flat_copy_to_dir", exe_arg, njtargets_orderdep
    );
}

cgn::NinjaFile::BuildSection* CopyWorker::postgen_copy(
    cgn::CGNTargetMaker *confirmed_mk, 
    const std::vector<std::string> &src_rel_patterns, 
    const std::vector<std::string> &src_rel_exclude_patterns, 
    const std::string &src_base,
    const std::string &dst_dir,
    const std::vector<std::string> &njtargets_orderdep
) {
    std::vector<std::string> exe_arg;
    for (auto ss : src_rel_patterns)
        exe_arg.push_back("@src " + ss);
    for (auto ss : src_rel_exclude_patterns)
        exe_arg.push_back("@srcex " + ss);
    exe_arg.push_back("@sbase " + dst_dir);
    exe_arg.push_back("@dst " + dst_dir);

    return this->mkninja(
        confirmed_mk, "copy_to_dir", exe_arg, njtargets_orderdep
    );
}


// class CopyInterpreter
// =================================

void CopyInterpreter::context_type::copy_on_build(
    const std::vector<std::string> &src, 
    const std::vector<std::string> &src_exclude, 
    const cgn::CGNPath &src_base, 
    const cgn::CGNPath &dst_dir
) {
    if (src.empty())
        return ;
    copy_records.push_back([=](cgn::CGNTargetMaker *mk, CopyWorker *w) {
        return w->postgen_copy(mk, src, src_exclude,
            api.rebase_path(src_base, ".", mk), 
            api.rebase_path(dst_dir, ".", mk),
            {} // opt->quickdep_ninja_dynhdr
        )->outputs[0];
    });
}

void CopyInterpreter::context_type::flat_copy_on_build(
    const std::vector<cgn::CGNPath> &src_list, 
    const std::vector<cgn::CGNPath> &src_exclude_list, 
    const cgn::CGNPath &dst_dir
) {
    if (src_list.empty())
        return ;
    copy_records.push_back([=](cgn::CGNTargetMaker *mk, CopyWorker *w) {
        std::vector<std::string> srcls, exls;
        for (auto it : src_list)
            srcls.push_back(api.rebase_path(it, ".", mk));
        for (auto it : src_exclude_list)
            exls.push_back(api.rebase_path(it, ".", mk));
        return w->postgen_flat_copy(
            mk, srcls, exls, api.rebase_path(dst_dir, ".", mk))->outputs[0];
    });
}

void CopyInterpreter::context_type::copy_rename_on_build(
    const cgn::CGNPath &src_file,
    const cgn::CGNPath &dst_file
) {
    copy_records.push_back([=](cgn::CGNTargetMaker *mk, CopyWorker *w) {
        return w->postgen_copy_rename(mk, api.rebase_path(src_file, ".", mk),
            api.rebase_path(dst_file, ".", mk), {} // mk->quickdep_ninja_dynhdr
        )->outputs[0];
    });
}


// Using c++17 compiled copy helper
//  rule advcopy
//    command = ${exe} -MD ${out}.d --stamp ${out} ${argfile}
//  build <id>.stamp : adv_copy
//    exe = ""
//    cmd = "flat_copy_to_dir"
//    args = arg1 $
//           arg2
void CopyInterpreter::interpret(context_type &x)
{
    // copy rule configuration
    // advcopy.exe varies by host_os and host_cpu.
    CopyWorker cpw;
    std::string errmsg1 = cpw.preconfig(x.opt);
    if (errmsg1.size()) {
        x.opt->set_fail(errmsg1);
        return ;
    }

    // confirm
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;

    // result require ninja_order_only_dep
    std::vector<std::string> phone_out_njesc 
        = {mk->ninja->escape_path(mk->ninja_entry)};
    for (auto it : x.ninja_build_trigger) {
        std::string it_path = api.rebase_path(it, ".", mk);
        if (it.type == it.BASE_ON_OUTPUT && api.is_file_inside(it_path, mk->out_prefix))
            phone_out_njesc += {mk->ninja->escape_path(it_path)};
        else {
            mk->errmsg = "output which is not in out_prefix is not supported";
            return ;
        }
    }
    // opt->result.ninja_dep_level = opt->result.NINJA_LEVEL_DYNDEP;

    for (auto it : x.analysis_outputs)
        mk->outputs += {api.rebase_path(it, ".", mk)};

    // return if no file need update
    if (mk->file_unchanged)
        return ;

    // generate ninja file
    std::vector<std::string> cpstamps_njesc;
    for (auto fn : x.copy_records)
        cpstamps_njesc.push_back( fn(mk, &cpw) );

    // phony
    auto *phony = mk->ninja->append_build();
    phony->rule = "phony";
    phony->inputs = cpstamps_njesc;
    phony->outputs = phone_out_njesc;
}
