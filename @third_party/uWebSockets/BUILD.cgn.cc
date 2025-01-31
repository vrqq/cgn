#include <cgn>

git("uWebSockets.git", x) {
    x.repo = "https://github.com/uNetworking/uWebSockets.git";
    x.commit_id = "10f73df63cd484a97c0f289a5acd96537f4e3937"; // v20.62.0
    x.dest_dir = "repo";
}

file_utility("copy_include", x) {
    x.copy_on_build({"*"}, cgn::make_path_base_script("repo/src"), cgn::make_path_base_script("repo_include/uWebSockets"));
}

cxx_prebuilt("uWebSockets", x) {
    x.pub.include_dirs = {"repo_include"};
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
    x.add_dep(":copy_include");
} //cxx_prebuilt("uWebSockets")

