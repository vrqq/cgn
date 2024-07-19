#include <iostream>

extern int func1();
void show_maker() {
    std::cout<<func1()<<std::endl;
}