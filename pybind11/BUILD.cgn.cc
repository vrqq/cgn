#include <cgn>

// Version 3.0.4 (Apr 19, 2026)
git("pybind11.git", x) {
    x.repo = "https://github.com/pybind/pybind11.git";
    x.commit_id = "d03662f0984f652b60e7ddce53d3868002275197";
    x.dest_dir = "repo";
}

cxx_prebuilt("pybind11", x) {
    x.pub.include_dirs = {"repo/include", "/usr/include/python3.12"};
}