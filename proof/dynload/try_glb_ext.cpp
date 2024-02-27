#include <iostream>


#ifdef __linux__
    __attribute__((constructor)) 
#endif
void func1() {
    std::cout<<"    INIT-1"<<std::endl;
}

#ifdef __linux__
    __attribute__((constructor)) 
#endif
void func2() {
    std::cout<<"    INIT-2"<<std::endl;
}

#ifdef __linux__
    __attribute__((destructor)) 
#endif
void func3() {
    std::cout<<"    DEINIT"<<std::endl;
}

// #ifdef _WIN32
struct WinDummy {
    WinDummy() {
        std::cout<<"    WinDummy-INIT"<<std::endl;
    }
    ~WinDummy() {
        std::cout<<"    WinDummy-DEINIT"<<std::endl;
    }
}win_dummy;
// #endif

struct STAnchor {
    STAnchor() {
        std::cout<<"    STAnchor-INIT"<<std::endl;
    }
    ~STAnchor() {
        std::cout<<"    STAnchor-DEINIT"<<std::endl;
    }
};
class STCLASS {
    static STAnchor a;
};
STAnchor STCLASS::a;
