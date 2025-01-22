#include <fstream>
#include <sstream>
#include <optional>
#include <filesystem>
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
#else
extern void cgn_setup(cgnv1::CGNInitSetup &x) __attribute__((weak));
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
#endif

// for only debug purpersal in dev
// void cgn_setup(CGNInitSetup &x) {}

namespace cgnv1 {

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

// NodeName == unique_label (like @cgn.d//library/cxx.cgn.bundle)
// case1: script loaded && stat(files[]) == Latest
//        return ;
// case2: script loaded && stat(files[]) == Stale
//        unload script => goto case 3
// case3: script not-load && stat(files[]) == Stale
//        rebuild => goto case 4
// case4: script not-load && stat(files[]) == Latest
//        load and return;
std::pair<GraphNode*, std::string> 
CGNImpl::active_script(const std::string &label)
{
    logger.println("ActiveScript ", label);
    auto [labe2, _expand_err] = _expand_cell(label);
    if (_expand_err.size()) {
        if (halt_on_error)
            throw std::runtime_error{_expand_err};
        return {nullptr, _expand_err};
    }
    if (std::string_view{labe2.data(), 3} == "../")
        throw std::runtime_error{"Invalid label " + label};
    
    CGNScript s; //the next value of scripts[label]

    if (auto fd = scripts.find(label); fd != scripts.end()) {
        graph.test_status(fd->second.anode);
        if (fd->second.anode->status == GraphNode::Latest)
            return {fd->second.anode, ""}; // case1: scripts existed, graph Latest
        
        // case2: script existed, graph Stale. 
        //        erase script and goto case 3
        s.anode  = fd->second.anode;
        s.sofile = fd->second.sofile;
        // offline_script(&(fd->second));
        scripts.erase(fd);
    }
    else
        graph.test_status(s.anode = graph.get_node("S" + label));

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

        //(re)generate GraphNode.files[]
        // script_srcs: file in fetched bundle or .rsp
        // script_all : script_srcs + header info hint by compiler
        std::vector<std::string> script_srcs = expand_scripts(fpath);
        std::unordered_set<std::string> script_all;
        script_all.insert(script_srcs.begin(), script_srcs.end());

        //start build, cases of "label => factory_prefix (string format)"
        //                //BUILD.cgn.cc  =>  "//:"
        //          //hello/BUILD.cgn.cc  =>  "//hello:"
        //        //my_script.cgn.bundle  =>  "//my_script.cgn.bundle:"
        //  @cgn.d//library/cmake.cgn.cc  =>  "@cgn.d//library/cmake.cgn.cc:"
        std::string def_var_prefix = mangle_var_prefix(labe2);
        std::string def_ulabel_prefix = "\"//:\"";
        if (auto fd = label.rfind('/'); fd != label.npos) {
            std::string_view file_name{label.c_str() + fd + 1};
            if (file_name == "BUILD.cgn.cc")
                def_ulabel_prefix = "\"" + label.substr(0, fd) + ":\"";
            else
                def_ulabel_prefix = "\"" + label + ":\"";
        }
        // if (auto fd = labe2.rfind('/'); fd != labe2.npos) 
        //     def_ulabel_prefix = "\"//" + labe2.substr(0, fd) + ":\"";

        auto cc_end_with = [&](const std::string &want) {
            if (script_cc.size() >= want.size())
                return script_cc.substr(script_cc.size() - want.size()) == want;
            return false;
        };

        bool is_win  = (Tools::get_host_info().os == "win");
        bool is_unix = !is_win;
        bool is_msvc  = cc_end_with("cl.exe");
        bool is_clang = cc_end_with("clang") || cc_end_with("clang++");

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
            linker_in += Tools::shell_escape(outname) + " ";
            std::ofstream frsp(rspname); 

            if (is_msvc && is_win) {
                //TODO: Since msvc cl.exe /D cannot process '#' in command line
                //      but filename can accept it, we consider 2 solution here
                //      1. use /FI to insert char that can't be defined in cmd
                //      2. use TLS to storage CGN_ULABEL_PREFIX when load_library
                //
                frsp<< "/c " << ("." / pt).string() <<" /nologo /showIncludes /Od /Gy "
                    "/DWINVER=0x0A00 /D_WIN32_WINNT=0x0603 /D_AMD64_ "
                    " /DCGN_VAR_PREFIX=" + def_var_prefix +
                    " /D\"CGN_ULABEL_PREFIX=\"" + def_ulabel_prefix + "\"\"" + 
                    " /I. /utf-8 /EHsc /MD /Fo: " + Tools::shell_escape(outname);
                if (scriptcc_debug_mode)
                    frsp<<" /Od /Zi /Fd: " + Tools::shell_escape(outname) + ".pdb";
            }
            else if (is_unix) {
                if (is_clang && scriptcc_debug_mode) //llvm debug (lldb)
                    frsp<<"-g -glldb -fstandalone-debug -fno-limit-debug-info "
                          "-fsanitize=address ";
                if (!is_clang && scriptcc_debug_mode) //gcc debug
                    frsp<<"-g ";
                frsp<<"-c " << it << " -MMD -MF " + Tools::shell_escape(depname) +
                        " -fPIC -fdiagnostics-color=always -std=c++11 -I. " + 
                        " -DCGN_VAR_PREFIX=" + Tools::shell_escape(def_var_prefix) +
                        " -DCGN_ULABEL_PREFIX=" + Tools::shell_escape(def_ulabel_prefix) + 
                        " -o " + Tools::shell_escape(outname);
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
                run_link(dbg_flag + " -fPIC --shared -Wl,-undefined,dynamic_lookup -o " + s.sofile);
            else if (is_clang) //llvm-linker is faster then gnu linker
                run_link(dbg_flag + " -fuse-ld=lld -fPIC --shared -o " + s.sofile);
            else
                run_link(dbg_flag + " -fPIC --shared -o " + s.sofile);
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
        // graph.forward_status(s.anode);
        graph.set_node_files(s.anode, node_vals);
        graph.clear_file0_mtime_cache(s.anode);
        graph.set_node_status_to_latest(s.anode);
    }

    // case4: script not loaded, graph Latest
    //        load into scripts[]
    s.sohandle = std::make_unique<DLHelper>(s.sofile);
    if (!s.sohandle->valid())
        throw std::runtime_error{"cannot load cgn script."};
    return {(scripts[label] = std::move(s)).anode, ""};
} //CGNImpl::active_script()

void CGNImpl::offline_script(const std::string &label)
{
    // see also DEVNOTE in doc.       
    if (auto fd = scripts.find(label); fd != scripts.end()){
        graph.set_node_status_to_unknown(fd->second.anode);
        scripts.erase(fd);
    }
}

// Internal CGNTargetOpt struct to save variable inside;
struct CGNTargetOptIntl : CGNTargetOpt {
    std::string script_label;
    GraphNode *script_anode;

