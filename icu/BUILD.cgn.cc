#include <cgn>

// ICU 78.2 release, Jan 7 2026
git("icu.git", x) {
    x.dest_dir = "repo";
    x.repo = "https://github.com/unicode-org/icu.git";
    x.commit_id = "f1b3db8ecd39d5b3a6eff4d5641b176c7f914dfb";
}

// Reference .github/workflows/icu4c.yml
// For linux   : check usage from ./icu4c/sources/runConfigureICU
// For windows : compile icu4c/source/allinone/allinone.sln
shell_script("icu4c_build", x) {
    if (x.cfg["os"] == "linux") {
        api.active_script("@cgn.d//library/cxx/cxx.cgn.cc");
        auto cxxparam = cxx::CxxInterpreter::test_param(x.cfg, "default");

        // '-fvisibility=hidden' fail the compiler
        // Error:
        //     clang++ -std=c++17 -fvisibility=hidden -fno-common -fcolor-diagnostics -Wreturn-type -I. -fPIC -pthread -g -Wall -Wextra -Wno-unused-parameter -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftemplate-backtrace-limit=0 -fno-limit-debug-info -fstandalone-debug -glldb -fcoverage-mapping -fprofile-instr-generate -ftime-trace -fsanitize=address -W -Wall -pedantic -Wpointer-arith -Wwrite-strings -Wno-long-long  -Qunused-arguments -Wno-parentheses-equality --warn-backrefs   -o ../../bin/makeconv gencnvex.o genmbcs.o makeconv.o ucnvstat.o -L../../lib -licutu -L../../lib -licui18n -L../../lib -licuuc -L../../stubdata -licudata -lpthread -lm  
        //     /usr/bin/ld: ../../bin/makeconv: hidden symbol `atexit' in /usr/lib64/libc_nonshared.a(atexit.oS) is referenced by DSO
        //
        auto remove_flag = [](std::vector<std::string> &ctnr, const std::string pattern) {
            for (auto iter = ctnr.begin(); iter != ctnr.end();)
                if (*iter == pattern)
                    iter = ctnr.erase(iter);
                else
                    iter++;
        };
        remove_flag(cxxparam.c_arg.cflags,   "-fcoverage-mapping");
        remove_flag(cxxparam.cpp_arg.cflags, "-fcoverage-mapping");
        remove_flag(cxxparam.c_arg.cflags,   "-fprofile-instr-generate");
        remove_flag(cxxparam.cpp_arg.cflags, "-fprofile-instr-generate");

        x.worker.append_setenv("CXX", api.shell_escape(cxxparam.exe_cxx, x.cfg["host_shell"]));
        x.worker.append_setenv("CC",  api.shell_escape(cxxparam.exe_cc, x.cfg["host_shell"]));
        x.worker.append_setenv("CFLAGS",api.shell_escape(
            api.convert_list_to_string(cxxparam.c_arg.cflags), 
            x.cfg["host_shell"]));
        x.worker.append_setenv("CXXFLAGS",api.shell_escape(
            api.convert_list_to_string(cxxparam.cpp_arg.cflags), 
            x.cfg["host_shell"]));
        x.worker.append_setenv("LDFLAGS",api.shell_escape(
            api.convert_list_to_string(cxxparam.exe_arg.ldflags), 
            x.cfg["host_shell"]));

        std::vector<std::string> OPTS = {"--enable-static", "--enable-renaming"};
        OPTS += {x.cfg["optimization"] == "release"? "--enable-debug": "--disable-release"};

        if (x.cfg["cxx_asan"] != "" || x.cfg["cxx_lsan"] != "")
            OPTS += {"--enable-tracing"};
        
        // comfirm the out_preifx
        // Out of source compile
        // - build on ${out_prefix}/build
        // - install on ${out_prefix}/install
        cgn::CGNTargetMaker *mk = x.opt->confirm();
        api.mkdir(mk->out_prefix + "build");
        api.mkdir(mk->out_prefix + "install");
        OPTS += {"--prefix=" + api.rebase_path(mk->out_prefix + "install", "")};
        
        // call $configure $OPTS $@
        x.worker.append_pushd(cgn::make_path_base_out("build"), mk);

        std::string configure_bin = api.rebase_path("repo/icu4c/source/configure", mk->out_prefix + "build", mk->src_prefix);
        x.worker.append_cmd(std::vector<std::string>{configure_bin} + OPTS);

        x.worker.append_escaped_cmd("make -j");

        x.worker.append_escaped_cmd("make install");

        x.worker.write_to_file(mk->out_prefix + "run.sh");


        x.extra_watch_files = {
            cgn::make_path_base_script("repo"),
            cgn::make_path_base_script("repo/icu4c/source/configure")
        };
        x.script_outputs = {
            cgn::make_path_base_out("./install/lib/libicuuc.a"), 
            cgn::make_path_base_out("./install/lib/libicuuc.so.78.2"), 
            cgn::make_path_base_out("./install/lib/libicui18n.a"), 
            cgn::make_path_base_out("./install/lib/libicui18n.so.78.2"), 
            cgn::make_path_base_out("./install/lib/libicuio.a"), 
            cgn::make_path_base_out("./install/lib/libicuio.so.78.2"), 
            cgn::make_path_base_out("./install/lib/libicutu.a"), 
            cgn::make_path_base_out("./install/lib/libicutu.so.78.2"), 
            cgn::make_path_base_out("./install/lib/libicutest.a"), 
            cgn::make_path_base_out("./install/lib/libicutest.so.78.2"), 
            cgn::make_path_base_out("./install/lib/libicudata.so.78.2"), 
            cgn::make_path_base_out("./install/lib/libicudata.a"), 
            cgn::make_path_base_out("./install/share/icu/78.2/mkinstalldirs"), 
            cgn::make_path_base_out("./install/share/icu/78.2/install-sh"), 
            cgn::make_path_base_out("./install/bin/makeconv"), 
            cgn::make_path_base_out("./install/bin/genrb"), 
            cgn::make_path_base_out("./install/bin/derb"), 
            cgn::make_path_base_out("./install/bin/genbrk"), 
            cgn::make_path_base_out("./install/bin/gencnval"), 
            cgn::make_path_base_out("./install/bin/icuinfo"), 
            cgn::make_path_base_out("./install/bin/pkgdata"), 
            cgn::make_path_base_out("./install/bin/gencfu"), 
            cgn::make_path_base_out("./install/bin/gendict"), 
            cgn::make_path_base_out("./install/bin/icuexportdata"), 
            cgn::make_path_base_out("./install/bin/uconv"), 
            cgn::make_path_base_out("./install/bin/icu-config"), 
            cgn::make_path_base_out("./install/sbin/gensprep"), 
            cgn::make_path_base_out("./install/sbin/genccode"), 
            cgn::make_path_base_out("./install/sbin/gencmn"), 
            cgn::make_path_base_out("./install/sbin/icupkg"), 
            cgn::make_path_base_out("./install/sbin/gennorm2"), 
            cgn::make_path_base_out("./install/sbin/escapesrc")
        };
        x.analysis_outputs = {cgn::make_path_base_out("install")};
    }
}


