#include <iostream>

int main() {

#ifdef THIS_IS_CPP1
    std::cout<<"This is CPP1"<<std::endl;
#endif

#ifdef THIS_IS_CPP2
    std::cout<<"This is CPP2"<<std::endl;
#endif

#ifdef WE_DEFINE_IT
    std::cout<<"WE_DEFINE_IT"<<std::endl;
#else
    std::cout<<"WE_NOT_DEFINE_IT"<<std::endl;
#endif

    return 0;
}