#include <fstream>
#include <sstream>
#include <thread>
#include <optional>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <cassert>
#include <array>
#include "raymii_command.hpp"
#include "cgn_impl.h"
#include "cgn_api.h"
#include "ninja_file.h"
#include "../pe_loader/pe_file.h"

// header from ninja-build src
#include "../ninjabuild/src/clparser.h"
#include "../ninjabuild/src/depfile_parser.h"

#ifdef _WIN32
// extern void __declspec(selectany) cgn_setup(CGNInitSetup &x);
// CGN_EXPORT void cgn_setup(CGNInitSetup &x);
#elif defined(__linux__)
extern void cgn_setup(cgnv1::CGNInitSetup &x) __attribute__((weak, visibility("default")));
#else
extern void cgn_setup(cgnv1::CGNInitSetup &x) __attribute__((visibility("default")));
#endif

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
std::string self_realpath()
{
    TCHAR name[4096];
    ZeroMemory(name, sizeof(name));
    if (auto len = GetModuleFileNameA(NULL, name, sizeof(name)); len > 0)
        return name;
    return "";
}
#elif __APPLE__
#include <mach-o/dyld.h>
std::string self_realpath()
{
    char path[4096];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        throw std::runtime_error("Buffer too small for executable path");
    }
    return std::filesystem::canonical(path);
}
#else
#include <unistd.h>
std::string self_realpath()
{
    char path[4096];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count == -1) throw std::runtime_error("Cannot get executable path");
    path[count] = '\0';
    return std::filesystem::path(path);
}
#endif

// for only debug purpersal in dev
// void cgn_setup(CGNInitSetup &x) {}