    std::string _facty_name;
    std::string current_unit_name;

    Configuration cfg_before_trim;

    std::string out_prefix_unixsep;

    // recursivable sub target
    std::vector<CGNTargetOptIntl> sub_units;

    CGNTargetOptIntl *result_opt = nullptr;

    // filled in confirm_target_opt()
    // for normal target
    //   @cell//folder:target#AABBCCDD
    // for unit target
    //   @cell//folder:target#AABBCCDD-sub1#AA11BB22.4-subsub#BB22CC33.6
    // GraphNode name: "T" + <cache_label>
    std::string cache_label;

    bool target_confirmed = false;

    CGNTargetOptIntl() : CGNTargetOpt(_facty_name) {}
};

CGNTargetOptIn *CGNTargetOpt::create_sub_target(const std::string &name, bool as_result)
{
    CGNTargetOptIntl *self = dynamic_cast<CGNTargetOptIntl*>(this);
    auto &rv = self->sub_units.emplace_back();

    if (as_result)
        self->result_opt = &rv;
    
    // CGNTargetOptIn
    rv._facty_name = self->_facty_name;
    rv.src_prefix  = self->src_prefix;
    rv.out_prefix  = self->out_prefix;
    rv.cache_result_found = false;

    // CGNTarget
    rv.anode = nullptr;
    rv.ninja = nullptr;
    rv.result.factory_label = self->factory_label;
    rv.result.trimmed_cfg   = self->cfg_before_trim;

    // CGNTargetOptIntl
    rv.script_label = self->script_label;
    rv.script_anode = self->script_anode;
    rv.current_unit_name = name;
    rv.out_prefix_unixsep = self->out_prefix_unixsep;
    rv.cache_label = self->cache_label;

    return &rv;
}

CGNTarget *CGNTargetOpt::get_real_result()
{
    for (auto i = dynamic_cast<CGNTargetOptIntl*>(this); ; i=i->result_opt)
        if (i->result_opt == nullptr){
            if (i->target_confirmed)
                return &(i->result);
            return nullptr;
        }
}

// cache supported (cache detection in API::confirm_target_opt())
// Prepare variable and call
// @param label: //hello:world
CGNTarget CGNImpl::analyse_target(
    const std::string &label, 
    const Configuration &cfg
) {
    CGNTargetOptIntl opt;

    CGNTarget &rv = opt.result;
    rv.factory_label = label;
    rv.trimmed_cfg   = cfg;

    ConfigurationID cfg_id_in = cfg.get_id();
    if (cfg_id_in.empty()) {
        Configuration tmp = cfg;
        tmp.visit_all_keys();
        tmp.trim_lock();
        cfg_id_in = cfg_mgr->commit(tmp);
    }
    opt.cfg_before_trim = cfg;
    logger.println("Analyse ", label + " #" + cfg_id_in);

    //expand short label and generate src_prefix and out_prefix
    //    [IN] label: @cell//project:nameA
    //          cell: @cell or <NULLSTR>
    //          stem: project
    // factory_label: @cell//project:nameA
    //    facty_name: nameA
    // std::string &factory_label = opt.factory_label;
    // std::string &facty_name    = opt.factory_name;
    // std::string script_label;
    auto [dir_in, _expand_err] = _expand_cell(label);  //unix_sep
    if (_expand_err.size()) {
        rv.errmsg = _expand_err;
        return rv;
    }
    if (auto fdname = label.rfind(':'); fdname != label.npos) {
        opt._facty_name = label.substr(fdname+1);
        dir_in.resize(dir_in.size() - opt.factory_name.size() - 1);  //remove ":xxx" suffix
        opt.script_label = label.substr(0, fdname) + "/BUILD.cgn.cc";
    }else
        opt.script_label = label + "/BUILD.cgn.cc";

    opt.out_prefix = (cgn_out / "obj").string();
    opt.out_prefix_unixsep = cgn_out_unixsep + "/obj";
    std::string last_dir;
    for (std::size_t i=0, fd=0; fd<dir_in.size(); i=fd+1) {
        if (fd = dir_in.find('/', i); fd == dir_in.npos)
            fd = dir_in.size();
        last_dir = dir_in.substr(i, fd-i);
        opt.out_prefix += opt.path_separator + last_dir + "_";
        opt.out_prefix_unixsep += "/" + last_dir + "_";
    }
    opt.out_prefix += opt.path_separator;
    opt.out_prefix_unixsep += "/";

    if (opt._facty_name.empty()) {
        if (last_dir.empty()) {
            rv.errmsg = "target factory name must be assgined.";
            return rv;
        }
        opt._facty_name   = last_dir;
        rv.factory_label += ":" + opt._facty_name;
    }

    //Cycle dep detection
    // Since the real "factory + cfgid" would determinate in cfg lock
    // (usually in interpreter). This string only use to check cycle
    // dependency.
    std::string cycle_check_ss = opt.factory_label + "#" + cfg_id_in;
    
    auto adep_pop = [&](std::string errmsg = ""){ 
        adep_cycle_detection.erase(cycle_check_ss); 
        rv.errmsg = errmsg;
        return rv;
    };
    if (adep_cycle_detection.insert(cycle_check_ss).second == false)
        return adep_pop("analyse: cycle-dependency");

    //Prepare CGNTargetOpt to call target builder fn
    // opt.api = this->host_api;
    opt.src_prefix = dir_in + "/";
    
    //Call target builder
    // active_script and find factories 
    auto [script_anode, script_err] = active_script(opt.script_label);
    if (script_err.size())
        return adep_pop(script_err);
    opt.script_anode = script_anode;
    std::function<void(CGNTargetOpt*)> fn_loader;
    if (auto fd = factories.find(opt.factory_label); fd != factories.end())
        fn_loader = fd->second;
    else
        return adep_pop("target factory not found.");
    
    // call target builder (user lambda fn and interpreter inside)
    //  the API.confirm_target_opt() would process into next phase.
    this->runtime_env.src_prefix = opt.src_prefix;
    fn_loader(&opt);
    this->runtime_env.src_prefix = "";

    // pop up adep_cycle_detection
    adep_cycle_detection.erase(cycle_check_ss); 

    // write down the current target and its sub-unit-targets
    std::ofstream fout(obj_main_ninja, std::ios::app);

    // DFS opt.sub_units[]
    // submit all sub target units in recursive.
    std::vector<std::pair<CGNTargetOptIntl*, std::size_t>> opt_dfs = {{&opt, 0}};
    while (!opt_dfs.empty()) {
        CGNTargetOptIntl *ptr = opt_dfs.back().first;
        std::size_t &child_id = opt_dfs.back().second;
        if (child_id == 0) {
            if (ptr->target_confirmed == false)
                throw std::runtime_error{"target not confirmed."};
            
            // deliver errmsg to all child-unit-targets
            if (ptr->result.errmsg.size()) {
                for (auto &child_ptr : ptr->sub_units)
                    if (child_ptr.result.errmsg.empty())
                        child_ptr.result.errmsg = ptr->result.errmsg;
            }

            // write cache if new generation
            // opt->cache_label was filled by CGNImpl::confirm_target_opt();
            if (!ptr->cache_result_found && ptr->result.errmsg.empty())
                targets[ptr->cache_label] = ptr->result;

            // 'ptr->ninja' created in confirm_target_opt() if file modified.
            // release ninja file handle to write build.ninja down to disk
            // then fstat() could get the right mtime to written down to fileDB
            if (ptr->ninja) {
                delete ptr->ninja;
                ptr->ninja = nullptr;
            }

            if (ptr->result.errmsg.empty()) {
                // insert into main_subninja if interpreter successed.
                // subninja command enforce '/' path-sep
                std::string ninja_file_unixsep = ptr->out_prefix_unixsep + CGNTargetOpt::BUILD_NINJA;
                if (main_subninja.insert(ninja_file_unixsep).second)
                    fout<<"subninja "<<NinjaFile::escape_path(ninja_file_unixsep)<<"\n";
            }

            // if ninja file and it's inner dependency changed, 
            // update GraphNode and write down ninja file.
            if (!ptr->file_unchanged && ptr->result.errmsg.empty()) {
                // TODO: we want all sub-target have its own factory_label
                //       then they could call ctx.add_dep() by its own way.
                //       USER should add adep relations manually
                for (auto &child_ptr : ptr->sub_units)
                    graph.add_edge(ptr->anode, child_ptr.anode);

                //update mtime in fileDB after interpreter returned successful.
                // file[0] : usually 'libSCRIPT.cgn.so' or 'build.ninja of target'
                graph.clear_file0_mtime_cache(ptr->anode);
                // graph.forward_status(ptr->anode);
                graph.set_node_status_to_latest(ptr->anode);
            }
        }
        if (child_id < ptr->sub_units.size())
            opt_dfs.push_back({&ptr->sub_units[child_id++], 0});
        else
            opt_dfs.pop_back();
    } //end while(DFS stack not empty)

    for (auto i = &opt; ; i=i->result_opt)
        if (i->result_opt == nullptr) {
            if (halt_on_error && i->result.errmsg.size())
                throw std::runtime_error{i->result.errmsg};
            return i->result;
        }
} //CGNImpl::analyse()

// case1: target_cache[] existed, graph(target) Latest
//        return cache
// case2: target_cache[] existed, graph(target) Stale
//        delete last cache and goto case 3
// case3: target_cache[] not existed
//        return normal struct, then graph(target) would be assigned in CGNImpl::analyse()
//
CGNTargetOpt *CGNImpl::confirm_target_opt(CGNTargetOptIn *in)
{
    CGNTargetOptIntl *opt = dynamic_cast<CGNTargetOptIntl*>(in);

    if (opt->target_confirmed)
        return opt;
    opt->target_confirmed = true;
    
    // lock config and get cfg_id
    opt->cfg.trim_lock();
    ConfigurationID cfg_id = cfg_mgr->commit(opt->cfg);
    logger.println("Analysing ", opt->factory_label 
                + (opt->current_unit_name.size()? (" [" + opt->current_unit_name + "] "): "")
                + " #" + opt->cfg_before_trim.get_id() + " -(trim)-> #" + cfg_id);

    // convert dir_in to dir_out (add '_' suffix for each folder in path)
    // complete variable in opt when out_dir confirmed.
    {
        std::string mid_name = (opt->current_unit_name.size()?
                                opt->current_unit_name : opt->factory_name);
        opt->out_prefix += mid_name + "_" + cfg_id + opt->path_separator;
        opt->out_prefix_unixsep += mid_name + "_" + cfg_id + "/";
    }

    // string to find cache
    if (opt->current_unit_name.size())
        opt->cache_label += "||" + opt->current_unit_name + "#" + opt->cfg.get_id() 
                          + "." + std::to_string(opt->current_unit_name.size());
    else
        opt->cache_label = opt->factory_label + "#" + opt->cfg.get_id();
    if (auto fd = targets.find(opt->cache_label); fd != targets.end()) {
        // case 1: cache found and Latest, return directly
        if (fd->second.anode->status == GraphNode::Latest){
            opt->cache_result_found = opt->file_unchanged = true;
            opt->result = fd->second;
            logger.verbose_paragraph("confirm_target_opt(" 
                + opt->cache_label + ") in_memory cache found");
            return opt;
        }
        
        // case 2: delete current and goto case 3
        targets.erase(fd);
    }

    // case 3 below: normal case, (re)generate ninja file.
    opt->anode = opt->result.anode = graph.get_node("T" + opt->cache_label);
    opt->result.ninja_entry = opt->out_prefix + opt->BUILD_ENTRY;

    // create dir opt->out_prefix
    api.mkdir(opt->out_prefix);

    // anode file[] (build.ninja) to GraphNode
    // if current target GraphNode is latest, build.ninja would not need to update.
    std::string ninja_file_ossep = opt->out_prefix + CGNTargetOpt::BUILD_NINJA;
    if (opt->anode->files.size()) {
        if (graph.test_status(opt->anode); opt->anode->status == GraphNode::Latest) {
            opt->ninja = new NinjaFile("");
            opt->file_unchanged = true;
            logger.verbose_paragraph("confirm_target_opt(" 
                + opt->cache_label + ") ninja file unchanged.");
        }
    }
    if (opt->ninja == nullptr)
        opt->ninja = new NinjaFile(ninja_file_ossep);

    // special case: build_check mode, only opt->result.ninja_entry utilized 
    //               by caller.
    // the anode->files[] is empty for create by get_node() above right now.
    // if (current_analysis_level == 'b' && opt->anode->files.size()) {
    //     graph.test_status(opt->anode);
    //     if (opt->anode->status == GraphNode::Latest) {
    //         opt->cache_result_found = true;
    //         logger.verbose_paragraph("[build_check_mode] confirm_target_opt(" + opt->cache_label 
    //             + ") quick-return: " + opt->result.ninja_entry);
    //         return opt;
    //     }
    //     else
    //         logger.verbose_paragraph("[build_check_mode] confirm_target_opt(" + opt->cache_label 
    //             + ") but target stale, continue analysing...");
    // }
    // if (current_analysis_level == 'a') {  //TODO
    //     graph.test_status(opt->anode);
    //     if (opt->anode->status != GraphNode::Latest) {
    //         opt->ninja = new NinjaFile(ninja_file_ossep);
    //         logger.verbose_paragraph("confirm_target_opt(" + cache_label 
    //             + ") analysis_only but target stale, regenerate ninja file.");
    //     }
    // }

    // forward GraphNode::Stale
    // graph.forward_status(opt->anode);

    // register GraphNode* and remove the previous adep information.
    if (!opt->file_unchanged) {
        graph.remove_inbound_edges(opt->anode);
        graph.set_node_files(opt->anode, {ninja_file_ossep});
        graph.add_edge(opt->script_anode, opt->anode);
        for (auto *early : opt->quickdep_early_anodes)
            graph.add_edge(early, opt->anode);
    }
    opt->quickdep_early_anodes.clear();

    return opt;
}


void CGNImpl::add_adep(GraphNode *early, GraphNode *late)
{
    graph.add_edge(early, late);
}


void CGNImpl::start_new_round()
{
    graph.clear_mtime_cache();
    adep_cycle_detection.clear();
    // current_analysis_level = 0;
}


std::shared_ptr<void> CGNImpl::bind_target_builder(
    const std::string &ulabel, std::function<void(CGNTargetOptIn*)> loader
) {
    if (factories.insert({ulabel, loader}).second == false)
        throw std::runtime_error{"Factory exist: " + ulabel};

    //fn remove_target
    return std::shared_ptr<void>(nullptr, 
        [this, ulabel](void*) mutable{ factories.erase(ulabel); }
    );
}

std::pair<std::string, int> CGNImpl::build_target(
    const std::string &label, const Configuration &cfg
) {
    // current_analysis_level = 'b';
    auto rv = analyse_target(label, cfg);
    if (rv.errmsg.size())
        throw std::runtime_error{rv.errmsg};
    
    logger.verbose_paragraph("=================================================================\n");

    //generate compile_commands.json
    std::string compdb = "ninja -f " + obj_main_ninja.string()
                       + " -t compdb > " + cgn_out.string() 
                       + "/obj/compile_commands.json";
    system(compdb.c_str());
    logger.println(label + " analysed", "");
    graph.db_flush();
    
    std::string cmd = "ninja -f " + obj_main_ninja.string() 
                    + " " + Tools::shell_escape(rv.ninja_entry);
    if (logger.is_verbose())
        cmd += " --verbose";
    logger.paragraph(cmd + "\n");
    
    int exitcode = system(cmd.c_str());
    
    if (exitcode == 0 && rv.outputs.size())
        logger.println("Build success: ", rv.outputs[0]);
    return {(rv.outputs.size()?rv.outputs[0]:""), exitcode};
}

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
    scriptcc_debug_mode = cmd_kvargs.count("scriptcc_debug");
    halt_on_error = cmd_kvargs.count("halt_on_error");
    logger.set_verbose(cmd_kvargs.count("verbose"));

