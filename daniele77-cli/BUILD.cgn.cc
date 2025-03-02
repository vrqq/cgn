#include <cgn>

// v2.2 Oct 26, 2024
git("daniele77-cli.git", x) {
    x.repo = "https://github.com/daniele77/cli.git";
    x.commit_id = "80541c47bc4c7a6baa205136ca558860cb5e61af";
    x.dest_dir  = "repo";
}

cxx_sources("daniele77-cli", x) {
    x.pub.include_dirs = {"repo/include"};
}
