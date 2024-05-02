#include <cgn>

alias("cpp2alias", x) {
    x.actual_label = "../cpp2";
}

group("gp1", x) {
    x.add_dep(":cpp2alias");
    x.add_dep("../cmake:zlib");
}