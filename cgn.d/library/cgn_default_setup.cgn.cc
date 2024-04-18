#include "../cgn.h"

template<typename T> struct KVReader {
    struct Data{const char *a, *b; };
    static const char *key(T i)   { return Data(i).a; }
    static const char *value(T i) { return Data(i).b; }
};
template<> struct KVReader<const char*> {
    static const char *key(const char *i)   { return i; }
    static const char *value(const char *i) { return i; }
};

std::string get_one_of(const std::unordered_set<std::string> &pool) { return ""; }
std::string select_one_of(const std::unordered_set<std::string> &pool) { return ""; }
template<typename ...U> std::string select_one_of(
    const std::unordered_set<std::string> &pool,
    const char *k, const char *v, U ...others
) {
    if (pool.count(k) == 1)
        return v;
    return select_one_of(pool, others...);
}
template<typename ...U> std::string get_one_of(
    const std::unordered_set<std::string> &pool,
    const char* one, U ...others
) {
    if (pool.count(one) == 1)
        return one;
    return get_one_of(pool, others...);
}

cgn::Configuration gene_config(const std::string argstr)
{
    std::unordered_set<std::string> argls;
    for (std::size_t i=0, j=0; j<argstr.size(); i = j+1) {
        while (j<argstr.size() && (argstr[j]!=' ' || argstr[j]!=','))
            j++;
        if (i < j)
            argls.insert(argstr.substr(i, j-i));
    }

    cgn::HostInfo host = api.get_host_info();

    cgn::Configuration cfg;

    if ((cfg["os"] = get_one_of(argls, "win", "mac", "linux")) == "")
        cfg["os"] = host.os;

    if ((cfg["cpu"] = get_one_of(argls, "x86", "x86_64", "arm64")) == "")
        cfg["cpu"] = host.cpu;

    if ((cfg["optimization"] = get_one_of(argls, "debug", "release")) == "")
        cfg["optimization"] = "release";

    cfg["generator"] = select_one_of(argls, "gcc", "sys_gcc", 
                        "llvm", "sys_llvm", "msvc", "sys_msvc");
    
    return cfg;
}

void cgn_setup(CGNInitSetup &x) {
    x.cfg_restrictions["os"]  = {"win", "mac", "linux"};
    x.cfg_restrictions["cpu"] = {"x86", "x86_64", "mips64", "arm64", "ia64"};
    x.cfg_restrictions["generator"] = {"sys_gcc", "sys_llvm", "sys_msvc", "user_hxsec"};
    x.cfg_restrictions["toolchain"] = {"gcc", "llvm", "msvc"};
    x.cfg_restrictions["optimization"]   = {"debug", "release"};
    x.cfg_restrictions["msvc_runtime"]   = {"MD", "MDd"};
    x.cfg_restrictions["msvc_subsystem"] = {"CONSOLE", "WINDOW"};
    x.cfg_restrictions["llvm_stl"]       = {"libc++", "libstdc++"};

    x.cfg_restrictions["asan"] = x.cfg_restrictions["lsan"] = {"F"};
    x.cfg_restrictions["msan"] = x.cfg_restrictions["tsan"] = {"F"};
    x.cfg_restrictions["ubsan"] = {"F"};
    // x.cfg_restrictions["address_sanitizer"] = {"F"};
    // x.cfg_restrictions["leak_sanitizer"]    = {"F"};
    // x.cfg_restrictions["memory_sanitizer"]  = {"F"};
    // x.cfg_restrictions["thread_sanitizer"]  = {"F"};
    // x.cfg_restrictions["undefined_behavior_sanitizer"] = {"F"};
    // x.cfg_restrictions["sanitizer"] = {"asan", "lsan", "msan", "tsan", "ubsan"};

    std::string host_arg = "release,";
    cgn::HostInfo hinfo = api.get_host_info();
    host_arg += hinfo.cpu + "," + hinfo.os;
    if (hinfo.os == "win")
        host_arg += "sys_msvc,msvc_MD,CONSOLE,";
    else if (hinfo.os == "linux")
        host_arg += "sys_gcc,";
    else
        host_arg += "sys_llvm,";
    x.configs["host_release"] = gene_config(host_arg);

    std::string default_argss = host_arg;
    auto fdarg = api.get_kvargs().find("target");
    if (fdarg != api.get_kvargs().end())
        default_argss = fdarg->second;
    x.configs["DEFAULT"] = gene_config(default_argss);
}