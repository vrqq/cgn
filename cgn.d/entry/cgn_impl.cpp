#include <fstream>
#include <sstream>
#include <optional>
#include <filesystem>
#include <cassert>
#include <array>
#include "raymii_command.hpp"
#include "cgn_impl.h"
#include "ninja_file.h"
#include "debug.h"
#include "../pe_loader/pe_file.h"

// header from ninja-build src
#include "../ninjabuild/src/clparser.h"
#include "../ninjabuild/src/depfile_parser.h"

#ifdef _WIN32
// extern void __declspec(selectany) cgn_setup(CGNInitSetup &x);
// CGN_EXPORT void cgn_setup(CGNInitSetup &x);
#else
extern void cgn_setup(CGNInitSetup &x) __attribute__((weak));
#endif

// for only debug purpersal in dev
// void cgn_setup(CGNInitSetup &x) {}

namespace cgn {

std::string self_realpath();

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
    return _expand_cell(in);
}

// NodeName == unique_label
// case1: script loaded && stat(files[]) == Latest
//        return ;
// case2: script loaded && stat(files[]) == Stale
//        unload script => goto case 3
// case3: script not-load && stat(files[]) == Stale
//        rebuild => goto case 4
// case4: script not-load && stat(files[]) == Latest
//        load and return;
const CGNScript &CGNImpl::active_script(const std::string &label)
{
    logger.print(logger.color("ActiveScript ") + label);
    std::string labe2 = _expand_cell(label);
    std::string ulabel = "//" + labe2;
    if (std::string_view{labe2.data(), 3} == "../")
        throw std::runtime_error{"Invalid label " + label};
    
    CGNScript s; //the next value of scripts[label]

    if (auto fd = scripts.find(ulabel); fd != scripts.end()) {
        graph.test_status(fd->second.adep);
        if (fd->second.adep->status == GraphNode::Latest)
            return fd->second; // case1: scripts existed, graph Latest
        
        // case2: script existed, graph Stale. 
        //        erase script and goto case 3
        s.adep   = fd->second.adep;
        s.sofile = fd->second.sofile;
        // offline_script(&(fd->second));
        scripts.erase(fd);
    }
    else
        graph.test_status(s.adep = graph.get_node(ulabel));

    if (s.sofile.empty()) {
        std::filesystem::path fpath{labe2};
        #ifdef _WIN32
            s.sofile = (analysis_path / fpath.parent_path() 
                        / (fpath.stem().string() + ".dll")).string();
        #else
            s.sofile = analysis_path / fpath.parent_path() 
                        / ("lib" + fpath.stem().string() + ".so");
        #endif
    }
    
    // case3: script not loaded, graph Stale
    //        prepare CGNScript and GraphNode fields if necessary,
    //        then build cgn script and goto case 4 if build successful.
    if (s.adep->files.empty() || s.adep->status == GraphNode::Stale) {
        std::filesystem::path fpath{labe2};

        //(re)generate GraphNode.files[]
        // script_srcs: file in fetched bundle or .rsp
        // script_all : script_srcs + header info hint by compiler
        std::vector<std::string> script_srcs = expand_scripts(fpath);
        std::unordered_set<std::string> script_all;
        script_all.insert(script_srcs.begin(), script_srcs.end());

        //start build
        std::string def_var_prefix = mangle_var_prefix(labe2);
        std::string def_ulabel_prefix = "//:";
        if (auto fd = labe2.rfind('/'); fd != labe2.npos)
            def_ulabel_prefix = "\"//" + labe2.substr(0, fd) + ":\"";

        auto cc_end_with = [&](const std::string &want) {
            if (script_cc.size() >= want.size())
                return script_cc.substr(script_cc.size() - want.size()) == want;
            return false;
        };

        bool is_win  = (api.get_host_info().os == "win");
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
        MSVCTrampo  win_trampo;
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
                frsp<< "/c " << it <<" /nologo /showIncludes /Od /Gy "
                    "/DWINVER=0x0A00 /D_WIN32_WINNT=0x0603 /D_AMD64_ "
                    " /DCGN_VAR_PREFIX=" + def_var_prefix +
                    " /D\"CGN_ULABEL_PREFIX=\"" + def_ulabel_prefix + "\"\"" + 
                    " /I. /I" + Tools::shell_escape(cell_lnk_path.string()) +
                    " /utf-8 /EHsc /MD /Fo: " + Tools::shell_escape(outname);
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
                        " -fPIC -fdiagnostics-color=always -std=c++11"
                        " -I. -I " + Tools::shell_escape(cell_lnk_path.string()) + 
                        " -DCGN_VAR_PREFIX=" + Tools::shell_escape(def_var_prefix) +
                        " -DCGN_ULABEL_PREFIX=" + Tools::shell_escape(def_ulabel_prefix) + 
                        " -o " + Tools::shell_escape(outname);
            }
            frsp.close();

            logger.print(logger.color("ScriptCC ") + outname);
            // logger.printer.SetConsoleLocked(true);
            auto build_rv = raymii::Command::exec(
                "\"" + script_cc + "\" @" + rspname
            );
            // logger.printer.SetConsoleLocked(false);
            if (build_rv.exitstatus != 0)
                throw std::runtime_error{outname + " " + build_rv.output};
            
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
            logger.print(logger.color("ScriptCC ") + s.sofile);
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
            win_trampo.make_asmfile(asm_in);

            // compile .asm to .obj
            logger.print(logger.color("ScriptCC(ml64) ") + asm_obj);
            auto build_rv = raymii::Command::exec(
                "\"ml64.exe\" /nologo /c /Cx /Fo " + asm_obj + " " + asm_in
            );
            if (build_rv.exitstatus != 0)
                throw std::runtime_error{asm_obj + " " + build_rv.output};
            linker_in += asm_obj + " ";

            // linking
            std::string dbg_flag = scriptcc_debug_mode?"/DEBUG ":"";
            run_link("/nologo /utf-8 /link /DLL " + dbg_flag + cgnapi_winimp + " /OUT:\"" + s.sofile + "\" /PDB:\"" + s.sofile + ".pdb\"");
            clpar.includes_.insert(script_srcs.begin(), script_srcs.end());
            node_vals.insert(node_vals.end(), clpar.includes_.begin(), 
                             clpar.includes_.end());
        }
        else if (is_unix) {
            std::string dbg_flag = scriptcc_debug_mode?" -g":"";
            // in macos no -fuse-ld=lld supported in XCode
            if (api.get_host_info().os == "mac")
                run_link(dbg_flag + " -fPIC --shared -Wl,-undefined,dynamic_lookup -o " + s.sofile);
            else if (is_clang) //llvm-linker is faster then gnu linker
                run_link(dbg_flag + " -fuse-ld=lld -fPIC --shared -o " + s.sofile);
            else
                run_link(dbg_flag + " -fPIC --shared -o " + s.sofile);
            dfcoll.insert(script_srcs.begin(), script_srcs.end());
            node_vals.insert(node_vals.end(), dfcoll.begin(), dfcoll.end());
        }

        //build successful, update graph and goto case 4
        if (logger.verbose) {
            std::string content;
            for (auto it: node_vals){
                if (!content.empty())
                    content += ", ";
                content += it;
            }
            logger.paragraph("CGNScript "
                + ulabel +" rebuilt with files[]: " + content + "\n");
        }
        graph.set_node_files(s.adep, node_vals);
        graph.clear_file0_mtime_cache(s.adep);
        graph.set_node_status_to_latest(s.adep);
    }

    // case4: script not loaded, graph Latest
    //        load into scripts[]
    s.sohandle = std::make_unique<DLHelper>(s.sofile);
    if (!s.sohandle->valid())
        throw std::runtime_error{"cannot load cgn script."};
    return scripts.emplace(ulabel, std::move(s)).first->second;
} //CGNImpl::active_script()

