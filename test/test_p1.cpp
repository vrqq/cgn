// CGN Comprehensive TEST
// $cwd: @cgn.d/test/home
// cgn_setup : $cwd/test_setup.cgn.cc
// cgn_out : $cwd/out
#ifndef GOTO_DIR
    #error "GOTO_DIR should be defined on compile."
#endif

#include <filesystem>
#include <gtest/gtest.h>
#include "../v1/cgn_api.h"
#include "home/@p1/gtest_env.h"

thread_local std::unique_ptr<GTestEnv> gtest_env;

TEST(CGNTest, TLRuntime)
{
}

TEST(CGNTest, CycleDep)
{
}

TEST(CGNTest, CfgTrim)
{
}

int main (int argc, char **argv) {
    // change cwd to test home directory
    std::filesystem::path home_dir = std::filesystem::path(GOTO_DIR);
    std::filesystem::current_path(home_dir);

    // check cwd
    if (!std::filesystem::exists("flag_test_home")) {
        std::cerr<<"Incorrent cwd : " <<std::filesystem::current_path().string()<<std::endl;
        return 2;
    }

    std::cout<<"Current CWD is "<<std::filesystem::current_path()<<std::endl;
    std::cout<<"Warning! This is an intrusive test, executing it\n"
               "         will alter files located in ${CWD}/out\n"
               "-- Press any key to continue --"
               <<std::endl;
    getchar();

    // start test
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}