#!/bin/sh
rm libfn.so
g++ -shared -fPIC -g fn.cpp -o libfn.so

rm libfn2.so
g++ -shared -fPIC -g fn2.cpp -o libfn2.so

# g++ main.cpp -fPIC -L. -lfn -Wl,--rpath=\$ORIGIN -o main
rm main
g++ main.cpp -fPIC -g -Wl,--rpath=\$ORIGIN -o main
