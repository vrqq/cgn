#define GENERAL_CGN_BUNDLE_IMPL
#include <map>
#include "../../std_operator.hpp"
#include "../../cgn.h"
#include "../helper/helper.hpp"
#include "../cxx.cgn.bundle/cxx.cgn.h"
#include "bin_devel.cgn.h"

GENERAL_CGN_BUNDLE_API const cgn::BaseInfo::VTable*
BinDevelInfo::_glb_bindevel_vtable()
{
    const static cgn::BaseInfo::VTable v = {
        []() -> std::shared_ptr<cgn::BaseInfo> {
            return std::make_shared<BinDevelInfo>();
        },
        [](void *ecx, const void *rhs) {
            return ;
        }, 
        [](const void *ecx, char type) -> std::string { 
            auto *self = (BinDevelInfo *)ecx;
            return std::string{"{\n"}
                + "   base: " + self->base + "\n"
                + " incdir: " + self->include_dir + "\n"
                + " libdir: " + self->lib_dir + "\n"
                + "}";
        }
    };
    return &v; 
}

void FileCollect::Context::add(
    const std::string src_dir, 
    const std::vector<std::string> &src_files,
    const std::string &dst_dir
) {
    Pattern p;
    p.src_basedir = src_dir;
    p.src_files   = src_files;
    p.dst_dir     = dst_dir;
    mapper.push_back(p);
}

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

// TODO:
// if (srcls.size() == 1) {
//     build->rule = "quick_run";
//     build->inputs  = {opt.ninja->escape_path(opt.src_prefix + srcls[0].file_part1)};
//     build->outputs = {opt.ninja->escape_path(cp_out + srcls[0].file_part1)};
//     build->variables["cmd"] 
//         = "dst=" + two_escape(var_out) + ";"
//         + "pushd " + api.shell_escape(srcls[0].src_cd)
//         + " && cp --parent -r " + api.shell_escape(*srcls[0].file_full)
//         + " $dst";
cgn::TargetInfos FileCollect::interpret(
    FileCollect::Context &x, cgn::CGNTargetOpt opt
) {
    struct SrcFiles {
        bool recursive = false;
        std::string file_part1;  // "<opt.src_prefix>/path/in/src" (os-sep)
        std::string file_part2;  // "*", "**.h"
    };
    struct BatCmd {
        std::string bat_file;
        std::vector<SrcFiles> src_files;
    };

    // ninja_target : single file or folder
    //  tbl_pattern[<opt.out_prefix>/install/fileA] = "<opt.src_prefix>/src1/fileA"
    std::unordered_map<std::string, std::string> tgt_single;

    // ninja_target : src with '**' or '*'
    //  tbl_pattern[<opt.out_prefix>/install/<file_part1>] 
    //  = [{"<opt.src_prefix>/src_path", "*.h"}, 
    //     {"/abs_path2", "**.md"}, ...]
    std::unordered_map<std::string, BatCmd> tgt_pattern;

    // Generate tgt_single and tgt_single
    std::string instdir = opt.out_prefix + "install" + opt.path_separator;
    std::string batdir  = opt.out_prefix + "bat" + opt.path_separator;
    for (auto &dirinfo : x.mapper) {
        if (dirinfo.src_basedir.empty())
            dirinfo.src_basedir = ".";
        for (auto &fp1 : dirinfo.src_files) {
            if (fp1.empty())
                continue;
            auto fdstar = fp1.rfind('*');
            if (fdstar == fp1.npos) { //normal file
                std::string src = api.rebase_path(dirinfo.src_basedir + "/" + fp1, ".", opt.src_prefix);
                std::string dst = instdir + api.locale_path(dirinfo.dst_dir + "/" + fp1);
                if (dst.back() == opt.path_separator[0])
                    dst.pop_back();
                if (src.back() == opt.path_separator[0])
                    src.pop_back();
                tgt_single[dst] = src;
            }
            else { //file with '*'
                std::string mid_path = ".";

                SrcFiles cpcmd;
                cpcmd.recursive = (fdstar && fp1[fdstar-1]=='*');
                auto fdslash = fp1.rfind('/');
                if (fdslash != fp1.npos) {
                    if (fdslash > fdstar)
                        throw std::runtime_error{
                            "FileCollect: Unsupported file " + fp1 
                            + " at " + opt.factory_ulabel
                        };
                    mid_path = fp1.substr(0, fdslash);
                    cpcmd.file_part2 = fp1.substr(fdslash+1);
                } else {
                    cpcmd.file_part2 = fp1;
                }
                cpcmd.file_part1 = api.rebase_path(dirinfo.src_basedir + "/" + mid_path, ".", opt.src_prefix);
                if (cpcmd.file_part1.back() == '/')
                    cpcmd.file_part1.pop_back();

                std::string tmp;
                if (dirinfo.dst_dir.size())
                    tmp = api.locale_path(dirinfo.dst_dir + "/" + mid_path);
                else
                    tmp = api.locale_path(mid_path);
                std::string real_dst = instdir + tmp;
                if (real_dst.back() == opt.path_separator[0])
                    real_dst.pop_back();
                auto &batdesc = tgt_pattern[real_dst];
                if (batdesc.bat_file.empty())
                    batdesc.bat_file = batdir + tmp + ".cmd";
                batdesc.src_files.push_back(cpcmd);
            }
        } //foreach(files[] : dirinfo.files[])
    } //foreach(dirinfo : x.mapper)

    // Prepare build.ninja
    std::string rulepath = api.get_filepath("@cgn.d//library/general.cgn.bundle/rule.ninja");
    opt.ninja->append_include(rulepath);
    
    auto *entry = opt.ninja->append_build();
    entry->rule = "phony";
    entry->outputs = {opt.out_prefix + opt.BUILD_ENTRY};

    if (x.cfg["shell"] == "bash") {
        // single file: run command directly
        for (auto &it : tgt_single) {
            auto *build = opt.ninja->append_build();
            build->rule    = "unix_cp";
            build->outputs = {opt.ninja->escape_path(it.first)};
            build->inputs  = {opt.ninja->escape_path(it.second)};

            entry->inputs += build->outputs;  //append entry
        }

        // file with "**" or "*"
        //  if multiple: generate script
        //  if single: run command directly (TODO)
        for (auto it : tgt_pattern) {
            auto &dst_dir = it.first;
            auto &batdesc = it.second;
            auto *build = opt.ninja->append_build();

            //create bat file
            api.mkdir(api.parent_path(batdesc.bat_file)); // Prepare folder
            std::ofstream bat{batdesc.bat_file};

            //write bat file
            bat<<"dst=$(pwd)/" + dst_dir + "\n"
                 "mkdir -p $dst\n";
            for (auto cpentry : batdesc.src_files) {
                bat<<"pushd " + cpentry.file_part1 + "\n";
                if (cpentry.recursive)
                    bat<<"find . -name '" + cpentry.file_part2 + "' "
                         "| xargs cp --parent -r -t $dst\n";
                else
                    bat<<"cp --parent -r " + cpentry.file_part2 + " $dst\n";
                bat<<"popd\n";
                
                build->implicit_inputs += {opt.ninja->escape_path(cpentry.file_part1)};
            }

            // close after write
            bat.close();

            // set permission
            api.set_permission(batdesc.bat_file, "0755");

            // ninja build section
            build->rule    = "run";
            build->inputs  = {opt.ninja->escape_path(batdesc.bat_file)};
            build->outputs = {opt.ninja->escape_path(dst_dir)};
            build->variables["exe"] = "bash -c";

            entry->inputs += build->outputs;
        } //foreach(tgt_pattern)

    } //x.cfg["shell"] == "bash"

    cgn::TargetInfos rv;
    auto *def = rv.get<cgn::DefaultInfo>();
    def->target_label = opt.factory_ulabel;
    def->build_entry_name = entry->outputs[0];
    def->outputs = entry->inputs;
    def->enforce_keep_order = true;

    return rv;
} //FileCollect::interpret()

