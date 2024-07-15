#include <iostream>
#include "dll1.h"
#include "main1.h"

int A::static_variable = 99;
int A::static_func()  { return 1; }
int A::dynamic_func() { return 2; }

static int __reg_line = [](){
    std::cout<<"Dll1: reg_line before global_reg()"<<std::endl;
    global_reg([](){return (void*)new A;});
    std::cout<<"Dll1: reg_line after global_reg()"<<std::endl;
    return 0;
}();
