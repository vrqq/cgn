#define CGN_UTILITY_IMPL
#include <fstream>
#include "../general.cgn.bundle/bin_devel.cgn.h"
#include "file_utility.cgn.h"

namespace {
    // struct IWorker {
    //     virtual void config(FileUtility &x) = 0;
    //     virtual void gen(FileUtility &x, cgn::CGNTargetOpt *opt) = 0;
    // };

    // struct CopyWorker : IWorker
    // {
    //     std::string cprule;
    //     virtual void config(FileUtility &x) {
            
    //         cprule = (x.cfg["host_os"] == "win"? "win_cp_to_dir" : "unix_cp_to_dir");
    //     }
    //     virtual void gen(FileUtility &x, cgn::CGNTargetOpt *opt) {
    //     }
    // }; //struct CopyWorker

    // struct BinDevelWorker : IWorker
    // {
    //     virtual void config(FileUtility &x) {
    //     }
    //     virtual void gen(FileUtility &x, cgn::CGNTargetOpt *opt) {
    //     }
    // }; //struct BinDevelWorker

    std::string to_working_root(cgn::CGNTargetOpt *opt, const cgn::CGNPath &it) {
        return api.rebase_path(it, ".", opt);
    }

    std::string two_escape(const std::string &in) {
        return cgn::NinjaFile::escape_path(api.shell_escape(in));
    }

} //namespace

// Class CopyWorker
// ================

std::string CopyWorker::preconfig(cgn::CGNTargetOptIn *opt, const std::string &argfile_prefix)
{
    this->argfile_prefix = argfile_prefix;
    
    // copy rule configuration
    // advcopy.exe varies by host_os and host_cpu.
    cgn::CGNTarget advcopy = opt->quick_dep_namedcfg("@cgn.d//advcopy", "host_release", false);
    opt->cfg.visit_keys({"host_os", "host_cpu"});
    if (advcopy.errmsg.size() || advcopy.outputs.empty());
        return "Cannot load advcopy: " + advcopy.errmsg;
    advcopy_exe = two_escape(advcopy.outputs[0]);
    return "";
}

cgn::NinjaFile::BuildSection* CopyWorker::mkninja(
    cgn::CGNTargetOpt *opt, const std::string &command,
    const std::vector<std::string> &arg_content,
    const std::vector<std::string> &njtargets_orderdep
) {
    if (opt->file_unchanged)
        return target_n++, nullptr;
    
    if (target_n == 0) {
        std::string rulepath = api.get_filepath("@cgn.d//library/utility/advcp.ninja");
        opt->ninja->append_include(rulepath);
    }
    
    // generate copy_<i>.rsp
    std::string path_stub = opt->out_prefix + this->argfile_prefix 
                             + std::to_string(target_n++);
    std::ofstream argout(path_stub + ".rsp");
    for (const auto &arg : arg_content)
        argout << arg << "\n";
    argout.close();

    auto *field = opt->ninja->append_build();
    field->rule = "advcopy";
    field->variables["subcmd"] = command;
    field->variables["desc"] = command + " " + *(++arg_content.rbegin()) 
                             + " -> " + arg_content.back();
    field->variables["exe"] = two_escape(advcopy_exe);
    field->implicit_inputs  = {cgn::NinjaFile::escape_path(advcopy_exe)};
    field->order_only       = cgn::NinjaFile::escape_path(njtargets_orderdep);
    field->inputs  = {opt->ninja->escape_path(path_stub + ".rsp")};
    field->outputs = {opt->ninja->escape_path(path_stub + ".stamp")};
    return field;
}

cgn::NinjaFile::BuildSection* CopyWorker::postgen_copyone(
    cgn::CGNTargetOpt *confirmed_opt,
    const std::string &src_file, const std::string &dst_file,
    const std::vector<std::string> &njtargets_orderdep
) {
    return this->mkninja(
        confirmed_opt, "copyone", {src_file, dst_file}, njtargets_orderdep
    );
}

