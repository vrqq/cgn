// ninja-build return 1 when build falure
// return 0 : build successful / nothing to do
//
// g++ -MMD -MF hello.d -c hello.cpp -o hello.o
// g++ -MMD -MF dll1.d -c dll1.cpp -o dll1.o
// g++ hello.o dll1.o -o hexe
#include <iostream>
#include "dll1.h"
 
int main() {
    std::cout<<"Hello!"<<get()<<std::endl;
    return 0;
}