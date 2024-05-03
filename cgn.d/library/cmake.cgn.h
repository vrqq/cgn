// Rule cmake("name", x)
// return value : LinkAndRunInfo[], CxxInfo[]
// depend on    : cxx.cgn.h
//
#pragma once
#include <vector>
#include <unordered_map>
#include "../cgn.h"
#include "../rule_marco.h"
#include "../provider_dep.h"
#include "cxx.cgn.bundle/cxx.cgn.h"

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

    std::vector<std::string> outputs;

    // the CxxInfo and LinkAndRunInfo return value would be filled from 
    // CMAKE_INSTALL_BINDIR, CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR,
    // the variable below would be appended into LinkAndRunInfo.runtime[]
    // cgn::FileLayout additional_runtime;

    cxx::CxxInfo pub;

    CMakeContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt);

    // user should read return value then fill into vars manually.
    using cgn::TargetInfoDep<true>::add_dep;

    friend class CMakeInterpreter;
};

struct CMakeInterpreter {
    using context_type = CMakeContext;
    
    constexpr static cgn::ConstLabelGroup<2> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle",
                "@cgn.d//library/cmake.cgn.cc"};
    }
    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
}; //CMakeInterpreter

// } //namespace


#define cmake(name, x) CGN_RULE_DEFINE(::CMakeInterpreter, name, x)