cgn::NinjaFile::BuildSection* CopyWorker::postgen_flat_copy(
    cgn::CGNTargetOpt *confirmed_opt,
    const std::vector<std::string> &src_patterns, const std::string &dst_dir,
    const std::vector<std::string> &njtargets_orderdep
) {
    return this->mkninja(
        confirmed_opt, "flat_copy_to_dir", 
        src_patterns + std::vector<std::string>{dst_dir}, 
        njtargets_orderdep
    );
}
cgn::NinjaFile::BuildSection* CopyWorker::postgen_copy(
    cgn::CGNTargetOpt *confirmed_opt, 
    const std::vector<std::string> src_rel_patterns, 
    const std::string &src_base,
    const std::string &dst_dir,
    const std::vector<std::string> &njtargets_orderdep
) {
    return this->mkninja(
        confirmed_opt, "copy_to_dir", 
        src_rel_patterns + std::vector<std::string>{src_base, dst_dir},
        njtargets_orderdep
    );
}

// class FileUtility and Interpreter
// =================================

void FileUtility::copy_on_build(
    const std::vector<std::string> &src, 
    const cgn::CGNPath &src_base, 
    const cgn::CGNPath &dst_dir
) {
    if (src.empty())
        return ;
    copy_records.push_back([=](cgn::CGNTargetOpt *opt, CopyWorker *w) {
        return w->postgen_copy(opt, src, 
            api.rebase_path(src_base, ".", opt), api.rebase_path(dst_dir, ".", opt),
            opt->quickdep_ninja_dynhdr
        )->outputs[0];
    });
}

void FileUtility::flat_copy_on_build(
    const std::vector<cgn::CGNPath> &src_list, 
    const cgn::CGNPath &dst_dir
) {
    if (src_list.empty())
        return ;
    copy_records.push_back([=](cgn::CGNTargetOpt *opt, CopyWorker *w) {
        std::vector<std::string> srcls;
        for (auto it : src_list)
            srcls.push_back(api.rebase_path(it, ".", opt));
        return w->postgen_flat_copy(
            opt, srcls, api.rebase_path(dst_dir, ".", opt))->outputs[0];
    });
}

void FileUtility::copy_rename_on_build(
    const cgn::CGNPath &src_file,
    const cgn::CGNPath &dst_file
) {
    copy_records.push_back([=](cgn::CGNTargetOpt *opt, CopyWorker *w) {
        return w->postgen_copyone(opt, api.rebase_path(src_file, ".", opt),
            api.rebase_path(dst_file, ".", opt), opt->quickdep_ninja_dynhdr
        )->outputs[0];
    });
}

