#include <cgn>

git("uSockets.git", x) {
    x.repo = "https://github.com/uNetworking/uSockets.git";
    x.commit_id = "833497e8e0988f7fd8d33cd4f6f36056c68d225d"; // v0.8.8
    x.dest_dir = "repo";
}

cxx_static("uSockets", x) {
    x.pub.include_dirs = x.include_dirs = {"repo/src"};
    x.defines = {
        "WITH_LIBUV=1",
        "LIBUS_USE_OPENSSL"
    };
    x.srcs = {
        "repo/src/internal/networking/bsd.h",
        "repo/src/bsd.c",
        "repo/src/context.c",
        "repo/src/libusockets.h",
        "repo/src/loop.c",
        "repo/src/socket.c",

        "repo/src/crypto/openssl.c",
        "repo/src/crypto/sni_tree.cpp",

        "repo/src/internal/internal.h",
        "repo/src/internal/loop_data.h",
        "repo/src/eventing/libuv.c"
    };

    if (x.cfg["os"] == "win")
        x.cflags += {
            "/wd4703",
            "/wd4996"
        };
    
    if (x.cfg["os"] == "linux") {
        x.cflags += {"-Wno-sign-compare"};
        x.srcs += {
            "repo/src/eventing/epoll_kqueue.c",
            "repo/src/eventing/gcd.c",
        };
    }

    x.add_dep("@third_party//libuv", cxx::private_dep);
    x.add_dep("@third_party//openssl", cxx::private_dep);
} //cxx_static("uSockets")
