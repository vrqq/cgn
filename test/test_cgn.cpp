#include <filesystem>
#include <gtest/gtest.h>
#include "../v1/cgn_api.h"

#ifdef _WIN32
    constexpr static bool is_win = true;
#else
    constexpr static bool is_win = false;
#endif

static std::string atosep(const std::string &in) {
    if constexpr(is_win) {
        std::string rv;
        for (auto ch : in)
            rv.push_back(ch == '/'?'\\':ch);
        return rv;
    }
    return in;
}

TEST(CGNTest, LocalePath)
{
    EXPECT_EQ(api.locale_path("x/y/z"),            atosep("x/y/z"));
    EXPECT_EQ(api.locale_path("././x/y/z"),        atosep("x/y/z"));
    EXPECT_EQ(api.locale_path("././x/././y/./z"),  atosep("x/y/z"));

    EXPECT_EQ(api.locale_path("x"),     atosep("x"));
    EXPECT_EQ(api.locale_path("./x"),   atosep("x"));

    EXPECT_EQ(api.locale_path("././x/."),  atosep("x/"));
    EXPECT_EQ(api.locale_path("././x/./"), atosep("x/"));
    EXPECT_EQ(api.locale_path("././x/"),   atosep("x/"));

    EXPECT_EQ(api.locale_path(""),     "");
    EXPECT_EQ(api.locale_path("."),    "");
    EXPECT_EQ(api.locale_path("./"),   "");
    EXPECT_EQ(api.locale_path(".//."), "");

    if constexpr(is_win) {
        //TBD
        // EXPECT_EQ(api.locale_path("c:"),          "C:\\");

        EXPECT_EQ(api.locale_path("c:\\windows"), "C:\\windows");
        EXPECT_EQ(api.locale_path("c:/windows"),  "C:\\windows");
        EXPECT_EQ(api.locale_path(".\\x\\y\\z"),  "x\\y\\z");
        EXPECT_EQ(api.locale_path("./x/y\\z"),    "x\\y\\z");

        // "/x/y" is illegial in windows
        // EXPECT_EQ(api.locale_path("/x/y\\z"),     "x\\y\\z");
    }
    else {
        // '\' is win-path-sep, here as normal character in unix
        EXPECT_EQ(api.locale_path("c:\\windows"),   "c:\\windows");
        EXPECT_EQ(api.locale_path("./c:\\windows"), "c:\\windows");
        EXPECT_EQ(api.locale_path("./c:/windows"),  "c:/windows");

        EXPECT_EQ(api.locale_path("/x/y/z/"), "/x/y/z/");
        EXPECT_EQ(api.locale_path("/x/../y/.././z"), "/z");
        EXPECT_EQ(api.locale_path("/x/../y/./.././z"), "/z");

        EXPECT_EQ(api.locale_path("/"),     "/");
        EXPECT_EQ(api.locale_path("/."),    "/");
        EXPECT_EQ(api.locale_path("/./"),   "/");
        EXPECT_EQ(api.locale_path("/.///"), "/");

        // "/../" is illegial in linux
        // TODO: behavior with '~'
    }
}


TEST(CGNTest, ManglePath) {
    if constexpr(is_win) {
        EXPECT_EQ(api.mangle_path_to_relative("c:\\windows"),    "AC_3A_\\windows");

        // TODO: bug here
        // EXPECT_EQ(api.mangle_path_to_relative(".\\c:\\windows"), "RC_3A_\\windows");
    }else {
        EXPECT_EQ(api.mangle_path_to_relative("D:\\efg/hij"), "RD_3A_5Cefg_/hij");
        EXPECT_EQ(api.mangle_path_to_relative("/dir/f2"), "Adir_/f2");
        EXPECT_EQ(api.mangle_path_to_relative("/./f2"),   "Af2");
        EXPECT_EQ(api.mangle_path_to_relative("str1"),    "Rstr1");
        EXPECT_EQ(api.mangle_path_to_relative("./name2"), "Rname2");
        EXPECT_EQ(api.mangle_path_to_relative("_/x"),     "R___/x");
        EXPECT_EQ(api.mangle_path_to_relative("../file"), "R.._/file");
        EXPECT_EQ(api.mangle_path_to_relative("A../file"), "RA.._/file");
        EXPECT_EQ(api.mangle_path_to_relative("./././a/b/c"), "Ra_/b_/c");
    }
}

