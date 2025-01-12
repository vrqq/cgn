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
#include "../v1/raymii_command.hpp"
#include "general.cgn.bundle/bin_devel.cgn.h"
#include "cmake.cgn.h"

// namespace cmake{

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

CMakeContext::CMakeContext(cgn::CGNTargetOptIn *opt)
: name(opt->factory_name), cfg(opt->cfg), opt(opt) {
    auto cinfo = cxx::CxxInterpreter::test_param(cfg);

    // vars["CMAKE_MESSAGE_LOG_LEVEL"] = "ERROR";
    // vars["CMAKE_INSTALL_MESSAGE"] = "NEVER";
    vars["CMAKE_C_COMPILER"]   = cinfo.c_exe;
    vars["CMAKE_CXX_COMPILER"] = cinfo.cxx_exe;
    
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
    // value check
    if (x.outputs.empty()) {
        x.opt->confirm_with_error("output field must be assigned");
        return ;
    }
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // dir for cmake
    std::string build_dir = opt->out_prefix + "build";
    std::string install_dir = opt->out_prefix + "install";
    std::string src_dir = opt->src_prefix + api.locale_path(x.sources_dir);

    // override cmake variable
    x.vars["CMAKE_INSTALL_PREFIX"] = install_dir;
    // vars["CMAKE_INSTALL_BINDIR"] = install_prefix + "bin";
    // vars["CMAKE_INSTALL_LIBDIR"] = install_prefix + "lib";
    // vars["CMAKE_INSTALL_INCLUDEDIR"] = install_prefix + "include";

    std::vector<std::string> cmake_out_njesc;

    // The 'include file' usually not appear in output
    opt->result.ninja_dep_level = cgn::CGNTarget::NINJA_LEVEL_DYNDEP;

    // generate LinkAndRunInfo and BinDevelInfo in return value
    // only <output_dir>/<lib_dir> applied
    auto *bin_devel = opt->result.get<BinDevelInfo>(true);
    bin_devel->base = install_dir;
    bin_devel->include_dir = install_dir + opt->path_separator + "include";

    auto *lrinfo = opt->result.get<cgn::LinkAndRunInfo>(true);
    std::unordered_set<std::string> dllstem, alldirs;
    std::vector<std::pair<std::string,std::string>> dotlib;
    for (auto &file : x.outputs) {
        std::string fullp = api.locale_path(install_dir + "/" + file);

        auto fd1 = file.rfind('/');
        fd1 = (fd1 == file.npos? 0: fd1+1);
        std::string filename = file.substr(fd1);
        std::string ext  = cgn::Tools::get_lowercase_extension(filename);
        std::string stem = filename.substr(0, filename.size() - ext.size());
        if (fd1) { // dir existed
            std::string dir = file.substr(0, fd1-1);
            if (dir == "bin")
                bin_devel->bin_dir = install_dir + opt->path_separator + "bin";
            if (dir == "lib" || dir == "lib64")
                bin_devel->lib_dir = install_dir + opt->path_separator + dir;
        }
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
        cmake_out_njesc.push_back(opt->ninja->escape_path(fullp));
    }
    for (auto item : dotlib)
        if (dllstem.count(item.first) != 0)
            lrinfo->shared_files.push_back(item.second);
        else
            lrinfo->static_files.push_back(item.second);

    // generate CxxInfo in return value
    x.pub.include_dirs.push_back(bin_devel->include_dir);
    opt->result.set(x.pub);

    // prepare cmake gen command
    auto prepare_cmdgen = [&](std::function<std::string(std::string)> fn_escape) {
        std::string cmd = "cmake";
        if (x.cfg["cmake_exe"] != "")
            cmd = fn_escape(x.cfg["cmake_exe"]);
        cmd += " -G Ninja -S " + fn_escape(src_dir)
            + "  -B " + fn_escape(build_dir);
        for (auto item : x.vars) {
            cmd += " -D" + fn_escape(item.first);
            if (item.second.size())
                cmd += "=" + fn_escape(item.second);
        }
        return cmd;
    };

    // [NINJA FILE] cmake_havedep_mode
    if (x.enforce_havedep_mode || opt->quickdep_early_anodes.size()) {
        // rule to run custom command
        auto *rule = opt->ninja->append_rule();
        rule->name = "quick_run";
        rule->command = "${cmd}";
        rule->variables["description"] = "${desc}";

        // target cmake gen
        auto *gen = opt->ninja->append_build();
        gen->rule    = "quick_run";
        gen->inputs  = {opt->ninja->escape_path(api.locale_path(src_dir + "/CMakeLists.txt"))};
        gen->implicit_inputs = opt->ninja->escape_path(opt->quickdep_ninja_full);
        gen->order_only      = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
        gen->outputs = {opt->ninja->escape_path(api.locale_path(build_dir + "/CMakeCache.txt"))};
        gen->variables["cmd"] = prepare_cmdgen(&two_escape) + nul_suffix;
        gen->variables["desc"] = "CMAKE_GEN " + src_dir;

        // target cmake build && install
        std::string logfile_esc = two_escape(opt->out_prefix + ".log");
        auto *build = opt->ninja->append_build();
        build->rule    = "quick_run";
        build->inputs  = gen->outputs;
        build->outputs = cmake_out_njesc;
        #ifdef _WIN32
        build->variables["cmd"] = "cmd.exe /c \"ninja -C " + two_escape(build_dir)
                                + " install\" 1> " + logfile_esc + " 2>&1";
        #else
        build->variables["cmd"] = "ninja -C " + two_escape(build_dir)
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
        auto *efield = opt->ninja->append_build();
        efield->rule = "phony";
        efield->inputs  = build->outputs;
        efield->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
    }
    else { // [NINJA FILE] cmake_nodep_mode below
        x.vars["CMAKE_NINJA_OUTPUT_PATH_PREFIX"] = build_dir;
        auto exe_result = raymii::Command::exec(prepare_cmdgen(&api.shell_escape) + " 2>&1");
        if (exe_result.exitstatus != 0)
            throw std::runtime_error{"cmake gen failure."};

        // Generate ninja entry.
        // the build and install phase has been combined in build.ninja
        opt->ninja->append_include(build_dir + "/build.ninja");

        auto *efield = opt->ninja->append_build();
        efield->rule = "phony";
        efield->inputs  = {opt->ninja->escape_path(build_dir + "/install")};
        efield->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};

        // Generate ninja result-field and return value
        // TODO: fetch by cmake script like "cmake_install.cmake"
        auto *rfield = opt->ninja->append_build();
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
        x.opt->confirm_with_error("output field must be assigned");
        return ;
    }

    auto *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // dir for cmake
    std::string build_dir = opt->out_prefix + "build";
    std::string src_dir = api.locale_path(opt->src_prefix + x.sources_dir);

    // prepare return value
    opt->result.ninja_dep_level = cgn::CGNTarget::NINJA_LEVEL_DYNDEP;

    // std::vector<std::string> dot_cmake_files;
    std::vector<std::string> cmake_out_njesc;
    for (auto &file : x.outputs) {
        std::string fullp = build_dir + opt->path_separator + file;
        opt->result.outputs += {fullp};
        cmake_out_njesc.push_back(opt->ninja->escape_path(fullp));
        // auto ext = cgn::Tools::get_lowercase_extension(file);
        // if (ext == "cmake")
        //     dot_cmake_files.push_back(file);
    }

    // prepare cmake gen command
    std::string cmd = "cmake";
    if (x.cfg["cmake_exe"] != "")
        cmd = two_escape(x.cfg["cmake_exe"]);
    cmd += " -G Ninja -S " + two_escape(src_dir)
        + "  -B " + two_escape(build_dir);
    for (auto item : x.vars) {
        cmd += " -D" + two_escape(item.first);
        if (item.second.size())
            cmd += "=" + two_escape(item.second);
    }

    // rule to run custom command
    std::string rulepath = api.get_filepath("@cgn.d//library/general.cgn.bundle/rule.ninja");
    opt->ninja->append_include(rulepath);

    // target cmake gen
    auto *gen = opt->ninja->append_build();
    gen->rule    = "quick_run";
    gen->inputs  = {opt->ninja->escape_path(api.locale_path(src_dir + "/CMakeLists.txt"))};
    gen->implicit_inputs = opt->ninja->escape_path(opt->quickdep_ninja_full);
    gen->order_only      = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
    gen->outputs = {opt->ninja->escape_path(api.locale_path(build_dir + "/CMakeCache.txt"))};
    gen->variables["cmd"] = cmd + nul_suffix;
    gen->variables["desc"] = "CMAKE_GEN " + src_dir;

    // target .entry
    auto *entry = opt->ninja->append_build();
    entry->rule = "phony";
    entry->inputs  = gen->outputs;
    entry->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};

    // Generate BinDevelInfo
    // BinDevelContext devel_ctx{x.cfg, opt};
    // devel_ctx.lib = {
    //     {build_dir, dot_cmake_files}
    // };
    // auto devel_info = BinDevelCollect::interpret(devel_ctx, opt);
    // rv.set(*devel_info.get<BinDevelInfo>());
} // CMakeConfigInterpeter::interpret()

// } //namespace