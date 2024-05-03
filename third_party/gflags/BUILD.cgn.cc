#include <cgn>

git("gflags.git", x) {
    x.repo = "https://github.com/gflags/gflags.git";
    x.commit_id = "a738fdf9338412f83ab3f26f31ac11ed3f3ec4bd";
}

cmake("gflags", x) {
    x.sources_dir = "src";
    x.outputs = {"lib/libgflags.a", "bin/gflags_completions.sh"};
}

// cxx_static("gflags", x) {
//     x.include_dirs =  x.pub.include_dirs = {"src/include"};
//     x.defines = {
//         "GFLAGS_BAZEL_BUILD", "GFLAGS_INTTYPES_FORMAT_C99", "GFLAGS_IS_A_DLL=0",
//         "HAVE_STDINT_H", "HAVE_SYS_TYPES_H", "HAVE_INTTYPES_H", "HAVE_SYS_STAT_H",
//         "HAVE_STRTOLL", "HAVE_STRTOQ", "HAVE_RWLOCK",
//     };
//     x.srcs = {"/src/src/gflags.cc", "/src/src/gflags_completions.cc", 
//               "/src/src/gflags_reporting.cc"};

//     if (x.cfg["os"] == "win") {
//         x.srcs += {"/src/src/windows_port.cc"};
//         x.defines += {"OS_WINDOWS"};
//     }else
//         x.defines += {"HAVE_UNISTD_H", "HAVE_FNMATCH_H", "HAVE_PTHREAD"};
// }