// TargetDir
// include: tgt.h
//     bin: tgt_win.dll  tgt_win.exe  tgt_linux
//   lib64: tgt_linux.so  tgt_win.lib
cgn::CGNTarget FileUtility::collect_devel_on_build(
    const std::string &label, 
    DevelOpt devel_setting
) {
    if (devel_setting.perferred_libdir.empty()) {
        if (cfg["cpu"] == "x86_64")
            devel_setting.perferred_libdir = "lib64";
        else
            devel_setting.perferred_libdir = "lib";
    }

    auto early = opt->quick_dep(label, cfg, true);
    if (early.errmsg.size())
        return early;

    if (devel_setting.allow_bindevel){
        BinDevelInfo *info = early.get<BinDevelInfo>(false);
        if (info) {
            copy_on_build(
                {"*"}, cgn::make_path_base_working(info->base), 
                cgn::make_path_base_out("."));
        }
    }

    // CxxInfo::include_dirs => include
    {
        cxx::CxxInfo *info = early.get<cxx::CxxInfo>(false);
        if (devel_setting.allow_cxxinclude && info){
            for (auto incdir : info->include_dirs)
                copy_on_build(
                    {"*"}, cgn::make_path_base_working(incdir.rpath), 
                    cgn::make_path_base_out("include"));
        }
        devel_cxxinfo.merge_entry(info);
    }

    // LinkAndRunInfo::shared_files => lib64
    // LinkAndRunInfo::static_files => lib64
    // LinkAndRunInfo::runtime_files => bin
    if (devel_setting.allow_linknrun) {
        cgn::LinkAndRunInfo *info = early.get<cgn::LinkAndRunInfo>(false);
        if (info) {
            std::vector<cgn::CGNPath> solibs, exes;
            for (auto so : info->shared_files)
                solibs.push_back(cgn::make_path_base_working(so));
            for (auto a : info->static_files)
                solibs.push_back(cgn::make_path_base_working(a));
            for (auto exe : info->runtime_files)
                exes.push_back(cgn::make_path_base_working(exe.second));
            flat_copy_on_build(solibs, cgn::make_path_base_out(devel_setting.perferred_libdir));
            flat_copy_on_build(exes, cgn::make_path_base_out(devel_setting.perferred_libdir));
        }
    }
    
    // .exe .dll => bin
    // .so .a .lib => lib64
    if (devel_setting.allow_output) {
        std::vector<cgn::CGNPath> solibs, exes;
        for (auto it : early.outputs) {
            auto ext = api.extension_of_path(it);
            if (ext == ".a" || ext == ".so" || ext == ".lib")
                solibs.push_back(cgn::make_path_base_working(it));
            else if (ext == ".exe" || ext == ".dll")
                exes.push_back(cgn::make_path_base_working(it));
        }
        flat_copy_on_build(solibs, cgn::make_path_base_out(devel_setting.perferred_libdir));
        flat_copy_on_build(exes, cgn::make_path_base_out(devel_setting.perferred_libdir));
    }

    have_devel = true;
    devel_basedir = devel_setting.target_dir;
    devel_lib_dirname = devel_setting.perferred_libdir;

    return early;
} //FileUtility::collect_devel_on_build()

// Using c++17 compiled copy helper
//  rule advcopy
//    command = ${exe} -MD ${out}.d --stamp ${out} ${argfile}
//  build <id>.stamp : adv_copy
//    exe = ""
//    cmd = "flat_copy_to_dir"
//    args = arg1 $
//           arg2
void FileUtilityInterpreter::interpret(context_type &x)
{
    // copy rule configuration
    // advcopy.exe varies by host_os and host_cpu.
    CopyWorker cpw;
    std::string errmsg1 = cpw.preconfig(x.opt);
    if (errmsg1.size()) {
        x.opt->confirm_with_error(errmsg1);
        return ;
    }

    // confirm
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // result require ninja_order_only_dep
    bool need_dyndep = false;
    std::vector<std::string> phone_out_njesc 
        = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
    for (auto it : x.ninja_outputs) {
        std::string it_path = to_working_root(opt, it);
        if (it.type == it.BASE_ON_OUTPUT && api.is_file_inside(it_path, opt->out_prefix))
            phone_out_njesc += {opt->ninja->escape_path(it_path)};
        else {
            need_dyndep = true;
            if (!opt->file_unchanged)
                api.add_placeholder_file(it_path);
        }
    }
    if (need_dyndep)
        opt->result.ninja_dep_level = opt->result.NINJA_LEVEL_DYNDEP;

    for (auto it : x.analysis_outputs)
        opt->result.outputs += {to_working_root(opt, it)};

    // Bindevel postprocess
    if (x.have_devel) {
        auto *info = opt->result.get<BinDevelInfo>(true);
        info->base = to_working_root(opt, x.devel_basedir);
        info->bin_dir = info->base + "bin";
        info->lib_dir = info->base + x.devel_lib_dirname;
        info->include_dir = info->base + "include";

        x.devel_cxxinfo.include_dirs = {info->base + "include"};
        opt->result.merge_entry(x.devel_cxxinfo.name(), &(x.devel_cxxinfo));
    }

    // return if no file need update
    if (opt->file_unchanged)
        return ;

    // generate ninja file
    std::vector<std::string> cpstamps_njesc;
    for (auto fn : x.copy_records)
        cpstamps_njesc.push_back( fn(opt, &cpw) );

    // phony
    auto *phony = opt->ninja->append_build();
    phony->rule = "phony";
    phony->inputs = cpstamps_njesc;
    phony->outputs = phone_out_njesc;
}
