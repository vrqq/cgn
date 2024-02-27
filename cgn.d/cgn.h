//Public header for CGN factory and rule author
// CXX-Generate-Ninja 2024 vrqq.org
#pragma once
#include <unordered_map>
#include <string>
#include <functional>

//Import basic header
#include "providers/provider.h"
#include "entry/configuration.h"
#include "entry/ninja_file.h"
#include "tools/tools.h"

//
struct CGNInitSetup {
    //platforms[platform_name] = kv-pair
    std::unordered_map<std::string, Configuration> configs;

    //cfg_restrictions[key] = value[]
    std::unordered_map<std::string, std::vector<std::string>> cfg_restrictions;
};
extern void cgn_setup(CGNInitSetup &x);

// C++ 11 compatiblity without std::filesystem
struct TargetOpt {
    constexpr static const char BUILD_NINJA[] = "build.ninja",
                                BUILD_STAMP[] = ".build_stamp",
                                ANALYSIS_STAMP[] = ".analysis_stamp";

    // like "//project1:hello"
    std::string factory_ulabel;

    //enforce regenerate build.ninja evenif cfg unchanged.
    bool enforce_regenerate;

    // BUILD.cgn.cc unique file label.
    // like "//project1/BUILD.cgn.cc"
    std::string fin_ulabel;

    // a rel path which trailing with '/' (ninja-format)
    // like "cgn-out/obj/project1_/hello_12CDEF/"
    std::string fout_prefix;
};

class CGNImpl;
class CGN {
public:
    void load_cgn_script(const std::string &label);

    void unload_cgn_script(const std::string &label);

    TargetInfos analyse(
        const std::string &tf_label, 
        const Configuration &plat_cfg
    );

    static HostInfo get_host_info();

    static std::string shell_escape(const std::string &in);

    // @param label: the file with cgn file-label format.
    // @return array of filepath relavent to woriking-root
    std::vector<std::string> get_path(const std::string &label);

    //Get output folder of specific target
    std::string get_output_folder(
        const std::string &tf_label, 
        ConfigurationID cfg_hash_id
    );

    ConfigurationID commit_config(const Configuration &plat_cfg);

    const Configuration *query_config(const std::string &name);

    // Add target ninja file into main-entry.ninja 
    // at cgn-out/obj/main.ninja
    void register_ninjafile(const std::string &ninja_file_path);

    template<typename Interpreter> std::shared_ptr<void> bind_target_factory(
        const std::string &label_prefix, const std::string &name,
        void(*factory)(typename Interpreter::context_type&)
    );

    // void set_default_config(const std::string &name);

    //command line args
    const std::unordered_map<std::string, std::string> &cmd_kvargs;

    CGN(CGNImpl *impl);

private:
    CGNImpl *pimpl;

    std::shared_ptr<void> bind_factory_part2(
        const std::string &label,
        std::function<TargetInfos(const Configuration&, TargetOpt)> fn_apply
    );
};

template<typename Interpreter> std::shared_ptr<void> CGN::bind_target_factory(
    const std::string &label_prefix, const std::string &name, 
    void(*factory)(typename Interpreter::context_type&)
) {
    auto fn = [this, name, factory](const Configuration &cfg, TargetOpt opt) {
        if (Interpreter::script_label)
            load_cgn_script(Interpreter::script_label);
        typename Interpreter::context_type x{name, cfg};
        factory(x);
        return Interpreter::interpret(x, opt);
    };
    return bind_factory_part2(label_prefix + name, std::move(fn));
}

extern CGN glb;
