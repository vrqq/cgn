#pragma once
#include <string>
#include <unordered_set>

#if defined(CGN_SETUP_IMPL) && defined(_WIN32)
    #define CGN_SETUP_IF __declspec(dllexport)
#else
    #define CGN_SETUP_IF
#endif

inline std::string extract(
    std::unordered_set<std::string> &in, std::initializer_list<std::string> opts
) {
    std::string rv;
    for (auto it : opts)
        if (in.erase(it) == 1) {
            if (rv.size())
                throw std::runtime_error{
                    "Multiple selection: " + rv + ", " + it};
            rv = it;
        }
    return rv;
}

inline std::string extract2(
    std::unordered_set<std::string> &in, 
    std::initializer_list<std::pair<std::string,std::string>> opts
) {
    std::string rv;
    for (auto it : opts)
        if (in.erase(it.first) == 1){
            if (rv.size())
                throw std::runtime_error{
                    "Multiple selection: " + rv + ", " + it.first};
            rv = it.second;
        }
    return rv;
}

inline std::unordered_set<std::string> str_to_set(const std::string &argstr)
{
    std::unordered_set<std::string> argls;
    for (std::size_t i=0, j=0; j<argstr.size(); i = ++j) {
        while (j<argstr.size() && argstr[j] != ' ' && argstr[j] != ',')
            j++;
        if (i < j)
            argls.insert(argstr.substr(i, j-i));
    }
    return argls;
}

inline cgn::Configuration config_guessor(std::unordered_set<std::string> &argls)
{
    cgn::HostInfo host = api.get_host_info();
    cgn::Configuration cfg;
    
    if ((cfg["os"] = extract(argls, {"win", "mac", "linux"})) == "")
        cfg["os"] = host.os;
    
    if ((cfg["cpu"] = extract(argls, {"x86", "x86_64", "arm64", "ia64", "mips64"})) == "")
        cfg["cpu"] = host.cpu;
    
    if ((cfg["cxx_toolchain"] = extract(argls, {"gcc", "llvm", "msvc"})) == "") {
        if (cfg["os"] == "win")
            cfg["cxx_toolchain"] = "msvc";
        else if (cfg["os"] == "mac")
            cfg["cxx_toolchain"] = "llvm";
        else
            cfg["cxx_toolchain"] = "gcc";
    }

    if ((cfg["optimization"] = extract(argls, {"debug", "release"})) == "")
        cfg["optimization"] = "release";
    
    if (cfg["os"] == "win") {
        if ((cfg["msvc_runtime"] = extract2(argls, 
            {{"msvc_MD", "MD"}, {"msvc_MDd", "MDd"}, {"msvc_MT", "MT"}, {"msvc_MTd", "MTd"}})
        ) == "")
            cfg["msvc_runtime"] = (cfg["optimization"] == "release"? "MD" : "MDd");
        
        if ((cfg["msvc_subsystem"] = extract(argls, {"WINDOW", "CONSOLE"})) == "")
            cfg["msvc_subsystem"] = "CONSOLE";
    }
    
    if (cfg["toolchain"] == "llvm") {
        if ((cfg["llvm_stl"] = extract(argls, {"libc++"})) == "")
            cfg["llvm_stl"] = "libstdc++";
    }

    return cfg;
} //config_guessor()

inline cgn::Configuration config_guessor_notest(
    std::unordered_set<std::string> argls
) {
    return config_guessor(argls);
}

inline cgn::Configuration generate_host_release()
{
    cgn::HostInfo hinfo = api.get_host_info();
    std::unordered_set<std::string> args{"release", hinfo.cpu, hinfo.os};
    if (hinfo.os == "win")
        args.insert({"msvc", "msvc_MD", "CONSOLE"});
    else if (hinfo.os == "linux")
        args.insert({"gcc"});
    else
        args.insert({"llvm"});
    return config_guessor(args);
}
