#include <iostream>
#include "test.h"

int main() {
    std::unordered_set<std::string> data{"a", "bb", "ccc"};
    for (auto iter = data.begin(); iter != data.end(); iter++)
        std::cout<<*iter<<std::endl;
    return 0;
}
