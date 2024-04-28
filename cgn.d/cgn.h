#pragma once
#include <string>
#include <memory>
#include <functional>
#include "entry/configuration.h"
#include "entry/ninja_file.h"
#include "provider.h"

namespace cgn {

class  CGNImpl;
struct DLHelper;
struct GraphNode;

// 2 examples of CGN Script
// -----------------------
// (build.cgn.cc) target-factory descriptor
//   label : //demo/build.cgn.cc
//   files : ["demo/build.cgn.cc", "cgn.d/library/lang_rust.bundle/lang_rust.h"]
// (cgn.d/library/lang_rust.bundle) interpreter
//   label : //cgn.d/library/lang_rust.bundle
//   files : ["cgn.d/library/lang_rust.bundle/lang_rust.cgn.cc"]
//           ["cgn.d/library/lang_rust.bundle/lang_rust.cgn.h"]
//           ["cgn.d/library/lang_rust.bundle/rule.ninja"]
struct CGNScript
{
    // the output dll
    std::string sofile;
    std::unique_ptr<DLHelper> sohandle;

    GraphNode *adep;

    // const std::vector<std::string> &files();
};

class NinjaFile;
struct CGNTargetOpt {
    static std::string path_separator;

    // target BUILD_ENTRY
    //  某个target编译时会产生多个outputs 通过 ninja BUILD_STAMP 来编译整个target
    constexpr static const char BUILD_NINJA[] = "build.ninja",
                                BUILD_ENTRY[] = ".stamp";

    // factory_ulabel : "//demo:hello_world"
    // factory_name   : "hello_world"
    std::string factory_ulabel;
    std::string factory_name;

    // a rel path which trailing with '/' or '\' (system-path-separator)
    // like "cgn-out/obj/project1_/hello_FFFF1234/"
    std::string out_prefix;

    // a rel path which trailing with '/' or '\' (system-path-separator)
    // like 'project1/'
    std::string src_prefix;
    
    // the target ninja
    // "out_prefix + BUILD_ENTRY" is entry
    NinjaFile *ninja;

    GraphNode *adep;
};

struct CGNTarget
{
    const CGNScript *cgn_script = nullptr;

    GraphNode *adep = nullptr;

    TargetInfos infos;

    std::string errmsg;
};

using CGNFactoryLoader = std::function<TargetInfos(Configuration cfg, CGNTargetOpt opt)>;

struct HostInfo {
    //os : win, linux, mac
    //cpu: x86, x64, arm64
    std::string os, cpu;

    // gnu_get_libc_version() : 2.8
    // gnu_get_libc_release() : stable
    std::string glibc_version, glibc_release;
};

struct Tools {
    static uint32_t host_to_u32be(uint32_t in);
    static uint32_t u32be_to_host(uint32_t in);

    static std::string shell_escape(
        const std::string &in
    );

    static HostInfo get_host_info();

    static std::string rebase_path(const std::string &p, const std::string &base);

    static std::string locale_path(const std::string &in);

    static std::string parent_path(const std::string &in);

    static std::unordered_map<std::string, std::string> read_kvfile(
        const std::string &filepath);

    static int64_t stat(const std::string &filepath);

    static std::unordered_map<std::string, int64_t> win32_stat_folder(
        const std::string &folder_path);

    static bool win32_long_paths_enabled();

    static bool is_win7_or_later();

    std::string absolute_label(const std::string &p, std::string base);

}; //struct Tools

class CGN : public Tools {
public:
    std::string get_filepath(const std::string &file_label) const;

    const CGNScript *active_script(const std::string &label);

    void offline_script(const std::string &label);

    CGNTarget analyse_target(
        const std::string &label, const Configuration &cfg
    );

    void build(const std::string &label, const Configuration &cfg);

    ConfigurationID commit_config(const Configuration &plat_cfg);

    const Configuration *query_config(const std::string &name) const;
    
    void add_adep_edge(GraphNode *early, GraphNode *late);

    template<typename Interpreter> std::shared_ptr<void> bind_target_factory(
        const std::string &label_prefix, const std::string &name, 
        void(*factory)(typename Interpreter::context_type&)
    ) {
        auto loader = [this, factory](const Configuration &cfg, CGNTargetOpt opt) {
            if (Interpreter::script_label) {
                const CGNScript *s = active_script(Interpreter::script_label);
                add_adep_edge(s->adep, opt.adep);
            }
            active_script(Interpreter::script_label);
            typename Interpreter::context_type x{cfg, opt};
            factory(x);
            return Interpreter::interpret(x, opt);
        };
        return bind_factory_part2(label_prefix + name, loader);
    }

    // bool clean_all();

    void init(const std::unordered_map<std::string, std::string> &kvargs);

    const std::unordered_map<std::string, std::string> &get_kvargs() const;

    ~CGN();

private:
    CGNImpl *pimpl;

    std::shared_ptr<void> bind_factory_part2(
        const std::string &label, CGNFactoryLoader loader
    );
};

} //nemspace

// cgn_init functions
// ------------------

struct CGNInitSetup {
    //platforms[platform_name] = kv-pair
    std::unordered_map<std::string, cgn::Configuration> configs;

    //cfg_restrictions[key] = value[]
    std::unordered_map<std::string, std::vector<std::string>> cfg_restrictions;
};

extern cgn::CGN api;