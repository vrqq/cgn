#pragma once
#include <vector>
#include <unordered_set>
#include "../../cgn.h"
#include "../../rule_marco.h"

namespace cxx {

enum class DepType {
    // (DO NOT CONSUME ANY TargetInfo from dep)
    // only define the build order, drop the return info from deps. 
    _order_dep = 0,

    // (aka PRIVATE)
    // the default flag, the dependents were only used in private,
    // see details in cxx language note.
    _private_dep = 1L << 0,

    // (aka PUBLIC)
    // consume and inherit the CxxInfo and LinkAndRunInfo from dependents, 
    // like to add '/WHOLEARCHIVE' if current is exe target.
    _inherit = 1L << 1,
};

constexpr static DepType order_dep   = DepType::_order_dep;
constexpr static DepType private_dep = DepType::_private_dep;
constexpr static DepType inherit     = DepType::_inherit;

struct CxxInfo : cgn::BaseInfo {
    std::vector<std::string>  //(PENDING: unordered_set also accepted)
        include_dirs,  // dirs (no escape, '/' separate)
                       // as TargetInfos: relavent to working-root
                       // as UserTargetFactory define: relavent to current BUILD.cgn.cc
        defines;       // c++ define (no escape)
    std::vector<std::string> 
        cflags,        // compiler specific cflags, shell-escaped required
                       // e.g.: "-Idir\\ 1"
        ldflags;       // flags when linking, shell-escaped required
                       // e.g.："-Wl,--rpath=\\$ORIGIN", "/L:ws2_32.lib"
                       
    static const char *name() { return "CxxInfo"; }
    CxxInfo() : BaseInfo{&v} {}

private:
    static const VTable v;
};

struct CxxContext : CxxInfo
{
    // 'x': cxx_executable, 's': cxx_shared, 'a': cxx_static, 'o': cxx_sources
    const char role;
    const std::string name;

    std::vector<std::string> srcs; //only c, cpp source file included, no header required.

    CxxInfo pub;  //the cxx build argument apply on 

    cgn::Configuration cfg;

    // add_dep() would not change (CxxInfo*)this / this.pub 
    // whether flag == priv_dep / inherit / order_only
    cgn::TargetInfos add_dep(
        const std::string &label, cgn::Configuration cfg, DepType flag
    );
    cgn::TargetInfos add_dep(const std::string &label, DepType flag) {
        return add_dep(label, this->cfg, flag);
    }

    void set_runtime(const std::string &rout, const std::string &src);

protected:
    CxxContext(char role, const cgn::Configuration &cfg, cgn::CGNTargetOpt opt);

private: friend struct CxxInterpreter; // field for interpreter
    const cgn::CGNTargetOpt opt;  //self opt

    cgn::LinkAndRunInfo dep_lr_self;
    cxx::CxxInfo dep_cxx_self, dep_cxx_pub;

    // path of xxx.a xxx.so, public part from dep_lr_self, as the value of 
    // current target return, only used when this.role == .so/.exe
    std::vector<std::string> pub_a, pub_so;

    std::vector<std::string> phony_order_only;  // ninja target name
};

template <char ROLE> struct CxxContextType : CxxContext {
    CxxContextType(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
    : CxxContext(ROLE, cfg, opt) {}
};
using CxxSourcesContext = CxxContextType<'o'>;
using CxxStaticContext  = CxxContextType<'a'>;
using CxxSharedContext  = CxxContextType<'s'>;
using CxxExecutableContext = CxxContextType<'x'>;

struct CxxToolchainInfo
{
    std::string c_exe, cxx_exe;
    
};

struct CxxInterpreter
{
    using context_type = CxxContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle"};
    }

    static CxxToolchainInfo test_param(const cgn::Configuration &cfg);

    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
    static cgn::TargetInfos msvc_interpret(
        context_type &x, cgn::CGNTargetOpt opt
    );
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
    const cgn::Configuration cfg;

    CxxInfo pub;

    // perferred dll storage dir for windows or pkg mode for all os.
    std::string runtime_dir;

    //windows shared lib: both .dll and .libs
    //windows static lib: .libs
    //linux shared/static lib: .so / .a / .o
    std::vector<std::string> files;

    PrebuiltContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
    : name(opt.factory_name), cfg(cfg) {}
};

struct CxxPrebuiltInterpreter {
    using context_type = PrebuiltContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle"};
    }
    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
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