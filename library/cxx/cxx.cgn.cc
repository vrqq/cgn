#define LANGCXX_CGN_BUNDLE_IMPL
#include "cxx.cgn.h"
#include "cxx_worker.hxx"

namespace cxx {

const cgn::BaseInfo::VTable &CxxInfo::_glb_cxx_vtable()
{
    const static cgn::BaseInfo::VTable v = {
        []() -> std::shared_ptr<cgn::BaseInfo> {
            return std::make_shared<CxxInfo>();
        },
        [](void *ecx, const cgn::BaseInfo *rhs) {
            if (rhs == nullptr)
                return false;
            CxxInfo *self = (CxxInfo*)ecx, *r = (CxxInfo*)rhs;
            for (auto pth : r->include_dirs)
                if (pth.type != pth.BASE_ON_WORKINGROOT)
                    return false;
            self->include_dirs = r->include_dirs + self->include_dirs;
            self->defines += r->defines;
            self->ldflags += r->ldflags;
            self->cflags  += r->cflags;
            return true;
        }, 
        [](const void *ecx, char type) -> std::string { 
            auto *self = (CxxInfo *)ecx;
            const char *indent = "           ";
            size_t len = (type=='h'?5:999);
            return std::string{"{\n"}
                + "   cflags: " + cgn::Logger::fmt_list(self->cflags, indent, len) + "\n"
                + "  ldflags: " + cgn::Logger::fmt_list(self->ldflags, indent, len) + "\n"
                + "  incdirs: " + cgn::Logger::fmt_list(self->include_dirs, indent, len) + "\n"
                + "  defines: " + cgn::Logger::fmt_list(self->defines, indent, len) + "\n"
                + "}";
        }
    };
    return v;
} //CxxInfo::_glb_cxx_vtable()

CxxContext::CxxContext(char role, cgn::CGNTargetOpt *opt)
: cgn::QuickDepContext(opt), role(role), name(opt->name), cfg(opt->cfg) {}

cgn::CGNTarget CxxContext::add_dep(
    const std::string &label, cgn::Configuration new_cfg, DepType flag
) {
    // merge TargetInfos[] manually
    cgn::CGNTarget early = quick_dep(label, cfg, false);
    if (early.errmsg.size()) // return if error occured.
        return early;

    // cxx::order_dep
    // Ignore remote CGNTarget value and return.
    if (flag == DepType::_order_dep)
        return early;

    // call merge() for unused field
    for (auto &rhs : early.data())
        if (rhs.first != "CxxInfo" && rhs.first != "LinkAndRunInfo")
            _pub_infos.merge_entry(rhs.first, rhs.second.get());

    // rhs[CxxInfo]
    //   cxx::inherit : append to interpreter_rv[CxxInfo] as is, 
    //                  and also apply on current target.
    //   cxx::private : save to _cxx_to_self to use for current target only.
    if ((flag & DepType::_inherit))
        _pub_infos.merge_entry(early.get<CxxInfo>(false));

    _cxx_to_self.merge_entry(early.get<CxxInfo>(false));

    // rhs[LinkAndRunInfo]
    cgn::LinkAndRunInfo *r_lnr = early.get<cgn::LinkAndRunInfo>(false);

    // rhs[LinkAndRunInfo] (for both msvc and GNU) 
    // cxx_sources() : keep as is (do not consume anyone), whatever private or inherit.
    // cxx_static()  : move(r_lnr.obj) to cmd "ar rcs" later in interpreter if _archive, 
    //                 then expose others as-is, whatever private or inherit.
    if (r_lnr && role == 'o')
        _pub_infos.merge_entry(r_lnr);
    if (r_lnr && role == 'a') {
        if (flag & DepType::_archive)
            _lnr_to_self.object_files += std::move(r_lnr->object_files);
        _pub_infos.get<cgn::LinkAndRunInfo>(true)->merge_entry(r_lnr);
    }
    
    // rhs[LinkAndRunInfo] 
    // In default, WHOLEARCHIVE has been assigned unless _no_whole assgned.
    //
    // cxx_executable() and cxx_shared() for both msvc and GNU
    //  r_lnk.obj & r_lnk.a & r_lnk.so-> _self (for priv_dep)
    //      Note: utilized by current target interpreter
    //  r_lnk.obj & r_lnk.a & r_lnk.so-> _self & _pub (for cxx::inherit)
    //      Note: valid for both export and link to self since visiblity(hidden)
    //  r_lnk.runtime[BASE_ON_OUTPUT] -> executable._self (for priv)
    //  r_lnk.runtime[BASE_ON_OUTPUT] -> executable._self & _pub (for inherit)
    //  r_lnk.runtime[BASE_ON_WROOT]  -> executable._pub  (both priv & inherit)
    //  r_lnk.runtime -> shared._pub (for both priv & inherit)
    if (r_lnr && (role == 's' || role == 'x')) {
        // process the _inherit case first.
        if (flag & DepType::_inherit)
            _pub_infos.merge_entry(r_lnr);

        // Record for '-no-wholearchive' in the separator along with '_lnr_to_self.static'
        if (flag & DepType::_no_whole)
            _self_no_whole_archive.insert(
                _self_no_whole_archive.end(),
                std::make_move_iterator(r_lnr->static_files.begin()),
                std::make_move_iterator(r_lnr->static_files.end()));
        _lnr_to_self.static_files += std::move(r_lnr->static_files);
        _lnr_to_self.object_files += std::move(r_lnr->object_files);
        _lnr_to_self.shared_files += std::move(r_lnr->shared_files);

        // special for cxx_executable()
        auto *_pub_lnr = _pub_infos.get<cgn::LinkAndRunInfo>(false);
        if (role == 'x') {
            for (auto &entry : r_lnr->runtime_files)
                if (entry.first.type == cgn::CGNPath::BASE_ON_OUTPUT)
                    _lnr_to_self.runtime_files.insert(entry);
                else if (!(flag & DepType::_inherit))
                    _pub_lnr->runtime_files.insert(entry);
        }
        else { //role == 's'
            if (!(flag & DepType::_inherit))
                _pub_lnr->runtime_files.insert(
                    std::make_move_iterator(r_lnr->runtime_files.begin()),
                    std::make_move_iterator(r_lnr->runtime_files.end())
                );
        }
    }

    return early;
} //CxxContext::add_dep()

void CxxInterpreter::interpret(context_type &x)
{
    CxxWorker worker;
    std::string errmsg = worker.step1_test_param(x.cfg, "");
    if (errmsg.size())
        return x.opt->set_fail(errmsg);

    errmsg = worker.step2_confirm(x);
    if (errmsg.size())
        return x.opt->set_fail(errmsg);
    if (!worker.mk)
        return ;
    worker.mk->merge_from(x.quickdep_result);

    worker.step3_gen_ninja();

    worker.mk->merge_entry(&worker.s3out);
    worker.mk->outputs = worker.s3out.object_files
                       + worker.s3out.shared_files
                       + worker.s3out.static_files;
}

CxxToolchainInfo CxxInterpreter::test_param(
    cgn::Configuration &cfg, const std::string &via
) {
    CxxWorker worker;
    std::string errmsg = worker.step1_test_param(cfg, via);
    if (errmsg.size())
        throw std::runtime_error{errmsg};
    return worker.s1out;
}

// CxxPrebuiltInterpreter
// ----------------------

void CxxPrebuiltInterpreter::interpret(context_type &x)
{
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;
    mk->merge_from(x.quickdep_result);

    // result[CxxInfo]
    for (auto &it : x.pub.include_dirs)
        it = api.convert_cgnpath_to_working_root(it, x.opt);
    mk->merge_entry(&x.pub);

    // TargetInfos[LinkAndRunInfo]
    cgn::LinkAndRunInfo *lrinfo = mk->get<cgn::LinkAndRunInfo>(true);
    bool have_sofile = false;
    std::unordered_set<std::string> dllstem;
    std::vector<std::pair<std::string,std::string>> dotlib;
    for (auto file : x.files) {
        auto fd1   = file.rpath.rfind('/');
        auto fddot = file.rpath.rfind('.');
        fd1 = (fd1 == file.rpath.npos? 0: fd1+1);
        if (fddot == file.rpath.npos || fddot < fd1)
            continue;
        std::string stem = file.rpath.substr(fd1, fddot-fd1);
        std::string ext  = file.rpath.substr(fddot);
        // std::string fullp = api.locale_path(opt->src_prefix + file);
        std::string fullp = api.rebase_path(file, ".", mk);
        if (ext == ".so")
            lrinfo->shared_files.push_back(fullp);
        else if (ext == ".a")
            lrinfo->static_files.push_back(fullp);
        else if (ext == ".dll") {
            lrinfo->runtime_files[cgn::make_path_base_out(stem + ".dll")] = fullp;
            dllstem.insert(stem);
        }
        else if (ext == ".lib")
            dotlib.push_back({stem, fullp});
        else
            lrinfo->runtime_files[cgn::make_path_base_out(stem + "." + ext)] = fullp;
        mk->outputs += {fullp};
    }
    for (auto item : dotlib)
        if (dllstem.count(item.first) != 0)
            lrinfo->shared_files.push_back(item.second);
        else
            lrinfo->static_files.push_back(item.second);

    // build.ninja
    if (mk->ninja) {
        auto *entry = mk->ninja->append_build();
        entry->rule = "phony";
        entry->order_only = mk->ninja->escape_path(x.quickdep_ninja_target);
        entry->outputs = {mk->ninja->escape_path(mk->ninja_entry)};
    }
} //CxxPrebuiltInterpreter::interpret()

} //namespace