void CGNImpl::offline_script(const std::string &label)
{
    std::string ulabel = "//" + _expand_cell(label);
    
    // WARN: status只标注文件变动 和当前node对应的 Target/Script 在CGN系统里的状态无关
    //       若文件修改 需要用户单独调用set_node_status_to_unknown() 重置状态
    // e.g.: CGNService后台常驻，不是监听iNode改变而rebuild，而是在每次调用 cgn build 时
    //       as new round to re-analyse all related Nodes
    //       
    // if (auto fd = scripts.find(ulabel); fd != scripts.end())
    //     graph.set_node_status_to_unknown(fd->second.adep);
    
    scripts.erase(ulabel);
}


// cache supported
// label: //hello:world
// case1: target_cache[] existed, graph(target) Latest, last_tgt.no_store == false
//        return cache
// case2: target_cache[] existed, graph(target) Stale or last_tgt.no_store == true
//        delete last target and goto case 3
// case3: target_cache[] not existed
//        re-active(target.cgn_script) and call into interpreter,
//        then graph(target) would be updated to Latest while interpreter()
//
CGNTarget CGNImpl::analyse_target(
    const std::string &label, 
    const Configuration &cfg,
    std::string *adep_test
) {
    std::string labe2 = _expand_cell(label);
    ConfigurationID cfg_id = cfg_mgr->commit(cfg);
    logger.print(logger.color("Analyse ") + label + " #" + cfg_id);
    // logout<<"CGN::analyse_target("<< label <<", "<<cfg_id<<")"<<std::endl;

    //expand short label
    // factory_label: @cell//project:nameA
    // labe2: cell_folder/project:nameA
    // => stem: cell_folder/project ('/' slash only)
    // => facty_name: nameA
    std::string stem, facty_name;
    if (auto fdc = labe2.rfind(':'); fdc != labe2.npos)
        facty_name = labe2.substr(fdc+1), labe2.resize(fdc);
    for (size_t last=0; last<labe2.size();) {
        auto fdbs = labe2.find('/', last);
        if (fdbs == labe2.npos)
            fdbs = labe2.size();
        if (fdbs > last) {
            if (stem.size())
                stem.push_back('/');
            stem += labe2.substr(last, fdbs - last);
        }
        last = fdbs + 1;
    }

    std::filesystem::path escaped_mid, src_dir;
    std::string last_dir;
    for (std::size_t i=0, fd=0; fd<stem.size(); i=fd+1) {
        if (fd = stem.find('/', i); fd == stem.npos)
            fd = stem.size();
        escaped_mid /= (last_dir = stem.substr(i, fd-i)) + "_";
    }
    if (facty_name.empty()){
        if (last_dir.empty())
            throw std::runtime_error{"target factory name must be assgined: "+ stem};
        facty_name = last_dir;
    }
    std::string factory_label = "//" + stem + ":" + facty_name;
    std::string tgt_label = factory_label + "#" + cfg_id;
    auto out_prefix_path = cgn_out / "obj" / escaped_mid / (facty_name + "_" + cfg_id);
    std::string out_prefix = out_prefix_path.string();
    out_prefix.push_back(std::filesystem::path::preferred_separator);
    
    if (adep_cycle_detection.insert(tgt_label).second == false)
        throw std::runtime_error{"analyse: cycle-dependency " + tgt_label};
    auto adep_pop = [&](){ adep_cycle_detection.erase(tgt_label); };

    //special case:
    //  check the stat of target ninja file
    //  return empty if Latest existed, otherwise enter normal analyse
    if (adep_test != nullptr) {
        GraphNode *node = graph.get_node(out_prefix);
        if (node->files.size()) { //if existed in db (generate by previous analyse)
            graph.test_status(node);
            if (node->status == GraphNode::Latest) {
                *adep_test = out_prefix + CGNTargetOpt::BUILD_ENTRY;
                adep_pop();
                return {};
            }
        }
    } //endif (adep_test != nullptr)

    if (auto fd = targets.find(tgt_label); fd != targets.end()) {
        // case 1: cache found and Latest, return directly
        if (fd->second.adep->status == GraphNode::Latest)
            return adep_pop(), fd->second;
        
        // case 2: delete current and goto case 3
        targets.erase(fd);
    }

    // case 3: active_script() and interpreter()

    //prepare CGNTarget return value and CGNTargetOpt interpret parameter
    std::string ninja_path = out_prefix + CGNTargetOpt::BUILD_NINJA;
    CGNTarget rv;
    CGNTargetOpt opt;
    opt.factory_ulabel = factory_label;
    opt.factory_name   = facty_name;
    opt.src_prefix = stem;
    opt.src_prefix.push_back(std::filesystem::path::preferred_separator);
    opt.out_prefix = out_prefix;
    rv.cgn_script = &active_script("//" + stem + "/BUILD.cgn.cc");

    // find factories after acrive_script
    cgn::CGNFactoryLoader fn_loader;
    if (auto fd = factories.find(factory_label); fd != factories.end())
        fn_loader = fd->second;
    else  //return immediately if factory not found
        return adep_pop(), cgn::CGNTarget{};

    rv.adep = opt.adep = graph.get_node(opt.out_prefix);
    graph.remove_inbound_edges(opt.adep);
    graph.set_node_files(opt.adep, {ninja_path});
    graph.add_edge(rv.cgn_script->adep, rv.adep);

    std::unique_ptr<NinjaFile> nj = std::make_unique<NinjaFile>(ninja_path);
    opt.ninja = nj.get();

    //call interpreter()
    //  factory_entry(tgt) : xx_factory(xctx), xx_interpreter(xctx, tgt);
    try {
        rv.infos = fn_loader(cfg, opt);
    }catch(std::runtime_error &e) {
        rv.errmsg = e.what();
        rv.infos.no_store = true;
        return adep_pop(), rv;  //string 'adep_test' also no changed
    }

    // insert into main_subninja if interpreter successed.
    // subninja command enforce '/' path-sep
    std::string ninja_path_unix = ninja_path;
    #ifdef _WIN32
        std::replace(ninja_path_unix.begin(), ninja_path_unix.end(), '\\', '/');
    #endif
    if (main_subninja.insert(ninja_path_unix).second) {
        std::ofstream fout(obj_main_ninja, std::ios::app);
        fout<<"subninja "<<NinjaFile::escape_path(ninja_path_unix)<<"\n";
    }

    // release ninja file handle to write build.ninja down to disk
    // then fstat() could get the right mtime to written down to fileDB
    nj.reset(); 

    //update mtime in fileDB after interpreter returned successful.
    // file[0] : usually 'libSCRIPT.cgn.so' or' build.ninja of target'
    graph.clear_file0_mtime_cache(rv.adep);
    graph.set_node_status_to_latest(opt.adep);

    //special case:
    if (adep_test) {
        assert(opt.adep->status == GraphNode::Latest);
        *adep_test = out_prefix + CGNTargetOpt::BUILD_ENTRY;
    }
    
    if (!rv.infos.no_store)
        targets[tgt_label] = rv;
    return adep_pop(), rv;
} //CGNImpl::analyse()


