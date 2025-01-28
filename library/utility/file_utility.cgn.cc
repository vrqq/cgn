#define CGN_UTILITY_IMPL
#include <fstream>
#include "../general.cgn.bundle/bin_devel.cgn.h"
#include "file_utility.cgn.h"

namespace {
    struct IWorker {
        virtual void config(FileUtility &x) = 0;
        virtual void gen(FileUtility &x, cgn::CGNTargetOpt *opt) = 0;
    };

    struct CopyWorker : IWorker
    {
        std::string cprule;
        virtual void config(FileUtility &x) {
            
            cprule = (x.cfg["host_os"] == "win"? "win_cp_to_dir" : "unix_cp_to_dir");
        }
        virtual void gen(FileUtility &x, cgn::CGNTargetOpt *opt) {
        }
    }; //struct CopyWorker

    struct BinDevelWorker : IWorker
    {
        virtual void config(FileUtility &x) {
        }
        virtual void gen(FileUtility &x, cgn::CGNTargetOpt *opt) {
        }
    }; //struct BinDevelWorker

    std::string to_working_root(cgn::CGNTargetOpt *opt, const cgn::CGNPath &it) {
        if (it.type == it.BASE_ON_OUTPUT)
            return api.rebase_path(it.rpath, ".", opt->out_prefix);
        else if (it.type == it.BASE_ON_SCRIPT_SRC)
            return api.rebase_path(it.rpath, ".", opt->src_prefix);
        return it.rpath;
    }

} //namespace

void FileUtility::copy_on_build(
    const std::vector<std::string> &src, 
    const cgn::CGNPath &src_base, 
    const cgn::CGNPath &dst_dir
) {
    if (src.empty())
        return ;
    constexpr static char CMD_COPY[] = "copy_to_dir";

    auto gen = [=](cgn::CGNTargetOpt *opt) {
        std::vector<std::string> rv;
        for (auto it : src)
            rv.push_back(it);
        rv.push_back(to_working_root(opt, src_base));
        rv.push_back(to_working_root(opt, dst_dir));
        return rv;
    };
    copy_records.push_back(CopyRecord{CMD_COPY, src_base.to_string() + "=>" + dst_dir.to_string(), gen});
}


void FileUtility::flat_copy_on_build(
    const std::vector<cgn::CGNPath> &src_list, 
    const cgn::CGNPath &dst_dir
) {
    if (src_list.empty())
        return ;
    constexpr static char CMD_FLAT_COPY[] = "flat_copy_to_dir";
    auto gen = [=](cgn::CGNTargetOpt *opt) {
        std::vector<std::string> rv;
        for (auto it : src_list)
            rv.push_back(to_working_root(opt, it));
        rv.push_back(to_working_root(opt, dst_dir));
        return rv;
    };
    copy_records.push_back(CopyRecord{CMD_FLAT_COPY, dst_dir.to_string(), gen});
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
                    {"*"}, cgn::make_path_base_working(incdir), 
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
    cgn::CGNTarget advcopy = x.opt->quick_dep_namedcfg("@cgn.d//advcopy", "host_release", false);
    x.cfg.visit_keys({"host_os", "host_cpu"});
    if (advcopy.errmsg.size()) {
        x.opt->confirm_with_error("Cannot load advcopy: " + advcopy.errmsg);
        return ;
    }

    // confirm
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // result require ninja_order_only_dep
    opt->result.ninja_dep_level = opt->result.NINJA_LEVEL_DYNDEP;

    for (auto it : x.perferred_outputs)
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

    // include advcp.ninja
    std::string rulepath = api.get_filepath("@cgn.d//library/utility/advcp.ninja");
    opt->ninja->append_include(rulepath);

    // generate ninja file
    std::vector<std::string> cpstamps_njesc;
    for (std::size_t i=0; i<x.copy_records.size(); i++) {
        auto &rec = x.copy_records[i];

        // generate copy_<i>.rsp
        std::string argfile_path = opt->out_prefix + "copy_" + std::to_string(i) + ".rsp";
        std::ofstream argfile(argfile_path);
        for (const auto &arg : rec.arg_gen(opt))
            argfile << arg << "\n";
        argfile.close();

        // generate ninja target copy_<i>.stamp
        auto *field = opt->ninja->append_build();
        field->rule = "advcopy";
        field->variables["subcmd"] = rec.command;
        field->variables["desc"] = std::string{rec.command} + " " + rec.desc;
        field->variables["exe"]  = opt->ninja->escape_path(advcopy.outputs[0]);
        field->implicit_inputs   = opt->quickdep_ninja_full;
        field->implicit_inputs  += {field->variables["exe"]};
        field->order_only        = opt->quickdep_ninja_dynhdr;
        field->inputs  = {opt->ninja->escape_path(argfile_path)};
        field->outputs = {opt->ninja->escape_path(opt->out_prefix + "copy_" + std::to_string(i) + ".stamp")};
        cpstamps_njesc.push_back(field->outputs[0]);
    }

    // phony
    auto *phony = opt->ninja->append_build();
    phony->rule = "phony";
    phony->inputs = cpstamps_njesc;
    phony->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};    
}
