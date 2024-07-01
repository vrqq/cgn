#include <cgn>

git("spdlog.git", x) {
    x.repo = "https://github.com/gabime/spdlog.git";
    x.
    x.commit_id = "27cb4c76708608465c413f6d0e6b8d99a4d84302";
}

cxx_static("spdlog", x) {
    const static std::string base = "src";

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
