#include <cgn>

git("jsoncpp.git", x) {
    x.repo = "https://github.com/open-source-parsers/jsoncpp.git";
    x.commit_id = "89e2973c754a9c02a49974d839779b151e95afd6";
    x.dest_dir = "repo";
}

cxx_static("jsoncpp", x) {
    x.include_dirs = x.pub.include_dirs = {"repo/include"};
    x.defines = {"JSON_USE_EXCEPTION=0"}; // "JSON_HAS_INT64"
    x.srcs = {
        "repo/src/lib_json/json_reader.cpp",
        "repo/src/lib_json/json_tool.h",
        "repo/src/lib_json/json_value.cpp",
        "repo/src/lib_json/json_writer.cpp",
    };

}
