#include <cgn>

git("cnl.git", x) {
    x.repo = "https://github.com/vrqq/cnl-fixing.git";
    x.commit_id = "731242356be760648d4fde21454867604631be6c";
    x.dest_dir = "repo";
}

cxx_prebuilt("cnl", x) {
    x.pub.include_dirs = {"src/include"};
}