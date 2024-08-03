#include <iostream>

extern void pusherror() __attribute__((weak));

void fn2() {
    return pusherror();
    // throw std::logic_error{"INTERNAL"};
}