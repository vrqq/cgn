#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <list>
#include <filesystem>
#include <thread>
#include <atomic>
#include <optional>

#include "cgn_api.h"

void init_win_exception_handler();

int show_helper(const char *arg0) {
    std::cerr<<arg0<<"\n"
             <<"     analyse <target_label>\n"
             <<"     build   <target_label>\n"
             <<"     run     <target_label>\n"
             <<"     query   <target_label> <config name>\n"
             <<"     preload\n"
             <<"     clean\n"
             <<"  Options:\n"
             <<"     -C / --cgn-out + <dir_name>\n"
             <<"     -V / --verbose\n"
             <<"     --winenv\n"
             <<"     --scriptcc_debug\n"
             <<"     --halt_on_error\n"
             <<"     --scriptcc + xxx.exe\n"
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

// extern int dev_helper();

int main(int argc, char **argv)
{
    // parse input cmdline
    // -------------------
    std::unordered_set<std::string> single_options{
        "scriptcc_debug", "halt_on_error", "verbose", "winenv"
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

    if (args.empty())
        return show_helper(argv[0]);

    // register windows SEH handler
    init_win_exception_handler();

try{do{
    // cgn::CGNTools (static functions)
    // --------------------------------
    // [CMD] cgn tool xxxxxx
    if (args[0] == "tool" || args[0] == "tools") {
        if (args.size() == 2 && args[1] == "parentprocess") {
            std::cout<<api.get_parent_process_name()<<"\n";
            return 0;
        }
        if (args.size() == 4 && args[1] == "abslabel") {
            std::cout<<api.absolute_label(args[2], args[3])<<"\n";
            return 0;
        }
        if (args.size() >= 4 && args[1] == "rebase") {
            if (args.size() == 4)
                std::cout<<api.rebase_path(args[2], args[3])<<"\n";
            if (args.size() > 4)
                std::cout<<api.rebase_path(args[2], args[3], args[4])<<"\n";
            return 0;
        }
        if (args.size() > 2 && args[1] == "locale") {
            std::cout<<api.locale_path(args[2])<<"\n";
            return 0;
        }
        if (args.size() > 2 && args[1] == "mangle") {
            std::cout<<api.mangle_path(args[2], "example_of/mangle_base/")<<"\n";
            return 0;
        }
        if (args.size() > 2 && args[1] == "caps") {
            std::cout<<(api.is_directory_case_sensitive(args[2])?
                    "Case-Sensitive\n": "Case-Insensitive\n");
            return 0;
        }
        // if (args.size() == 2 && args[1] == "dev")
        //     return dev_helper();
        if (args.size() == 4 && args[1] == "wincp")
            return cgnv1::Tools::win_copy(args[2], args[3]);
        if (args.size() == 3 && args[1] == "fileglob") {
            auto ls = api.file_glob(args[2]);
            for (auto &ss : ls)
                std::cout<<ss<<"\n";
        }
        else
            return show_helper(argv[0]);
    }

    // cgn::CGN API call
    // -----------------

    // requirement argument check
    if (auto fd = args_kv.find("cgn-out"); fd != args_kv.end()){
        if (fd->second.empty()) {
            std::cerr<<"Invalid cgn-out dir"<<std::endl;
            return show_helper(argv[0]);
        }
    }else
        args_kv["cgn-out"] = "cgn-out";


    auto api_release = std::shared_ptr<int>(new int, [](int* p){ 
        api.release(); delete p;
    });

    // api.init()
    api.init(args_kv);
    
    // using config 'DEFAULT' if no cfgname assigned
    auto load_cfg = [](const std::string &name) -> cgnv1::Configuration {
        auto [cfg, adep] = api.query_config(name.empty()?"DEFAULT":name);
        if (adep)
            return cfg;
        else
            throw std::runtime_error{"'" + name + "' config not found."};
    };

    // [CMD] cgn analyse @cell//target [cfgname]
    if ((args[0] == "analyze" || args[0] == "analyse") && args.size() >= 2) {
        auto rv = api.analyse_target(args[1], load_cfg(args.size()>=3?args[2]:""));
        if (rv.errmsg.size())
            api.logger->paragraph(rv.errmsg);
        return 0;
    }

    // [CMD] cgn build @cell//target [cfgname]
    if (args[0] == "build" && args.size() >= 2) {
        api.build(args[1], load_cfg(args.size()>=3?args[2]:""));
        return 0;
    }

    // [CMD] cgn run @cell//target [cfgname]
    if (args[0] == "run" && args.size() >= 2) {
        std::string cmd;
        auto exe = api.build(args[1], load_cfg(args.size()>=3?args[2]:""));
        if (exe.size()) {
            cmd = api.shell_escape(exe);
            for (std::size_t i=2; i<args.size(); i++)
                cmd += " " + api.shell_escape(args[i]);
        }

        if (cmd.size())
            system(cmd.c_str());
        else
            api.logger->paragraph("No executable found.");
        return 0;
    }

    // [CMD] cgn query @cell//target [cfgname]
    if (args[0] == "query" && args.size() >= 2) {
        auto cfg = load_cfg(args.size()>=3?args[2]:"");
        auto rv = api.analyse_target(args[1], cfg);

        char type = api.get_kvargs().count("verbose")?'H':'h';
        std::cout<<"\n--- Target ---\n"
                 <<args[1]<<" #"<<cfg.get_id()<<std::endl;
        std::cout<<"\n--- Input Configuration ---\n"
                 <<cgnv1::Logger::fmt_list(cfg, "", 999) <<std::endl;

        std::cout<<"\n--- Analyse Result ---\n"<<rv.to_string(type)<<std::endl;
        if (rv.errmsg.size())
            std::cout<<rv.errmsg<<std::endl;
        return 0;
    }

    // [CMD] cgn preload
    if (args[0] == "preload"){
        cgn_preload_all();
        return 0;
    }

    // [CMD] cgn clean
    if (args[0] == "clean") {
        std::cout<<"Cleaning..."<<std::endl;
        std::filesystem::path dir{args_kv["cgn-out"]};
        if (std::filesystem::exists(dir / ".cgn_out_root.stamp"))
            std::filesystem::remove_all(dir);
        else
            std::cerr<<dir.string()<<"\n"
                     <<"Warning: it seems not a cgn-out folder, do nothing."<<std::endl;
        return 0;
    }

}while(0);}catch(std::exception &e) { //windows CRT won't show anything for unhandled exception.
    std::cerr<<"\n---EXCEPTION---\n"
             <<e.what()
             <<"\n==============="<<std::endl;
    return 1;
}

    return show_helper(argv[0]);
} //int main()