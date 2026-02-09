#include <filesystem>
#include <gtest/gtest.h>
#include "../v1/cgn_api.h"
#include "../library/cxx/cxx.cgn.h"

TEST(CGNTest, CxxAnalysis)
{
    auto cfg = api.query_config("host_release");
    ASSERT_TRUE(cfg.second != nullptr);

    cgn::CGNTarget lib = api.create_target("@cgn.d//test/cxx_test1:func1", cfg.first);
    ASSERT_TRUE(lib.errmsg.empty());

    cgn::CGNTarget exe = api.create_target("@cgn.d//test/cxx_test1", cfg.first);
    ASSERT_TRUE(exe.errmsg.empty());

    auto *lib_cinfo = lib.get<cxx::CxxInfo>(false);
    ASSERT_TRUE(lib_cinfo != nullptr);
    EXPECT_EQ(lib_cinfo->defines.count("HAVE_FUNC1"), 1ul);

    auto *exe_cinfo = exe.get<cxx::CxxInfo>(false);
    ASSERT_TRUE(exe_cinfo != nullptr);    
}

TEST(CGNTest, CxxBuild)
{
    auto host = api.query_config("host_release");
    ASSERT_TRUE(host.second != nullptr);

    cgn::Configuration x86 = host.first;
    x86["target_cpu"] = "x86";
    std::string exe32 = api.build("@cgn.d//test/cxx_test1", x86);
    EXPECT_TRUE(exe32.size());

    cgn::Configuration arm64 = host.first;
    arm64["target_cpu"] = "arm64";
    std::string exe_arm64 = api.build("@cgn.d//test/cxx_test1", arm64);
    EXPECT_TRUE(exe_arm64.size());
}

int main (int argc, char **argv) {
    std::cout<<"Warning! This is an intrusive test, executing it"
               "         will alter files located in ${CWD}/test-cgn-out"<<std::endl;
    std::cout<<"Press any key to continue."<<std::endl;
    getchar();

    std::unordered_map<std::string, std::string> kvargs;
    kvargs["cgn-out"] = "test-cgn-out";
    #ifdef _WIN32
    kvargs["winenv"] = "";
    #endif
    api.init(kvargs);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}