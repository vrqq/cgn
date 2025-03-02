#include <cgn>

static const std::string repo = "repo";

// Jan 1, 2024
git("exprtk.git", x) {
    x.repo = "https://github.com/ArashPartow/exprtk.git";
    x.commit_id = "9910dc988d472d17ecf309c1c9c3e38430bd9c0f";
    x.dest_dir = repo;
}

cxx_prebuilt("exprtk", x) {
    x.pub.include_dirs = {repo};
}
