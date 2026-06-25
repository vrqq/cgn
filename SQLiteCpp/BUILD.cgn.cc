#include <cgn>

git("SQLiteCpp.git", x) {
    x.repo = "https://github.com/SRombauts/SQLiteCpp.git";
    x.commit_id = "d66a92a53dc4c333e8491584a8ca452dc058c977";
}

cxx_static("SQLiteCpp", x) {
    x.srcs = {
        "repo/src/Backup.cpp",
        "repo/src/Column.cpp",
        "repo/src/Database.cpp",
        "repo/src/Exception.cpp",
        "repo/src/Savepoint.cpp",
        "repo/src/Statement.cpp",
        "repo/src/Transaction.cpp"
    };
    x.defines = {
        // "SQLITE_ENABLE_COLUMN_METADATA", 
        // "SQLITE_HAS_CODEC"
    };
    x.include_dirs = x.pub.include_dirs = {"repo/include"};
    x.add_dep("@third_party//sqlite3", cxx::private_dep);
}

