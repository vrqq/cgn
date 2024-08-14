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

// filegroup("devel", x) {
//     x.add("src/googlemock/include/gmock", {"*.h"}, "include/gmock");
//     x.add("src/googletest/include/gtest", {"*.h"}, "include/gtest");

//     auto info = x.add_target_dep(":googletest", x.cfg);
//     x.flat_add_rootbase(info.get<cgn::LinkAndRunInfo>(false)->shared_files, 
//                         (x.cfg["cpu"]=="x86_64"?"lib64":"lib"));
// }
bin_devel("devel", x) {
    x.include = {
        {"src/googlemock/include", {"gmock/*.h"}},
        {"src/googletest/include", {"gtest/*.h"}}
    };
    x.add_from_target(":googletest", x.allow_linknrun);
}
