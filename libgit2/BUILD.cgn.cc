#include "cgn"

git("libgit2.git", x) {
    x.repo = "https://github.com/libgit2/libgit2.git";
    x.commit_id = "338e6fb681369ff0537719095e22ce9dc602dbf0"; // v1.9.0
    x.dest_dir = "repo";
}

cmake("libgit2", x) {
    x.sources_dir = "repo";
    x.outputs = {"bin/git2", "lib64/libgit2.so", "lib64/libgit2.so.1.9", 
                 "lib64/libgit2.so.1.9.0"};
}
