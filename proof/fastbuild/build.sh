#!/bin/sh

## RUN this script in working root
clang++ -x c++-header -std=c++11 -fPIC ./proof/fastbuild/cgn_pch.h -I. -Icgn-out/cell_include -DCGN_VAR_PREFIX=hello_2FBUILD_2Ecgn_2Ecc -DCGN_ULABEL_PREFIX=\"//hello:\"

## LLVM-IR PCH 210ms
# time clang++ -include-pch ./proof/fastbuild/cgn_pch.h.gch -emit-llvm -S -fPIC -fdiagnostics-color=always -ftime-trace -g -glldb -std=c++11 -I. -Icgn-out/cell_include -DCGN_VAR_PREFIX=hello_2FBUILD_2Ecgn_2Ecc -DCGN_ULABEL_PREFIX=\"//hello:\" hello/BUILD.cgn.cc -o fastb.ll

## dynlib PCH 270ms
time clang++ -include-pch ./proof/fastbuild/cgn_pch.h.gch -fPIC -fdiagnostics-color=always -ftime-trace -g -glldb -std=c++11 -I. -Icgn-out/cell_include -DCGN_VAR_PREFIX=hello_2FBUILD_2Ecgn_2Ecc -DCGN_ULABEL_PREFIX=\"//hello:\" hello/BUILD.cgn.cc --shared -o libfastb.so

mv /tmp/BUILD-*.json .

## NO PCH: 600ms
# time clang++ -fPIC -fdiagnostics-color=always -ftime-trace -g -glldb -std=c++11 -I. -Icgn-out/cell_include -DCGN_VAR_PREFIX=hello_2FBUILD_2Ecgn_2Ecc -DCGN_ULABEL_PREFIX=\"//hello:\" --shared ./proof/fastbuild/BUILD.cgn.cc -o ./proof/fastbuild/libfastb.so