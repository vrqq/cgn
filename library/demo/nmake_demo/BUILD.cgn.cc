#include <cgn>

nmake("nmake_demo", x) {
    x.src_base = "repo";
    x.nmake_run_dir = "win32";
    x.makefile = "Makefile";
    x.outputs = {"main.exe", "fn1.dll"};

    x.build_dir_varname = "OUTDIR";
    x.install_prefix_varname = "INSTALLDIR";
}
