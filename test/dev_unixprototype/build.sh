#!/bin/sh

clang++ -shared -o mydl.so -fvisibility=hidden -fPIC mydl.cpp
nm -g mydl.so

clang++ -o main main.cpp -ldl -Wl,-rpath,., -Wl,-undefined,dynamic_lookup
