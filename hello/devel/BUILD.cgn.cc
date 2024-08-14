#include <cgn>

filegroup("devel", x) {
    x.add("src1", {"legal/*.md", "*.md"}, "");
    x.add("src1", {"**.h"}, "include");
}

filegroup("devel_devtest", x) {
    x.add("zzzz", {"././././."}, "2222");

    x.add("", {"*.file"}, "");

    x.add("", {"legal/*.md"}, ".");

    // include/a : cp src1/*.c
    // include   : find . -name='*.h' | cp --parent -r -t include
    x.add("src1/../src1/./", {"./**.h", "file1_or_folder1", "././a/../*.c", "sub/"}, "include/");
}


// BinDevel from cxx

cxx_static("abc", x) {
    x.pub.include_dirs = {"src2_cxx/hdr"};
    x.srcs = {"src2_cxx/libabc.cpp"};
}

bin_devel("abc.devel", x) {
    x.add_from_target(":abc");
    x.include.push_back({"src2_cxx/hdr2", {"*.h"}});
}