namespace cgnv1 {

// dirty patch: popen run /bin/sh with POSIX
// https://man.uex.se/3/popen
static std::string HOST_SHELL = Tools::get_host_info().os == "win"? "cmd" : "bash";

static void loop_dir(std::vector<std::string> *out, std::filesystem::path p)
{
    if (std::filesystem::is_directory(p))
        for (auto it : std::filesystem::directory_iterator(p))
            loop_dir(out, it);
    else
        out->push_back(p.string());
}

static std::vector<std::string> expand_scripts(std::filesystem::path p)
{
    std::vector<std::string> rv;
    if (std::filesystem::is_directory(p))
        loop_dir(&rv, p);
    else if (std::filesystem::exists(p)){
        if (p.extension() == ".rsp") {
            std::ifstream fin(p);
            for (std::string ss; !fin.eof() && std::getline(fin, ss);) {
                // auto fpath = p.parent_path() / std::filesystem::path(ss).make_preferred();
                auto fpath = p.parent_path() / ss;
                rv.push_back(fpath.string());
            }
        }
        else
            rv.push_back(p.make_preferred().string());
    }
    else
        throw std::runtime_error{p.string() + " not found."};
    
    return rv;
}

static std::string mangle_var_prefix(const std::string &in) {
    constexpr static std::array<bool, 256> chk = [](){
        std::array<bool, 256> rv{};
        for (bool &bv : rv) bv=0;
        for (unsigned char c='0'; c<='9'; c++) rv[c]=1;
        for (unsigned char c='a'; c<='z'; c++) rv[c]=1;
        for (unsigned char c='A'; c<='Z'; c++) rv[c]=1;
        return rv;
    }();

    constexpr static std::array<std::array<char, 3>, 256> rep = [](){
        std::array<char, 16> hex{
            '0', '1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
        std::array<std::array<char, 3>, 256> rv{std::array<char, 3>{0}};
        for (std::size_t i=0; i<256; i++)
            rv[i] = {'_', hex[i/16], hex[i%16]};
        return rv;
    }();

    std::string out;
    for (auto ch : in) {
        if (chk[ch])
            out.push_back(ch);
        else
            out.append(rep[ch].data(), 3);
    }
    return out;
}

std::string CGNImpl::expand_filelabel_to_filepath(const std::string &in) const
{
    auto [result, errmsg] = _expand_cell(in);
    if (errmsg.size())
        throw std::runtime_error{errmsg};
    return result;
}

// NodeName == unique_label (like @cgn.d//library/cxx/cxx.cgn.cc)
// start: (lock)
// case1: script loaded && stat(files[]) == Latest
//        return ;
// case2: script loaded && stat(files[]) == Stale
//        unload script => goto case 3
// case3: script not-load && stat(files[]) == Stale
//        (unlock)-rebuild-(lock) => goto case 4
// case4: script not-load && stat(files[]) == Latest
//        load and return;
// finally: (unlock)
// TODO: (bug to be fixed) return anode even if file not found.
std::pair<GraphNode*, std::string> 
CGNImpl::active_script(const std::string &label, bool parallel_build_mode)
{
    logger.println("ActiveScript ", label);

    auto [labe2, _expand_err] = _expand_cell(label);
    if (_expand_err.size()) {
        if (halt_on_error)
            throw std::runtime_error{"ActiveScript:" + _expand_err};
        return {nullptr, _expand_err};
    }
    if (std::string_view{labe2.data(), 3} == "../")
        throw std::runtime_error{"Invalid label " + label};
    
    CGNScript s; //the next value of scripts[label]

    std::unique_lock parallel_lock(parallel_active_script_mutex, std::defer_lock);
    if (parallel_build_mode)
        parallel_lock.lock();

    // compiling in other thread (case3), wait until loaded
    // for(auto fd = scripts.find(label); fd != scripts.end() && !fd->second.sohandle; ) {
    //     lk.unlock();
    //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //     lk.lock();
    //     fd = scripts.find(label);
    // }

    // enter case check
    if (auto fd = scripts.find(label); fd != scripts.end()) {
        graph.test_status(fd->second.anode);
        if (fd->second.anode->status == GraphNode::Latest) {
            // Record API calling-dependency
            if (parallel_build_mode == false && tls_runtime)
                tls_runtime->dep_anodes.insert(fd->second.anode);
            return {fd->second.anode, ""}; // case1: scripts existed, graph Latest
        }
        
        // case2: script existed, graph Stale. 
        //        erase script and goto case 3
        s.anode  = fd->second.anode;
        s.sofile = fd->second.sofile;
        // offline_script(&(fd->second));
        scripts.erase(fd);
    }
    else
        graph.test_status(s.anode = graph.get_node("S" + label));

    // Record API calling-dependency
    if (parallel_build_mode == false && tls_runtime)
        tls_runtime->dep_anodes.insert(s.anode);

    if (s.sofile.empty()) {
        auto fpath = std::filesystem::path{labe2}.make_preferred();
        #ifdef _WIN32
            s.sofile = (analysis_path / fpath.parent_path().make_preferred()
                        / (fpath.stem().string() + ".dll")).string();
        #else
            s.sofile = analysis_path / fpath.parent_path().make_preferred()
                        / ("lib" + fpath.stem().string() + ".so");
        #endif
    }
    
    // case3: script not loaded, graph Stale
    //        prepare CGNScript and GraphNode fields if necessary,
    //        then build cgn script and goto case 4 if build successful.
    if (s.anode->files.empty() || s.anode->status == GraphNode::Stale) {
        auto fpath = std::filesystem::path{labe2}.make_preferred();
        if (std::filesystem::exists(fpath) == false) {
            if (halt_on_error)
                throw std::runtime_error{fpath.string() + " not found."};
            return {nullptr, fpath.string() + " not found."};
        }

        if (parallel_build_mode)
            parallel_lock.unlock();

        //(re)generate GraphNode.files[]
        // script_srcs: file in fetched bundle or .rsp
        // script_all : script_srcs + header info hint by compiler
        std::vector<std::string> script_srcs = expand_scripts(fpath);
        std::unordered_set<std::string> script_all{script_srcs.begin(), script_srcs.end()};

        auto cc_end_with = [&](const std::string &want) {
            if (script_cc.size() >= want.size())
                return script_cc.substr(script_cc.size() - want.size()) == want;
            return false;
        };
        bool is_win   = (Tools::get_host_info().os == "win");
        bool is_unix  = !is_win;
        bool is_msvc  = cc_end_with("cl.exe");
        bool is_clang = cc_end_with("clang") || cc_end_with("clang++") 
                     || cc_end_with("clang.exe") || cc_end_with("clang++.exe");

        // .rsp file is temporary and not included in adep->files[]
        // .so / .dll is in adep->files[] when first created.
        // Since GCC header detection can only analyse only for one sources
        // at same time (output into .d files), so we have to compile each
        // sources code separately and link them together.
        std::filesystem::create_directories(analysis_path / fpath.parent_path());
        CLParser clpar;
        std::unordered_set<std::string> dfcoll;
        std::string linker_in;
        cgnv1::MSVCTrampo win_trampo;
        for (auto it : script_srcs) {
            std::filesystem::path pt = it;
            auto ext = pt.extension();
            if (ext != ".cc" && ext != ".cpp" && ext != ".c++" && ext != ".cxx")
                continue;
            std::string rspname = s.sofile + "-" + pt.stem().string() + ".rsp";
            std::string outname = s.sofile + "-" + pt.stem().string() + ".o" + (is_win?"bj":"");
            std::string depname = s.sofile + "-" + pt.stem().string() + ".d";
            linker_in += Tools::shell_escape(outname, HOST_SHELL) + " ";
            std::ofstream frsp(rspname); 

            if (is_msvc && is_win) {
                //TODO: Since msvc cl.exe /D cannot process '#' in command line
                //      but filename can accept it, we consider 2 solution here
                //      1. use /FI to insert char that can't be defined in cmd
                //      2. use TLS to storage CGN_ULABEL_PREFIX when load_library
                //
                frsp<< "/c " << ("." / pt).string() <<" /nologo /showIncludes /Gy "
                    "/DWINVER=0x0603 /D_WIN32_WINNT=0x0603 /D_AMD64_ "
                    // " /DCGN_VAR_PREFIX=" + def_var_prefix +
                    // " /D\"CGN_ULABEL_PREFIX=\"" + def_ulabel_prefix + "\"\"" + 
                    " /I. /utf-8 /EHa /MP /fp:fast /Fo: " + Tools::shell_escape(outname, HOST_SHELL);
                #ifdef _DEBUG
                    frsp<<" /D_DEBUG /MDd";
                #else
                    frsp<<" /MD";
                #endif
                if (scriptcc_debug_mode)
                    frsp<<" /Od /Z7 /Fd: " + Tools::shell_escape(outname, HOST_SHELL) + ".pdb";
            }
            else if (is_unix) {
                if (is_clang && scriptcc_debug_mode) //llvm debug (lldb)
                    frsp<<"-g -glldb -fstandalone-debug -fno-limit-debug-info "
                          "-fsanitize=address -fsanitize=undefined ";
                if (!is_clang && scriptcc_debug_mode) //gcc debug
                    frsp<<"-g ";
                frsp<<"-c " << it << " -MMD -MF " + Tools::shell_escape(depname, HOST_SHELL) +
                        " -fPIC -fdiagnostics-color=always -std=c++11 -I. " + 
                        // " -DCGN_VAR_PREFIX=" + Tools::shell_escape(def_var_prefix) +
                        // " -DCGN_ULABEL_PREFIX=" + Tools::shell_escape(def_ulabel_prefix) + 
                        " -o " + Tools::shell_escape(outname, HOST_SHELL);
            }
            frsp.close();

            logger.println("ScriptCC ", outname);
            // logger.printer.SetConsoleLocked(true);
            auto build_rv = raymii::Command::exec(
                "\"" + script_cc + "\" @" + rspname
            );
            // logger.printer.SetConsoleLocked(false);
            if (build_rv.exitstatus != 0){
                if (halt_on_error)
                    throw std::runtime_error{outname + " " + build_rv.output};
                return {nullptr, outname + " " + build_rv.output};
            }
            
            // (windows only) parse .obj and find UNDEFINED symbol
            if (is_win)
                win_trampo.add_objfile(outname);

            //header dep
            std::string errmsg;
            std::string dummy_arg;
            if (is_msvc) {
                if (!clpar.Parse(build_rv.output, "", &dummy_arg, &errmsg))
                    throw std::runtime_error{errmsg};
            }else {
                std::ifstream fin(depname);
                std::stringstream sss; sss<<fin.rdbuf();
                std::string full_content = sss.str();

                DepfileParser dfpar;
                if (!dfpar.Parse(&full_content, &errmsg))
                    throw std::runtime_error{errmsg};
                for (auto ss : dfpar.ins_) {
                    std::filesystem::path p(ss.begin(), ss.end());
                    if (auto e = p.extension(); 
                        e==".h" || e==".hxx" || e==".hpp" || e==".hh")
                            dfcoll.insert(p.lexically_normal().string());
                }
            }
        } //end for (file in script_srcs[])
        
        //add header dep before set_node_files
        auto run_link = [&](std::string arg) {
            std::ofstream frsp(s.sofile + ".rsp");
            frsp << linker_in << arg;
            frsp.close();
            logger.println("ScriptCC ", s.sofile);
            auto link_rv = raymii::Command::exec("\"" + script_cc + "\" @" + s.sofile + ".rsp");
            if (link_rv.exitstatus != 0)
                throw std::runtime_error{link_rv.output};
        };

        std::vector<std::string> node_vals;
        node_vals.push_back(s.sofile);
        if (is_msvc && is_win) {
            // add cgn.lib
            win_trampo.add_lib(cgnapi_winimp);

            // generate .asm trampoline
            std::string asm_in  = s.sofile + "_asmplugin.asm";
            std::string asm_obj = s.sofile + "_asmplugin.obj";
            std::string asm_def = s.sofile + "_asmplugin.def";
            std::string asm_lib = s.sofile + "_asmplugin.lib";
            std::string asm_dll = s.sofile + "_asmplugin.dll";
            win_trampo.make_asmfile(asm_in, asm_def);

            std::string cmd_suffix;
            if (asm_def.size()) {
                std::ofstream asm_rsp(asm_dll + ".rsp");
                asm_rsp<<" /nologo /Cx /Fo " + asm_obj + " " + asm_in 
                       + " /link /nologo /DLL /ENTRY:DllMain /DEF:" + asm_def 
                       + " /OUT:" + asm_dll
                       + " " + cgnapi_winimp;
                asm_rsp.close();
                cmd_suffix = "@" + asm_dll + ".rsp";
            }
            else
                cmd_suffix = "/nologo /c /Cx /Fo " + asm_obj + " " + asm_in;

            // compile .asm to .obj
            logger.println("ScriptCC(ml64) ", asm_obj);
            auto build_rv = raymii::Command::exec(
                "\"ml64.exe\" " + cmd_suffix
            );
            // 'ml64.exe' is not recognized as an internal or external command,
            // operable program or batch file.
            if (build_rv.exitstatus == 9009)
                build_rv = raymii::Command::exec(
                    "\"ml.exe\" " + cmd_suffix
                );
            if (build_rv.exitstatus != 0) {
                if (halt_on_error)
                    throw std::runtime_error{asm_obj + " " + build_rv.output};
                return {nullptr, asm_obj + " " + build_rv.output};
            }
            
            // if rename function_symbol required, pack .obj to .lib
            if (asm_def.size()) {
                // auto libpack_rv = raymii::Command::exec(
                //     "link.exe /nologo /DLL /OUT:" + asm_dll + " /DEF:" + asm_def + " " + asm_obj
                //     // "\"lib.exe\" /nologo /OUT:" + asm_lib + " /DEF:" + asm_def + " " + asm_obj
                // );
                // if (libpack_rv.exitstatus != 0) {
                //     if (halt_on_error)
                //         throw std::runtime_error{asm_lib + " " + libpack_rv.output};
                //     return {nullptr, asm_lib + " " + build_rv.output};
                // }
                linker_in += asm_lib + " ";
            }
            else
                linker_in += asm_obj + " ";

            // linking
            std::string dbg_flag = scriptcc_debug_mode?"/DEBUG ":"";
            run_link("/nologo /utf-8 /link /DLL " + dbg_flag + cgnapi_winimp + " /OUT:\"" + s.sofile + "\" /PDB:\"" + s.sofile + ".pdb\"");
            clpar.includes_.insert(script_srcs.begin(), script_srcs.end());
            node_vals.insert(node_vals.end(), clpar.includes_.begin(), 
                             clpar.includes_.end());
            
            // (windows only) convert 'c:\' to 'c:/'
            for (auto &it : node_vals)
                std::replace(it.begin(), it.end(), '\\', '/');
        }
        else if (is_unix) {
            std::string dbg_flag = scriptcc_debug_mode?" -g":"";
            // in macos no -fuse-ld=lld supported in XCode
            if (Tools::get_host_info().os == "mac")
                run_link(dbg_flag + " -fPIC --shared -fvisibility=hidden -Wl,-undefined,dynamic_lookup -o " + s.sofile);
            else if (is_clang) //llvm-linker is faster then gnu linker
                run_link(dbg_flag + " -fuse-ld=lld -fPIC -fvisibility=hidden --shared -o " + s.sofile);
            else
                run_link(dbg_flag + " -fPIC -fvisibility=hidden --shared -o " + s.sofile);
            dfcoll.insert(script_srcs.begin(), script_srcs.end());
            node_vals.insert(node_vals.end(), dfcoll.begin(), dfcoll.end());
        }

        //build successful, update graph and goto case 4
        {
            std::string content;
            for (auto it: node_vals){
                if (!content.empty())
                    content += ", ";
                content += it;
            }
            logger.verbose_paragraph("CGNScript "
                + label +" rebuilt with files[]: " + content + "\n");
        }

        if (parallel_build_mode)
            parallel_lock.lock();

        // graph.forward_status(s.anode);
        graph.set_node_files(s.anode, node_vals);
        graph.clear_file0_mtime_cache(s.anode);
        graph.set_node_status_to_latest(s.anode);
    }

    // for compile only mode, no anode return evenif anode is latest.
    if (parallel_build_mode)
        return {nullptr, ""};

    // case4: script not loaded, graph Latest
    //        load into scripts[]
    // Create TLRuntime and load script
    TLRuntime now_rt;
    if (label.rfind("/BUILD.cgn.cc") == label.size() - strlen("/BUILD.cgn.cc"))
        now_rt.label = label.substr(0, label.size() - strlen("/BUILD.cgn.cc"));
    else
        now_rt.label = label;
    
    tls_push(&now_rt);
    s.sohandle = std::make_unique<DLHelper>(s.sofile);
    tls_pop(&now_rt);

    if (!s.sohandle->valid())
        throw std::runtime_error{"cannot load cgn script " + s.sofile};
    graph.remove_inbound_edges(s.anode);
    for (GraphNode *p : now_rt.dep_anodes)
        graph.add_edge(p, s.anode);
    return {(scripts[label] = std::move(s)).anode, ""};
} //CGNImpl::active_script()

std::string CGNImpl::offline_script(const std::string &label)
{
    // std::unique_lock lk(scripts_mtx);
    // see also DEVNOTE in doc.       
    for (auto fd = scripts.find(label); fd != scripts.end(); ){
        // current script is building in another thread, wait for done
        // if (fd->second.sohandle == nullptr){
        //     lk.unlock();
        //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
        //     lk.lock();
        //     fd = scripts.find(label);
        //     continue;
        // }
        
        // current script could be removed.
        graph.set_node_status_to_unknown(fd->second.anode);
        scripts.erase(fd);
        return "";
    }
    return "script " + label + " not found.";
}

std::string CGNImpl::add_factory(
    const std::string &factory_label,   
    std::function<void(CGNTargetOpt*)> loader
) {
    if (named_factories.insert({factory_label, loader}).second == false)
        return "factory " + factory_label + " existed";
    return "";
}

std::string CGNImpl::remove_factory(
    const std::string &factory_label
) {
    if (named_factories.erase(factory_label) == 1)
        return "";
    return "factory " + factory_label + " not found";
}

// bool CGNImpl::_check_infinite_loop(const std::string &current_key) {
//     for (auto it = tls_runtime; it; it = it->call_from)
//         if (it->loop_detect_key == current_key)
//             return true;
//     return false;
// }

// cache supported (cache detection in API::confirm_target_opt())
// Prepare variable and call
// @param label: //hello:world
CGNTarget CGNImpl::create_target(
    const std::string &factory_label_in, 
    const Configuration &cfg
) {
    return _create_target_impl<true>(factory_label_in, cfg, nullptr, nullptr);
} //CGNImpl::create_target()

CGNTarget CGNImpl::create_target(
    CGNTargetOpt *in,
    std::function<void(CGNTargetOpt *in)> loader
) {
    return _create_target_impl<false>("", {}, in, loader);
} //CGNImpl::create_target()


template<bool ByNamedFactory> CGNTarget 
CGNImpl::_create_target_impl(
    const std::string &a_label_in, 
    const Configuration &a_cfg_in,
    CGNTargetOpt *b_opt_in,
    std::function<void(CGNTargetOpt *in)> b_fn_loader
) {
    ConfigurationID cfgid_before_trim;
    std::string suggest_label;

    if constexpr (ByNamedFactory) {
        Configuration tmp = a_cfg_in;
        tmp.visit_all_keys(); tmp.trim_lock();
        cfgid_before_trim = cfg_mgr->commit(tmp);
        suggest_label = a_label_in;
    }
    else {
        Configuration tmp = b_opt_in->cfg;
        tmp.visit_all_keys(); tmp.trim_lock();
        cfgid_before_trim = cfg_mgr->commit(tmp);
        suggest_label = b_opt_in->out_parent_prefix + b_opt_in->name;
    }

    logger.println("CreateTarget ", suggest_label + " #" + cfgid_before_trim);

    CGNTargetOpt a_opt;
    
    std::function<void(CGNTargetOpt *in)> fn_loader;
    TLRuntime now_rt;
    now_rt.label = suggest_label;
    now_rt.call_from = tls_runtime;
    now_rt.cfgid_before_trim = cfgid_before_trim;
    tls_push(&now_rt);

    std::string override_anode_name;
    auto make_ret = [&](const std::string &errmsg) {
        if (halt_on_error && errmsg.size()) {
            tls_pop(&now_rt);
            throw std::runtime_error{"CreateTarget(" + suggest_label + "): " + errmsg};
        }
        
        CGNTarget rv;
        if (now_rt.target_now)
            rv = *now_rt.target_now;
        else{
            rv.label = suggest_label;
            rv.anode = nullptr;
        }
        rv.errmsg = errmsg;
        
        if (override_anode_name.size()) {
            // always empty file[] for phase 1 GraphNode
            rv.anode = graph.get_node(override_anode_name);
            graph.set_node_files(rv.anode, {});
            graph.remove_inbound_edges(rv.anode);
            for (auto it : now_rt.dep_anodes)
                graph.add_edge(it, rv.anode);
        }

        if (rv.anode && now_rt.call_from)
            now_rt.call_from->dep_anodes.insert(rv.anode);

        tls_pop(&now_rt);
        return rv;
    };

    //case by factory_label
    if constexpr(ByNamedFactory) {
        //expand short label and generate src_prefix and out_prefix
        //    [IN] label: @cell//project:nameA
        //          cell: @cell or <NULLSTR>
        //          stem: project
        // factory_label: @cell//project:nameA
        //    facty_name: nameA
        // std::string &factory_label = opt.factory_label;
        // std::string &facty_name    = opt.factory_name;
        std::string script_label;
        auto [dir_in, _expand_err] = _expand_cell(a_label_in);  //unix_sep
        if (_expand_err.size())
            return make_ret(_expand_err);
        
        if (auto fdname = a_label_in.rfind(':'); fdname != a_label_in.npos) {
            a_opt.name = a_label_in.substr(fdname+1);
            dir_in.resize(dir_in.size() - a_opt.name.size() - 1);  //remove ":xxx" suffix
            script_label = a_label_in.substr(0, fdname) + "/BUILD.cgn.cc";
        }else
            script_label = a_label_in + "/BUILD.cgn.cc";
        a_opt.out_parent_prefix = (cgn_out / "obj").string();
        a_opt.out_parent_prefix_unixsep = cgn_out_unixsep + "/obj";
        std::string last_dir;
        for (std::size_t i=0, fd=0; fd<dir_in.size(); i=fd+1) {
            if (fd = dir_in.find('/', i); fd == dir_in.npos)
                fd = dir_in.size();
            last_dir = dir_in.substr(i, fd-i);
            a_opt.out_parent_prefix += CGNTargetMaker::PATH_SEPARATOR + last_dir + "_";
            a_opt.out_parent_prefix_unixsep += "/" + last_dir + "_";
        }
        a_opt.out_parent_prefix += CGNTargetMaker::PATH_SEPARATOR;
        a_opt.out_parent_prefix_unixsep += "/";
        a_opt.src_prefix = dir_in + "/";
        a_opt.cfg = a_cfg_in;

        if (a_opt.name.empty()) { 
            if (last_dir.empty()) // for label="@cell//"
                return make_ret("CreateTarget: target factory name must be assgined.");
            a_opt.name = last_dir;
            suggest_label += ":" + last_dir;
        }

        // GraphNode with Untrimmed config
        override_anode_name = "U" + a_opt.out_parent_prefix_unixsep + a_opt.name + "_" + cfgid_before_trim;

        // CGNTargeta_opt ready, load pimpl.named_factories[] by active_script()
        //   now_rt.anode <-- script_label(BUILD.cgn.cc) added with make_ret() later
        auto [script_anode, script_err] = active_script(script_label);
        if (script_err.size())
            return make_ret(script_err);
        
        // Find by factory_label, 
        // Note: the adep of Interpreter::preload_labels() would be added inside fn_loader
        if (auto fd = named_factories.find(suggest_label); fd != named_factories.end())
            fn_loader = fd->second;
        else
            return make_ret("CreateTarget: target factory " + suggest_label + " not found.");
        
        now_rt.active_opt = &a_opt;
    } //endif ByNamedFactory

    if (!ByNamedFactory) {
        std::string _parent_dir = b_opt_in->out_parent_prefix_unixsep;
        if (b_opt_in->out_parent_prefix.size())
            _parent_dir = b_opt_in->out_parent_prefix;
        
        if (b_opt_in->name.empty() || b_opt_in->src_prefix.empty() || _parent_dir.empty())
            return make_ret("CreateTarget: CGNTargetOpt misssing fields");
        
        if (!b_fn_loader)
            return make_ret("CreateTarget: argument fn_loader required.");

        if (Tools::is_absolute_path(_parent_dir) 
        && !Tools::is_file_inside(_parent_dir, cgn_out.string()))
            return make_ret("CreateTarget: out_parent_prefix should inside " + cgn_out.string());
        
        std::filesystem::path parent_dir{_parent_dir};
        b_opt_in->out_parent_prefix_unixsep = b_opt_in->out_parent_prefix 
            = Tools::locale_path(parent_dir.string() + "/");
        #ifdef _WIN32
            std::replace(b_opt_in->out_parent_prefix_unixsep.begin(),
                         b_opt_in->out_parent_prefix_unixsep.end(),
                         '\\', '/');
        #endif

        now_rt.active_opt = b_opt_in;
        fn_loader = std::move(b_fn_loader);

        override_anode_name = "U" + b_opt_in->out_parent_prefix_unixsep + b_opt_in->name + "_" + cfgid_before_trim;
    } //endif not ByNamedFactory

    //Cycle dep detection
    // Since the real "factory + cfgid" would determinate in cfg lock
    // (usually in interpreter). This string only use to check cycle
    // dependency.
    now_rt.loop_detect_key = now_rt.active_opt->out_parent_prefix_unixsep 
                           + now_rt.active_opt->name 
                           + "/" + (std::string)now_rt.cfgid_before_trim;
    for (auto it = tls_runtime->call_from; it; it = it->call_from)
        if (it->loop_detect_key == now_rt.loop_detect_key)
            return make_ret("CreateTarget: cycle-dependency");

    // call target builder (user lambda fn and interpreter inside)
    //  the API.confirm_target_opt() would be called inside fn_loader.
    now_rt.active_opt->_api_pimpl = this;
    fn_loader(now_rt.active_opt);

    // if there's no opt.confirm() or opt.set_fail() called.
    if (now_rt.target_now == nullptr)
        return make_ret("CreateTarget: " + now_rt.label  + " unconfirmed.");

    // Case of enter confirm_target_opt()
    //  1. target_cache found and anode.files[] latest
    //     -> return directly
    //  2. target_cache found but anode.files[] stale
    //     -> target_cache has been removed in confirm() and goto case 4
    //  3. target_cache not exist, anode.files[] latest
    //     -> target_maker.file_unchanged == true, assign target_cache here
    //  4. target_cache not exist, anode.files stale
    //     -> target_maker.file_unchanged == false, assign target_cache here

    // case 1
    if (now_rt.target_maker == nullptr)
        return override_anode_name="", make_ret("");
    
    // if no cache found and the error msg has been set
    if (now_rt.target_maker->errmsg.size()) {
        // if errmsg set after confirm, the anode has been assigned,
        // here we ignore it, because of all nodes which depend on this anode
        // would get result from current function, and the return value and
        // now_rt.call_from.deps[] of current function are not mention to anode.
        //
        // rollback to phase 1, make_ret() to get phase 1 anode
        return make_ret(now_rt.target_maker->errmsg);
    }
    
    // case 2 and 4: Regenerate current Node if file changed
    if (now_rt.target_maker->file_unchanged == false) {
        // remove all deps from current node 
        graph.remove_inbound_edges(now_rt.target_maker->anode);

        // watch {build.ninja + maker.ninja_file_appendix[]}
        now_rt.target_maker->ninja_file_appendix.insert(
            now_rt.target_maker->ninja_file_appendix.begin(),
            now_rt.target_maker->out_prefix + CGNTargetMaker::NINJA_ENTRY_FILENAME
        );
        graph.set_node_files(now_rt.target_maker->anode, 
            now_rt.target_maker->ninja_file_appendix);

        // release to write down build.ninja to disk, then set_node_status_to_latest()
        // would get the right mtime.
        now_rt.target_maker->ninja = nullptr;
        
        // set anode dep from current
        // Script GraphNode has been added by active_script() below
        for (GraphNode *p : now_rt.dep_anodes)
            graph.add_edge(p, now_rt.target_maker->anode);
        
        //update mtime in fileDB after interpreter returned successful.
        // file[0] : usually 'libSCRIPT.cgn.so' or 'build.ninja of target'
        graph.clear_file0_mtime_cache(now_rt.target_maker->anode);
        // graph.forward_status(ptr->anode);
        graph.set_node_status_to_latest(now_rt.target_maker->anode);
    } // otherwise case 3: do not change anode

    // write target cache
    targets[now_rt.target_maker->get_cache_name()] = *now_rt.target_maker.get();

    // put current target into main_ninja
    std::ofstream fout(obj_main_ninja, std::ios::app);
    std::string ninja_file_unixsep = now_rt.target_maker->out_prefix_unixsep 
                                   + CGNTargetMaker::NINJA_ENTRY_FILENAME;
    if (main_subninja.insert(ninja_file_unixsep).second)
        fout<<"subninja "<<NinjaFile::escape_path(ninja_file_unixsep)<<"\n";

    logger.verbose_paragraph("CreateTarget: subninja " + ninja_file_unixsep + " generated.");
    return override_anode_name="", make_ret("");
} //CGNImpl::_create_target_impl()

std::pair<CGNTarget, int> CGNImpl::create_and_build_target(
    const std::string &label, 
    const Configuration &cfg
) {
    auto rv = create_target(label, cfg);
    if (rv.errmsg.size())
        return {rv, -1};
    
    logger.verbose_paragraph("=================================================================\n");

    //generate compile_commands.json
    std::string compdb = "ninja -f " + obj_main_ninja.string()
                       + " -t compdb > " + cgn_out.string() 
                       + "/obj/compile_commands.json";
    system(compdb.c_str());
    logger.println(label + " analysed", "");
    graph.db_flush();
    
    std::string cmd = (scriptcc_debug_mode?"ninja -f ":"ninja -d keepdepfile -d keeprsp -f ")
                    + obj_main_ninja.string() 
                    + " " + Tools::shell_escape(rv.ninja_entry, HOST_SHELL);
    if (logger.is_verbose())
        cmd += " --verbose";
    logger.paragraph(cmd + "\n");
    
    int exitcode = system(cmd.c_str());
    
    if (exitcode == 0 && rv.outputs.size())
        logger.println("Build success: ", rv.outputs[0] + "\n");
    return {rv, exitcode};
}

// case1: target_cache[] existed, graph(target) Latest
//        return cache
// case2: target_cache[] existed, graph(target) Stale
//        delete last cache and goto case 3
// case3: target_cache[] not existed
//        test anode inside TargetMaker and return TargetMaker to let user to fill them.
//        return normal struct, then graph(target) would be assigned in CGNImpl::analyse()
//
CGNTargetMaker *CGNImpl::confirm_target_opt(CGNTargetOpt *in, const std::string &with_errmsg)
{
    assert(tls_runtime->active_opt == in);

    // If the target has been confirmed before
    if (tls_runtime->target_maker)
        return tls_runtime->target_maker.get();

    // Set failed before config lock
    //  No adep to rt.call_from
    //  No ninja*, anode*, ...
    if (with_errmsg.size()) {
        tls_runtime->target_maker = std::make_unique<CGNTargetMaker>(*in);
        tls_runtime->target_maker->errmsg = with_errmsg;
        tls_runtime->target_now = tls_runtime->target_maker.get();
        return nullptr;
    }

    // lock config and get cfg_id
    in->cfg.trim_lock();

    ConfigurationID cfg_id = cfg_mgr->commit(in->cfg);
    logger.println("CreateTarget ", tls_runtime->label
                + " #" + tls_runtime->cfgid_before_trim + " -(trim)-> #" + cfg_id);

    // std::string cache_name = in->out_parent_prefix_unixsep + in->name + "_" + cfg_id;
    // auto [tgt_iter, tgt_nx] = targets.emplace(cache_name, CGNTarget{});
    // tls_runtime->maker = std::make_unique<CGNTargetMaker>(*in, tgt_iter->second, false);
    std::unique_ptr<CGNTargetMaker> maker = std::make_unique<CGNTargetMaker>(*in);
    ((CGNTarget*)maker.get())->label = in->get_label();
    std::string cache_name = maker->get_cache_name();

    if (auto fd = targets.find(cache_name); fd != targets.end()) {
        // case 1: cache found and Latest, return directly
        if (fd->second.anode->status == GraphNode::Latest){
            logger.verbose_paragraph("CreateTarget: confirm_target_opt(" 
                + cache_name + ") in_memory cache found");
            tls_runtime->target_now = &(fd->second);
            return nullptr;
        }
        
        // case 2: delete current and goto case 3
        targets.erase(fd);
    }

    // case 3 below: target cache not exist or removed, (re)generate ninja file.
    // If the anode->files is empty, we cannot set _m_file_unchanged to true, 
    // because the newly inited Graphnode don't include any files[], it should 
    // include a build.ninja at least.
    GraphNode *anode = graph.get_node("T" + cache_name);
    maker->_m_file_unchanged = false;
    if (anode->files.size()) {
        if (graph.test_status(anode); anode->status == GraphNode::Latest) {
            maker->_m_file_unchanged = true;
            logger.verbose_paragraph("CreateTarget: confirm_target_opt(" 
                + cache_name + ") ninja file unchanged.");
        }
        else {
            logger.verbose_paragraph("CreateTarget: confirm_target_opt(" 
                + cache_name + ") anode.files[] changed, regen anode.");
            graph.remove_inbound_edges(anode);
        }
    }
    
    ((CGNTarget*)maker.get())->anode = anode;
    maker->ninja_entry = maker->out_prefix + maker->NINJA_ENTRY_TARGET;

    // create dir opt->out_prefix
    api.mkdir(maker->out_prefix);

    // anode file[] (build.ninja) to GraphNode
    // if current target GraphNode is latest, build.ninja would not need to update.
    std::string ninja_file_ossep = maker->out_prefix + CGNTargetMaker::NINJA_ENTRY_FILENAME;
    if (maker->file_unchanged == false)
        maker->ninja = std::make_unique<NinjaFile>(ninja_file_ossep);
    
    // forward GraphNode::Stale
    // graph.forward_status(opt->anode);

    tls_runtime->target_maker = std::move(maker);
    tls_runtime->target_now = tls_runtime->target_maker.get();
    return tls_runtime->target_maker.get();
}


void CGNImpl::add_adep(GraphNode *early, GraphNode *late)
{
    graph.add_edge(early, late);
}


// void CGNImpl::start_new_round()
// {
//     graph.clear_mtime_cache();
//     adep_cycle_detection.clear();
//     // current_analysis_level = 0;
// }

std::pair<std::string, std::string> CGNImpl::_expand_cell(const std::string &ss) const
{
    // ss: @cell//project:nameA
    // => rv: dir_to/@cell_folder/project:nameA
    
    if (ss[0]=='/' && ss[1]=='/' && ss[2] != '@')
        return {ss.substr(2), ""};
    else if (ss[0] == '@') {
        auto fd = ss.find('/', 1);
        if (fd == ss.npos || ss[fd+1] != '/')
            return{"", "Wrong label: '@cell//' required"};

        std::string cellname = ss.substr(0, fd);
        if (cells.count(cellname))
            return {cellname + "/" + ss.substr(fd+2), ""};
        // std::string cellname = ss.substr(1, fd-1);
        // if (auto fd2 = cells.find(cellname); fd2 != cells.end())
        //     return fd2->second + "/" + ss.substr(fd+2);
        return {"", "Wrong label: '" + cellname + "' cell not found."};
    }
    else
        return {"", ss + " Invalid label format."};
} //CGNImpl::_expand_cell()


CGNImpl::CGNImpl(std::unordered_map<std::string, std::string> cmd_kvargs)
: cmd_kvargs(cmd_kvargs), graph(&logger){

    //init logger system
    // (always true, current in development)
    // recommended writing: (using underscore way)
    //  "scriptcc_debug", "halt_on_error"
    scriptcc_debug_mode = cmd_kvargs.count("scriptcc_debug") || cmd_kvargs.count("scriptcc-debug");
    halt_on_error = cmd_kvargs.count("halt_on_error") || cmd_kvargs.count("halt_onerror") 
                 || cmd_kvargs.count("halt-on-error") || cmd_kvargs.count("halt-onerror");
    logger.set_verbose(cmd_kvargs.count("verbose"));

    logger.verbose_paragraph("CWD: " + std::filesystem::current_path().string());

    //init path
    std::string dsuffix = (scriptcc_debug_mode? "d":"");
    cgn_out = Tools::locale_path(cmd_kvargs.at("cgn-out"));
    cgn_out_unixsep = cgn_out.string();
    #ifdef _WIN32
        #ifdef _DEBUG
            dsuffix = "MDd";
        #else
            dsuffix = "MD";
        #endif
    dsuffix += (scriptcc_debug_mode? "pdb":"");
    std::replace(cgn_out_unixsep.begin(), cgn_out_unixsep.end(), '\\', '/');
    #endif
    analysis_path = cgn_out / ("analysis_" + Tools::get_host_info().os + dsuffix);
    obj_main_ninja = cgn_out / "obj" / "main.ninja";
    // obj_placeholder_ninja = cgn_out / "obj" / "placeholder.ninja";


    // replace path\to\cgn.exe => path\to\cgn.lib (win only)
    #ifdef _WIN32
    cgnapi_winimp = self_realpath();
    if (cgnapi_winimp.size() > 4)
        cgnapi_winimp = cgnapi_winimp.substr(0, cgnapi_winimp.size()-4) + ".lib";
    if (!std::filesystem::is_regular_file(cgnapi_winimp))
        throw std::runtime_error{"Cannot locate cgn.lib"};
    #endif //_WIN32

    if (std::filesystem::exists(cgn_out)) {
        if (!std::filesystem::is_directory(cgn_out))
            throw std::runtime_error{cgn_out.string() + "is not folder."};
    }else
        std::filesystem::create_directories(cgn_out);

    std::filesystem::create_directories(analysis_path);
    std::filesystem::create_directories(cgn_out / "obj");

    std::ofstream stamp_file(cgn_out / ".cgn_out_root.stamp");
    stamp_file.close();

    // copy cgn.exe to $analysis_path if changed, then build.ninja can use internal tools.
    std::filesystem::path cgn_exe_now{self_realpath()};
    cgn_exe_shadow = analysis_path / cgn_exe_now.filename();
    if (!std::filesystem::exists(cgn_exe_shadow) || 
        std::filesystem::last_write_time(cgn_exe_shadow) < std::filesystem::last_write_time(cgn_exe_now)
    ) {
        std::filesystem::copy_file(cgn_exe_now, cgn_exe_shadow, 
            std::filesystem::copy_options::overwrite_existing);
        logger.println(cgn_exe_shadow.string() + " updated.");
    }else
        logger.verbose_paragraph(cgn_exe_shadow.string() + " no need to update, mtime="
            + std::to_string((int64_t)std::filesystem::last_write_time(cgn_exe_shadow).time_since_epoch().count()));

    // run vcvars64.bat if necessary
    #ifdef _WIN32
    if (cmd_kvargs.count("winenv")) {
        std::vector<std::string> possible_scripts = {
            R"(C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat)",
            R"(C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat)",
            R"(C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat)",
            R"(C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat)",
        };

        if (cmd_kvargs["winenv"].size()){
            possible_scripts = {cmd_kvargs["winenv"]};
            logger.verbose_paragraph("Using winenv bat file from cli: " + possible_scripts[0]);
        }
        else {
            raymii::CommandResult vswhere_test = raymii::Command::exec(
                R"("C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")" 
                " -latest -products *"
                " -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
                " -property installationPath");
            if (vswhere_test.exitstatus == 0) {
                std::string vsdir = vswhere_test.output;
                if (auto fd = vsdir.find('\n'); fd != vsdir.npos)
                    vsdir = vsdir.substr(0, fd);
                possible_scripts = {vsdir + R"(\VC\Auxiliary\Build\vcvars64.bat)",
                                    vsdir + R"(\VC\Auxiliary\Build\vcvars32.bat)"};
                logger.verbose_paragraph("Using vswhere.exe to detect script : " + possible_scripts[0]);
            }
        }

        raymii::CommandResult resp;
        for (auto vcbat : possible_scripts) {
            resp = raymii::Command::exec("\"" + vcbat + "\" 2>&1 >NUL && set");
            if (resp.exitstatus == 0)
                break;
        }
        if (resp.exitstatus)
            throw std::runtime_error{"cannot run vcvars64.bat"};
        for (std::size_t b = 0, fdnext; b < resp.output.size(); b = fdnext+1) {
            fdnext = resp.output.find('\n', b);
            if (fdnext == resp.output.npos)
                fdnext = resp.output.size();
            std::string exp = resp.output.substr(b, fdnext-b);
            if (auto fd = exp.find('='); fd != exp.npos)
                Tools::setenv(exp.substr(0, fd).c_str(), exp.substr(fd+1).c_str());
            else
                throw std::runtime_error{"setenv failure: " + exp};
        }
    }
    #endif //win32

    // scriptcc variable
    #ifdef _WIN32
    script_cc = "cl.exe";
    #else
    script_cc = "clang++";
    #endif
    if (auto fd = cmd_kvargs.find("scriptcc"); fd != cmd_kvargs.end())
        script_cc = fd->second;

    // if (cmd_kvargs.count("regeneration"))
    //     regen_all = true;
    logger.println("Init configuration manager");
    cfg_mgr = std::make_unique<ConfigurationManager>(
                (cgn_out / "configurations").string(), &graph);

    //prepare obj-main-ninja (entry of all targets)
    logger.println("Loading previous main.ninja entrypoint");
    std::ifstream fin(obj_main_ninja, std::ios::in);
    if (!fin) //create if not existed
        std::ofstream{obj_main_ninja};
    else { //Load and test previous main.ninja
        // helper fn to test some .ninja files missing in obj_main_ninja
        // auto test_ninja_file = [&](const std::string &ninja_file) {
        //     if (!std::filesystem::is_regular_file(ninja_file))
        //         return false;
        //     std::string tgt = api.parent_path(ninja_file) + "/.stamp";
        //     auto rv = raymii::Command::exec("ninja -f " + obj_main_ninja.string() + " -t query " + tgt);
        //     return rv.exitstatus == 0;
        // };
        
        // // load all rows in main.ninja
        
        // bool entry_tested = false, need_rebuild = false;
        // for (std::string ln; !fin.eof() && std::getline(fin, ln);)
        //     if (ln.size() > SUBNINJA.size()) {
        //         auto subfile = NinjaFile::parse_ninja_str(
        //                         ln.substr(SUBNINJA.size()));
        //         if (!entry_tested && !test_ninja_file(subfile)) {
        //             need_rebuild = true;
        //             logger.verbose_paragraph("\n" + subfile + " missing or invalid content, regenerate.\n");
        //             break;
        //         }
        //         entry_tested = true;  //test the first one entry
        //         main_subninja.insert(subfile);
        //     }

        bool need_rebuild = false;
        constexpr std::string_view SUBNINJA{"subninja "};
        for (std::string ln; !fin.eof() && std::getline(fin, ln);)
            if (ln.size() > SUBNINJA.size()) {
                auto subfile = NinjaFile::parse_ninja_str(ln.substr(SUBNINJA.size()));
                if (main_subninja.insert(subfile).second == false) {
                    logger.verbose_paragraph("\n" + subfile + " duplicate, regenerate.\n");
                    need_rebuild = true;
                    break;
                }
            }

        if (!need_rebuild) {
            auto rv = raymii::Command::exec("ninja -f " + obj_main_ninja.string() + " -n");
            need_rebuild |= (rv.exitstatus != 0);
        }

        // clear previous main.ninja if error found.
        if (fin.close(); need_rebuild) {
            logger.verbose_paragraph("Rebuild ninja entry " + obj_main_ninja.string() + "\n");
            main_subninja.clear();
            std::ofstream fout{obj_main_ninja};
            fout.close();
        }
    }
        
    // graph init (load previous one)
    logger.println("Loading fileDB");
    graph.db_load((analysis_path / ".cgn_deps").string());
    if (cmd_kvargs.count("verbose")) {
        std::ofstream fmermaid(analysis_path / ".memraid");
        fmermaid<<graph.get_memraid_flowchart();
    }

    //CGN cell init
    // scan folder starting with '@' at working-root
    for (auto it : std::filesystem::directory_iterator(".")) {
        std::error_code ec;
        // name: string like "@base"
        std::string name = it.path().filename().string();
        if (it.is_directory(ec) && name[0] == '@' && !ec) {
            cells.insert(name);
            logger.verbose_paragraph("Cell " + name + " detected.");
        }
    }

    //create folder symbolic link
    // TODO: permission denied when create_directory_symlink() on windows platform
    // GraphNode *init_node = graph.get_node(".cgn_init");
    // graph.test_status(init_node);
    // if (init_node->files.empty() || init_node->status != GraphNode::Latest) {
    //     graph.set_node_files(init_node, {".cgn_init"});
    //     std::filesystem::remove_all(cell_lnk_path);
    //     std::filesystem::create_directory(cell_lnk_path);
    //     for (auto [k, v] : cells) {
    //         if (k == "CELL_SETUP")
    //             continue;
    //         std::filesystem::path out = cell_lnk_path / ("@" + k);
    //         std::error_code ec;
    //         std::filesystem::create_directory_symlink(std::filesystem::absolute(v), out, ec);
    //         if (ec)
    //             throw std::runtime_error{"error no symlink cell_include: " + v 
    //                     + ", error_code=" + std::to_string(ec.value())};
    //     }
    //     graph.set_node_status_to_latest(init_node);
    // }

    std::pair<cgnv1::GraphNode *, std::string> setup_so_resp;
    if (cmd_kvargs.count("use_default_cgn_setup"))
        setup_so_resp = active_script("@cgn.d//library/cgn_default_setup.cgn.hxx");
    else { //call cgn_setup()
        const std::string cgn_setup_filename = "cgn_setup.cgn.cc";
        if (!std::filesystem::exists(cgn_setup_filename))
            throw std::runtime_error{cgn_setup_filename + " not found"};
        setup_so_resp = active_script("//" + cgn_setup_filename);
    }
    if (setup_so_resp.first == nullptr)
        throw std::runtime_error{"Cannot load cgn_setup :" + setup_so_resp.second};
    
    CGNInitSetup x;
    #ifndef _WIN32
        cgn_setup(x);
    #else
        using FnSetup = void(*)(CGNInitSetup&);
        // FnSetup fn_setup = (FnSetup)GlobalSymbol::find("?cgn_setup@@YAXAEAUCGNInitSetup@@@Z");
        FnSetup fn_setup = (FnSetup)GlobalSymbol::find("?cgn_setup@@YAXAEAUCGNInitSetup@cgnv1@@@Z");
        if (!fn_setup)
            throw std::runtime_error{"cgn_setup() not found, not exported?"};
        fn_setup(x);
    #endif
    if (x.log_message.size())
        logger.verbose_paragraph(x.log_message);
    for (auto &[name, cfg] : x.configs) {
        cfg.visit_all_keys();
        cfg.trim_lock();
        std::string uid = cfg_mgr->commit(cfg);
        cfg_mgr->set_name(name, uid);
    }

} //CGNImpl::init()

CGNImpl::~CGNImpl()
{
    auto *ptr = tls_runtime;
    // std::cerr<<"PTR ADDR: "<<(void*)ptr<<std::endl;
    // std::cerr<<"PTR: "<<ptr->label<<std::endl;
    assert(tls_runtime == nullptr);

    targets.clear();
    named_factories.clear();
    cfg_mgr.reset();

    // some deconstructor may exist in side-load dll, so we dlclose() in finally
    scripts.clear();
} //CGNImpl::release()

thread_local TLRuntime *CGNImpl::tls_runtime = nullptr;

} //namespace