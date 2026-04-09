#pragma once
#include <vector>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include "../../cgn.h"
#include "windef.h"

namespace cxx {

// /WHOLEARCHIVE in default
enum class DepType : char{
    // (DO NOT CONSUME ANY TargetInfo from dep)
    // Only establishes build order; all return info from deps is dropped.
    // Adds an order-only dependency for the current target.
    _order_dep = 1,

    // (aka PRIVATE)
    // The default flag. The dependency is used privately:
    // consume dep[CxxInfo] and dep[LinkAndRunInfo] in private,
    // and do not expose them from the current target.
    // See details in the cxx language note.
    _private_dep = 1L << 2,

    // (aka PUBLIC)
    // Consume and propagate dep[CxxInfo] and dep[LinkAndRunInfo] upstream.
    _inherit = 1L << 3,

    // Archive (ar rcs) both self.srcs[] and deps[LinkAndRun].object_files
    // into the current static library.
    _archive = 1L << 4,

    // (shared library only) Do not apply whole-archive on deps[LinkAndRun].static_library.
    _no_whole = 1L << 5,
};

constexpr static DepType order_dep   = DepType::_order_dep;
constexpr static DepType private_dep = DepType::_private_dep;
constexpr static DepType inherit     = DepType::_inherit;
constexpr static DepType archive     = DepType::_archive;
constexpr static DepType _no_whole   = DepType::_no_whole;

inline constexpr bool 
operator&(DepType a, DepType b) { return ((char)a & (char)b); }

inline constexpr DepType 
operator|(DepType a, DepType b) { return DepType((char)a | (char)b); }


struct CxxInfo : cgn::BaseInfo
{
    std::vector<std::string>
        defines;       // c++ define (no escape)

    // std::vector<cgn::CGNPath>
    cgn::CGNPathArray 
        include_dirs;  // dirs (no escape, '/' separate)
                       // as TargetInfos: relavent to working-root

    std::vector<std::string>
        cflags,        // compiler specific cflags, shell-escaped required
                       // e.g.: "-Idir\\ 1"
        ldflags,       // flags for linker(auto '-Wl,' prefix added), shell-escaped required
                       // e.g.："-rpath=\\$ORIGIN", "/L:ws2_32.lib"
        arflags;       // flags for static library archiver
                       // 'ar' on linux, 'lib.exe' on windows
                       
    CxxInfo() : BaseInfo{&_glb_cxx_vtable()} {}

private:
    LANGCXX_CGN_BUNDLE_API const static cgn::BaseInfo::VTable &_glb_cxx_vtable();
}; //struct CxxInfo

struct CxxToolchainInfo
{
    std::string exe_cc, exe_cxx, exe_asm, exe_solink, exe_xlink, exe_ar;
 
    // for MSVC : derivatives of vcvarsall.bat
    // for RHEL : "scl enable gcc-toolset-x bash" (NOT IMPLEMENT)
    cgn::GraphNode *env_loader_script_anode = nullptr;
    std::string     env_loader_script;
    
    // MSVC143 : Visual C++ 2022 (aka Visual C++ 14.3)
    // MSVC142 : Visual C++ 2019 (aka Visual C++ 14.2)
    // MSVC141 : Visual C++ 2017 (aka Visual C++ 14.1)
    // MSVC140 : Visual C++ 2015 (aka Visual C++ 14.0)
    std::string msvc_ver1;

    // TBD: rename to c_opt, cpp_opt, exe_opt, ar_opt
    struct CompilingOption {
        // include_dirs and defines would be escaped in stage2 with current shell.
        std::vector<std::string> include_dirs, cflags, defines;
    }c_arg, cpp_arg, asm_arg;

    struct {
        // true: $(exe_cc == exe_solink / exe_xlink) -fuse-ld=lld 
        //       $(exe_cc == exe_solink / exe_xlink) -fuse-ld=gold
        // false: exe_solink / exe_xlink = ld / ld.lld / lib.exe / ...
        bool is_compiler_controlled_link;

        // is_compiler_controlled_link == true: ldflags like '-Wl,--rpath=$ORIGIN'
        // is_compiler_controlled_link == false: ldflags like '-rpath=\\$ORIGIN' or '/L:ws2_32.lib'
        std::vector<std::string> ldflags;
    }exe_arg, so_arg;

    std::vector<std::string> ar_arg_arflags;
}; //struct CxxToolchainInfo

struct CxxContext : CxxInfo, protected cgn::QuickDepContext
{
    // 'x': cxx_executable, 's': cxx_shared, 'a': cxx_static, 'o': cxx_sources
    const char role;
    const std::string name;

    // using this name instead of generate from x.name
    //  if not assign, the default name below:
    //  xnixStatic: "lib" + x.name + ".a"
    //  xnixShared: "lib" + x.name + ".so"
    //  xnixExE   : x.name
    //  winShared : x.name + ".dll"
    //  winStatic : x.name + ".lib"
    //  winExE    : x.name + ".exe"
    std::string perferred_binary_name;

