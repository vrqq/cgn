#include <cgn>

git("googletest.git", x) {
    x.repo = "https://github.com/google/googletest.git";
    x.commit_id = "2954cb8d879886403d55343f941ae7d0216e0f6b";
}


cxx_shared("gtest", x) {
    const static std::string gtest = "src/googletest";

    x.pub.include_dirs = {gtest + "/include"};
    x.pub.defines = {
        "GTEST_LINKED_AS_SHARED_LIBRARY=1", "GTEST_ENABLE_CATCH_EXCEPTIONS_=1"
    };

    x.defines = {"GTEST_CREATE_SHARED_LIBRARY=1"};
    x.include_dirs = {gtest, gtest + "/include"};
    x.srcs = {gtest + "/src/gtest-all.cc"};
}

cxx_shared("gmock", x) {
    const static std::string gmock = "src/googlemock";

    x.pub.include_dirs = {gmock + "/include"};
    x.defines = {"GTEST_CREATE_SHARED_LIBRARY=1"};
    x.include_dirs = {gmock, gmock + "/include"};
    x.srcs = {gmock + "/src/gmock-all.cc"};
    x.add_dep(":gtest", cxx::inherit);
}

// group("googletest", x) {
//     x.add_dep(":gmock");
//     x.add_dep(":gtest");
// }
alias("googletest", x) {
    x.actual_label = ":gmock";
}
