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
    // only define the build order, drop the return info from deps. 
    // Add order_only dependency for current target.
    _order_dep = 1,

    // (aka PRIVATE)
    // the default flag, the dependents were only used in private,
    // utilize dep[CxxInfo] and dep[LinkAndRunInfo] in private and 
    // do not expose from current target,
    // see details in cxx language note.
    _private_dep = 1L << 2,

    // (aka PUBLIC)
    // consume and inherit the dep[CxxInfo] and dep[LinkAndRunInfo] from dependents, 
    _inherit = 1L << 3,

    // ar rcs on both self.srcs[] and deps[LinkAndRun].object_files
    // consume dep[CxxInfo].object_files to current static library (ar rcs).
    _archive = 1L << 4,

    // [shared_library only] no-whole-archive on deps[LinkAndRun].static_library
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

    // Add target dependency
    // @param label : factory label
    // @param cfg   : the config
    // @param flag  :
    //    - order_dep : same as order_only in ninjabuild
    //    - priv_dep  : apply CxxInfo and LinkAndRunInfo on current target only
    //    - inherit   : priv_dep and expose from current target
    //    - archive   : (static lib only) archive deps[LinkAndRun].object to current .a
    //    - no_whole  : (shared lib only) do not use /WHOLEARCHIVE on deps[LinkAndRun].static
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



struct CxxToolchainInfo
{
    std::string exe_cc, exe_cxx, exe_asm, exe_solink, exe_xlink, exe_ar;

    // is_compiler_controlled_link: 
    //   true: $(exe_cc == exe_solink) -fuse-ld=lld -fuse-ld=gold
    bool is_compiler_controlled_link;
 
    // for MSVC : derivatives of vcvarsall.bat
    // for RHEL : "scl enable gcc-toolset-x bash" (NOT IMPLEMENT)
    cgn::GraphNode *env_loader_script_anode = nullptr;
    std::string     env_loader_script;
    
    // MSVC143 : Visual C++ 2022 (aka Visual C++ 14.3)
    // MSVC142 : Visual C++ 2019 (aka Visual C++ 14.2)
    // MSVC141 : Visual C++ 2017 (aka Visual C++ 14.1)
    // MSVC140 : Visual C++ 2015 (aka Visual C++ 14.0)
    std::string msvc_ver1;

    //
    // extra_cflags_cpp, extra_cflags_c, extra_cflags_asm: extra flag for specific language.
    //
    // No str-escape for the variable below; user must escape as needed.  
    // The first part is usually compiler options and needs no escaping  
    // (e.g., `--sysroot=`). Only the latter part generally requires it  
    // (e.g., `$ORIGIN` → `\$ORIGIN`).  
    // CxxInfo arg;

    // In stage1, all variable valid
    // In stage2, merge all variable into only *_src.cflags and *_out.ldflags.
    // In stage3, generate ninja file
    // TODO: rename to c_opt, cpp_opt, exe_opt, ar_opt
    struct {
        std::vector<std::string> include_dirs, cflags, defines;
    }c_arg, cpp_arg, asm_arg;
    struct {
        std::vector<std::string> ldflags, compiler_driven_ldflags;
    }exe_arg, so_arg;

    std::vector<std::string> ar_arg_arflags;
}; //struct CxxToolchainInfo

class CxxWorker {
public:
    // win10==0x0A00; win7==0x0601;
    // win8.1/Server2012R2==0x0603;
    static constexpr const char* DEFAULT_MINIMUM_WINVER = "0x0A00";

    virtual std::string step1_test_param(cgn::Configuration &cfg, const std::string &via);

    // TODO: cannot visit CxxContext from inherit class.
    virtual std::string step2_confirm(CxxContext &x);

    virtual void step3_gen_ninja();

    template<typename T, 
        typename = typename std::enable_if<std::is_base_of<CxxWorker, T>::value>::type> 
    static std::shared_ptr<void> bind_custom_worker(const std::string &name) {
        std::string label = api.get_debug_runtime()->label + ":" + name;
        gen_worker[label] = []() ->std::unique_ptr<CxxWorker> { return std::unique_ptr<T>(new T); };
        return std::shared_ptr<void>(nullptr, [label](void*){
            gen_worker.erase(label);
        });
    }

protected: friend class CxxInterpreter;
    // step1 output (for step2 input)
    CxxToolchainInfo s1out;

    // dep_pack.run  (-> exe)
    // dep_pack.obj  (-> static_lib) pack whole
    // dep_pack.a    (-> shared)     pack whole
    // dep_use.a     (-> shared)     pack necessary
    // dep_use.obj   (-> shared)     pach whole
    // dep_use.so    (-> shared)     link
    // self_extra
    //
    // step2 output, for step3 input
    // self_src, self_extra
    // - self_src    have {'a.cpp', 'b.cpp'}, don't have '*.cpp'
    // - self_extra  have LinkAndRunInfo{.shared[], .static[], .object[]}
    // exe_cc, exe_link, ...
    // cflags_c, cflags_cpp, cflags_asm, ldflags_so, ldflags_exe, arflags
    // - cflags_ANY  don't have {'/I$include_dirs', '/D$defines' ...}
    // - ldflags_ANY don't have {'-wholearchive', '/DEF', ...}
    // - arflags     don't have {'/DEF:', ...}
    char target_role;
    std::string perferred_binary_name;
    cgn::CGNTargetMaker *mk;
    CxxToolchainInfo s2out;
    std::vector<std::string> self_src;
    cgn::LinkAndRunInfo      self_extra; // considered as output of self_src
    std::vector<std::string> *self_extra_no_whole = nullptr;
    std::vector<std::string> ninja_order_only_dep;
    
    // step3 output : s3out
    // generate current target output by 
    //   * compiler and its flags $s2out
    //   * source code $self_src
    //   * lib deps $self_extra
    // foreach ninja obj target, set order_only=$ninja_order_only_dep
    cgn::LinkAndRunInfo s3out;

protected:
    //helper function: convert list to string
    template<typename T> static std::string 
    list2str(const T &in, const std::string prefix="", 
        std::function<std::string(const std::string&)> fn_escape = nullptr)
    {
        std::string rv;
        for (auto &it : in)
            rv += prefix + (fn_escape?fn_escape(it):it) + " ";
        return rv;
    }

    std::string two_escape(const std::string &in) const {
        return cgn::NinjaFile::escape_path(
            cgn::CGN::shell_escape(in, mk->trimmed_cfg["host_shell"])
        );
    }
    std::vector<std::string> two_escape(std::vector<std::string> in) const {
        for (auto &it : in)
            it = two_escape(it);
        return in;
    }

private:
    // storage for bind_worker()
    static std::unordered_map<std::string, std::function<std::unique_ptr<CxxWorker>()>> gen_worker;

    void default_step3_win();
    void default_step3_xnix();
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