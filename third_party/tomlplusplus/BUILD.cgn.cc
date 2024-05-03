#include <cgn>

git("tomlplusplus.git", x) {
    x.repo = "https://github.com/marzer/tomlplusplus.git";
    x.commit_id = "1f7884e59165e517462f922e7b6de131bd9844f3";
    x.dest_dir = "repo";
}

cxx_sources("tomlplusplus", x) {
    x.pub.include_dirs = {"repo/include"};
    if (x.cfg["os"] == "linux")
        x.pub.cflags = {"-Wno-misleading-indentation"};
}
