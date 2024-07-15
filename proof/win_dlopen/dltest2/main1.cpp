// The full version of dlopen test
//  (UNUSED currently)
#include <iostream>
#include <windows.h>

#define MAIN_IMPL
#include "main1.h"
#include "dll1.h"

std::function<void*()> addr_in_main;

__declspec(dllexport) void global_reg(std::function<void*()> fn_new) {
    addr_in_main = fn_new;
}

int main(int argc, char **argv)
{
    auto rv = LoadLibrary("dll.dll");
    if (rv == nullptr){
        std::cerr<<"LoadFailure"<<GetLastError()<<std::endl;
        return 1;
    }
    if (addr_in_main)
        std::cout<<"addr_in_main recorded."<<std::endl;

    std::cout<<"before call addr_in_main()"<<std::endl;
    void *vptr = addr_in_main();
    std::cout<<"after call addr_in_main(), vptr="<<vptr<<std::endl;

    A *pimpl = (A*)vptr;
    std::cout<<"A::dynamic_func = "<<pimpl->dynamic_func()<<std::endl;
    std::cout<<"A::static_func = "<<pimpl->static_func()<<std::endl;
    std::cout<<"A::static_variable = "<<pimpl->static_variable<<std::endl;
    
    return 0;
}