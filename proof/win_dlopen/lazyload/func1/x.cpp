#include <iostream>
#include <memory>
#include <windows.h>

#pragma comment(lib, "delayimp")

extern int work2();

struct X {
    X() {
        std::cout<<"- func1 X construct()"<<std::endl;
        // Cannot run work2() here even if LoadLibrary()
        //  But it can when LoadLibrary(func2) in main() before LoadLibrary(func1)
        if (LoadLibrary("func2/x.dll"))
            std::cout<<"- call work2() from func2/x.dll "<<work2()<<std::endl;
        else
            std::cout<<"- LoadLibrary(func2/x.dll) failed: "<<GetLastError()<<std::endl;
    }
    ~X() { std::cout<<"- func1 X destroy()"<<std::endl; }
};

static std::unique_ptr<X> x = std::make_unique<X>();


__declspec(dllexport) int work1() { return 1 + work2(); };