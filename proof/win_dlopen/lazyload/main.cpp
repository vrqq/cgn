// Result
//      LoadLibrary() can load dll from subfolder

#include <iostream>
#include <windows.h>
#pragma comment(lib, "delayimp")

extern int work1();

extern int rootfn();

int main()
{
    std::cout<<"MAIN EXECUTE."<<std::endl;
    // return 1;

    // auto ptr2 = LoadLibrary("func2/x.dll");
    // if (ptr2)
    //     std::cout<<"func2/x.dll loaded"<<std::endl;
    // else
    //     std::cout<<"func2/x.dll load failure "<<GetLastError()<<std::endl;

    auto ptr1 = LoadLibrary("func1/x.dll");

    if (ptr1)
        std::cout<<"func1/x.dll loaded"<<std::endl;
    else
        std::cout<<"func1/x.dll load failure "<<GetLastError()<<std::endl;

    // cannot find work1() even if func1/x.dll loaded by LoadLibrary()
    // std::cout<<"WORK1 return "<<work1()<<std::endl;

    // It's ok to lazyload
    std::cout<<"ROOTFN return "<<rootfn()<<std::endl;


    return 0;
}