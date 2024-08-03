#include <cgn>

git("tinyxml2.git", x) {
    x.repo = "https://github.com/leethomason/tinyxml2.git";
    x.commit_id = "312a8092245df393db14a0b2427457ed2ba75e1b";
    x.dest_dir = "repo";
}

cxx_sources("tinyxml2", x) {
    x.pub.include_dirs = {"repo"};
    x.srcs = {"repo/tinyxml2.cpp"};
}
