#include "xcode.cgn.h"
#include "../../std_operator.hpp"
#include "../classification.hpp"

namespace xcode {

cgn::TargetInfos XCodeProjectInterpreter::interpret(
    context_type &x, cgn::CGNTargetOpt opt
) {
    if (x.cfg["os"] != "mac")
        throw std::runtime_error{"Unsupported cfg[os], only mac accepted"};
    
    std::string projpath = api.locale_path(opt.src_prefix + x.project);

    std::string args = "xcodebuild -project " 
                     + opt.ninja->escape_path(api.shell_escape(projpath))
                     + " -scheme All ";
    
    if (x.cfg["optimization"] == "debug")
        args += "-configuration Debug ";
    else if (x.cfg["optimization"] == "release")
        args += "-configuration Release ";
    else if (x.cfg["optimization"] != "")
        args += "-configuration " + (std::string)x.cfg["optimization"] + " ";
    
    if (x.sdk.size())
        args += "-sdk " + x.sdk + " ";
    
    if (x.cfg["os"] == "mac" && x.cfg["cpu"] == "x86_64")
        args += "-arch x86_64 ";
    if (x.cfg["os"] == "mac" && x.cfg["cpu"] == "armv8a")
        args += "-arch arm64 ";  //apple M1
    
    if (x.xcconfig.size())
        args += "-xcconfig " + x.xcconfig + " ";
    
    args += "-derivedDataPath " + opt.out_prefix + "bin ";
    
    if (x.buildsettings.count("CONFIGURATION_BUILD_DIR"))
        api.print_debug(opt.factory_ulabel + " CONFIGURATION_BUILD_DIR would be overrided.");
    x.buildsettings["CONFIGURATION_BUILD_DIR"] = opt.out_prefix + "build";
    for (auto iter : x.buildsettings)
        args += cgn::Tools::shell_escape(iter.first) + "=" + cgn::Tools::shell_escape(iter.second) + " ";
    
    // generate ninja file
    constexpr const char *rule = "@cgn.d//library/general.cgn.bundle/rule.ninja";
    static std::string rule_path = api.get_filepath(rule);
    opt.ninja->append_include(rule_path);

    std::vector<std::string> njesc_esc_path_out;
    std::vector<std::string> target_outfile;
    for (auto &it : x.outputs) {
        std::string path1 = api.locale_path(opt.out_prefix + "bin/" + it);
        njesc_esc_path_out += {opt.ninja->escape_path(path1)};
        target_outfile     += {path1};
    }

    auto *build = opt.ninja->append_build();
    build->rule = "quick_run";
    build->variables["cmd"] = args;
    build->outputs = njesc_esc_path_out;
    build->implicit_inputs = {
        opt.ninja->escape_path(projpath + "/project.pbxproj")
    };
    build->implicit_outputs = {opt.out_prefix + opt.BUILD_ENTRY};
    build->implicit_inputs += x.ninja_target_dep;

    auto &rv = x.merged_info;
    auto *def = rv.get<cgn::DefaultInfo>(true);
    def->outputs = target_outfile;

    auto lrinfo = cgn::LinkAndRunInfo_classification(x.outputs, opt.src_prefix);
    rv.get<cgn::LinkAndRunInfo>(true)->merge_from(&lrinfo);

    return rv;
} //XCodeProjectInterpreter::interpret()

} //namespace