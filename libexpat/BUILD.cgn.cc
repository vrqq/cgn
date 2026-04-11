#include <cgn>

static const std::string EXPAT_ROOT = "repo/expat";

// 2.7.5
git("libexpat.git", x) {
    x.repo = "https://github.com/libexpat/libexpat.git";
    x.dest_dir = "repo";
    x.commit_id = "f31adfd584b7f6c50bbf4d22eb928538ffc9145a";
}

cxx_static("expat_static", x) {
    x.pub.include_dirs = {EXPAT_ROOT + "/lib"};
    x.include_dirs = {EXPAT_ROOT + "/lib"};

    x.srcs = {
        EXPAT_ROOT + "/lib/xmlparse.c",
        EXPAT_ROOT + "/lib/xmlrole.c",
        EXPAT_ROOT + "/lib/xmltok.c",
        EXPAT_ROOT + "/lib/xmltok_impl.c",
        EXPAT_ROOT + "/lib/xmltok_ns.c"
    };

    x.defines = {"XML_STATIC"};
}

alias("libexpat", x) {
    x.actual_label = ":expat_static";
}