cxx_prebuilt("icu4c_static", x) {
    auto tgt = x.add_dep(":icu4c_build");
    std::string instdir = tgt.outputs[0];
    x.pub.include_dirs = {
        cgn::make_path_base_working(instdir + "/include")
    };
    x.files = {
        cgn::make_path_base_working(instdir + "/lib/libicuuc.a"),
        cgn::make_path_base_working(instdir + "/lib/libicui18n.a"),
        cgn::make_path_base_working(instdir + "/lib/libicuio.a"),
        cgn::make_path_base_working(instdir + "/lib/libicutu.a"),
        cgn::make_path_base_working(instdir + "/lib/libicudata.a"),
    };
}

// The library is too large so we have to use dynamic library here
cxx_prebuilt("icu4c_shared", x) {
    auto tgt = x.add_dep(":icu4c_build");
    std::string instdir = tgt.outputs[0];
    x.pub.include_dirs = {
        cgn::make_path_base_working(instdir + "/include")
    };
    x.files = {
        cgn::make_path_base_working(instdir + "/lib/libicuuc.so.78.2"),
        cgn::make_path_base_working(instdir + "/lib/libicui18n.so.78.2"),
        cgn::make_path_base_working(instdir + "/lib/libicuio.so.78.2"),
        cgn::make_path_base_working(instdir + "/lib/libicutu.so.78.2"),
        cgn::make_path_base_working(instdir + "/lib/libicudata.so.78.2"),
    };
}

alias("icu", x) {
    x.actual_label = ":icu4c_shared";
}