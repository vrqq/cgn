// [Case] nodep mode
// IF current cmake don't have any dependent in cgn build system
// it can use 'cmake_nodep_mode' to migrate cmake output into cgn ninja file
//   https://cmake.org/cmake/help/v3.13/generator/Ninja.html
//      CMake would generate target 'all' in ninja
//      so 'cmake -G ninja' would run in analyse phase
//   https://cmake-developers.cmake.narkive.com/AhjJ4WsA/using-cmake-generated-ninja-file-as-a-subninja-file
//      using -DCMAKE_NINJA_OUTPUT_PATH_PREFIX to assign path in output ninja
//
// [Case] normal mode
// run cmake -B -S in ninja command then run cmake install to deploy
// 
#define CMAKE_CGN_IMPL
#include "../../v1/raymii_command.hpp"
#include "../utility/bin_devel_info.cgn.h"
#include "cmake.cgn.h"

// namespace cmake{

static std::string two_escape(const std::string &in, const std::string &escape_type) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in, escape_type));
}

CMakeContext::CMakeContext(cgn::CGNTargetOpt *opt)
: cgn::QuickDepContext{opt}, name(opt->name), cfg(opt->cfg) {
    cxx::CxxToolchainInfo cinfo = cxx::CxxInterpreter::test_param(opt->cfg);
    cc_env_loader      = cinfo.env_loader_script;
    // cc_env_loader_adep = cinfo.env_loader_script_anode;

    // vars["CMAKE_MESSAGE_LOG_LEVEL"] = "ERROR";
    // vars["CMAKE_INSTALL_MESSAGE"] = "NEVER";
    vars["CMAKE_C_COMPILER"]   = cinfo.exe_cc;
    vars["CMAKE_CXX_COMPILER"] = cinfo.exe_cxx;
    
    if (cfg["optimization"] == "debug")
        vars["CMAKE_BUILD_TYPE"] = "DEBUG";
    if (cfg["optimization"] == "release")
        vars["CMAKE_BUILD_TYPE"] = "RELEASE";

    if (cfg["msvc_runtime"] == "MD")
        vars["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreadedDLL";
    else if (cfg["msvc_runtime"] == "MDd")
        vars["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreadedDebugDLL";
    else if (cfg["msvc_runtime"] == "MT")
        vars["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded";
    else if (cfg["msvc_runtime"] == "MTd")
        vars["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreadedDebug";
    
    if (cfg["sysroot"] != "")
        vars["CMAKE_SYSROOT"] = cfg["sysroot"];

    std::string sans;
    auto append_san = [&](const char *cfgname, const std::string &ss) {
        if (opt->cfg[cfgname] != "")
            sans += (sans.size()? ",": "") + ss;
    };
    append_san("cxx_asan", "address");
    append_san("cxx_msan", "memory");
    append_san("cxx_lsan", "leak");
    append_san("cxx_tsan", "thread");
    append_san("cxx_ubsan", "undefined");
    if (sans.size()) {
        auto append_var = [](std::string &out, std::string ss) {
            out += (out.size()?" ":"") + ss;
        };
        append_var(vars["CMAKE_CXX_FLAGS"], "-fsanitize=" + sans);
        append_var(vars["CMAKE_C_FLAGS"],   "-fsanitize=" + sans);
        append_var(vars["CMAKE_SHARED_LINKER_FLAGS"], "-fsanitize=" + sans);
    }
    
    auto host = api.get_host_info();
    if (cfg["cpu"] != host.cpu || cfg["os"] != host.os) {
        std::string cpu1 = cfg["cpu"], os1 = cfg["os"];
        if (cpu1 == "x86_64")
            cpu1 = "amd64";
        if (os1 == "win")
            os1 = "WindowsStore";
        else if (os1 == "linux")
            os1 = "Linux";
        vars["CMAKE_C_COMPILER_TARGET"] = vars["CMAKE_CXX_COMPILER_TARGET"]
            = cpu1 + "-" + os1 + "-gnu";    
    }
} //CMakeContext


#ifdef _WIN32
constexpr const char *nul_suffix = " 1> nul";
#else
constexpr const char *nul_suffix = " 1> /dev/null";
#endif

void CMakeInterpreter::interpret(context_type &x)
{
    std::string cfg_host_shell = x.cfg["host_shell"];

    // value check
    if (x.outputs.empty()) {
        x.opt->set_fail("output field must be assigned");
        return ;
    }
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;

    // add cc_env_adep from cinfo
    // if (x.cc_env_loader_adep)
    //     api.add_adep_edge(x.cc_env_loader_adep, mk->anode);

    // dir for cmake
    std::string build_dir = mk->out_prefix + "build";
    std::string install_dir = mk->out_prefix + "install";
    std::string src_dir = mk->src_prefix + api.locale_path(x.sources_dir);

    // override cmake variable
    x.vars["CMAKE_INSTALL_PREFIX"] = install_dir;
    // vars["CMAKE_INSTALL_BINDIR"] = install_prefix + "bin";
    // vars["CMAKE_INSTALL_LIBDIR"] = install_prefix + "lib";
    // vars["CMAKE_INSTALL_INCLUDEDIR"] = install_prefix + "include";

    std::vector<std::string> cmake_out_njesc;

    // The 'include file' usually not appear in output
    // opt->result.ninja_dep_level = cgn::CGNTarget::NINJA_LEVEL_DYNDEP;

    // generate LinkAndRunInfo and BinDevelInfo in return value
    // only <output_dir>/<lib_dir> applied
    auto *bin_devel = mk->get<BinDevelInfo>(true);
    bin_devel->install_dir = install_dir;
    // bin_devel->include_dir = install_dir + opt->path_separator + "include";

    auto *lrinfo = mk->get<cgn::LinkAndRunInfo>(true);
    std::unordered_set<std::string> dllstem, alldirs;
    std::vector<std::pair<std::string,std::string>> dotlib;
    for (auto &file : x.outputs) {
        std::string fullp = api.locale_path(install_dir + "/" + file);

        auto fd1 = file.rfind('/');
        fd1 = (fd1 == file.npos? 0: fd1+1);
        std::string filename = file.substr(fd1);
        std::string ext  = cgn::Tools::get_lowercase_extension(filename);
        std::string stem = filename.substr(0, filename.size() - ext.size());
        // if (fd1) { // dir existed
        //     std::string dir = file.substr(0, fd1-1);
        //     if (dir == "bin")
        //         bin_devel->bin_dir = install_dir + opt->path_separator + "bin";
        //     if (dir == "lib" || dir == "lib64")
        //         bin_devel->lib_dir = install_dir + opt->path_separator + dir;
        // }
        if (ext == "a")
            lrinfo->static_files.push_back(fullp);
        else if (ext == "so" || filename.find(".so.") != filename.npos)
            lrinfo->shared_files.push_back(fullp); // .so.2.17
        else if (ext == "dll") {
            lrinfo->runtime_files[filename] = fullp;
            dllstem.insert(stem);
        }
        else if (ext == "lib")
            dotlib.push_back({stem, fullp});
        cmake_out_njesc.push_back(cgn::NinjaFile::escape_path(fullp));
    }
    for (auto item : dotlib)
        if (dllstem.count(item.first) != 0)
            lrinfo->shared_files.push_back(item.second);
        else
            lrinfo->static_files.push_back(item.second);

    // generate CxxInfo in return value
    for (auto &dir : x.pub.include_dirs)
        dir = cgn::make_path_base_working(api.rebase_path(dir, ".", mk));
    x.pub.include_dirs.push_back(cgn::make_path_base_working(install_dir + mk->PATH_SEPARATOR + "include"));
    mk->set(x.pub);

    // prepare cmake gen command
    auto prepare_cmdgen = [&](std::function<std::string(const std::string&, const std::string&)> fn_escape) {
        std::string cmd = "cmake";
        if (x.cfg["cmake_exe"] != "")
            cmd = fn_escape(x.cfg["cmake_exe"], x.cfg["host_shell"]);
        if (x.cc_env_loader.size())
            cmd = x.cc_env_loader + " && " + cmd;
        
        cmd += " -G Ninja -S " + fn_escape(src_dir, x.cfg["host_shell"])
            + "  -B " + fn_escape(build_dir, x.cfg["host_shell"]);
        for (auto item : x.vars) {
            cmd += " -D" + fn_escape(item.first, x.cfg["host_shell"]);
            if (item.second.size())
                cmd += "=" + fn_escape(item.second, x.cfg["host_shell"]);
        }
        return cmd;
    };

    if (mk->ninja == nullptr)
        return ;

    // [NINJA FILE] cmake_havedep_mode
    if (x.enforce_havedep_mode) {
        // rule to run custom command
        auto *rule = mk->ninja->append_rule();
        rule->name = "quick_run";
        rule->command = "${cmd}";
        rule->variables["description"] = "${desc}";

        // target cmake gen
        auto *gen = mk->ninja->append_build();
        gen->rule    = "quick_run";
        gen->inputs  = {cgn::NinjaFile::escape_path(api.locale_path(src_dir + "/CMakeLists.txt"))};
        // gen->implicit_inputs = cgn::NinjaFile::escape_path(opt->quickdep_ninja_full);
        gen->order_only      = cgn::NinjaFile::escape_path(x.quickdep_ninja_target);
        gen->outputs = {cgn::NinjaFile::escape_path(api.locale_path(build_dir + "/CMakeCache.txt"))};
        gen->variables["cmd"] = prepare_cmdgen(&two_escape) + nul_suffix;
        gen->variables["desc"] = "CMAKE_GEN " + src_dir;

        // target cmake build && install
        std::string logfile_esc = two_escape(mk->out_prefix + ".log", x.cfg["host_shell"]);
        auto *build = mk->ninja->append_build();
        build->rule    = "quick_run";
        build->inputs  = gen->outputs;
        build->outputs = cmake_out_njesc;
        #ifdef _WIN32
        build->variables["cmd"] = "cmd.exe /c \"ninja -C " + two_escape(build_dir)
                                + " install\" 1> " + logfile_esc + " 2>&1";
        #else
        build->variables["cmd"] = "ninja -C " + two_escape(build_dir, x.cfg["host_shell"])
                                + " install 1> " + logfile_esc + " 2>&1";
        #endif
        
        // build->variables["cmd"] = "cmake --build " + two_escape(build_dir)
        //                       + " && " + "cmake --install " 
        //                       + two_escape(build_dir)
        //                       + " 1> /dev/null 2>&1 ";
        //                       + " 2>&1 > " + opt.out_prefix + "build.log";
        //
        // The CMakeCache.txt may regenerated by config update but the binary
        // file may cached in previous build, so "restat=1" here.
        build->variables["desc"] = "CMAKE_BUILD " + src_dir;
        build->variables["restat"] = "1";

        // target .entry
        auto *efield = mk->ninja->append_build();
        efield->rule = "phony";
        efield->inputs  = build->outputs;
        efield->outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry)};
    }
    else { // [NINJA FILE] cmake_nodep_mode below
        x.vars["CMAKE_NINJA_OUTPUT_PATH_PREFIX"] = build_dir;
        auto exe_result = raymii::Command::exec(prepare_cmdgen(&api.shell_escape) + " 2>&1");
        if (exe_result.exitstatus != 0)
            throw std::runtime_error{"cmake gen failure."};

        // Generate ninja entry.
        // the build and install phase has been combined in build.ninja
        mk->ninja->append_include(build_dir + "/build.ninja");

        auto *efield = mk->ninja->append_build();
        efield->rule = "phony";
        efield->inputs  = {cgn::NinjaFile::escape_path(build_dir + "/install")};
        efield->outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry)};

        // Generate ninja result-field and return value
        // TODO: fetch by cmake script like "cmake_install.cmake"
        auto *rfield = mk->ninja->append_build();
        rfield->rule = "phony";
        rfield->inputs  = efield->inputs;
        rfield->outputs = cmake_out_njesc;
    }

} //CMakeInterpreter::interpret()

