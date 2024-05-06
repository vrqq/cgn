#include "custom_processor.cgn.h"

// using 'cgn build //hello/2factory:xyz.1' and 
// 'cgn build //hello/2factory:xyz.2' to build.

my_rule("xyz", x) {
    x.cppsrc = "main.cpp";
    x.use_custom_define = true;
}
