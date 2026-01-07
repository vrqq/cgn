#include <iostream>

__attribute__((visibility("default"))) int cgn_setup(int x) {
    return x * 2;
}

__attribute__((visibility("default"))) 
int func() {
    return 10;
}
