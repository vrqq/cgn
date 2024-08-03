#include <string>
#include <iostream>
#include "publ.h"

extern MyTypeA func1(int x, std::string y);
extern int func1_inc(int& y);
extern std::vector<MyTypeA> func1_arr(const MyTypeA* y);

__declspec(dllexport) int funcuser(int n)
{
    std::cout<<"[in funcuser] Call func1()\n";
    auto rv = func1(n, "from_funcuser");
    std::cout<<"[in funcuser] func1() return: "<<rv.str1<<" "<<rv.str2<<"\n";
    throw std::runtime_error{"NORMAL RETURN BY EXCEPTION"};
}


__declspec(dllexport) int funcuser_X(int n)
{
    std::cout << "[in funcuserX] Before call func1_inc() " << n << "\n";
    func1_inc(n);
    std::cout << "[in funcuserX]  After call func1_inc() " << n << "\n";

    MyTypeA elem;
    elem.str1 = std::to_string(n) + "-str1";
    elem.str2 = std::to_string(n) + "-str2";
    std::cout << "[in funcuserX] Before call func1_arr()\n";
    auto rv = func1_arr(&elem);
    std::cout << "[in funcuserX] After call func1_arr(): size="<<rv.size()
        << " " << rv[0].str1 << " " << rv[0].str2
        << " " << rv[1].str1 << " " << rv[1].str2
        << "\n";
    rv[1].str2 = "ABABCCDD";
    std::cout << "[in funcuserX] After modification: " << rv[1].str2 << std::endl;
    return n * 9;
}