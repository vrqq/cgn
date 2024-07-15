#include <iostream>
#include <memory>

#pragma comment(lib, "delayimp")

extern int work2();

struct X {
    X() {
        std::cout<<"- func1 X construct()"<<std::endl;
        std::cout<<"- call work2() from func2/x.dll "<<work2()<<std::endl;
    }
    ~X() { std::cout<<"- func1 X destroy()"<<std::endl; }
};

static std::unique_ptr<X> x = std::make_unique<X>();


__declspec(dllexport) int work1() { return 1 + work2(); };