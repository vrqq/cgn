#include <fstream>
#include <filesystem>

#include "cgn_impl.h"
#include "raymii_command.hpp"
#include "ninja_file.h"
#include "common.hpp"

bool CGNImpl::load_cgn_script(const std::string &_label)
{
    ScriptLabel t = parse_script_label(_label);
    if (!analytics_file_visited.insert(t.unique_label).second)
        return false;
    
    std::vector<std::string> script_files{t.filepath_in};

    //create .ninja file if not existed.
    // * .cgn.rsp : read content, convert file path
    // * .cgn.cc  : writeen down directly
    if (!std::filesystem::exists(t.ninjapath_mid) || regen_all) {
        NinjaFile fout(t.ninjapath_mid);
        fout.append_variable("builddir", t.ninjapath_mid.parent_path());
        fout.append_include(scriptc_ninja);

        auto *field = fout.append_build();
        field->rule = "cgn_cc";
        field->variables["var_prefix"] = t.def_var_prefix;
        field->variables["ulabel_prefix"] = t.def_ulabel_prefix;
        field->outputs = {t.libpath_out.string()};
        if (t.filepath_in.extension() == ".rsp") {
            std::ifstream fin(t.filepath_in);
            for (std::string ss; !fin.eof() && std::getline(fin, ss);) {
                auto fpath = t.filepath_in.parent_path() / ss;
                script_files.push_back(fpath);
                if (fpath.extension() == ".cpp" || fpath.extension() == ".cc" || fpath.extension() == ".cxx")
                    field->inputs.push_back(fpath);
                else
                    field->implicit_inputs.push_back(fpath);
            }
        }
        else
            field->inputs.push_back(t.filepath_in);
    }

    //run ninja build
    auto exe_result = raymii::Command::exec(
        "ninja -f " + t.ninjapath_mid.string()
    );
    if (exe_result.exitstatus != 0)
        throw std::runtime_error{
            "label:" + _label + " ninjabuild: " + exe_result.output
        };

    //(re)load dll
    if (scripts[t.unique_label].handle) {
        if (exe_result.output == NINJA_NO_WORK_TO_DO)
            return false;
        unload_cgn_script(t.unique_label);
    }
    scripts[t.unique_label].handle = std::make_shared<DLHelper>(t.libpath_out);
    scripts[t.unique_label].files  = script_files;
    // if (*scripts[t.unique_label].handle)
    //     throw std::runtime_error{"Cannot load library: " + t.libpath_out.string()};
    return true;
}

void CGNImpl::unload_cgn_script(const std::string &_label)
{
    ScriptLabel t = parse_script_label(_label);
    if (scripts.erase(t.unique_label) != 1)
        throw std::runtime_error{"Script not loaded " + _label};
}

TargetInfos CGNImpl::analyse(
    const std::string &tf_label, 
    const Configuration &plat_cfg
) {
    TargetOpt t = get_target_dir(tf_label, cfg_mgr->commit(plat_cfg));
    bool enforce_regen = load_cgn_script(t.fin_ulabel);

    auto fd = factories.find(t.factory_ulabel);
    if (fd == factories.end())
        throw std::runtime_error{"analyse: " + tf_label + " not found."};
    auto &node = fd->second;

    enforce_regen |= fd->second.flag_no_cache;
    enforce_regen |= (std::filesystem::exists(t.fout_prefix + t.BUILD_NINJA) == false);
    return fd->second.fn_apply(plat_cfg, t);
}

int CGNImpl::build(
    const std::string &tf_label,
    const Configuration &plat_cfg
) {
    TargetOpt t = get_target_dir(tf_label, cfg_mgr->commit(plat_cfg));

    //test need regeneration
    // if all of the BUILD.cgn.cc unmodified, the target must same with
    // the last time.
    auto result = raymii::Command::exec(
        "ninja -f" + obj_main_ninja + " " 
        + t.fout_prefix + t.ANALYSIS_STAMP + " 2&>1"
    );
    if (result.output != NINJA_NO_WORK_TO_DO)
        analyse(tf_label, plat_cfg);
    
    //All build script up to date, run ninja to build
    std::string cmd = "ninja -f" + obj_main_ninja 
                    + " " + t.fout_prefix + t.BUILD_STAMP;
    return system(cmd.c_str());
}

// void CGNImpl::clean_all()
// {}

std::shared_ptr<void> CGNImpl::auto_target_factory(
    const std::string &label, TargetApplyFunc fn_apply
) {
    //fn add_target
    auto &node = factories[label];
    if (node.fn_apply)
        throw std::runtime_error{"TargetFactory exist: " + label};
    node.fn_apply = std::move(fn_apply);

    //fn remove_target
    return std::shared_ptr<void>(nullptr, 
        [this, label](void*) mutable{ factories.erase(label); }
    );
}

