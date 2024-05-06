#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <list>
#include <filesystem>
#include <thread>
#include <atomic>
#include "../cgn.h"

cgn::CGN api;

int show_helper(const char *arg0) {
    std::cerr<<arg0<<"\n"
             <<"     analyse <target_label>\n"
             <<"     build   <target_label>\n"
             <<"     run     <target_label>\n"
             <<"     preload\n"
             <<"     clean\n"
             <<"  Options:\n"
             <<"     -C / --cgn-out + <dir_name>\n"
             <<"     -V / --verbose\n"
             <<"     --regeneration\n"
             <<std::endl;
    return 1;
}

// scan all files end with .cgn.cc, .cgn.rsp, or folder with .cgn.bundle
// and call api.active_script() in parallel
int cgn_preload_all()
{
    using std::filesystem::is_directory;
    using std::filesystem::is_regular_file;

    std::vector<std::string> cgn_scripts;
    std::vector<std::filesystem::path> stack;
    stack.push_back(".");
    for (auto p : std::filesystem::directory_iterator(stack.back())) {
        auto end_with = [&](std::string want) {
            std::string s = p.path().filename().string();
            if (s.size() > want.size())
                return s.substr(s.size() - want.size()) == want;
            return false;
        };
        if (is_regular_file(p) && (end_with(".cgn.cc") || end_with(".cgn.rsp")))
            cgn_scripts.push_back(p.path().string());
        if (is_directory(p) && end_with(".cgn.bundle"))
            cgn_scripts.push_back(p.path().string());
    }

    std::size_t ncpu = std::thread::hardware_concurrency();
    if (ncpu > cgn_scripts.size())
        ncpu = cgn_scripts.size();
    std::cout<<"Scan complete, "<< cgn_scripts.size() <<" scripts found, "
             <<"loading in "<<ncpu<<" threads parallelly.";
    size_t n_jobs_base = cgn_scripts.size() / ncpu;

    std::atomic<std::size_t> error_count{0};
    std::list<std::thread> thread_pools;
    for (std::size_t i=0; i<cgn_scripts.size(); ) {
        size_t njobs = n_jobs_base + (i < cgn_scripts.size()%ncpu ? 1:0);
        thread_pools.emplace_back([&](std::size_t off, std::size_t count) {
            try {
                for (std::size_t j=0; j<count; j++)
                    api.active_script(cgn_scripts[off+j]);
            } catch(std::runtime_error &e) {
                error_count.fetch_add(std::memory_order::memory_order_relaxed);
            }
        }, i, njobs);
        i += njobs;
    }
    std::cout<<"Waiting for thread finished..."<<std::endl;
    for (auto &th : thread_pools)
        th.join();
    std::cout<<"All thread done, "<<error_count.load()<<" errors occured. "<<std::endl;
    return error_count.load();
}

extern int dev_helper();
extern int wincp(std::string src, std::string dst);

int main(int argc, char **argv)
{
    //parse input cmdline
    std::unordered_set<std::string> single_options{
        "scriptcc_debug", "verbose"
    };
    std::vector<std::string> args;
    std::unordered_map<std::string, std::string> args_kv;
    for (int i=1; i<argc;) {
        std::string_view k{argv[i]};
        if (k[0] == '-') {
            if (k.size() >= 2 && k[1] == '-')
                k = k.substr(2);
            else
                k = k.substr(1);
            //try expand
            if (k == "C")
                k = "cgn-out";
            else if (k == "V")
                k = "verbose";

            if (single_options.count(k.data()))
                args_kv[k.data()]= "", i++;
            else if(i+1 < argc)
                args_kv[k.data()] = argv[i+1], i+=2;
            else
                return show_helper(argv[0]);
        }
        else
            args.push_back(argv[i++]);
    }

    //argument check
    if (auto fd = args_kv.find("cgn-out"); fd != args_kv.end()){
        if (fd->second.empty()) {
            std::cerr<<"Invalid cgn-out dir"<<std::endl;
            return show_helper(argv[0]);
        }
    }else
        args_kv["cgn-out"] = "cgn-out";

    if (args.empty())
        return show_helper(argv[0]);
    
    auto init0 = [&]() {
        api.init(args_kv);
        const auto *cfg = api.query_config("DEFAULT");
        if (cfg)
            return *cfg;
        else {
            std::cerr<<"'DEFAULT' config not found."<<std::endl;
            exit(2);
        }
    };

    if (args[0] == "analyze" || args[0] == "analyse") {
        if (args.size() != 2)
            return show_helper(argv[0]);
        api.init(args_kv);
        auto rv = api.analyse_target(args[1], *api.query_config("DEFAULT"));
        if (rv.infos.empty())
            throw std::runtime_error{"analyse: " + args[1] + " not found."};
    }
    if (args[0] == "build") {
        if (args.size() != 2)
            return show_helper(argv[0]);
        api.init(args_kv);
        api.build(args[1], *api.query_config("DEFAULT"));
    }
    if (args[0] == "run") {
        if (args.size() != 2)
            return show_helper(argv[0]);
    }
    if (args[0] == "preload")
        return cgn_preload_all();
    if (args[0] == "tool") {
        if (args.size() == 2 && args[1] == "dev")
            return dev_helper();
        if (args.size() == 4 && args[1] == "wincp")
            return wincp(args[2], args[3]);
        if (args.size() == 3 && args[1] == "fileglob") {
            auto ls = api.file_glob(args[2]);
            for (auto &ss : ls)
                std::cout<<ss<<"\n";
        }
        else
            return show_helper(argv[0]);
    }
    if (args[0] == "clean") {
        std::cout<<"Cleaning..."<<std::endl;
        std::filesystem::path dir{args_kv["cgn-out"]};
        if (std::filesystem::exists(dir / ".cgn_out_root.stamp"))
            std::filesystem::remove_all(dir);
        else
            std::cerr<<dir.string()<<"\n"
                     <<"Warning: it seems not a cgn-out folder, do nothing."<<std::endl;
    }

    return 0;
}