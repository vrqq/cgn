#include <iostream>

extern void fn2() __attribute__((weak));

int get() { fn2(); return 10; }

void pusherror() { 
    throw std::logic_error{"123123"};
};