// Resolution:
//  windows LoadLibrary() cannot work as linux dlopen(RTLD_GLOBAL), it cannot
//  load any symbol into current context, or to the code below. In this example, 
//  func1() would always not be found with any type of declarations.
//
// Solution:
//  https://edll.sourceforge.net/
//  https://github.com/ocaml/flexdll?tab=readme-ov-file

#include <iostream>
#include <windows.h>
#include <delayimp.h>

#pragma comment(lib, "delayimp")

__declspec(dllimport) int func1();
// extern int func1();
// int func1();

int main(int argc, char **argv)
{

    //* case 1 */
    auto rv = LoadLibrary("dll.dll");
    if (rv == nullptr){
        std::cerr<<"LoadFailure"<<GetLastError()<<std::endl;
        return 1;
    }

    //* case 2 */
    // HRESULT hr = __HrLoadAllImportsForDll("dll.dll");
    // if (FAILED(hr))
    //     std::cerr<<"Delay load failure"<<std::endl;
    // else
    //     std::cout<<"Delay loaded!"<<std::endl;

    // failed for both cases even if /FORCE:UNRESOLVED
    std::cout<<func1()<<std::endl;
    return 0;
}
