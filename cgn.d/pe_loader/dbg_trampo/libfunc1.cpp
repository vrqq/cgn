#include <iostream>
#include "publ.h"

__declspec(dllexport) MyTypeA func1(int x, std::string y) {
    MyTypeA rv;
    rv.str1 = std::to_string(x+1);
    rv.str2 = std::move(y);
    return rv;
}

__declspec(dllexport) int func1_inc(int &y) {
    return ++y;
}

__declspec(dllexport) std::vector<MyTypeA> func1_arr(const MyTypeA *y) {
    std::cout << "[func1.dll] ENTER ::func1_arr()" << std::endl;
    std::vector<MyTypeA> rv{ *y, *y };
    return rv;
}
