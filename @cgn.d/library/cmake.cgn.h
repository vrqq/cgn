// Rule cmake("name", x)
// return value : LinkAndRunInfo[], CxxInfo[]
// depend on    : cxx.cgn.h
//
#ifdef _WIN32
    #ifdef CMAKE_CGN_IMPL
        #define CMAKE_CGN_API  __declspec(dllexport)
    #else
        #define CMAKE_CGN_API
    #endif
#else
    #define CMAKE_CGN_API __attribute__((visibility("default")))
#endif
#pragma once
#include <vector>
#include <unordered_map>
#include "../cgn.h"
#include "../rule_marco.h"
#include "../provider_dep.h"
#include "cxx.cgn.bundle/cxx.cgn.h"
#include "general.cgn.bundle/bin_devel.cgn.h"

// namespace cmake{

// variables may pre-inited
//   CMAKE_INSTALL_PREFIX
//   CMAKE_C_COMPILER
//   CMAKE_CXX_COMPILER
//   CMAKE_BUILD_TYPE
//   CMAKE_SYSROOT          (inited when cfg["sysroot"] existed)
//   CMAKE_SYSTEM_NAME?      (inited when cross-compile)
//   CMAKE_SYSTEM_PROCESSOR? (inited when cross-compile)
//   CMAKE_C_COMPILER_TARGET   (inited when cross-compile)
//   CMAKE_CXX_COMPILER_TARGET (inited when cross-compile)
struct CMakeContext : cgn::TargetInfoDep<true> {
    
    const std::string name;

    //read only, using vars instead cfg to control target build
    using cgn::TargetInfoDep<true>::cfg;

    // There is a bug in the Ninja file generated by CMake, which is causing 
    // the CMake installation to fail.
    bool enforce_havedep_mode = true;

    // folder of CMakeLists.txt
    std::string sources_dir;

    std::unordered_map<std::string, std::string> vars;

    // *.a *.so *.dll, no .h required.
    std::vector<std::string> outputs;
    
    // BinDevelInfo    output_bin_devel;

    // the CxxInfo and LinkAndRunInfo return value would be filled from 
    // CMAKE_INSTALL_BINDIR, CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR,
    // the variable below would be appended into LinkAndRunInfo.runtime[]
    // cgn::FileLayout additional_runtime;

    cxx::CxxInfo pub;

    CMAKE_CGN_API CMakeContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt);

    // user should read return value then fill into vars manually.
    using cgn::TargetInfoDep<true>::add_dep;

    BinDevelInfo *get_bindevel(const std::string &factory_label) 
    { return add_dep(factory_label).get<BinDevelInfo>(false); }

    friend class CMakeInterpreter;
    friend class CMakeConfigInterpeter;
};

struct CMakeInterpreter {
    using context_type = CMakeContext;
    
    constexpr static cgn::ConstLabelGroup<2> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle",
                "@cgn.d//library/cmake.cgn.cc"};
    }
    CMAKE_CGN_API static cgn::TargetInfos 
    interpret(context_type &x, cgn::CGNTargetOpt opt);
}; //CMakeInterpreter

// struct CMakeConfigContext : cgn::TargetInfoDep<true>
// {
//     const std::string name;

//     //read only, using vars instead cfg to control target build
//     using cgn::TargetInfoDep<true>::cfg;
    
//     // folder of CMakeLists.txt
//     std::string sources_dir;

//     std::unordered_map<std::string, std::string> vars;

//     // *.a *.so *.dll, no .h required.
//     // std::vector<std::string> outputs_to_lib;

//     CMAKE_CGN_API CMakeConfigContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt);

//     // user should read return value then fill into vars manually.
//     using cgn::TargetInfoDep<true>::add_dep;

//     BinDevelInfo *get_bindevel(const std::string &factory_label) 
//     { return add_dep(factory_label).get<BinDevelInfo>(false); }

//     friend class CMakeConfigInterpeter;
// }; //CMakeConfigContext

struct CMakeConfigInterpeter
{
    using context_type = CMakeContext;
    
    constexpr static cgn::ConstLabelGroup<3> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle",
                "@cgn.d//library/general.cgn.bundle",
                "@cgn.d//library/cmake.cgn.cc"};
    }

    CMAKE_CGN_API static cgn::TargetInfos 
    interpret(context_type &x, cgn::CGNTargetOpt opt);
};

// struct CMakeMultiContext : CMakeContext {
//     using CMakeContext::CMakeContext;
//     void add_target(const std::string &suffix, std::initializer_list<std::string> _outputs);
// };

// std::vector<std::shared_ptr<void>> cmake_multi_processor();

// } //namespace


#define cmake(name, x) CGN_RULE_DEFINE(::CMakeInterpreter, name, x)
#define cmake_config(name, x) CGN_RULE_DEFINE(::CMakeConfigInterpeter, name, x)