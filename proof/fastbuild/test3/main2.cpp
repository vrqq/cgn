#include <iostream>
#include "test2.h"

int main() {
        std::unordered_set<std::string> a = {"x", "yy", "zzz"};
        for (auto &&e : a) {
                std::cout << e << "\n";
        }
}
