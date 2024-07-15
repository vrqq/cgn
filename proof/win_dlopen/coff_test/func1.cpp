// Concolution:
//  The code segment only applies to the code implement, and all symbols 
//  used in this implement would be placed in the same segment together. 
//  Therefore, code_seg cannot make a UNDEF symbol with the same declspec 
//  together. For instance, func2(), func3(), and strlen would in same place.
#pragma section("cgndef", read, execute, shared)
#include <string>

__declspec(code_seg("cgndef")) extern int func2();
int func3();

class CLA {
public:
    int funcX();
    double funcY();
};

CLA ext_cla;

// This is the wrong case
//func1.cpp(15): error C2375: 'func1': redefinition; different linkage
// extern int func1(); 

__declspec(dllexport, code_seg("cgndef")) int func1() { 
    // return 101 + func2() + func3() + ext_cla.funcX();
    std::string tmp = "AABBCCD";
    return tmp.size() + strlen(tmp.c_str()) + 101 + func2() + func3() + ext_cla.funcX(); 
}
