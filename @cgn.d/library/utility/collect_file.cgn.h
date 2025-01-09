#pragma once

#ifdef _WIN32
    #ifdef CGN_UTILITY_IMPL
        #define CGN_UTILITY_API  __declspec(dllexport)
    #else
        #define CGN_UTILITY_API
    #endif
#else
    #define CGN_UTILITY_API __attribute__((visibility("default")))
#endif

#include <cgn>
struct CopyInterpreter
{
    struct CopyResult
    {
        constexpr static char BASE_ON_OUTPUT = 0;
        constexpr static char BASE_ON_WORKINGROOT = 1;
        constexpr static char ABSOLUTE_PATH = 2;
        char type;

        std::string relpath;
    };
    
    struct CopyContext
    {
        const std::string &name;
        cgn::Configuration &cfg;

        // copy_wrbase_to_output
        // Argument:
        //      rel_srcs=["mod1/a.h"], 
        //      src_base_prefix_of_working_root="project1", 
        //      rel_in_output="include"
        // Result:
        //      <shell_cwd>/project1/mod1/a.h ==> <target.out_prefix>/include/mod1/a.h
        //
        // From: <workingroot (cwd)>/<arg:srcbase>/<arg:rel_srcs[]> 
        //   To: <target.out_prefix>/<arg:rel_out>/<arg:rel_srcs[]>
        //
        // If rel_src is absolute path, src_base_prefix_of_working_root of that entry will be ignored.
        // @param rel_srcs[] : the filepath base on the second argument
        // @param src_base_prefix_of_working_root : the folder prefix base on working root
        // @param rel_in_output: the output folder inside out_prefix
        // @return : {BASE_ON_OUTPUT, <rel_in_output>}
        CGN_UTILITY_API CopyResult copy_wrbase_to_output(
            const std::vector<std::string> rel_srcs,
            const std::string &src_base_prefix_of_working_root, 
            const std::string &rel_dir_in_output = ""
        );

        // copy_thisbase_to_output
        // Argument:
        //      rel_srcs=["mod1/a.h"], 
        //      src_base_prefix_of_this_script="" or ".", 
        //      rel_in_output="include"
        // Result:
        //      <target.src_prefix>/mod1/a.h ==> <target.out_prefix>/include/mod1/a.h
        //
        // From: <target.src_prefix>/<arg:srcbase>/<arg:rel_srcs[]> 
        //   To: <target.out_prefix>/<arg:rel_out>/<arg:rel_srcs[]>
        //
        // If rel_src is absolute path, src_base_prefix_of_working_root of that entry will be ignored.
        // @param rel_srcs[] : the filepath base on the second argument
        // @param src_base_prefix_of_working_root : the folder prefix base on current dir
        // @param rel_in_output: the output folder inside out_prefix
        // @return : {BASE_ON_OUTPUT, <rel_in_output>}
        CGN_UTILITY_API CopyResult copy_thisbase_to_output(
            const std::vector<std::string> rel_srcs,
            const std::string &src_base_prefix_of_this_script, 
            const std::string &rel_dir_in_output = ""
        ) {
            return copy_wrbase_to_output(rel_srcs, 
                opt->src_prefix + src_base_prefix_of_this_script, rel_dir_in_output);
        }


        // flat_copy_thisbase_to_output
        // Argument:
        //      rel_srcs=["mod1/a.h", "mod2/b.h", "folder3"],
        //      rel_in_output="include"
        // Result:
        //      <target.src_prefix>/mod1/a.h ==> <target.out_prefix>/include/a.h
        //      <target.src_prefix>/mod2/b.h ==> <target.out_prefix>/include/b.h
        //      <target.src_prefix>/folder3  ==> <target.out_prefix>/include/folder3
        //
        // @return : [{BASE_ON_OUTPUT, <rel_in_output>/<item in rel_srcs[]>}]
        CGN_UTILITY_API std::vector<CopyResult> flat_copy_thisbase_to_output(
            const std::vector<std::string> rel_srcs,
            const std::string &rel_dir_in_output = ""
        );


        // CGN_UTILITY_API copy_rename_thisbase_to_output(
        //     const std::string &rel_src, const std::string &rel_dst
        // );


        std::vector<CopyResult> target_results;
        
        cgn::CGNTarget add_dep(const std::string &label) { return add_dep(label); }
        cgn::CGNTarget add_dep(const std::string &label, const cgn::Configuration &cfg) {
            return opt->quick_dep(label, cfg);
        }

        CopyContext(cgn::CGNTargetOptIn *opt) 
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

    private: friend class CopyInterpreter;
        cgn::CGNTargetOptIn *opt;

        // keep order by copy_xxx()
        // pair<, >
        struct Record {
            std::string src_path;      // src_file_or_folder_inside_shellcwd
            std::string out_dir;       // dst_dir_inside_outprefix
            std::string out_filename;  // copied filename in dst_dir
        };
        std::vector<Record> records;
    };
    
    using context_type = CopyContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/collect_file.cgn.cc"};
    }
    CGN_UTILITY_API static void interpret(context_type &x);
}; //struct CopyInterpreter

#define copy(name, x) CGN_RULE_DEFINE(::CopyInterpreter, name, x)