TEST(CGNTest, RebasePath) {
    //rel + rel + rel
    EXPECT_EQ(api.rebase_path("dir1/./a/b/c/d.cpp", "."),     atosep("dir1/a/b/c/d.cpp"));
    EXPECT_EQ(api.rebase_path("d.cpp", ".", "a/b/c"),              atosep("a/b/c/d.cpp"));
    EXPECT_EQ(api.rebase_path("././d.cpp", ".", "a/./././b/./c"),  atosep("a/b/c/d.cpp"));
    
    EXPECT_EQ(api.rebase_path("repo/src/proto/a.proto",   "dir1", "."),   atosep("../repo/src/proto/a.proto"));
    EXPECT_EQ(api.rebase_path("./repo/src/proto/a.proto", "repo", "."),   atosep("src/proto/a.proto"));
    EXPECT_EQ(api.rebase_path("repo/src/proto/a.proto",   "repo", "."),   atosep("src/proto/a.proto"));
    EXPECT_EQ(api.rebase_path("repo/src/proto/a.proto",   "repo", "./"),  atosep("src/proto/a.proto"));

    EXPECT_EQ(api.rebase_path("repo/src/proto/a.proto", "repo/inc/", "."),  atosep("../src/proto/a.proto"));
    EXPECT_EQ(api.rebase_path("repo/src/proto/a.proto", "repo/inc",  "."),  atosep("../src/proto/a.proto"));

    EXPECT_EQ(api.rebase_path("repo/src/proto/a.proto", "repo/inc", "dir1/.."),    atosep("../src/proto/a.proto"));
    EXPECT_EQ(api.rebase_path("repo/src/proto/a.proto", "repo/inc", "dir1/../"),   atosep("../src/proto/a.proto"));
    EXPECT_EQ(api.rebase_path("repo/src/proto/a.proto", "repo/inc", "dir1/../."),  atosep("../src/proto/a.proto"));

    EXPECT_EQ(api.rebase_path("src/proto/a.proto", "repo/src", "repo"),           atosep("proto/a.proto"));
    EXPECT_EQ(api.rebase_path("src/proto/a.proto", "repo/src", "repo/."),         atosep("proto/a.proto"));
    EXPECT_EQ(api.rebase_path("src/proto/a.proto", "./repo/src", "repo"),         atosep("proto/a.proto"));
    EXPECT_EQ(api.rebase_path("./src/proto/a.proto", "repo/src", "repo"),         atosep("proto/a.proto"));
    EXPECT_EQ(api.rebase_path("././src/proto/a.proto", "repo/src", "repo"),       atosep("proto/a.proto"));
    EXPECT_EQ(api.rebase_path("././src/proto/a.proto", "repo/src", "./repo"),     atosep("proto/a.proto"));
    EXPECT_EQ(api.rebase_path("././src/proto/a.proto", "repo/src", "./repo/./"),  atosep("proto/a.proto"));

    EXPECT_EQ(api.rebase_path("txt/a.md", "repo/dir2", "repo/dir1"),    atosep("../dir1/txt/a.md"));
    EXPECT_EQ(api.rebase_path("txt/a.md", "repo/dir2", "repo/dir1/"),   atosep("../dir1/txt/a.md"));
    EXPECT_EQ(api.rebase_path("txt/a.md", "repo/dir2", "./repo/dir1"),  atosep("../dir1/txt/a.md"));

    //rel + empty(to_abs) + (abs or rel)
    // win special : convert disk label to OS canonical (uppercase)
    std::filesystem::path abscwd = std::filesystem::absolute(std::filesystem::current_path());
    EXPECT_EQ(api.rebase_path("c.txt", "", "a/b/"),      (abscwd/"a"/"b"/"c.txt").string());
    EXPECT_EQ(api.rebase_path("c.txt", "", "a/../a/b"),  (abscwd/"a"/"b"/"c.txt").string());
    EXPECT_EQ(api.rebase_path("c.txt", "", "a/../b"),    (abscwd/"b"/"c.txt").string());
    EXPECT_EQ(api.rebase_path("c.txt", "", "."),         (abscwd/"c.txt").string());

    EXPECT_EQ(api.rebase_path("c.txt", "", "a/.."),       (abscwd/"c.txt").string());
    EXPECT_EQ(api.rebase_path("c.txt", "", "./a/.."),     (abscwd/"c.txt").string());
    EXPECT_EQ(api.rebase_path("c.txt", "", "./a/../."),   (abscwd/"c.txt").string());
    EXPECT_EQ(api.rebase_path("c.txt", "", "./a/./.."),   (abscwd/"c.txt").string());

    if constexpr(is_win) {
        EXPECT_EQ(api.rebase_path("./c/d.txt", "", "C:\\dir"),       "C:\\dir\\c\\d.txt");
        EXPECT_EQ(api.rebase_path("./c/./d.txt", "", "c:\\dir"),     "C:\\dir\\c\\d.txt");
        EXPECT_EQ(api.rebase_path(".\\c\\d.txt", "", "C:\\dir"),     "C:\\dir\\c\\d.txt");
        EXPECT_EQ(api.rebase_path(".\\.\\c\\d.txt", "", "c:\\dir"),  "C:\\dir\\c\\d.txt");
    }
    else {
        EXPECT_EQ(api.rebase_path("./c/d.txt", "", "/dir"),  "/dir/c/d.txt");
        EXPECT_EQ(api.rebase_path("./c/d.txt", "", "/dir"),  "/dir/c/d.txt");
    }

    //abs + empty(to_abs) + (abs or rel)
    if constexpr(is_win) {
        EXPECT_EQ(api.rebase_path("c:/./c/d.txt", "", "anydir"),     "C:\\c\\d.txt");
        EXPECT_EQ(api.rebase_path("c:\\.\\c\\d.txt", "", "anydir"),  "C:\\c\\d.txt");
        EXPECT_EQ(api.rebase_path("c:\\.\\c\\d.txt", "", "x:/"),     "C:\\c\\d.txt");
        EXPECT_EQ(api.rebase_path("c:\\.\\c\\d.txt", "", "x:\\xx"),  "C:\\c\\d.txt");
    }
    else {
        EXPECT_EQ(api.rebase_path("/x/y/z.txt", "", "dummy"),   "/x/y/z.txt");
        EXPECT_EQ(api.rebase_path("/x/y/z.txt", "", "/dummy"),  "/x/y/z.txt");
        EXPECT_EQ(api.rebase_path("/x\\y\\z.txt", "", ""),      "/x\\y\\z.txt");
        EXPECT_EQ(api.rebase_path("/x\\y\\z.txt", "", "/dummy"),  "/x\\y\\z.txt");
    }

    //abs + rel/abs + rel/abs
    if constexpr(is_win) {
        EXPECT_EQ(api.rebase_path("c:/./c/d.txt", "C:", "anydir"),       "c\\d.txt");
        EXPECT_EQ(api.rebase_path("c:\\.\\c\\d.txt", "c:", "anydir"),    "c\\d.txt");
        EXPECT_EQ(api.rebase_path("c:\\.\\c\\d.txt", "c:/", "x:/"),      "c\\d.txt");
        EXPECT_EQ(api.rebase_path("c:\\.\\c\\d.txt", "C:\\e", "x:\\xx"),  "..\\c\\d.txt");

        EXPECT_EQ(api.rebase_path("c:/x.txt", "D:", "E:/yy"),    "C:\\x.txt");
        EXPECT_EQ(api.rebase_path("c:/x.txt", "D:\\", "E:/yy"),  "C:\\x.txt");
        EXPECT_EQ(api.rebase_path("c:/x.txt", "D:\\", "E:\\yy"), "C:\\x.txt");
        EXPECT_EQ(api.rebase_path("c:/x.txt", "D:/", ""),        "C:\\x.txt");
        EXPECT_EQ(api.rebase_path("c:/x.txt", "D:\\", ""),       "C:\\x.txt");
    }
    else {
        EXPECT_EQ(api.rebase_path("/x/y/z.txt", "/", "dummy"),       "x/y/z.txt");
        EXPECT_EQ(api.rebase_path("/x/y/z.txt", "/./", "/dummy"),    "x/y/z.txt");
        EXPECT_EQ(api.rebase_path("/x/y/z.txt", "", "/dummy"),       "/x/y/z.txt");
        EXPECT_EQ(api.rebase_path("/x\\y\\z.txt", "/././", "dummy"),  "x\\y\\z.txt");
        EXPECT_EQ(api.rebase_path("/x\\y\\z.txt", "/", ""),           "x\\y\\z.txt");
    }
}

int main (int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}