void CGNImpl::prepare_main_ninja()
{
    constexpr std::string_view SUBNINJA{"subninja "};
    std::ifstream fin(obj_main_ninja, std::ios::in);
    if (!fin) {//create if not existed
        std::ofstream{obj_main_ninja}; return;
    }
    for (std::string ln; !fin.eof() && std::getline(fin, ln);)
        if (ln.size() > SUBNINJA.size())
            main_subninja.insert(
                NinjaFile::parse_ninja_str(ln.substr(SUBNINJA.size()))
            );
}

void CGNImpl::register_ninjafile(const std::string &ninja_file_path)
{
    if (main_subninja.insert(ninja_file_path).second) {
        std::ofstream fout(obj_main_ninja, std::ios::app);
        fout<<"subninja "<<NinjaFile::escape_path(ninja_file_path)<<"\n";
    }
}


std::string CGNImpl::_expand_cell(const std::string &ss)
{
    // ss: @cell//project:nameA
    // => rv: cell_folder/project:nameA
    
    if (ss[0]=='/' && ss[1]=='/')
        return ss.substr(2);
    else if (ss[0] == '@') {
        auto fd = ss.find('/', 1);
        if (fd == ss.npos || ss[fd+1] != '/')
            throw std::runtime_error{"Wrong label: '@cell//' required"};

        std::string cellname = ss.substr(1, fd-1);
        if (auto fd2 = cells.find(cellname); fd2 != cells.end())
            return fd2->second + "/" + ss.substr(fd+2);
        throw std::runtime_error{"Wrong label: cell not found: " + cellname};
    }
    else
        throw std::runtime_error{"Invalid label format."};
}

