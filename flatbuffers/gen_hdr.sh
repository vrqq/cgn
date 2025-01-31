#!/bin/sh
ls1=$(find ./repo/src -name '*.cpp' ! -name 'flathash.cpp')
ls2=$(find ./repo/grpc/src/compiler -name '*.cc')
fout="flatbuf_srclist.cgn.h"

# $'\n'
# echo $ls1 > in.txt
# echo ${ls1//[[:alpha:]]/AA\n} > out.txt
# echo ${ls1//([a-zA-Z0-9/._]+)/'AA\n'}

echo "#pragma once" > $fout
echo "#include<string>" >> $fout
echo "#include<vector>" >> $fout
echo >> $fout
echo "static std::vector<std::string> flatc_srcs={" >> $fout

while IFS= read -r line; do
    echo "    \"$line\"," >> $fout
done <<< $ls1

while IFS= read -r line; do
    echo "    \"$line\"," >> $fout
done <<< $ls2

echo "};" >> $fout
