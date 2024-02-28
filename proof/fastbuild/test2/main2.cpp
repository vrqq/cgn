#include <iostream>
#include "test2.h"

int main() {
        std::vector<float> a = {1., 2., 3.};
        for (auto &&e : a) {
                std::cout << e << "\n";
        }
        std::vector<int> b = {1, 2, 3};
        for (auto &&e : b) {
                std::cout << e << "\n";
        }
}