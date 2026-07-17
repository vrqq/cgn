#include <cgn>

// May 26, 2016. Update CSV version to 5.3.0
git("csv-parser.git", x) {
    x.repo = "https://github.com/vincentlaucsb/csv-parser.git";
    x.commit_id = "32e99be14236b0585f33fb96f37e4fefc363f448";
    x.dest_dir = "repo";
}

#define CSV_SOURCE_DIR "repo/include/internal/"
cxx_sources("csv-parser", x) {
    x.srcs = {
        CSV_SOURCE_DIR "col_names.cpp",
        CSV_SOURCE_DIR "csv_format.cpp",
        CSV_SOURCE_DIR "parser/driver.cpp",
        CSV_SOURCE_DIR "parser/guessing.cpp",
        CSV_SOURCE_DIR "parser/mmap.cpp",
        CSV_SOURCE_DIR "csv_reader.cpp",
        CSV_SOURCE_DIR "csv_reader_iterator.cpp",
        CSV_SOURCE_DIR "csv_row.cpp",
        CSV_SOURCE_DIR "csv_utility.cpp",
    };
    x.pub.include_dirs = {"repo/include"};
}
