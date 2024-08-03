#!/bin/sh

if [ ! -f "libcgn_base.so" ]; then
    echo "Build libcgn_base.so"
    clang++ -fPIC -shared -g -glldb ../entry/cgn_tools.cpp ../ninjabuild/src/line_printer.cc ../ninjabuild/src/util.cc ../ninjabuild/src/string_piece_util.cc ../ninjabuild/src/edit_distance.cc -o libcgn_base.so
fi

echo "Build pefmt_debug"
clang++ -g -glldb -o pe_debug -fuse-ld=lld -L. -lcgn_base -Wl,--rpath=. \
    pefmt_debug.cpp pe_file.cpp ../entry/cgn_tools.cpp
