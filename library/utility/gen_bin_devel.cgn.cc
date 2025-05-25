#define CGN_LIBRARY_GENBINDEVEL_IMPL
#include "gen_bin_devel.cgn.h"

// BinDevelWorker
// --------------

// info->base => $out_prefix
void BinDevelWorker::copy_from_bindevelinfo(const BinDevelInfo *info)
{
    if (info->bin_dir.size())
        _njadd(cpw->postgen_copy(copt, {"*"}, {}, info->bin_dir, full_bindir, ninja_deps));
    if (info->include_dir.size())
        _njadd(cpw->postgen_copy(copt, {"*"}, {}, info->include_dir, full_incdir, ninja_deps));
    if (info->lib_dir.size())
        _njadd(cpw->postgen_copy(copt, {"*"}, {}, info->lib_dir, full_libdir, ninja_deps));
}

// CxxInfo::include_dirs => $incdir
void BinDevelWorker::copy_from_cxxinfo_include(const cxx::CxxInfo *info)
{
    for (auto dir : info->include_dirs)
        _njadd(cpw->postgen_copy(copt, {"*"}, {}, api.rebase_path(dir, ".", nullptr), full_incdir, ninja_deps));
}

// LinkAndRunInfo::shared_files  => $libdir
// LinkAndRunInfo::static_files  => $libdir
// LinkAndRunInfo::runtime_files => $bindir
void BinDevelWorker::copy_from_linkandruninfo(const cgn::LinkAndRunInfo *info)
{
    std::vector<std::string> solibs;
    for (auto so : info->shared_files)
        solibs.push_back(so);
    for (auto a : info->static_files)
        solibs.push_back(a);
    _njadd(cpw->postgen_flat_copy(copt, solibs, {}, full_libdir, ninja_deps));
    for (const auto &exe : info->runtime_files) {
        auto &dst = exe.first;
        auto &src = exe.second;
        if (dst.type != dst.BASE_ON_OUTPUT)
            continue; // TODO: add feature like 'BinDevelInfo.apphome' in future
        std::string dst_dir = api.locale_path(full_bindir + "/" + dst.rpath);
        _njadd(cpw->postgen_copyone(copt, src, dst_dir, ninja_deps));
    }
}

// .exe .dll   => $bindir
// .so .a .lib => $libdir
void BinDevelWorker::copy_from_output(const std::vector<std::string> &files)
{
    std::vector<std::string> solibs, exes;
    for (auto it : files) {
        auto ext = api.extension_of_path(it);
        if (ext == ".a" || ext == ".so" || ext == ".lib")
            solibs.push_back(it);
        else if (ext == ".exe" || ext == ".dll")
            exes.push_back(it);
    }
    _njadd(cpw->postgen_flat_copy(copt, solibs, {}, full_libdir, ninja_deps));
    _njadd(cpw->postgen_flat_copy(copt, exes, {}, full_bindir, ninja_deps));
}

BinDevelInfo BinDevelWorker::get_bin_devel_info()
{
    BinDevelInfo info;
    info.install_dir = this->full_install_dir;
    info.include_dir = this->full_incdir;
    info.lib_dir     = this->full_libdir;
    info.bin_dir     = this->full_bindir;

    return info;
}

// cxx::CxxInfo BinDevelWorker::get_cxx_info()
// {}

// GenerateBinDevelInterpreter
// ---------------------------

cgn::CGNTarget 
GenerateBinDevelInterpreter::context_type::collect_from_target(
    const std::string &label, 
    const cgn::Configuration &cfg, 
    CollectOpt collect_opt
) {
    auto early = opt->quick_dep(label, cfg, false);
    if (early.errmsg.size())
        return early;
    cpworker_dep.push_back(early.ninja_entry);

    if (collect_opt.copy_from_bin_devel) {
        BinDevelInfo *info = early.get<BinDevelInfo>(false);
        if (info) {
            auto inf = *info;
            workgen.push_back([inf](BinDevelWorker *w){
                w->copy_from_bindevelinfo(&inf);
            });
            if (info->within_cmakeconfig)
                this->have_cmakeconfig = true;
            if (info->within_pkgconfig)
                this->have_pkgconfig = true;
        }
    }
    if (collect_opt.copy_from_cxx_include) {
        cxx::CxxInfo *info = early.get<cxx::CxxInfo>(false);
        if (info){
            auto inf = *info;
            workgen.push_back([inf](BinDevelWorker *w){
                w->copy_from_cxxinfo_include(&inf);
            });
        }
    }
    if (collect_opt.copy_from_linknrun) {
        cgn::LinkAndRunInfo *info = early.get<cgn::LinkAndRunInfo>(false);
        if (info){
            auto inf = *info;
            workgen.push_back([inf](BinDevelWorker *w){
                w->copy_from_linkandruninfo(&inf);
            });
        }
    }
    if (collect_opt.copy_from_output) {
        auto out = early.outputs;
        workgen.push_back([out](BinDevelWorker *w){
            w->copy_from_output(out);
        });
    }

    return early;
}

void GenerateBinDevelInterpreter::interpret(GenerateBinDevelInterpreter::context_type &x)
{
    if (x.workgen.empty()) {
        x.opt->confirm();
        return;
    }

    CopyWorker cpworker;
    cpworker.preconfig(x.opt);

    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;
    
    // start generate build section by Worker
    BinDevelWorker binw(&cpworker, opt, x.cpworker_dep, "install", x.inc_dir, x.bin_dir, x.lib_dir);
    if (!opt->file_unchanged) {
        for (auto it : x.workgen)
            it(&binw);

        // build.ninja : entrypoint
        auto *field = opt->ninja->append_build();
        field->rule = "phony";
        field->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
        field->inputs  = opt->ninja->escape_path(binw.get_ninja_stamp_files());
        field->implicit_inputs = opt->ninja->escape_path(opt->quickdep_ninja_full);
        field->order_only      = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
    }

    //return value
    auto devel_info = binw.get_bin_devel_info();
    devel_info.within_cmakeconfig |= x.have_cmakeconfig;
    devel_info.within_pkgconfig   |= x.have_pkgconfig;
    opt->result.set(devel_info);
}
