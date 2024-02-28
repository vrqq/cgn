#!/bin/sh
echo "pch, explicit instantiate"
clang++ -x c++-header -std=c++11 -DENABLE_SPECIALIZATION test.h -o test1.h.gch
time clang++ -std=c++11 -include-pch test1.h.gch -DENABLE_SPECIALIZATION main.cpp 

echo "pch, no explicit instantiate"
clang++ -x c++-header -std=c++11 test.h -o test2.h.gch
time clang++ -std=c++11 -include-pch test2.h.gch main.cpp 
