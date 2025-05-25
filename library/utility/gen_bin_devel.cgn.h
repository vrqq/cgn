// BinDevelWorker
//  DEPS ON: @cgn.d//library/cxx/cxx.cgn.cc
//           @cgn.d//library/utility/copy.cgn.cc
//
#pragma once
#ifdef _WIN32
    #ifdef CGN_LIBRARY_GENBINDEVEL_IMPL
        #define CGN_LIBRARY_GENBINDEVEL_API  __declspec(dllexport)
    #else
        #define CGN_LIBRARY_GENBINDEVEL_API
    #endif
#else
    #define CGN_LIBRARY_GENBINDEVEL_API __attribute__((visibility("default")))
#endif

#include "bin_devel_info.cgn.h"
#include "copy.cgn.h"

// BinDevel folder collector Worker
// features:
//  * collect result from other target
//  * generate pkgconfig
//  * generate cmakeconfig
class BinDevelWorker
{
public:
    constexpr static cgn::ConstLabelGroup<3> preload_labels() { 
        return {
            "@cgn.d//library/utility/bin_devel.cgn.cc",
            "@cgn.d//library/utility/bin_devel_info.cgn.cc",
            "@cgn.d//library/utility/copy.cgn.cc"
        };
    }

    // struct CollectOpt
    // {
    //     bool copy_from_bin_devel     = false;
    //     bool copy_from_cxx_info      = false;
    //     bool copy_from_linknrun_info = false;
    //     bool copy_from_output        = false;

    //     cgn::CGNPath target_dir = cgn::make_path_base_out();
    //     std::string perferred_libdir;
    // };

    // @return errormsg
    // std::string preconfig(cgn::CGNTargetOptIn *opt);

    // info->base => $out_prefix
    // info->inc  => $incdir (with folder structure)
    // info->lib  => $libdir (with folder structure)
    // info->bin  => $bindir (with folder structure)
    CGN_LIBRARY_GENBINDEVEL_API void copy_from_bindevelinfo(const BinDevelInfo *info);

    // CxxInfo::include_dirs => $incdir
    CGN_LIBRARY_GENBINDEVEL_API void copy_from_cxxinfo_include(const cxx::CxxInfo *info);

    // LinkAndRunInfo::shared_files  => $libdir (flat copy, no folder structure)
    // LinkAndRunInfo::static_files  => $libdir (flat copy, no folder structure)
    // LinkAndRunInfo::runtime_files => $bindir (with folder structure)
    CGN_LIBRARY_GENBINDEVEL_API void copy_from_linkandruninfo(const cgn::LinkAndRunInfo *info);

    // .exe .dll   => $bindir (flat copy)
    // .so .a .lib => $libdir (flat copy)
    CGN_LIBRARY_GENBINDEVEL_API void copy_from_output(const std::vector<std::string> &files);

    // ninja entry filenames without escape.
    std::vector<std::string> get_ninja_stamp_files() { return ninja_out_files; }

    CGN_LIBRARY_GENBINDEVEL_API BinDevelInfo get_bin_devel_info();
    // CGN_LIBRARY_GENBINDEVEL_API BinDevelInfo get_cxx_info();

    BinDevelWorker(CopyWorker *cpw, 
        cgn::CGNTargetOpt *confirmed_opt,
        std::vector<std::string> ninja_deps,
        const std::string &install_dir = "install", 
        const std::string &incdir = "include",
        const std::string &bindir = "bin",
        const std::string &libdir = "lib"
    ) : cpw(cpw), copt(confirmed_opt), ninja_deps(ninja_deps), 
        full_install_dir(api.rebase_path(install_dir, ".", copt->out_prefix)),
        full_incdir(api.rebase_path(incdir, ".", full_install_dir)),
        full_bindir(api.rebase_path(bindir, ".", full_install_dir)),
        full_libdir(api.rebase_path(libdir, ".", full_install_dir)) {}

private:
    CopyWorker *cpw;
    cgn::CGNTargetOpt *copt;
    std::vector<std::string> ninja_deps;
    std::string full_install_dir; 
    std::string full_incdir;
    std::string full_bindir;
    std::string full_libdir;

    std::vector<std::string> ninja_out_files;
    void _njadd(const cgn::NinjaFile::BuildSection *section) {
        ninja_out_files.insert(ninja_out_files.end(), section->outputs.begin(), section->outputs.end());
    }
};

// Single arch library generator
// For multilib package, using copy_files() instead
class GenerateBinDevelInterpreter
{
public:
    struct context_type {
        const std::string &name;
        const cgn::Configuration &cfg;

        std::string inc_dir = "include";
        std::string lib_dir = "lib";
        std::string bin_dir = "bin";
        bool have_cmakeconfig = false;  //TBD:is necessary?
        bool have_pkgconfig   = false;  //TBD:is necessary?

        struct CollectOpt
        {
            bool copy_from_bin_devel   = false;
            bool copy_from_cxx_include = false;
            bool copy_from_linknrun    = false;
            bool copy_from_output      = false;
        };
        CollectOpt new_collect_opt() { return CollectOpt{}; }

        CGN_LIBRARY_GENBINDEVEL_API cgn::CGNTarget collect_from_target(
            const std::string &label, const cgn::Configuration &cfg, 
            CollectOpt collect_opt);

        // void write_pkgconfig(
        //     const std::string &name,
        //     const std::string &description,
        //     const std::string &url,
        //     const std::string &version,
        //     const std::string &requires,
        //     const std::string &libs,        // var ${libdir} predefined
        //     const std::string &cflags,      // var ${include_dir} predefined
        // );

        // Usually to generate pkgconfig 
        //
        // Example of pkgconfig (filename="lib/pkgconfig/xxx.pc")
        // 
        // # abseil pkgconfig example
        // prefix=/usr/local
        // exec_prefix=${prefix}
        // libdir=/usr/local/lib64
        // includedir=/usr/local/include
        // Name: absl_atomic_hook
        // Description: Abseil atomic_hook library
        // URL: https://abseil.io/
        // Version: head
        // Requires: absl_config = head, absl_core_headers = head
        // Libs: -L${libdir}  
        // Cflags: -I${includedir} -Wnon-virtual-dtor -DNOMINMAX
        //
        CGN_LIBRARY_GENBINDEVEL_API void write_file(
            const std::string &filename,
            void(*data_writer)(const std::string &install_dir)
        );
        
        cgn::CGNTarget add_order_dep(const std::string &label, cgn::Configuration cfg) {
            return opt->quick_dep(label, cfg, false);
        }

        void add_dep(cgn::GraphNode *anode) {
            opt->quickdep_early_anodes.push_back(anode);
        }

        context_type(cgn::CGNTargetOptIn *opt) 
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

    private: friend struct GenerateBinDevelInterpreter;
        cgn::CGNTargetOptIn *opt;

        std::vector<std::function<void(BinDevelWorker*)>> workgen;
        std::vector<std::string> cpworker_dep;
    }; //context_type

    constexpr static cgn::ConstLabelGroup<4> preload_labels() { 
        return {
            "@cgn.d//library/cxx/cxx.cgn.cc",
            "@cgn.d//library/utility/copy.cgn.cc",
            "@cgn.d//library/utility/bin_devel_info.cgn.cc",
            "@cgn.d//library/utility/gen_bin_devel.cgn.cc"
        };
    }
    CGN_LIBRARY_GENBINDEVEL_API static void interpret(context_type &x);
};

#define gen_bin_devel(name, x) CGN_RULE_DEFINE(::GenerateBinDevelInterpreter, name, x)
