#include <iostream>

struct X {
    X() { std::cout<<"- func2 X construct()"<<std::endl; }
    ~X() { std::cout<<"- func2 ~X destroy()"<<std::endl; }
};

static std::unique_ptr<X> x = std::make_unique<X>();

__declspec(dllexport) int work2() { return 2; }