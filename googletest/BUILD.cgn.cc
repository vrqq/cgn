#include <cgn>

void myfunc1(GitFetcher::context_type& x) {}
std::shared_ptr<void> myvar1 = api.bind_target_factory<GitFetcher>(CGN_ULABEL_PREFIX "NAMENAME", &myfunc1);


git("googletest.git", x) {
    x.repo = "https://github.com/google/googletest.git";
    x.commit_id = "2954cb8d879886403d55343f941ae7d0216e0f6b";
    x.dest_dir = "repo";
}

const static std::string gtest = "repo/googletest";
cxx_shared("gtest", x) {
    x.pub.include_dirs = {gtest + "/include"};
    x.pub.defines = {
        "GTEST_LINKED_AS_SHARED_LIBRARY=1", "GTEST_ENABLE_CATCH_EXCEPTIONS_=1"
    };

    x.defines = {"GTEST_CREATE_SHARED_LIBRARY=1"};
    x.include_dirs = {gtest, gtest + "/include"};
    x.srcs = {gtest + "/src/gtest-all.cc"};
}
cxx_sources("gtest_main", x) {
    x.srcs = {gtest + "/src/gtest_main.cc"};
    x.add_dep(":gtest", cxx::inherit);
}

const static std::string gmock = "repo/googlemock";
cxx_shared("gmock", x) {
    x.pub.include_dirs = {gmock + "/include"};
    x.defines = {"GTEST_CREATE_SHARED_LIBRARY=1"};
    x.include_dirs = {gmock, gmock + "/include"};
    x.srcs = {gmock + "/src/gmock-all.cc"};
    x.add_dep(":gtest", cxx::inherit);
}
cxx_sources("gmock_main", x) {
    x.srcs = {gmock + "/src/gmock_main.cc"};
    x.add_dep(":gmock", cxx::inherit);
}

alias("googletest", x) {
    x.actual_label = ":gmock";
}

// gen_bin_devel("devel", x) {
//     auto opt = x.new_collect_opt();
//     opt.copy_from_linknrun = true;
//     opt.copy_from_cxx_include = true;
//     x.collect_from_target(":gtest", x.cfg, opt);
//     x.collect_from_target(":gmock", x.cfg, opt);
// }
