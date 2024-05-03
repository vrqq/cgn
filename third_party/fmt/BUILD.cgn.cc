#include <cgn>

git("fmt.git", x) {
    x.repo = "https://github.com/fmtlib/fmt.git";
    x.commit_id = "e69e5f977d458f2650bb346dadf2ad30c5320281";
}

cxx_static("fmt", x) {
    x.include_dirs = x.pub.include_dirs = {"src/include"};
    x.srcs = {"src/src/format.cc", "src/src/os.cc"};
}
