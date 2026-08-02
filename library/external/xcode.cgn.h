#pragma once
#include <string>
#include <unordered_map>
#include "../../cgn.h"
#ifdef _WIN32
    #ifdef XCODEPROJ_CGN_IMPL
        #define XCODEPROJ_CGN_API  __declspec(dllexport)
    #else
        #define XCODEPROJ_CGN_API
    #endif
#else
    #define XCODEPROJ_CGN_API __attribute__((visibility("default")))
#endif


namespace xcode {

// Parameter to xcodebuild
//  * buildsettings[CONFIGURATION_BUILD_DIR]=<target_out_dir>/bin
// xcodebuild 
//  -project <context.project>
//  -scheme <context.scheme> or -target <context.targets[0]> ...
//  -configuration <cfg[optimization]>
//  -sdk <context.sdk>
//  -arch <cfg[cpu]>
//  -xcconfig <context.xcconfig>
//  -derivedDataPath <target_out_dir>/derived
//  x.buildsettings[] (k=v k=v ...)
// ---------------------------------------------------------------
struct XCodeProjectContext : protected cgn::QuickDepContext
{
    const std::string name;

    cgn::Configuration &cfg;

    // path to 'xxx.xcodeproj'
    std::string project;

    // arg -xcconfig
    std::string xcconfig;

    // build specified targets inside project
    // empty list to build all targets
    std::vector<std::string> targets;

    // build specified shared scheme inside project
    // if non-empty, this is used instead of targets
    std::string scheme;

    // file insided with relavent path of <target_out>/bin
    std::vector<std::string> outputs;

    // Files that should cause xcodebuild to rerun when changed.
    std::vector<cgn::CGNPath> extra_watch_files;

    // This value would be checked with cfg[os], empty allowed.
    // possible value: macosx10.11
    std::string sdk;

    std::unordered_map<std::string, std::string> buildsettings;

    XCODEPROJ_CGN_API XCodeProjectContext(cgn::CGNTargetOpt *opt) 
    : cgn::QuickDepContext(opt), name(opt->name), cfg(opt->cfg) {}

    friend struct XCodeProjectInterpreter;
};

struct XCodeProjectInterpreter
{
    using context_type = XCodeProjectContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/external/xcode.cgn.cc"};
    }

    // Return
    //  [DefaultInfo]
    //      output: .kext, .dylib, executable.<no-extension>
    //  [LinkAndRunInfo]
    //      shared: .dylib
    //      static: .a
    XCODEPROJ_CGN_API static void interpret(context_type &x);
};

}

#define xcode_project(name, x) CGN_RULE_DEFINE(xcode::XCodeProjectInterpreter, name, x)
