#include <cgn>

git("sqlite3.git", x) {
    x.repo = "https://github.com/sqlite/sqlite.git";
    x.commit_id = "b74eb00e2cb05d9749859e6fbe77d229ad1dc1e1";
}

url_download("sqlite3.https", x) {
    x.url = "https://www.sqlite.org/2024/sqlite-amalgamation-3470200.zip";
    x.outputs = {"sqlite-amalgamation-3470200.zip"};
}

cxx_shared("sqlite3", x) {
    x.srcs = {"repo/sqlite3.c"};
    x.pub.include_dirs = {"src"};
}
