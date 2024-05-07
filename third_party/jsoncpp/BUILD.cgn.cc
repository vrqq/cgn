#include <cgn>

git("date.git", x) {
    x.repo = "https://github.com/open-source-parsers/jsoncpp.git";
    x.commit_id = "69098a18b9af0c47549d9a271c054d13ca92b006";
    x.dest_dir = "repo";
}

cxx_static("jsoncpp", x) {
    x.defines = {"JSON_USE_EXCEPTION=0", "JSON_HAS_INT64"}
    x.srcs = {
        "repo/src/lib_json/json_reader.cpp",
        "repo/src/lib_json/json_tool.h",
        "repo/src/lib_json/json_value.cpp",
        "repo/src/lib_json/json_writer.cpp",
    };
}