#include <cgn>

// git("sqlite3.git", x) {
//     x.repo = "https://github.com/sqlite/sqlite.git";
//     x.commit_id = "b74eb00e2cb05d9749859e6fbe77d229ad1dc1e1";
// }

cxx_shared("sqlite3", x) {
    x.srcs = {"src/sqlite3.c"};
    x.pub.include_dirs = {"src"};
}