void CGNImpl::add_adep(GraphNode *early, GraphNode *late)
{
    graph.add_edge(early, late);
}


void CGNImpl::precall_reset()
{
    graph.clear_mtime_cache();
    adep_cycle_detection.clear();
}


std::shared_ptr<void> CGNImpl::bind_target_factory(
    const std::string &ulabel, CGNFactoryLoader loader
) {
    if (factories.insert({ulabel, loader}).second == false)
        throw std::runtime_error{"Factory exist: " + ulabel};

    //fn remove_target
    return std::shared_ptr<void>(nullptr, 
        [this, ulabel](void*) mutable{ factories.erase(ulabel); }
    );
}

void CGNImpl::build_target(
    const std::string &label, const Configuration &cfg
) {
    this->precall_reset();
    std::string ninja_target;
    auto rv = analyse_target(label, cfg, &ninja_target);
    if (rv.errmsg.size())
        throw std::runtime_error{rv.errmsg};
    if (ninja_target.empty())
        throw std::runtime_error{label + " target not found."};
    
    //generate compile_commands.json
    std::string compdb = "ninja -f " + obj_main_ninja.string()
                       + " -t compdb > " + cgn_out.string() 
                       + "/obj/compile_commands.json";
    system(compdb.c_str());
    logger.print(logger.color(label + " analysed", logger.GREEN));
    graph.db_flush_node();
    
    std::string cmd = "ninja -f " + obj_main_ninja.string() 
                    + " " + Tools::shell_escape(ninja_target);
    if (logger.verbose)
        cmd += " --verbose";
    logger.paragraph(cmd + "\n");
    system(cmd.c_str());
}

