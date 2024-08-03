#include <cgn>

git("cpp-httplib.git", x) {
    x.repo = "https://github.com/yhirose/cpp-httplib.git";
    x.commit_id = "5c00bbf36ba8ff47b4fb97712fc38cb2884e5b98";
    x.dest_dir = "repo";
}

cxx_sources("cpp-httplib", x) {
    x.pub.include_dirs = {"repo"};
    if (x.cfg["os"] == "mac"){
        x.pub.defines = {"CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN"};
        x.pub.ldflags = {"-framework"};
    }
    x.add_dep("@third_party//openssl", cxx::inherit);
}

cxx_prebuilt("without_ssl", x) {
    x.pub.include_dirs = {"repo"};
}
