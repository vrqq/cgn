#include <cgn>

git("daniele77-cli.git", x) {
    x.repo = "https://github.com/daniele77/cli.git";
    x.commit_id = "60a1f787c65cfe6625132d9584bb6c0bb0008e62";
    x.dest_dir  = "repo";
}

cxx_sources("daniele77-cli", x) {
    x.pub.include_dirs = {"repo/include"};
}
