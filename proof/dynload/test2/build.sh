#!/bin/sh

g++ -shared -fPIC dll1.cpp -o libdll1.so
g++ -fPIC -Wl,--rpath=\$ORIGIN main.cpp -o main