void CMakeConfigInterpeter::interpret(
    CMakeConfigInterpeter::context_type &x
) {
    // value check
    if (x.outputs.empty()) {
        x.opt->set_fail("output field must be assigned");
        return ;
    }

    x.cfg.visit_keys({"host_shell", "cmake_exe"});
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;
    mk->merge_from(x.quickdep_result);

    // dir for cmake
    std::string build_dir = mk->out_prefix + "build";
    std::string src_dir = api.locale_path(mk->src_prefix + x.sources_dir);

    // prepare return value
    // opt->result.ninja_dep_level = cgn::CGNTarget::NINJA_LEVEL_DYNDEP;

    // std::vector<std::string> dot_cmake_files;
    std::vector<std::string> cmake_out_njesc;
    for (auto &file : x.outputs) {
        std::string fullp = build_dir + mk->PATH_SEPARATOR + file;
        mk->outputs += {fullp};
        cmake_out_njesc.push_back(cgn::NinjaFile::escape_path(fullp));
        // auto ext = cgn::Tools::get_lowercase_extension(file);
        // if (ext == "cmake")
        //     dot_cmake_files.push_back(file);
    }

    // prepare cmake gen command
    std::string cmd = "cmake";
    if (x.cfg["cmake_exe"] != "")
        cmd = two_escape(x.cfg["cmake_exe"], x.cfg["host_shell"]);
    cmd += " -G Ninja -S " + two_escape(src_dir, x.cfg["host_shell"])
        + "  -B " + two_escape(build_dir, x.cfg["host_shell"]);
    for (auto item : x.vars) {
        cmd += " -D" + two_escape(item.first, x.cfg["host_shell"]);
        if (item.second.size())
            cmd += "=" + two_escape(item.second, x.cfg["host_shell"]);
    }

    // rule to run custom command (require 'quick_run' rule)
    std::string rulepath = api.get_filepath("@cgn.d//library/utility/quick_run.ninja");
    mk->ninja->append_include(rulepath);

    // target cmake gen
    auto *gen = mk->ninja->append_build();
    gen->rule    = "quick_run";
    gen->inputs  = {cgn::NinjaFile::escape_path(api.locale_path(src_dir + "/CMakeLists.txt"))};
    // gen->implicit_inputs = cgn::NinjaFile::escape_path(opt->quickdep_ninja_full);
    gen->order_only      = cgn::NinjaFile::escape_path(x.quickdep_ninja_target);
    gen->outputs = {cgn::NinjaFile::escape_path(api.locale_path(build_dir + "/CMakeCache.txt"))};
    gen->variables["cmd"] = cmd + nul_suffix;
    gen->variables["desc"] = "CMAKE_GEN " + src_dir;

    // target .entry
    auto *entry = mk->ninja->append_build();
    entry->rule = "phony";
    entry->inputs  = gen->outputs;
    entry->outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry)};

    // Generate BinDevelInfo
    // BinDevelContext devel_ctx{x.cfg, opt};
    // devel_ctx.lib = {
    //     {build_dir, dot_cmake_files}
    // };
    // auto devel_info = BinDevelCollect::interpret(devel_ctx, opt);
    // rv.set(*devel_info.get<BinDevelInfo>());
} // CMakeConfigInterpeter::interpret()

// } //namespace