    // only c, cpp source file included, no header required.
    // CGNPathArray srcs;
    cgn::CGNPathArray srcs;

    // the cxx build argument apply on target who depended on current one,
    // but not apply on current target.
    CxxInfo pub;

    cgn::Configuration &cfg;

    CxxToolchainInfo get_toolchain_info() {
        return get_toolchain_info(cfg);
    }
    CxxToolchainInfo get_toolchain_info(cgn::Configuration &cfg);

    // Add target dependency
    // @param label : factory label
    // @param cfg   : the config
    // @param flag  :
    //    - order_dep   : same as order_only in ninjabuild
    //    - private_dep : apply CxxInfo and LinkAndRunInfo on current target only
    //    - inherit     : private_dep and expose from current target
    //    - archive     : (static lib only) archive deps[LinkAndRun].object to current .a
    //    - no_whole    : (shared lib only) do not use /WHOLEARCHIVE on deps[LinkAndRun].static
    LANGCXX_CGN_BUNDLE_API cgn::CGNTarget add_dep(
        const std::string &label, cgn::Configuration cfg, DepType flag
    );

    cgn::CGNTarget add_dep(const std::string &label, DepType flag) {
        return add_dep(label, this->cfg, flag);
    }

    void add_ninja_order_only_dep(const std::string &ninja_target_entry) {
        quickdep_ninja_target.push_back(ninja_target_entry);
    }

protected:
    LANGCXX_CGN_BUNDLE_API CxxContext(char role, cgn::CGNTargetOpt *opt);

protected:
    friend struct CxxWorker;
    friend class CxxInterpreter;

    // collection from deps, as the part of interpreter return value.
    // also with [CxxInfo] and [DefaultInfo] field inherit from deps.
    // Added by this->add_dep(), consumed by TargetWorker::step30_prepare_opt().
    cgn::InfoTable _pub_infos;

    // collection from deps, only apply on current target.
    // Added by this->add_dep(), consumed by TargetWorker::step2_merge_selfarg().
    // _lnr_to_self.static_files[] param to /WHOLEARCHIVE
    CxxInfo             _cxx_to_self;
    cgn::LinkAndRunInfo _lnr_to_self;

    // [only valid when this == CxxShared] (role == 's')
    // static_files[] param to /NO-WHOLEARCHIVE
    // std::unordered_set<std::string> _self_no_whole_archive;
    std::vector<std::string> _self_no_whole_archive;
}; //struct CxxContext

template <char ROLE> struct CxxContextType : CxxContext {
    CxxContextType(cgn::CGNTargetOpt *opt)
    : CxxContext(ROLE, opt) {}
};
using CxxSourcesContext = CxxContextType<'o'>;
using CxxStaticContext  = CxxContextType<'a'>;
using CxxSharedContext  = CxxContextType<'s'>;
using CxxExecutableContext = CxxContextType<'x'>;


class LANGCXX_CGN_BUNDLE_API CxxWorker
{
public:
    struct PackOut {
        std::string packout_file; // .a / .so / .dll
        std::string packout_rt_filepath; // .dll path for runtime_files
        std::string packout_rt_filename; // .dll name for runtime_files
    };
    struct Stage3In {
        char target_role;
        cgn::CGNTargetMaker *mk;

        // exe_cc, exe_link, ...
        // cflags_c, cflags_cpp, cflags_asm, ldflags_so, ldflags_exe, arflags
        // - .cflags  don't have {'/I$include_dirs', '/D$defines' ...}
        // - .ldflags don't have {'-wholearchive', '/DEF', ...}
        // - arflags  don't have {'/DEF:', ...}
        CxxToolchainInfo *toolchain;

        // src_extra, src_extra
        // - self_src    have {'a.cpp', 'b.cpp'}, don't have '*.cpp'
        // - self_extra  have LinkAndRunInfo{.shared[], .static[], .object[]}
        std::string def_file;
        std::vector<std::string> src_files;
        cgn::LinkAndRunInfo      src_extra; // considered as output of self_src
        std::vector<std::string> *src_extra_no_whole = nullptr;
        std::vector<std::string> ninja_order_only_dep;

        
        // @return : the obj file of current src, and the file type ('c', 'A' or '+')
        std::pair<std::string, char> suggest_objout(std::string &src);

        // valid when target_role == 'a' / 's' / 'x'
        // For the case of windows shared lib, the .dll is runtime file and the .lib is import table.
        //    packout_file : 'path/to/<stem>.lib'
        //    packout_rt_filepath: 'path/to/<stem>.dll'
        //    packout_rt_filename: '<stem>.dll'
        // @return : lib/exe output path suggestion for current target.
        const PackOut &suggest_packout() const { return pack_out_suggestion; }

        std::vector<std::string> gen_cflags(
            const std::string &include_prefix,
            const std::string &define_prefix,
            const CxxToolchainInfo::CompilingOption &info) const;

