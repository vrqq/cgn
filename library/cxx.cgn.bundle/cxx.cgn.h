// cflags, ldflags: the last would cover the previous one
// include_dirs: search from left to right
//
// Order of cxx compiler arguments
//   LinkAndRunInfo:
//      link.exe <ldflags_interpreter> <ldflags_deps> <ldflags_current>
//   CxxInfo:
//      gcc.exe / cl.exe
//              <cflags_interpreter> <cflags_deps> <cflags_current>
//              <incdir_current> <incdir_deps> <incdir_interpreter>
//              <defines_any_order>
//
// ReturnValue[LinkAndRunInfo]
//   <current_target> + <deps>
// ReturnValue[CxxInfo]
//   .cflags  : <deps> <this.pub>
//   .ldflags : <deps> <this.pub>
//   .include_dirs : <this.pub> <deps>
//   .defines : any order
//
// 
// Supported configurations
// * cxx_toolchain="msvc" os="win"
//      cl.exe with link.exe
// * cxx_toolchain="gcc"  os="linux"
//      gcc/g++ with binutils-ld
// * cxx_toolchain="llvm" os="linux"
//      clang/clang++ with llvm-ld
// * cxx_toolchain="llvm" os="mac"
//      clang/clang++ with bsd-ld (os internal)
//
// Others are under development
// * toolchain="gcc"  os="mac"
//      GNU binutils's ld does not support Darwin (macOS)
//      gcc/g++ with bsd-ld (os internal)
// * toolchain="llvm" os="win"
//      clang++ and llvm-ld for windows
// * toolchain="llvm2" os="mac" (using llvm-ld)
//
#pragma once
#include <vector>
#include <unordered_set>
#include "../../cgn.h"
#include "windef.h"

namespace cxx {

enum class DepType : char{
    // (DO NOT CONSUME ANY TargetInfo from dep)
    // only define the build order, drop the return info from deps. 
    _order_dep = 1,

    // (aka PRIVATE)
    // the default flag, the dependents were only used in private,
    // see details in cxx language note.
    _private_dep = 1L << 2,

    // (aka PUBLIC)
    // consume and inherit the CxxInfo and LinkAndRunInfo from dependents, 
    // like to add '/WHOLEARCHIVE' if current is exe target.
    _inherit = 1L << 3,

    _pack_obj = 1L << 4,

    _bypass_obj = 1L << 5,
};

constexpr static DepType order_dep   = DepType::_order_dep;
constexpr static DepType private_dep = DepType::_private_dep;
constexpr static DepType inherit     = DepType::_inherit;
constexpr static DepType pack_obj    = DepType::_pack_obj;
constexpr static DepType bypass_obj  = DepType::_bypass_obj;

inline constexpr bool 
operator&(DepType a, DepType b) { return ((char)a & (char)b); }

inline constexpr DepType 
operator|(DepType a, DepType b) { return DepType((char)a | (char)b); }

struct CxxInfo : cgn::BaseInfo {
    std::unordered_set<std::string>  //(PENDING: unordered_set also accepted)
        defines;       // c++ define (no escape)
    std::vector<std::string>
        include_dirs,  // dirs (no escape, '/' separate)
                       // as TargetInfos: relavent to working-root
                       // as UserTargetFactory define: relavent to current BUILD.cgn.cc 
        cflags,        // compiler specific cflags, shell-escaped required
                       // e.g.: "-Idir\\ 1"
        ldflags;       // flags when linking, shell-escaped required
                       // e.g.："-Wl,--rpath=\\$ORIGIN", "/L:ws2_32.lib"
                       
    static const char *name() { return "CxxInfo"; }
    CxxInfo() : BaseInfo{&_glb_cxx_vtable()} {}

private:
    LANGCXX_CGN_BUNDLE_API const static cgn::BaseInfo::VTable &_glb_cxx_vtable();
};

struct CxxContext : CxxInfo
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
    std::vector<std::string> srcs; 

    // the cxx build argument apply on target who depended on current one,
    // but not apply on current target.
    CxxInfo pub;

    cgn::Configuration &cfg;

    // add_dep() would not change (CxxInfo*)this / this.pub 
    // whether flag == priv_dep / inherit / order_only
    LANGCXX_CGN_BUNDLE_API cgn::CGNTarget add_dep(
        const std::string &label, cgn::Configuration cfg, DepType flag
    );

