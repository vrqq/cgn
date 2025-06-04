#include <cgn>

// asio version 1.34.2 released 
// master branch on Apr 11, 2024
git("asio.git", x) {
    x.dest_dir = "repo";
    x.repo = "https://github.com/chriskohlhoff/asio.git";
    x.commit_id = "ed6aa8a13d51dfc6c00ae453fc9fb7df5d6ea963";
}

cxx_static("without_ssl", x) {
    const static std::string base = "repo/asio";

    x.include_dirs = x.pub.include_dirs = {base + "/include"};
    x.defines = x.pub.defines = {
        "ASIO_STANDALONE", "ASIO_SEPARATE_COMPILATION"};

    x.srcs = {base + "/src/asio.cpp"};
}

cxx_static("asio", x) {
    const static std::string base = "repo/asio";

    x.include_dirs = x.pub.include_dirs = {base + "/include"};
    x.defines = x.pub.defines = {"ASIO_STANDALONE", "ASIO_SEPARATE_COMPILATION"};

    x.srcs = {base + "/src/asio.cpp", base + "/src/asio_ssl.cpp"};
    x.add_dep("@third_party//openssl", cxx::inherit);
}
