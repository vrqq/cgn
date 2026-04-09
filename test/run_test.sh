#!/bin/sh

## compile and run test_stdop.cpp and other cpp files in the test directory.
## Args: $0 compile
##       $0 run
##       $0 clean

current_dir=$(dirname "$0")
working_root=current_dir/../../

if [ "$1" = "compile" ]; then
    g++ -std=c++17 -I ${working_root} -I @cgn.d/library/cxx/third_party/ -I @cgn.d/library/cxx/third_party/googletest/googletest/include/ -L @cgn.d/library/cxx/third_party/googletest/build/lib/ -lgtest -lgtest_main test/test_stdop.cpp -o test/test_stdop
elif [ "$1" = "run" ]; then
    ./test/test_stdop
elif [ "$1" = "clean" ]; then
    rm test/test_stdop
else
    echo "Usage: $0 {compile|run|clean}"
fi
