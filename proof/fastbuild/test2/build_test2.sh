#!/bin/sh
echo "no pch"
time clang++ -std=c++11 main.cpp

echo "pch, no explicit instantiate"
clang++ -std=c++11 -x c++-header test.h
time clang++ -std=c++11 -include-pch test.h.gch main.cpp 

echo "pch, explicit instantiate"
clang++ -std=c++11 -x c++-header test2.h
time clang++ -std=c++11 -include-pch test2.h.gch main2.cpp 

## TEST CASE FROM
## https://stackoverflow.com/a/45370435