constexpr std::array<bool, 256> gene_chk() {
    std::array<bool, 256> rv{};
    for (bool &bv : rv) bv=0;
    for (unsigned char c='0'; c<='9'; c++) rv[c]=1;
    for (unsigned char c='a'; c<='z'; c++) rv[c]=1;
    for (unsigned char c='A'; c<='Z'; c++) rv[c]=1;
    return rv;
}
constexpr std::array<std::array<char, 3>, 256> gene_rep() {
    std::array<char, 16> hex{
        '0', '1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    std::array<std::array<char, 3>, 256> rv{std::array<char, 3>{0}};
    for (std::size_t i=0; i<256; i++)
        rv[i] = {'_', hex[i/16], hex[i%16]};
    return rv;
}
static void mapchar(std::string &out, unsigned char c) {
    constexpr static std::array<bool, 256> chk = gene_chk();
    constexpr static std::array<std::array<char, 3>, 256> rep = gene_rep();

    if (chk[c])
        out.push_back(c);
    else
        out.append(rep[c].data(), 3);
}

CGNImpl::ScriptLabel CGNImpl::parse_script_label(const std::string &label)
{
    //label: @cell//project/BUILD.cgn.cc
    // => stem: cell_folder/project/BUILD.cgn.cc (on windows is '\\'backslash)
    std::string labe2 = _expand_cell(label);

    ScriptLabel rv;
    rv.unique_label = "//" + labe2;
    rv.filepath_in = std::filesystem::path{labe2}.make_preferred();

    std::filesystem::path adir = rv.filepath_in.parent_path();
    std::string stem_name = rv.filepath_in.stem();
    rv.ninjapath_mid = analysis_path / adir / (stem_name + ".ninja");
    rv.libpath_out   = analysis_path / adir / ("lib" + stem_name + ".so");

    for (char c: labe2)
        mapchar(rv.def_var_prefix, c);

    if (auto fd = labe2.rfind('/'); fd != labe2.npos)
        rv.def_ulabel_prefix = glb.shell_escape("\"//" + labe2.substr(0, fd) + ":\"");
    else
        rv.def_ulabel_prefix = "//:";
    return rv;
}

TargetOpt CGNImpl::get_target_dir(
    const std::string &tflabel, ConfigurationID hash_id
) {
    // tflabel: @cell//project:nameA
    // => stem: cell_folder/project ('/' slash only)
    // => facty_name: nameA
    std::string stem = _expand_cell(tflabel), facty_name;
    if (auto fdc = stem.rfind(':'); fdc != stem.npos)
        facty_name = stem.substr(fdc+1), stem.resize(fdc);

    std::filesystem::path objdir = cgn_out / "obj";
    std::string last_dir;
    for (std::size_t i=0, fd=0; fd<stem.size(); i=fd+1) {
        if (fd = stem.find('/', i); fd == stem.npos)
            fd = stem.size();
        objdir /= (last_dir = stem.substr(i, fd-i)) + "_";
    }

    if (facty_name.empty()){
        if (last_dir.empty())
            throw std::runtime_error{"target factory name must be assgined: "+ stem};
        facty_name = last_dir;
    }

    TargetOpt rv;
    rv.factory_ulabel = "//" + stem + ":" + facty_name;
    rv.fin_ulabel     = "//" + stem + "/BUILD.cgn.cc";
    rv.fout_prefix    = objdir / (facty_name + "_" + hash_id);
    rv.fout_prefix.push_back(std::filesystem::path::preferred_separator);
    return rv;
}

std::vector<std::string> CGNImpl::get_script_filepath(const std::string &label)
{
    std::string filepath = _expand_cell(label);
    if (auto fd = scripts.find("//" + filepath); fd != scripts.end())
        return fd->second.files;
    return {filepath};
}

void CGNImpl::init(std::unordered_map<std::string, std::string> cmd_kvargs)
{
    cgn_out = cmd_kvargs.at("cgn-out");
    analysis_path = cgn_out / "analysis";
    cell_lnk_path = cgn_out / "cell_include";
    scriptc_ninja = analysis_path / "scriptc.cgn.ninja";
    obj_main_ninja = cgn_out / "obj" / "main.ninja";

    if (std::filesystem::exists(cgn_out)) {
        if (!std::filesystem::is_directory(cgn_out))
            throw std::runtime_error{cgn_out.string() + "is not folder."};
    }else
        std::filesystem::create_directories(cgn_out);

    std::filesystem::create_directories(analysis_path);
    std::filesystem::create_directories(cell_lnk_path);
    std::filesystem::create_directories(cgn_out / "obj");

    std::ofstream stamp_file(cgn_out / ".cgn_out_root.stamp");
    stamp_file.close();
    
    if (cmd_kvargs.count("regeneration"))
        regen_all = true;

    cfg_mgr = std::make_unique<ConfigurationManager>(cgn_out / "configurations");
    prepare_scriptc_ninja(cmd_kvargs["scriptc"]);
    prepare_main_ninja();
    cgn_cell_init();
}


void CGNImpl::prepare_scriptc_ninja(std::string cc) {
constexpr static const char txt_unix[] = R"(
rule cgn_cc
    deps = gcc
    depfile = $out.d
    command = $compiler_cc @$out.rsp
    rspfile = $out.rsp
    rspfile_content = -MMD -MF $out.d -fPIC -fdiagnostics-color=always -g -glldb -std=c++11 -I. -I$cell_inc -DCGN_VAR_PREFIX=$var_prefix -DCGN_ULABEL_PREFIX=$ulabel_prefix --shared $in -o $out
)";

//TODO: detect msvc language
constexpr static const char txt_win[] = R"(
msvc_deps_prefix = Note: including file:
rule cgn_cc
    deps = msvc
    command = $compiler_cc /showIncludes /c $in @$out.rsp
    rspfile = $out.rsp
    rspfile_content = /I. /I$cell_inc /DWINVER=0x0A00 /D_WIN32_WINNT=0x0603 /D_CONSOLE /D_AMD64_ /MD /link /DLL /OUT:$out
)";

#ifdef _WIN32
    if (cc.empty())
        cc = "cl.exe";
    const char *txt2 = txt_win;
#else
    if (cc.empty())
        cc = "clang++";
    const char *txt2 = txt_unix;
#endif

    std::ofstream fout(scriptc_ninja);
    fout<<"compiler_cc = " + cc + "\n"
        <<"cell_inc = " + cell_lnk_path.string() + "\n"
        <<txt2<<std::endl;
}


void CGNImpl::cgn_cell_init()
{
    //load cell-path map from .cgn_init
    cells = read_kvfile(".cgn_init");

    //create folder symbolic link
    std::filesystem::remove_all(cell_lnk_path);
    std::filesystem::create_directory(cell_lnk_path);
    for (auto [k, v] : cells) {
        if (k == "CELL_SETUP")
            continue;
        std::filesystem::path out = cell_lnk_path / ("@" + k);
        std::filesystem::create_directory_symlink(std::filesystem::absolute(v), out);
    }

    if (auto fd = cells.find("CELL_SETUP"); fd != cells.end()) {
        load_cgn_script(fd->second);
        cells.erase(fd);

        CGNInitSetup x;
        cgn_setup(x);
        for (auto &[name, cfg] : x.configs){
            std::string uid = cfg_mgr->commit(cfg);
            cfg_mgr->set_name(name, uid);
        }
    }else{
        throw std::runtime_error{"CELL-SETUP UNASSIGNED!"};
        load_cgn_script("@cgn.d//library/setup.cgn.cc");
    }
}
