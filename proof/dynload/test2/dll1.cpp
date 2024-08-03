#include <iostream>
#include "dll1.h"

int global_val = [](){
    for (auto it: A::script_labels)
        std::cout<<it<<std::endl;
    return 0;
}();

