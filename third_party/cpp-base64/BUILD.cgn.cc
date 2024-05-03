#include <cgn>

git("cpp-base64.git", x) {
    x.repo = "https://github.com/ReneNyffenegger/cpp-base64.git";
    x.commit_id = "82147d6d89636217b870f54ec07ddd3e544d5f69";
    x.dest_dir = "repo";
}

cxx_sources("cpp-base64", x) {
    x.pub.include_dirs = {"repo"};
    x.srcs = {"repo/base64.cpp"};
}
