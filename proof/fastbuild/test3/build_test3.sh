#!/bin/sh
echo "no pch"
time clang++ -std=c++11 main.cpp

echo "pch, no explicit instantiate"
clang++ -std=c++11 -x c++-header -fPIC test.h
time clang++ -std=c++11 -include-pch test.h.gch --shared -fPIC main.cpp 

echo "pch, explicit instantiate"
clang++ -std=c++11 -x c++-header -fPIC test2.h
time clang++ -std=c++11 -include-pch test2.h.gch --shared -fPIC main2.cpp 
