#include <cgn>

git("websocketpp.git", x) {
    x.repo = "https://github.com/zaphoyd/websocketpp.git";
    x.commit_id = "1b11fd301531e6df35a6107c1e8665b1e77a2d8e"; // v0.8.8
    x.dest_dir = "repo";
}

cxx_prebuilt("websocketpp", x) {
    if (x.cfg["os"] == "win") {
        x.pub.defines = {
            "_WEBSOCKETPP_CPP11_FUNCTIONAL_",
            "_WEBSOCKETPP_CPP11_SYSTEM_ERROR_",
            "_WEBSOCKETPP_CPP11_RANDOM_DEVICE_",
            "_WEBSOCKETPP_CPP11_MEMORY_",
            "_WEBSOCKETPP_CPP11_TYPE_TRAITS_",
        };
        x.pub.cflags = {
            "/wd4996",
            "/wd4995",
            "/wd4355",
        };
        x.pub.defines = {"NOMINMAX"};
    }
    if (x.cfg["os"] == "linux")
        x.pub.defines = {
            "_WEBSOCKETPP_CPP11_STL_"
        };
    x.pub.defines += {"ASIO_STANDALONE"};

    x.add_dep("@third_party//openssl");
}
