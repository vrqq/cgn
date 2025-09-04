#define CGN_LIBRARY_MAKEPKG_IMPL
#include "copy.cgn.h"
#include "package.cgn.h"

cgn::CGNTarget PackageInterpreter::context_type::add_dep(const std::string &label, cgn::Configuration cfg)
{
    cgn::CGNTarget early = opt->quick_dep(label, cfg, true);
    auto info = early.get<cgn::LinkAndRunInfo>(false);
    if (early.errmsg.empty() && info)
        copy_record[early.ninja_entry] = info->runtime_files;
    return early;
}

void PackageInterpreter::context_type::add_deps(const std::vector<std::string> &labels, cgn::Configuration cfg)
{
    for (auto label : labels)
        add_dep(label, cfg);
}

void PackageInterpreter::interpret(context_type &x)
{
    CopyWorker cpw;
    std::string err_cpw = cpw.preconfig(x.opt);
    if (err_cpw.size())
        return x.opt->confirm_with_error(err_cpw);

    auto opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // copy files to current OUTPUT DIR, so dyndep here.
    opt->result.ninja_dep_level = opt->result.NINJA_LEVEL_DYNDEP;
    opt->result.outputs = {opt->out_prefix};
    
    // generate ninja file
    auto *rvinfo = opt->result.get<cgn::LinkAndRunInfo>(false);
    std::vector<std::string> all_copy_entries;
    for (const auto &rec : x.copy_record) {
        const std::string &ninja_target_dep = rec.first;
        for (const auto &filemap : rec.second) {
            auto &to   = filemap.first;
            auto &from = filemap.second;
            if (to.type == to.BASE_ON_OUTPUT && !api.is_absolute_path(to.rpath)) {
                auto psect = cpw.postgen_copy_rename(opt, from, 
                    api.locale_path(opt->out_prefix + to.rpath), {ninja_target_dep});
                all_copy_entries.push_back(psect->outputs[0]);

                // remove from current target return
                rvinfo->runtime_files.erase(to);
            }
        }
    }

    // auto *info = opt->result.get<cgn::LinkAndRunInfo>(false);
    // if (info)
    //     for (auto iter = info->runtime_files.begin(); iter != info->runtime_files.end();) {
    //         auto &to   = iter->first;
    //         auto &from = iter->second;
    //         if (to.type == to.BASE_ON_OUTPUT && !api.is_absolute_path(to.rpath)){
    //             auto psect = cpw.postgen_copy_rename(opt, from, api.locale_path(opt->out_prefix + to.rpath));
    //             deps_njesc.push_back(psect->outputs[0]);

    //             // remove current one from return value
    //             iter = info->runtime_files.erase(iter);
    //         }
    //         else
    //             iter++;
    //     }
    
    // write ninja entry
    auto *phony = opt->ninja->append_build();
    phony->rule = "phony";
    phony->inputs = all_copy_entries;
    phony->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
}
