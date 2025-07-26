#include <cgn>

// chore: release v8.5.0 (#1673)
git("node-addon-api.git", x) {
    x.repo = "https://github.com/nodejs/node-addon-api.git";
    x.commit_id = "6babc960154752f686a7dca8e712991a976a754b";
    x.dest_dir = "node-addon-api";
}

// user who use this header only library should add cpp define below
// NAPI_VERSION = $(node -p "process.env.NAPI_VERSION || process.versions.napi")
//
cxx_prebuilt("node-addon-api", x) {
    x.pub.include_dirs = {"node-addon-api"};
    x.pub.defines = {
        "NAPI_CPP_EXCEPTIONS",
        "NODE_ADDON_API_CPP_EXCEPTIONS_ALL",
        "NODE_ADDON_API_DISABLE_DEPRECATED"
    };
}

cxx_sources("node-gyp-image-redir", x) {
    x.srcs = {"node-gyp-src/win_delay_load_hook.cc"};
    x.defines = {"HOST_BINARY=\\\"node.exe\\\""};
    x.pub.ldflags = {"/DELAYLOAD:node.exe", "delayimp.lib"};
}


// nodejs/node repository
// ----------------------

// 2025-07-15, Version 20.19.4 'Iron' (LTS)
// ABI version 9
git("node20.git", x) {
    x.repo = "https://github.com/nodejs/node.git";
    x.commit_id = "b08ee299b07b6fd351865849949b2871b1020004";
    x.dest_dir = "node20";
}

// 2025-07-15, Version 22.17.1 'Jod' (LTS)
git("node22.git", x) {
    x.repo = "https://github.com/nodejs/node.git";
    x.commit_id = "1073ab06c8f80b6d7c995a1362502767cf498c0b";
    x.dest_dir = "node22";
}

// EXPORT
// ------

group("nodejs.git", x) {
    x.add_deps({":node-addon-api.git", ":node20.git", ":node22.git"});
}

// napi 9 for vanilla node 20
cxx_static("node20_win_export", x) {
    x.perferred_binary_name = "node.lib";
    if (x.cfg["os"] == "win" && x.cfg["cpu"] == "x86_64")
        x.srcs = {"node20-win64-export.def"};
    if (x.cfg["os"] == "win" && x.cfg["cpu"] == "x86")
        x.srcs = {"node20-win86-export.def"};
}
cxx_prebuilt("napi9", x) {
    x.pub.include_dirs = {"node20/src"};
    x.pub.defines = {"NAPI_VERSION=9"};
    x.add_dep(":node-addon-api");
    if (x.cfg["os"] == "win")
        x.add_dep(":node20_win_export");
}

// napi 10, for vanilla node 22
cxx_static("node22_win_export", x) {
    x.perferred_binary_name = "node.lib";
    if (x.cfg["os"] == "win" && x.cfg["cpu"] == "x86_64")
        x.srcs = {"node22-win64-export.def"};
    if (x.cfg["os"] == "win" && x.cfg["cpu"] == "x86")
        x.srcs = {"node22-win86-export.def"};
}
cxx_prebuilt("napi10", x) {
    x.pub.include_dirs = {"node22/src"};
    x.pub.defines = {"NAPI_VERSION=10"};
    x.add_dep(":node-addon-api");
    if (x.cfg["os"] == "win")
        x.add_dep(":node22_win_export");
}

// electron 37.2.4 within electron-node, libuv, v8, and openssl and others bundle inside
// Here we only provide the node_api header and napi header
cxx_static("electron_37.2.4_win_export", x) {
    x.perferred_binary_name = "node.lib";
    if (x.cfg["os"] == "win" && x.cfg["cpu"] == "x86_64")
        x.srcs = {"electron37.2.4-node-win64-export.def"};
    if (x.cfg["os"] == "win" && x.cfg["cpu"] == "x86")
        x.srcs = {"electron37.2.4-node-win86-export.def"};
}
cxx_prebuilt("electron_37.2.4", x) {
    x.pub.defines = {
        "NAPI_VERSION=10",
        // "NODE_GYP_MODULE_NAME=addon",
        // "USING_UV_SHARED=1", 
        // "USING_V8_SHARED=1", "V8_DEPRECATION_WARNINGS=1;",
        // "_FILE_OFFSET_BITS=64","ELECTRON_ENSURE_CONFIG_GYPI",
        // "USING_ELECTRON_CONFIG_GYPI", "V8_COMPRESS_POINTERS", "V8_COMPRESS_POINTERS_IN_SHARED_CAGE", 
        // "V8_31BIT_SMIS_ON_64BIT_ARCH", "V8_ENABLE_SANDBOX", "V8_EXTERNAL_CODE_SPACE",
        // "OPENSSL_NO_PINSHARED","OPENSSL_THREADS","OPENSSL_NO_ASM",
        // "NODE_ADDON_API_CPP_EXCEPTIONS",
        // "BUILDING_NODE_EXTENSION",
    };
    // x.pub.include_dirs = {"electron37.2.4_headers/include/node"};
    x.pub.include_dirs = {"node22/src"};
    x.add_dep(":node-addon-api");
    x.add_dep(":node-gyp-image-redir");
    // x.files = {"electron_node_headers/node2.lib"};
    if (x.cfg["os"] == "win")
        x.add_dep(":electron_37.2.4_win_export");
}
