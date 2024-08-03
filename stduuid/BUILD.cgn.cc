#include <cgn>

git("stduuid.git", x) {
    x.repo = "https://github.com/mariusbancila/stduuid.git";
    x.commit_id = "3afe7193facd5d674de709fccc44d5055e144d7a";
    x.dest_dir = "repo";
}

cxx_sources("stduuid", x) {
    x.pub.include_dirs = {"repo/include", "repo/gsl"};
}
