#include "xcode.cgn.h"
#include "../../v1/std_operator.hpp"

namespace xcode {

static std::string two_escape(const std::string &in, const std::string &shell_type)
{
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in, shell_type));
}

void XCodeProjectInterpreter::interpret(context_type &x)
{
    if (x.cfg["os"] != "mac")
        return x.opt->set_fail("Unsupported cfg[os], only mac accepted");
    if (x.project.empty())
        return x.opt->set_fail("field project required");
    if (x.outputs.empty())
        return x.opt->set_fail("field outputs required");
    
    x.cfg.visit_keys({"host_shell", "os", "cpu", "optimization"});
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return;
    mk->merge_from(x.quickdep_result);

    std::string projpath = api.rebase_path(x.project, ".", mk);
    std::string product_dir = api.rebase_path(cgn::make_path_base_out("bin"), "", mk);
    std::string derived_dir = api.rebase_path(cgn::make_path_base_out("derived"), "", mk);

    std::string args = "xcodebuild -project " 
                     + two_escape(projpath, x.cfg["host_shell"]) + " ";
    if (x.scheme.size())
        args += "-scheme " + two_escape(x.scheme, x.cfg["host_shell"]) + " ";
    else if (x.targets.empty())
        args += "-alltargets ";
    else
        for (const auto &target : x.targets)
            args += "-target " + two_escape(target, x.cfg["host_shell"]) + " ";
    
    if (x.cfg["optimization"] == "debug")
        args += "-configuration Debug ";
    else if (x.cfg["optimization"] == "release")
        args += "-configuration Release ";
    else if (x.cfg["optimization"] != "")
        args += "-configuration " + (std::string)x.cfg["optimization"] + " ";
    
    if (x.sdk.size())
        args += "-sdk " + two_escape(x.sdk, x.cfg["host_shell"]) + " ";
    
    if (x.cfg["os"] == "mac" && x.cfg["cpu"] == "x86_64")
        args += "-arch x86_64 ";
    if (x.cfg["os"] == "mac" && x.cfg["cpu"] == "arm64")
        args += "-arch arm64 ";  //apple M1
    
    if (x.xcconfig.size())
        args += "-xcconfig " + two_escape(api.rebase_path(x.xcconfig, ".", mk), x.cfg["host_shell"]) + " ";
    
    if (x.scheme.size())
        args += "-derivedDataPath " + two_escape(derived_dir, x.cfg["host_shell"]) + " ";
    
    x.buildsettings["CONFIGURATION_BUILD_DIR"] = product_dir;
    for (auto iter : x.buildsettings)
        args += two_escape(iter.first, x.cfg["host_shell"]) + "="
             + two_escape(iter.second, x.cfg["host_shell"]) + " ";
    
    std::vector<std::string> njesc_esc_path_out;
    std::vector<std::string> target_outfile;
    for (auto &it : x.outputs) {
        std::string path1 = api.locale_path(product_dir + "/" + it);
        target_outfile     += {path1};
    }
    mk->outputs = target_outfile;

    cgn::LinkAndRunInfo *lrinfo = mk->get<cgn::LinkAndRunInfo>(true);
    for (std::size_t i = 0; i < x.outputs.size(); i++) {
        std::string ext = cgn::Tools::lowercase_extension_of_path(x.outputs[i]);
        if (ext == ".a")
            lrinfo->static_files.push_back(target_outfile[i]);
        else if (ext == ".dylib")
            lrinfo->shared_files.push_back(target_outfile[i]);
        else
            lrinfo->runtime_files[cgn::make_path_base_out(x.outputs[i])] = target_outfile[i];
    }

    if (!mk->ninja)
        return;

    // generate ninja file
    constexpr const char *rule = "@cgn.d//library/utility/quick_run.ninja";
    static std::string rule_path = api.get_filepath(rule);
    mk->ninja->append_include(rule_path);

    for (auto &it : target_outfile)
        njesc_esc_path_out += {mk->ninja->escape_path(it)};

    auto *build = mk->ninja->append_build();
    build->rule = "quick_run";
    build->variables["cmd"] = args;
    build->variables["desc"] = "XCODEBUILD " + mk->label;
    build->outputs = njesc_esc_path_out;
    build->implicit_inputs = {
        mk->ninja->escape_path(projpath + "/project.pbxproj")
    };
    for (auto &path : x.extra_watch_files)
        build->implicit_inputs += {
            mk->ninja->escape_path(api.rebase_path(path, ".", mk))
        };
    build->order_only = x.quickdep_ninja_target;

    auto *phony = mk->ninja->append_build();
    phony->rule = "phony";
    phony->inputs = build->outputs;
    phony->outputs = {mk->ninja->escape_path(mk->ninja_entry)};

} //XCodeProjectInterpreter::interpret()

} //namespace
