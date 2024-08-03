#include <cgn>

git("date.git", x) {
    x.repo = "https://github.com/HowardHinnant/date.git";
    x.commit_id = "8f8336f42be73f4eefb042c3807a5b4399422fed";
    x.dest_dir = "repo";
}

cxx_sources("date", x) {
    x.pub.include_dirs = x.include_dirs = {"repo/include"};
    x.pub.defines = {"AUTO_DOWNLOAD=0", "HAS_REMOTE_API=0", 
                    //  "USE_OS_TZDB=0", "USE_BUNDLE_BUF"
    };
    x.defines = {"NOMINMAX"};
    if (x.cfg["os"] == "win")
        x.pub.defines = {"DATE_BUILD_LIB"};
    else
        x.pub.defines = {"USE_OS_TZDB=1"};
    x.defines += x.pub.defines;
    
    x.srcs = {"repo/src/tz.cpp"};
}
