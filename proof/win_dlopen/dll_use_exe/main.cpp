#include <iostream>
#include <windows.h>

__declspec(dllexport) int get_global() { return 10; }

int main()
{
    auto hnd = LoadLibrary("func1.dll");
    if (hnd)
        std::cout<<"Load successful\n";
    else
        std::cout<<"Load failure: "<<GetLastError()<<"\n";
    
    return 0;
}