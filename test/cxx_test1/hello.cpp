#include <iostream>
#include "func1.h"

int main() {
#ifdef HAVE_FUNC1
    std::cout<<"HAVE_FUNC1 : "<<func1()<<std::endl;
#endif
    std::cout<<"Hello world!"<<std::endl;
    return 0;
}