        std::string two_escape(const std::string &in) const;
        std::vector<std::string> two_escape(std::vector<std::string> in) const;
        std::string two_escape_to_string(std::vector<std::string> in) const;

        // for cxx_sources() of current target, use done_with_entry(obj_files[]),
        // otherwise use done_with_entry().
        cgn::LinkAndRunInfo done_with_entry(const std::vector<std::string> &obj_files);
        cgn::LinkAndRunInfo done_with_entry(const PackOut &out);
    
    private: friend class CxxWorker;
        PackOut pack_out_suggestion;
    }; //Stage3In

    // @return first: toolchain info for step2, second: error message if test failed.
    virtual std::pair<CxxToolchainInfo, std::string> 
    step1_test_param(cgn::Configuration &cfg, const std::string &via) const;
    
    // TODO: cannot visit CxxContext from inherit class, no virtual supported currently.
    Stage3In step2_confirm(CxxToolchainInfo &s1out, CxxContext &x) const;

    virtual cgn::LinkAndRunInfo step3_gen_ninja(Stage3In *in) const;

    template<typename T, 
        typename = typename std::enable_if<std::is_base_of<CxxWorker, T>::value>::type> 
    static std::shared_ptr<void> bind_custom_worker(const std::string &name) {
        std::string label = api.get_debug_runtime()->label + ":" + name;
        gen_worker[label] = []() ->std::unique_ptr<CxxWorker> { return std::unique_ptr<T>(new T); };
        return std::shared_ptr<void>(nullptr, [label](void*){
            gen_worker.erase(label);
        });
    }

private: friend class CxxInterpreter;
    // storage for bind_worker()
    static std::unordered_map<std::string, std::function<std::unique_ptr<CxxWorker>()>> gen_worker;
}; //class CxxWorker

struct CxxInterpreter
{
    using context_type = CxxContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/cxx/cxx.cgn.cc"};
    }

    // generate the cflags and ldflags for external build system like
    // pkg-config or cmake.
    //@param type : "minimum" = "cmake", "default" = "makefile"
    LANGCXX_CGN_BUNDLE_API static CxxToolchainInfo 
    test_param(cgn::Configuration &cfg, const std::string &type = "");

    LANGCXX_CGN_BUNDLE_API static void
    interpret(context_type &x);

private: friend class TargetWorker;
    // static std::unique_ptr<NinjaDedup> ninja_dedup;
}; //struct CxxInterpreter

template<typename TypeContext>
struct CxxInterpreterIF : public CxxInterpreter {
    using context_type = TypeContext;
};
using CxxSharedInterpreter     = CxxInterpreterIF<CxxSharedContext>;
using CxxStaticInterpreter     = CxxInterpreterIF<CxxStaticContext>;
using CxxSourcesInterpreter    = CxxInterpreterIF<CxxSourcesContext>;
using CxxExecutableInterpreter = CxxInterpreterIF<CxxExecutableContext>;


// Section: prebulit cxx library
// -----------------------------
struct PrebuiltContext : protected cgn::QuickDepContext {
    const std::string &name;
    const cgn::Configuration &cfg;

    CxxInfo pub;

    // perferred dll storage dir for windows or pkg mode for all os.
    std::string runtime_dir;

    //windows shared lib: both .dll and .libs
    //windows static lib: .libs
    //linux shared/static lib: .so / .a / .o
    std::vector<cgn::CGNPath> files;

    // string like : "dl", "libdl.so", "liblzo2.so.2" ...
    std::vector<std::string> system_libs;

    PrebuiltContext(cgn::CGNTargetOpt *opt) : name(opt->name), cfg(opt->cfg), cgn::QuickDepContext(opt) {}

    cgn::CGNTarget add_dep(const std::string &label, const cgn::Configuration &cfg_in) {
        return this->quick_dep(label, cfg_in, true);
    }
    cgn::CGNTarget add_dep(const std::string &label) {
        return this->quick_dep(label, cfg, true);
    }

    friend struct CxxPrebuiltInterpreter;    
};

struct CxxPrebuiltInterpreter {
    using context_type = PrebuiltContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/cxx/cxx.cgn.cc"};
    }
    LANGCXX_CGN_BUNDLE_API static void interpret(context_type &x);
};

} //namespace


#define cxx_executable(name, x) CGN_RULE_DEFINE( \
    cxx::CxxInterpreterIF<cxx::CxxExecutableContext>, name, x)
    
#define cxx_shared(name, x) CGN_RULE_DEFINE( \
    cxx::CxxInterpreterIF<cxx::CxxSharedContext>, name, x)

#define cxx_static(name, x) CGN_RULE_DEFINE( \
    cxx::CxxInterpreterIF<cxx::CxxStaticContext>, name, x)

#define cxx_sources(name, x) CGN_RULE_DEFINE( \
    cxx::CxxInterpreterIF<cxx::CxxSourcesContext>, name, x)

#define cxx_prebuilt(name, x) CGN_RULE_DEFINE( \
    cxx::CxxPrebuiltInterpreter, name, x)