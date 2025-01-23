#include <cgn>

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

// group("googletest", x) {
//     x.add_dep(":gmock");
//     x.add_dep(":gtest");
// }
alias("googletest", x) {
    x.actual_label = ":gmock";
}

file_utility("devel", x) {
    FileUtility::DevelOpt opt;
    opt.allow_linknrun = true;
    x.collect_devel_on_build(":gtest", opt);
    x.collect_devel_on_build(":gmock", opt);

    x.flat_copy_on_build({
        cgn::make_path_base_script("repo/googletest/include/gtest"),
        cgn::make_path_base_script("repo/googlemock/include/gmock")
    },cgn::make_path_base_out("include"));
}

// bin_devel("devel", x) {
//     x.include = {
//         {gmock + "/include", {"gmock/*.h"}},
//         {gtest + "/include", {"gtest/*.h"}}
//     };
//     x.add_from_target(":googletest", x.allow_linknrun);
// }
