#!/bin/sh

g++ -shared try_glb_ext.cpp -o libtry_glb_ext.so -fPIC
g++ try_glb_main.cpp -Wl,--rpath=\$ORIGIN -o tryglb