cgn::TargetInfos FileCollect::Context::add_target_dep(
    const std::string &label, const cgn::Configuration &cfg
) {
    auto rhs = api.analyse_target(label, cfg);
    if (rhs.errmsg.size())
        throw std::runtime_error{
            opt.factory_ulabel + " add_target_dep(" + label + "): " + rhs.errmsg
        };

    api.add_adep_edge(rhs.adep, opt.adep);
    auto *inf = rhs.infos.get<cgn::DefaultInfo>(false);
    if (inf)
        _order_only_dep.push_back(inf->build_entry_name);
    
    return rhs.infos;
} //FileCollect::Context::add_target_dep()


void BinDevelCollect::Context::add_from_target(const std::string &label)
{
    auto dep = api.analyse_target(label, cfg);
    if (dep.errmsg.size())
        throw std::runtime_error{dep.errmsg};
    gp.add_target_dep(label, cfg);

    auto *devinfo = dep.infos.get<BinDevelInfo>(false);
    if (devinfo) { //perferred
        include.push_back({devinfo->include_dir, {"*"}});
        bin.push_back({devinfo->bin_dir, {"*"}});
        lib.push_back({devinfo->lib_dir, {"*"}});
        return ;
    }
    
    // if no BinDevelInfo provider, guess from CxxInfo and LinkAndRunInfo
    auto *cxinfo = dep.infos.get<cxx::CxxInfo>(false);
    if (cxinfo)
        for (auto &incdir : cxinfo->include_dirs)
            include.push_back({incdir, {"**.h", "**.hpp", "**.hxx", "**.hh"}});

    auto *lrinfo = dep.infos.get<cgn::LinkAndRunInfo>(false);
    if (lrinfo) {
        for (auto &so : lrinfo->shared_files) {
            auto dir = api.parent_path(so);
            auto file = so.substr(dir.size());
            lib.push_back({dir, {file}});
        }
        for (auto &a : lrinfo->static_files) {
            auto dir = api.parent_path(a);
            auto file = a.substr(dir.size());
            lib.push_back({dir, {file}});
        }
        for (auto &rtfile : lrinfo->runtime_files) {
            auto dir = api.parent_path(rtfile.second);
            auto file = rtfile.second.substr(dir.size());
            std::string ext = get_ext(file);
            if (cfg["os"] == "win") {
                if (ext == "exe" || ext == "dll")
                    bin.push_back({dir, {file}});
            }
            else if (ext == "")
                bin.push_back({dir, {file}});
        }
    }
} //BinDevelCollect::Context::add_from_target()

cgn::TargetInfos BinDevelCollect::interpret(
    BinDevelCollect::context_type &x, cgn::CGNTargetOpt opt
) {
    FileCollect::context_type gp(x.cfg, opt);

    auto add_to_gp = [&](const std::vector<FilePattern> &list, const std::string &dst_dir) {
        for (auto &it : list)
            gp.add(it.src_basedir, it.src_files, dst_dir);
    };
    add_to_gp(x.include, "include");
    add_to_gp(x.bin, "bin");
    if (x.cfg["cpu"] == "x86_64")
        add_to_gp(x.lib, "lib64");
    else
        add_to_gp(x.lib, "lib");

    return FileCollect::interpret(x.gp, opt);
} //BinDevelCollect::interpret()