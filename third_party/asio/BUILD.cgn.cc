#include <cgn>

git("asio.git", x) {
    x.repo = "https://github.com/chriskohlhoff/asio.git";
    x.commit_id = "12e0ce9e0500bf0f247dbd1ae894272656456079";
}

cxx_static("without_ssl", x) {
    const static std::string base = "src";

    x.include_dirs = x.pub.include_dirs = {base + "/include"};
    x.pub.defines = {"ASIO_STANDALONE", "ASIO_SEPARATE_COMPILATION"};

    x.srcs = {base + "/src/asio.cpp"};
}

cxx_static("asio", x) {
    const static std::string base = "src";

    x.include_dirs = x.pub.include_dirs = {base + "/include"};
    x.pub.defines = {"ASIO_STANDALONE", "ASIO_SEPARATE_COMPILATION"};

    x.srcs = {base + "/src/asio_ssl.cpp"};
    x.add_dep("@third_party//openssl", cxx::private_dep);
}