    cgn::CGNTarget add_dep(const std::string &label, DepType flag) {
        return add_dep(label, this->cfg, flag);
    }

    // void set_runtime(const std::string &rout, const std::string &src);

protected:
    LANGCXX_CGN_BUNDLE_API CxxContext(char role, cgn::CGNTargetOptIn *opt);

private: friend struct CxxInterpreter; // field for interpreter
    friend struct TargetWorker;
    cgn::CGNTargetOptIn *opt;  //self opt

    // collection from deps, as the part of interpreter return value.
    // also with [CxxInfo] and [DefaultInfo] field inherit from deps.
    // Added by this->add_dep(), consumed by TargetWorker::step30_prepare_opt().
    cgn::InfoTable _pub_infos;

    // collection from deps, only apply on current target.
    // Added by this->add_dep(), consumed by TargetWorker::step2_merge_selfarg().
    cxx::CxxInfo        _cxx_to_self;
    cgn::LinkAndRunInfo _lnr_to_self;

    // [only valid when this == CxxShared]
    // path of xxx.a, param to /WHOLEARCHIVE
    std::vector<std::string> _wholearchive_a;

    // target name in build.ninja when cxx::order_dep
    //   build current_cxx : ... || <phony_order_only...>
    // std::vector<std::string> phony_order_only;

    // got TargetInfos[DefaultInfo].enforce_keep_order 
    // from add_dep(..., cxx::inherit) then transport this flag
    // bool _enforce_self_order_only = false;
};

template <char ROLE> struct CxxContextType : CxxContext {
    CxxContextType(cgn::CGNTargetOptIn *opt)
    : CxxContext(ROLE, opt) {}
};
using CxxSourcesContext = CxxContextType<'o'>;
using CxxStaticContext  = CxxContextType<'a'>;
using CxxSharedContext  = CxxContextType<'s'>;
using CxxExecutableContext = CxxContextType<'x'>;

struct CxxToolchainInfo
{
    std::string c_exe, cxx_exe;
    
    // MSVC143 : Visual C++ 2022 (aka Visual C++ 14.3)
    // MSVC142 : Visual C++ 2019 (aka Visual C++ 14.2)
    // MSVC141 : Visual C++ 2017 (aka Visual C++ 14.1)
    // MSVC140 : Visual C++ 2015 (aka Visual C++ 14.0)
    std::string msvc_ver1;
};

struct CxxInterpreter
{
    using context_type = CxxContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle"};
    }

    LANGCXX_CGN_BUNDLE_API static CxxToolchainInfo 
    test_param(const cgn::Configuration &cfg);

    // generate the mimimum cflags and ldflags for external build system like
    // pkg-config or cmake.
    // @return : CxxInfo::cflags and CxxInfo::ldflags
    LANGCXX_CGN_BUNDLE_API static CxxInfo
    test_minimum_flags(
        cgn::Configuration &cfg, const CxxInfo &in,
        const std::string &libfile = "");

    LANGCXX_CGN_BUNDLE_API static void
    interpret(context_type &x);
};

template<typename TypeContext>
struct CxxInterpreterIF : public CxxInterpreter {
    using context_type = TypeContext;
};

using CxxSharedInterpreter  = cxx::CxxInterpreterIF<cxx::CxxSharedContext>;
using CxxStaticInterpreter  = cxx::CxxInterpreterIF<cxx::CxxStaticContext>;
using CxxSourcesInterpreter = cxx::CxxInterpreterIF<cxx::CxxSourcesContext>;
using CxxExecutableInterpreter = cxx::CxxInterpreterIF<cxx::CxxExecutableContext>;


// Section: prebulit cxx library
// -----------------------------

struct PrebuiltContext {
    const std::string name;

    CxxInfo pub;

    // perferred dll storage dir for windows or pkg mode for all os.
    std::string runtime_dir;

    //windows shared lib: both .dll and .libs
    //windows static lib: .libs
    //linux shared/static lib: .so / .a / .o
    std::vector<std::string> files;

    PrebuiltContext(cgn::CGNTargetOptIn *opt) : opt(opt) {}

private: friend class CxxPrebuiltInterpreter;
    cgn::CGNTargetOptIn *opt;
};

struct CxxPrebuiltInterpreter {
    using context_type = PrebuiltContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle"};
    }
    LANGCXX_CGN_BUNDLE_API static void interpret(context_type &x);
};

} //namespace cxx

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