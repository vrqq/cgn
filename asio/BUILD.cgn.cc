#include <cgn>

// master branch on Nov 6, 2024
git("asio.git", x) {
    x.dest_dir = "repo";
    x.repo = "https://github.com/chriskohlhoff/asio.git";
    x.commit_id = "62481a25be6cf78cbe714419a4462fd89bd84ab9";
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
    x.add_dep("@third_party//openssl", cxx::private_dep);
}
