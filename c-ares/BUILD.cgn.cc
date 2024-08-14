#include <cgn>

git("c-ares.git", x) {
    x.repo = "https://github.com/c-ares/c-ares.git";
    x.commit_id = "5899dea2b1f8e78f311aaed7db98b82b5537c9f9";
    x.dest_dir = "repo";
}

alias("c-ares", x) {
    x.actual_label = ":build_cmake";
}

cmake("build_cmake", x) {
    // x.vars["ENABLE_DEBUG"] = (x.cfg["optimization"] == "debug"?"ON":"OFF");
    // x.vars["CARES_SHARED"] = "TRUE";
    x.sources_dir = "repo";
    if (x.cfg["os"] == "linux") {
        x.outputs = {"bin/adig", "bin/ahost",
            "lib64/libcares.so", "lib64/libcares.so.2", "lib64/libcares.so.2.17.3"};
    }
    if (x.cfg["os"] == "win") {
        x.outputs = {"bin/adig.exe", "bin/ahost.exe", 
                    "bin/cares.dll", "lib/cares.lib"};
    }
}

// Seems depecrated
nmake("build_win_nmake", x) {
    x.makefile = "Makefile.msvc";
    x.cwd = "repo";
    x.install_prefix_varname = "INSTALL_DIR";

    if (x.cfg["optimization"] == "debug") {
        x.override_vars["CFG"] = "dll-debug";
        x.outputs = {"lib/caresd.dll", "lib/caresd.lib", 
                     "lib/caresd.exp", "lib/caresd.pdb"};
    }
    else {
        x.override_vars["CFG"] = "dll-release";
        x.outputs = {"lib/caresd.dll", "lib/caresd.lib", "lib/caresd.exp"};
    }

    if (x.cfg["msvc_runtime"] == "MT" || x.cfg["msvc_runtime"] == "MTd")
        x.override_vars["RTLIBCFG"] = "static";

    if (x.cfg["cpu"] == "x86")
        x.override_vars["MACHINE"] = "x86";
    if (x.cfg["cpu"] == "x86_64")
        x.override_vars["MACHINE"] = "x64";
    
}

// sh_binary("build_win", x) {
//     x.cmd_build = {"cd", "repo/winbuild", "&&",
//         "nmake", "/f", "Makefile.vc", "mode=dll"};
    
//     x.cmd_build += {x.cfg["optimization"] == "debug"?"DEBUG=yes":"DEBUG=no"};
//     if (x.cfg["msvc_runtime"] == "MT" || x.cfg["msvc_runtime"] == "MTd")
//         x.cmd_build += {"RTLIBCFG=static"};

//     if (x.cfg["cpu"] == "x86")
//         x.cmd_build += {"MACHINE=x86"};
//     if (x.cfg["cpu"] == "x86_64")
//         x.cmd_build += {"MACHINE=x64"};
// }
