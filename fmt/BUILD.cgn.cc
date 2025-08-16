#include <cgn>

// 11.1.3
git("fmt.git", x) {
    x.repo = "https://github.com/fmtlib/fmt.git";
    x.dest_dir = "repo";
    x.commit_id = "9cf9f38eded63e5e0fb95cd536ba51be601d7fa2";
}

// cxx_prebuilt("header", x) {
//     x.pub.include_dirs = {"repo/include"};
// }
cxx_static("fmt", x) {
    x.include_dirs = x.pub.include_dirs = {"repo/include"};
    x.srcs = {"repo/src/format.cc", "repo/src/os.cc"};
}
