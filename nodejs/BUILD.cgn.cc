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

cxx_prebuilt("napi9", x) {
    x.pub.include_dirs = {"node20/src"};
    x.pub.defines = {"NAPI_VERSION=9"};
    x.add_dep(":node-addon-api");
}

cxx_prebuilt("napi10", x) {
    x.pub.include_dirs = {"node22/src"};
    x.pub.defines = {"NAPI_VERSION=10"};
    x.add_dep(":node-addon-api");
}
