#include <cgn>

git("sqlite3.git", x) {
    x.repo = "https://github.com/sqlite/sqlite.git";
    x.commit_id = "b74eb00e2cb05d9749859e6fbe77d229ad1dc1e1";
}

// cxx_static("sqlite3", x) {
//     x.srcs = {"repo/src/*.c"};
//     x.defines = {"SQLITE_ENABLE_JSON1"};
//     x.pub.include_dirs = {"repo/src"};
// }


// url_download("sqlite3.https", x) {
//     x.url = "https://www.sqlite.org/2024/sqlite-amalgamation-3530000.zip";
//     x.outputs = {"sqlite-amalgamation-3530000.zip"};
// }

cxx_static("sqlite3", x) {
    x.srcs = {"sqlite-amalgamation-3530000/sqlite3.c"};
    x.defines = {"SQLITE_ENABLE_JSON1"};
    x.pub.include_dirs = {"sqlite-amalgamation-3530000"};
}

cxx_executable("shell", x) {
    x.srcs = {"sqlite-amalgamation-3530000/shell.c"};
    x.add_dep(":sqlite3", cxx::private_dep);
    x.defines = {"SQLITE_ENABLE_JSON1"};
}