#include <cgn>

alias("cpp2alias", x) {
    x.actual_label = "../cpp2";
}

group("gp1", x) {
    x.add(":cpp2alias");
    x.add("../cmake:zlib");
}