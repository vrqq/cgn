// ninja-build return 1 when build falure
// return 0 : build successful / nothing to do
//
// g++ -MMD -MF hello.d -c hello.cpp -o hello.o
// g++ -MMD -MF dll1.d -c dll1.cpp -o dll1.o
// g++ hello.o dll1.o -o hexe
//
// CASE2:
//   g++ -MMD -MF exe.d dll1.cpp hello.cpp -o exe
//   可见 exe.d 中无 dllprivate.h 故推知g++只能针对单个.cpp生成depfile
//   结论: 针对 .script_label, cgn_impl 应单独编译其内部每一个 cpp
//   conclusion: for files pointered by script_label, cgn_impl should build
//     every cpp sources inside, instead of build together
#include <iostream>
#include "dll1.h"
 
int main() {
    std::cout<<"Hello!"<<get()<<std::endl;
    return 0;
}