#include <cgn>

git("nlohmann_json.git", x) {
    x.repo = "https://github.com/nlohmann/json.git";
    x.commit_id = "8c391e04fe4195d8be862c97f38cfe10e2a3472e";
    x.dest_dir = "repo";
}

cxx_sources("nlohmann_json", x) {
    x.pub.include_dirs = {"repo/single_include"};
}