std::string CGNImpl::_expand_cell(const std::string &ss) const
{
    // ss: @cell//project:nameA
    // => rv: cell_folder/project:nameA
    
    if (ss[0]=='/' && ss[1]=='/')
        return ss.substr(2);
    else if (ss[0] == '@') {
        auto fd = ss.find('/', 1);
        if (fd == ss.npos || ss[fd+1] != '/')
            throw std::runtime_error{"Wrong label: '@cell//' required"};

        std::string cellname = ss.substr(0, fd);
        if (cells.count(cellname))
            return (cell_lnk_path / cellname / ss.substr(fd+2)).string();
        // std::string cellname = ss.substr(1, fd-1);
        // if (auto fd2 = cells.find(cellname); fd2 != cells.end())
        //     return fd2->second + "/" + ss.substr(fd+2);
        throw std::runtime_error{"Wrong label: cell not found: " + cellname};
    }
    else
        throw std::runtime_error{ss + " Invalid label format."};
} //CGNImpl::_expand_cell()


void CGNImpl::init(std::unordered_map<std::string, std::string> cmd_kvargs)
{
    this->cmd_kvargs = cmd_kvargs;

    //init logger system
    // (always true, current in development)
    scriptcc_debug_mode = cmd_kvargs.count("scriptcc_debug");
    // scriptcc_debug_mode = true;
    logger.verbose = cmd_kvargs.count("verbose");
    logger.printer.set_smart_terminal(!logger.verbose);

    if (logger.verbose)
        logger.paragraph("CWD: " + std::filesystem::current_path().string());

    //init path
    std::string dsuffix = (scriptcc_debug_mode? "d":"");
    cgn_out = cmd_kvargs.at("cgn-out");
    analysis_path = cgn_out / ("analysis_" + Tools::get_host_info().os + dsuffix);
    cell_lnk_path = cgn_out / "cell_include";
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
    logger.print("Init configuration manager");
    cfg_mgr = std::make_unique<ConfigurationManager>(
                (cgn_out / "configurations").string());

    //prepare obj-main-ninja (entry of all targets)
    constexpr std::string_view SUBNINJA{"subninja "};
    std::ifstream fin(obj_main_ninja, std::ios::in);
    if (!fin) //create if not existed
        std::ofstream{obj_main_ninja};
    else
        for (std::string ln; !fin.eof() && std::getline(fin, ln);)
            if (ln.size() > SUBNINJA.size()) {
                auto subfile = NinjaFile::parse_ninja_str(
                                ln.substr(SUBNINJA.size()));
                if (std::filesystem::is_regular_file(subfile))
                    main_subninja.insert(subfile);
            }
        
    // graph init (load previous one)
    logger.print("Loading fileDB");
    graph.db_load((analysis_path / ".cgn_deps").string());

    //CGN cell init
    //load cell-path map from .cgn_init
    // cells = Tools::read_kvfile(".cgn_init");

    if (std::filesystem::is_directory(cell_lnk_path) == false)
        throw std::runtime_error{cell_lnk_path.string() + " is not a folder."};
    for (auto it : std::filesystem::directory_iterator(cell_lnk_path)) {
        // name: string like "@base"
        std::string name = it.path().filename().string();
        cells.insert(name);
        if (logger.verbose)
            logger.paragraph("Cell " + name + " detected.");
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
        FnSetup fn_setup = (FnSetup)GlobalSymbol::find("?cgn_setup@@YAXAEAUCGNInitSetup@@@Z");
        if (!fn_setup)
            throw std::runtime_error{"cgn_setup() not found, not exported?"};
        fn_setup(x);
    #endif
    if (x.log_message.size())
        logger.paragraph(x.log_message);
    for (auto &[name, cfg] : x.configs){
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

} //namespace