    logger.verbose_paragraph("CWD: " + std::filesystem::current_path().string());

    //init path
    std::string dsuffix = (scriptcc_debug_mode? "d":"");
    cgn_out = cmd_kvargs.at("cgn-out");
    cgn_out_unixsep = cgn_out.string();
    #ifdef _WIN32
    std::replace(cgn_out_unixsep.begin(), cgn_out_unixsep.end(), '\\', '/');
    #endif
    analysis_path = cgn_out / ("analysis_" + Tools::get_host_info().os + dsuffix);
    obj_main_ninja = cgn_out / "obj" / "main.ninja";


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

    // run vcvars64.bat if necessary
    #ifdef _WIN32
    if (cmd_kvargs.count("winenv")) {
        auto resp = raymii::Command::exec("\"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat\" 2>&1 >NUL && set");
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
    constexpr std::string_view SUBNINJA{"subninja "};
    std::ifstream fin(obj_main_ninja, std::ios::in);
    if (!fin) //create if not existed
        std::ofstream{obj_main_ninja};
    else {
        bool need_rebuild = false;
        for (std::string ln; !fin.eof() && std::getline(fin, ln);)
            if (ln.size() > SUBNINJA.size()) {
                auto subfile = NinjaFile::parse_ninja_str(
                                ln.substr(SUBNINJA.size()));
                if (std::filesystem::is_regular_file(subfile))
                    main_subninja.insert(subfile);
                else //some .ninja files missing
                    need_rebuild = true;
            }
        if (fin.close(); need_rebuild) {
            std::ofstream fout{obj_main_ninja};
            for (auto ln : main_subninja)
                fout<<"subninja " + NinjaFile::escape_path(ln) + "\n";
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

    //call cgn_setup()
    const std::string cgn_setup_filename = "cgn_setup.cgn.cc";
    if (!std::filesystem::exists(cgn_setup_filename))
        throw std::runtime_error{cgn_setup_filename + " not found"};

    active_script("//" + cgn_setup_filename);
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
        logger.paragraph(x.log_message);
    for (auto &[name, cfg] : x.configs) {
        cfg.visit_all_keys();
        cfg.trim_lock();
        std::string uid = cfg_mgr->commit(cfg);
        cfg_mgr->set_name(name, uid);
    }

    // if (auto fd = cells.find("CELL_SETUP"); fd != cells.end()) {
    //     active_script(fd->second);
    //     cells.erase(fd);

    //     CGNInitSetup x;
    //     cgn_setup(x);
    // }else{
    //     throw std::runtime_error{"CELL-SETUP UNASSIGNED!"};
    //     // active_script("@cgn.d//library/cgn_default_setup.cgn.cc");
    // }
} //CGNImpl::init()

CGNImpl::~CGNImpl()
{
    targets.clear();
    factories.clear();
    cfg_mgr.reset();

    // some deconstructor may exist in side-load dll, so we dlclose() in finally
    scripts.clear();
} //CGNImpl::release()

} //namespace