// http://luajit.org
#include <cgn>

static const std::string repo = "repo";
static const char *cxxtool = "@cgn.d//library/cxx.cgn.bundle";

// Jan 1, 2024
git("luajit.git", x) {
    // x.repo = "https://luajit.org/git/luajit.git";
    x.repo = "https://github.com/LuaJIT/LuaJIT.git"; //mirrow
    x.commit_id = "a4f56a459a588ae768801074b46ba0adcfb49eb1";
    x.dest_dir = repo;
}

custom_command("build_luajit", x, "@cgn.d//library/cxx.cgn.bundle")
{
    x.cfg.visit_keys({"os", "cpu", "host_os", "host_cpu"});

    // TODO: add debug (-g) and (-fsanitize) from CxxInterpreter?
    auto cinfo = cxx::CxxInterpreter::test_param(x.cfg, "default");
    
    // define output targets
    x.watch_outputs = {
        cgn::make_path_base_out("install/bin/luajit"),
        cgn::make_path_base_out("install/lib/libluajit-5.1.a"),
        cgn::make_path_base_out("install/lib/libluajit-5.1.so"),
    };

    x.analysis_outputs = {cgn::make_path_base_out("install")};

    if (x.opt_confirm_cached())
        return ;

    // run make on linux and msvcbuild.bat on windows
    std::string insdir = x.rebase_path(cgn::make_path_base_out("install"), "");
    if (x.cfg["host_os"] != "win") {
        std::string srcdir = x.rebase_path({"repo"});
        std::string cflags = api.convert_list_to_string(cinfo.arg.cflags, api.shell_escape);
        std::string ldflags = api.convert_list_to_string(cinfo.arg.ldflags, api.shell_escape);
        x.append_cmd({"make", "-j", 
            "-C", srcdir, 
            "PREFIX=" + insdir, 
            "CC=" + cinfo.exe_cc,
            cflags.empty()? "" : ("CFLAGS=" + cflags),
            ldflags.empty()? "" : ("LDFLAGS=" + ldflags),
            "install"});
        x.append_cmd({"make", "-C", srcdir, "clean"});
    }
    else { //win untested
        x.append_cmd({
            "cmd.exe", "/c", x.rebase_path({"repo/src/msvcbuild.bat"})
        });
    }
}

cxx_prebuilt("luajit", x)
{
    cgn::CGNTarget inst_target = x.add_dep(":build_luajit");
    std::string dir = inst_target.outputs[0];
    x.pub.include_dirs = {cgn::make_path_base_working(dir + "/include")};
    if (x.cfg["os"] == "win") //untested
        x.files = {cgn::make_path_base_working(dir + "/lib/luajit.lib")};
    else
        x.files = {cgn::make_path_base_working(dir + "/lib/luajit-5.1.a")};
}
