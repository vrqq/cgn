#pragma once
#include <cgn>
#include <list>
#include "@cgn.d/library/external/cmake_string_replace.h"
#include "abseil-src.h"

static int _target_init = []() { 
    static std::unordered_map<std::string, std::string> cmake_exp = {
        {"${ABSL_TEST_COPTS}", ""},
        {"$<$<BOOL:${EXECINFO_LIBRARY}>:${EXECINFO_LIBRARY}>", ""},
    };

    auto expand = [](cgn::Configuration &cfg, const std::string &in) {
        std::unordered_map<std::string, std::string> exp;
        exp["${ABSL_DEFAULT_COPTS}"] = "";
        exp["${ABSL_TEST_COPTS}"] = "";

        exp["$<$<BOOL:${LIBRT}>:-lrt>"] = "";
        exp["$<$<BOOL:${MINGW}>:-ladvapi32>"] = "";
        exp["$<$<BOOL:${EXECINFO_LIBRARY}>:${EXECINFO_LIBRARY}>"] = "";
        exp["$<$<BOOL:${MINGW}>:-ldbghelp>"] = "";
        exp["$<$<BOOL:${MSVC}>:-Z7>"] = "";
        exp["$<$<BOOL:${MSVC}>:-DEBUG>"] = "";
        exp["$<$<BOOL:${ANDROID}>:-llog>"] = "";
        exp["$<$<BOOL:${MINGW}>:-lbcrypt>"] = "";
        exp["$<$<PLATFORM_ID:AIX>:_LINUX_SOURCE_COMPAT>"] = "";
        exp["$<$<PLATFORM_ID:Windows>:internal/cctz/src/time_zone_name_win.cc>"] 
            = (cfg["os"] == "win")?"internal/cctz/src/time_zone_name_win.cc":"";
        exp["$<$<PLATFORM_ID:Windows>:internal/cctz/src/time_zone_name_win.h>"] 
            = (cfg["os"] == "win")?"internal/cctz/src/time_zone_name_win.h":"";
        exp["$<$<PLATFORM_ID:Darwin,iOS,tvOS,visionOS,watchOS>:-Wl,-framework,CoreFoundation>"] 
            = (cfg["os"] == "mac")? "-Wl,-framework,CoreFoundation>":"";
        return cmake_string_replace(exp, in);
    };
    static std::list<std::shared_ptr<void>> factories;

    // cxx_sources(in.name)
    std::vector<std::string> target_all;
    for (const auto &in : get_all_targets()) {
        if (in.testonly == true || 
           (in.name.size() > 5 && in.name.substr(in.name.size()-5) == "_test"))
            continue;
        if (in.public_)
            target_all.push_back(":" + in.name);
        auto factory = [in, expand](cxx::CxxSourcesContext &x) {
            for (auto it : in.srcs + in.hdrs) {
                std::string str = expand(x.cfg, it);
                std::string ext = api.lowercase_extension_of_path(str);
                if (str.size() && (ext == ".cc" || ext == ".c"))
                    x.srcs += {cgn::make_path_base_script("repo/" + in.basedir + "/" + str)};
            }
            if (x.srcs.size())
                x.include_dirs = {"repo"};
            
            for (auto it : in.deps)
                if (it.size() > 6 && it.substr(0, 6) == "absl::")
                    x.add_dep(":" + it.substr(6), cxx::private_dep);
            if (in.public_)
                x.pub.include_dirs = {"repo"};
        };
        factories.push_back(
            api.bind_target_factory<cxx::CxxSourcesInterpreter>(in.name, factory)
        );
    }

    //group(":all")
    factories.push_back(api.bind_target_factory<GroupInterpreter>("all", 
        [target_all](GroupInterpreter::context_type &x) {
            x.add_deps(target_all);
        }
    ));
    return 0;
}();