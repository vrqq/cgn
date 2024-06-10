#include <cgn>

alias("cpp2alias", x) {
    x.actual_label = "../cpp2";
}

group("gp1", x) {
    x.add_dep(":cpp2alias");
    x.add_dep("../cmake:zlib");
}

group("gp3", x) {
    x.add_deps({"@third_party//fmt", "@third_party//spdlog"});
}

cxx_shared("gp3_exe", x) {
    x.add_dep(":gp3", cxx::inherit);
}
