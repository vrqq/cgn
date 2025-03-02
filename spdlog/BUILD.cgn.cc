#include <cgn>

// Version 1.15.1
git("spdlog.git", x) {
    x.repo = "https://github.com/gabime/spdlog.git";
    x.dest_dir = "repo";
    x.commit_id = "f355b3d58f7067eee1706ff3c801c2361011f3d5";
}

cxx_static("spdlog", x) {
    const static std::string base = "repo";

    x.include_dirs = x.pub.include_dirs = {base + "/include"};
    x.include_dirs += {base + "/src"};

    x.defines = {"SPDLOG_FMT_EXTERNAL", "SPDLOG_COMPILED_LIB"};
    x.pub.defines = {
        "SPDLOG_FMT_EXTERNAL", "SPDLOG_COMPILED_LIB"
    };
    if (x.cfg["optimization"] == "debug")
        x.pub.defines += {"SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE"};
    else
        x.pub.defines += {"SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG"};

    x.srcs = {base + "/src/*.cpp"};
    x.add_dep("@third_party//fmt", cxx::inherit);
}
