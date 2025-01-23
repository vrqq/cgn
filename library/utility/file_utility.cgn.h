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

#include <vector>
#include <string>
#include <unordered_map>
#include <cgn>

namespace cgnv1 {

struct CGNPath {

    constexpr static char BASE_ON_OUTPUT      = 0;
    constexpr static char BASE_ON_SCRIPT_SRC  = 1;
    constexpr static char BASE_ON_WORKINGROOT = 2;
    char type = 2;

    std::string rpath;

    CGNPath(char t, const std::string &rel) : type(t), rpath(rel) {}
};

inline CGNPath make_path_base_out(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_OUTPUT, rel};
}
inline CGNPath make_path_base_script(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_SCRIPT_SRC, rel};
}
inline CGNPath make_path_base_working(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_WORKINGROOT, rel};
}

} //namespace

struct BinDevelOpt
{
    // const static int allow_bindevel    =     0b1;
    // const static int allow_cxxinclude  =    0b10;
    // const static int allow_linknrun    =  0b1100;
    // const static int allow_output      = 0b10000;
    
    bool allow_bindevel    = false;
    bool allow_cxxinclude  = false;
    bool allow_linknrun    = false;
    bool allow_output      = false;
    
    cgn::CGNPath target_dir = cgn::make_path_base_out();

    std::string perferred_libdir;
};

struct FileUtility
{
    const std::string &name;
    cgn::Configuration &cfg;

    void copy_on_build(
        const std::vector<std::string> &src, 
        const cgn::CGNPath &src_base, 
        const cgn::CGNPath &dst_dir
    );

    void flat_copy_on_build(
        const std::vector<cgn::CGNPath> &src_list, 
        const cgn::CGNPath &dst_dir
    );

    using DevelOpt = BinDevelOpt;
    cgn::CGNTarget collect_devel_on_build(
        const std::string &label, 
        BinDevelOpt opt
    );

    // --TODO--
    // cgn::CGNPath copy_rename_on_build(
    //     const cgn::CGNPath &src_file,
    //     const cgn::CGNPath &dst_file
    // );
    // cgn::CGNPath analysis_regex_file(
    //     const cgn::CGNPath &template_file,
    //     std::unordered_map<std::string, std::string> vars,
    //     const cgn::CGNPath &out_file
    // );
    // std::vector<cgn::CGNPath> analysis_gen_pkgconfig();
    // std::vector<cgn::CGNPath> analysis_gen_cmakeconfig();
    // --END TODO--

    std::vector<cgn::CGNPath> perferred_outputs;

    FileUtility(cgn::CGNTargetOptIn *opt) 
    : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

private: friend class FileUtilityInterpreter;
    cgn::CGNTargetOptIn *opt;

    // copy_xxx()
    struct CopyRecord {
        const char *command;
        std::string desc;
        std::function<std::vector<std::string>(cgn::CGNTargetOpt *opt)> arg_gen;
    };
    std::vector<CopyRecord> copy_records;

    // CGNTarget.infos<BinDevelInfo>
    bool result_have_bin_devel = false;

}; // struct FileUtility

struct FileUtilityInterpreter
{
    using context_type = FileUtility;

    constexpr static cgn::ConstLabelGroup<2> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle",
                "@cgn.d//library/utility/file_utility.cgn.cc"};
    }
    CGN_UTILITY_API static void interpret(context_type &x);
};

#define file_utility(name, x) CGN_RULE_DEFINE(::FileUtilityInterpreter, name, x)
