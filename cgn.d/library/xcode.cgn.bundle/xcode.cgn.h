#pragma once
#include <string>
#include <unordered_map>
#include "../../cgn.h"
#include "../../rule_marco.h"
#include "../../provider_dep.h"

namespace xcode {

// Parameter to xcodebuild
//  * buildsettings[CONFIGURATION_BUILD_DIR]=<target_out_dir>/bin
// xcodebuild 
//  -project <context.project>
//  -scheme All
//  -configuration <cfg[optimization]>
//  -sdk <context.sdk>
//  -arch <cfg[cpu]>
//  -target <cfg[os]>
//  -xcconfig <context.xcconfig>
//  -derivedDataPath <target_out_dir>/build
//  x.buildsettings[] (k=v k=v ...)
// ---------------------------------------------------------------
struct XCodeProjectContext : public cgn::TargetInfoDep<true>
{
    const std::string name;

    // path to 'xxx.xcodeproj'
    std::string project;

    // arg -xcconfig
    std::string xcconfig;

    // build specified targets inside project
    // empty list to build all targets
    std::vector<std::string> targets;

    // file insided with relavent path of <target_out>/bin
    std::vector<std::string> outputs;

    // This value would be checked with cfg[os], empty allowed.
    // possible value: macosx10.11
    std::string sdk;

    std::unordered_map<std::string, std::string> buildsettings;

    XCodeProjectContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt) 
    : cgn::TargetInfoDep<true>(cfg, opt), name(opt.factory_name) {}

    friend struct XCodeProjectInterpreter;
};

struct XCodeProjectInterpreter
{
    using context_type = XCodeProjectContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/xcode.cgn.bundle"};
    }

    // Return
    //  [DefaultInfo]
    //      output: .kext, .dylib, executable.<no-extension>
    //  [LinkAndRunInfo]
    //      shared: .dylib
    //      static: .a
    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
};

}

#define xcode_project(name, x) CGN_RULE_DEFINE(xcode::XCodeProjectInterpreter, name, x)