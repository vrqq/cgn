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
    cgn::HostInfo host = cgn::Tools::get_host_info();
    cgn::Configuration cfg;
    
    cfg["host_os"]  = host.os;
    cfg["host_cpu"] = host.cpu;
    
    if ((cfg["os"] = extract(argls, {"win", "mac", "linux"})) == "")
        cfg["os"] = host.os;
    
    if ((cfg["cpu"] = extract(argls, {"x86", "x86_64", "arm64", "ia64", "mips64"})) == "")
        cfg["cpu"] = host.cpu;
    
    if ((cfg["host_shell"] = extract(argls, {"cmd", "powershel", "bash"})) == "") {
        std::string parent_proc = cgn::Tools::get_parent_process_name();
        for (auto &ch : parent_proc)
            if ('A' <= ch && ch <= 'Z')
                ch = ch - 'A' + 'a';
        
        if (parent_proc == "cmd.exe")
            cfg["host_shell"] = "cmd";
        if (parent_proc == "powershell.exe")
            cfg["host_shell"] = "cmd";
        else if (parent_proc == "bash.exe" || parent_proc == "bash")
            cfg["host_shell"] = "bash";
        else if (parent_proc == "zsh")
            cfg["host_shell"] = "zsh";
        else if (host.os == "win")
            cfg["host_shell"] = "cmd";
        else
            cfg["host_shell"] = "bash";
    }

    if ((cfg["cxx_toolchain"] = extract(argls, {"gcc", "llvm", "msvc", "xcode"})) == "") {
        if (cfg["os"] == "win")
            cfg["cxx_toolchain"] = "msvc";
        else if (cfg["os"] == "mac")
            cfg["cxx_toolchain"] = "xcode";
        else
            cfg["cxx_toolchain"] = "gcc";
    }

    if ((cfg["optimization"] = extract(argls, {"debug", "release"})) == "")
        cfg["optimization"] = "release";
    
    cfg["cxx_asan"]  = extract(argls, {"asan"});
    cfg["cxx_tsan"]  = extract(argls, {"tsan"});
    cfg["cxx_msan"]  = extract(argls, {"msan"});
    cfg["cxx_lsan"]  = extract(argls, {"lsan"});
    cfg["cxx_ubsan"] = extract(argls, {"ubsan"});

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
    cgn::HostInfo hinfo = cgn::Tools::get_host_info();
    std::unordered_set<std::string> args{"release", hinfo.cpu, hinfo.os};
    if (hinfo.os == "win")
        args.insert({"msvc", "msvc_MD", "CONSOLE", "cmd"});
    else if (hinfo.os == "linux")
        args.insert({"gcc", "bash"});
    else if (hinfo.os == "mac")
        args.insert({"xcode", "zsh"});
    else
        args.insert({"llvm", "bash"});
    return config_guessor(args);
}
