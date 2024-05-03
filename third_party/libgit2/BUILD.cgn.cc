#include "cgn"

cmake("libgit2", x) {
    x.sources_dir = "src";
    x.outputs = {"bin/git2", "lib64/libgit2.so", "lib64/libgit2.so.1.8", 
                 "lib64/libgit2.so.1.8.0"};
}
