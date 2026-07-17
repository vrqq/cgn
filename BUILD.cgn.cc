// This folder is not as a part of cgn, it just a test for cgn.d
// For using this test suite, please compile any version of cgn to bootstrap it.
#include <cgn>

const static std::string base = "./";
const static std::string njbase = "./ninjabuild/src/";

cxx_static("cgn_static", x) {
    x.pub.defines = x.defines = {"CGN_EXE_IMPLEMENT"};
    x.pub.ldflags = {"-Wl,--export-dynamic", "-Wl,--rpath=$ORIGIN"};
    x.srcs = {
        base + "pe_loader/msvc_symbol_host.cpp",
        base + "pe_loader/msvc_trampo.cpp",
        base + "pe_loader/pe_file.cpp",
        base + "v1/cgn_api.cpp",
        base + "v1/cgn_impl.cpp",
        base + "v1/cgn_tools.cpp",
        base + "v1/cgn_tools_advcopy.cpp",
        base + "v1/cgn_tools_parentproc.cpp",
        base + "v1/cgn_type.cpp",
        // base + "v1/cli.cpp",
        base + "v1/configuration_mgr.cpp",
        base + "v1/dl_helper.cpp",
        base + "v1/graph.cpp",
        base + "v1/logger.cpp",
        base + "v1/ninja_file.cpp",
        base + "v1/win_exception.cpp",

        base + "v1/ninja_build_implement.cpp",
        // njbase + "util.cc",
        // njbase + "edit_distance.cc",
        // njbase + "clparser.cc",
        // njbase + "depfile_parser.cc",
        // njbase + "metrics.cc",
        // njbase + "line_printer.cc",
        // njbase + "string_piece_util.cc",
        // njbase + "elide_middle.cc",
    };
}

cxx_executable("cgn", x) {
    x.srcs = {base + "v1/cli.cpp", base + "v1/cli_advcopy.cpp", base + "mcp/mcp_server.cpp"};
    if (x.cfg["os"] == "mac")
        x.ldflags = {"-Wl,-undefined,dynamic_lookup"};
    
    x.add_dep(":cgn_static", cxx::private_dep);
}

cxx_executable("advcopy", x) {
    x.defines = {"STANDALONE_ADVCOPY"};
    x.srcs = {base + "v1/cli_advcopy.cpp", base + "v1/cgn_tools_advcopy.cpp"};
}

alias("cgn_host_dbg", x) {
    x.actual_label = ":cgn";

    cgn::Configuration newcfg;
    newcfg["optimization"] = "debug";
    newcfg["cpu"] = x.cfg["cpu"];
    newcfg["os"]  = x.cfg["os"];
    if (x.cfg["os"] == "linux")
        newcfg["cxx_asan"] = newcfg["cxx_ubsan"] = "true";

    x.cfg = newcfg;
}

alias("cgn_host_rel", x) {
    x.actual_label = ":cgn";

    cgn::Configuration newcfg;
    newcfg["optimization"] = "release";
    newcfg["cpu"] = x.cfg["cpu"];
    newcfg["os"]  = x.cfg["os"];
    x.cfg = newcfg;
}

alias("cgn_host", x) {
    x.actual_label = ":cgn";
    x.cfg["optimization"] = "release";
    x.cfg["cpu"] = api.get_host_info().cpu;
    x.cfg["os"]  = api.get_host_info().os;
    // if (x.cfg["CompileCGN"] != "")
    //     x.actual_label = ":cgn";
    // else
    //     x.actual_label = ":copy_cgn_host_tool";
}

// custom_command("update", x) {
//     x.append_cmd({"git", "submodule", "foreach", "git", "pull"});

//     x.append_pushd("@cgn.d");
//     if (x.cfg["host_os"] == "win") {
//         x.append_cmd({"ninja", "-f", "build_msvc.ninja"});
//         x.append_cmd({"ninja", "-f", "build_msvcrel.ninja"});
//     }
//     if (x.cfg["host_os"] == "linux") {
//         x.append_cmd({"ninja", "-f", "build_linux.ninja"});
//         x.append_cmd({"ninja", "-f", "build_linuxrel.ninja"});
//     }
//     x.append_popd();
// }