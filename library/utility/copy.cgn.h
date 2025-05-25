// provide CopyWorker and copu_file() rule
// c++ 17 and above in current version
// DEPS ON : @cgn.d//advcopy
//
#pragma once
#ifdef _WIN32
    #ifdef CGN_LIBRARY_COPY_IMPL
        #define CGN_LIBRARY_COPY_API  __declspec(dllexport)
    #else
        #define CGN_LIBRARY_COPY_API
    #endif
#else
    #define CGN_LIBRARY_COPY_API __attribute__((visibility("default")))
#endif

#include <vector>
#include <string>
#include <unordered_map>
#include "../../cgn.h"

// Stateful ninjafile writer
// Both srcbase and dst are absolute path or relative path of CWD.
// Stamp file: <opt.outprefix> + <argfile_prefix> + <1, 2, ...> + ".stamp"
//  Args file: <opt.outprefix> + <argfile_prefix> + <1, 2, ...> + ".rsp"
//
// in ninja build section, 'OrderOnly' dependency is enough, because the real
// file-dependency is shown as '.rsp' file.
struct CopyWorker
{
    CGN_LIBRARY_COPY_API std::string 
    preconfig(cgn::CGNTargetOptIn *opt, const std::string &argfile_prefix = "copy_");

    // @confirmed_opt : variable by opt->confirm();
    // @param from, to: absolute or relpath of CWD
    // @return Ninja build section, errmsg
    CGN_LIBRARY_COPY_API cgn::NinjaFile::BuildSection*
    postgen_copyone(
        cgn::CGNTargetOpt *confirmed_opt,
        const std::string &src_file, const std::string &dst_file,
        const std::vector<std::string> &njtargets_orderdep = {}
    );
    // CGN_LIBRARY_COPY_API cgn::NinjaFile::BuildSection*
    // postgen_copyone(
    //     cgn::CGNTargetOpt *confirmed_opt,
    //     const cgn::CGNPath &src_file, const cgn::CGNPath &dst_file,
    //     const std::vector<std::string> &ninja_orderdep = {}
    // );

    // @confirmed_opt : variable by opt->confirm();
    // @param src_patterns: the source pattern
    // @param dst_dir : the destion directory
    // @return Ninja build section, errmsg
    CGN_LIBRARY_COPY_API cgn::NinjaFile::BuildSection*
    postgen_copy(
        cgn::CGNTargetOpt *confirmed_opt, 
        const std::vector<std::string> &src_rel_patterns, 
        const std::vector<std::string> &src_rel_exclude_patterns, 
        const std::string &src_base,
        const std::string &dst_dir,
        const std::vector<std::string> &njtargets_orderdep = {}
    );

    // @confirmed_opt : variable by opt->confirm();
    // @param src_patterns: the source pattern
    // @param dst_dir : the destion directory
    // @return Ninja build section, errmsg
    CGN_LIBRARY_COPY_API cgn::NinjaFile::BuildSection*
    postgen_flat_copy(
        cgn::CGNTargetOpt *confirmed_opt,
        const std::vector<std::string> &src_patterns, 
        const std::vector<std::string> &src_exclude_patterns, 
        const std::string &dst_dir,
        const std::vector<std::string> &njtargets_orderdep = {}        
    );

    // std::vector<std::string> postgen_get_ninja_entry();

private:
    std::string argfile_prefix;
    std::string advcopy_exe_2esc;
    size_t target_n = 0;

    cgn::NinjaFile::BuildSection* mkninja(
        cgn::CGNTargetOpt *opt, const std::string &command,
        const std::vector<std::string> &arg_content,
        const std::vector<std::string> &njtargets_orderdep
    );
}; //struct CopyWorker


struct CopyInterpreter
{
    struct context_type {
        const std::string &name;
        const cgn::Configuration &cfg;

        // remove folder or file on build
        CGN_LIBRARY_COPY_API void remove_on_build(
            const std::vector<cgn::CGNPath> &path_list
        );

        // copy $src[] from $src_base directory to $dst_dir directory inside
        CGN_LIBRARY_COPY_API void copy_on_build(
            const std::vector<std::string> &src, 
            const std::vector<std::string> &src_exclude, 
            const cgn::CGNPath &src_base, 
            const cgn::CGNPath &dst_dir
        );

        // copy $src_list[] to $dst_dir directory inside
        CGN_LIBRARY_COPY_API void flat_copy_on_build(
            const std::vector<cgn::CGNPath> &src_list, 
            const std::vector<cgn::CGNPath> &src_exclude_list, 
            const cgn::CGNPath &dst_dir
        );

        // copy $src_file to $dst_file
        CGN_LIBRARY_COPY_API void copy_rename_on_build(
            const cgn::CGNPath &src_file,
            const cgn::CGNPath &dst_file
        );

        context_type(cgn::CGNTargetOptIn *opt) 
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}
        
        // the ninja target output where can trigger this build section run
        std::vector<cgn::CGNPath> ninja_build_trigger;

        // CGNTarget.result.outputs[]
        std::vector<cgn::CGNPath> analysis_outputs;

        private: friend struct CopyInterpreter;
            cgn::CGNTargetOptIn *opt;
            using FnCopyWork = std::function<std::string(cgn::CGNTargetOpt *opt, CopyWorker *w)>;
            std::vector<FnCopyWork> copy_records;
    };

    constexpr static cgn::ConstLabelGroup<1> preload_labels() { 
        return {"@cgn.d//library/utility/copy.cgn.cc"};
    }
    CGN_LIBRARY_COPY_API static void interpret(context_type &x);
};

#define copy_files(name, x) CGN_RULE_DEFINE(::CopyInterpreter, name, x)
