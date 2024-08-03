#include <cgn>

git("uWebSockets.git", x) {
    x.repo = "https://github.com/uNetworking/uWebSockets.git";
    x.commit_id = "10f73df63cd484a97c0f289a5acd96537f4e3937"; // v20.62.0
    x.dest_dir = "repo";
}

cxx_prebuilt("uWebSockets", x) {
    x.pub.include_dirs = {"repo/src"};
    x.pub.defines = {
        "WITH_LIBUV=1",
        "WITH_OPENSSL=1",
        "WITH_ASIO=1",
    };

    if (x.cfg["os"] == "win")
        x.pub.cflags += {
            "/wd4996"
        };
    
    x.add_dep("@third_party//uSockets");
    x.add_dep("@third_party//openssl");
    x.add_dep("@third_party//zlib");
} //cxx_static("uWebSockets")
