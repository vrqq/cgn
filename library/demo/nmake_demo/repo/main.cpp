#include <iostream>

__declspec(dllimport) int fn1();

int main() {
    std::cout<<"fn1: "<<fn1()<<std::endl;
    return 0;
}
