#!/bin/sh

mkdir -p aa
mkdir -p aa/bb

# files in dir aa/bb
echo fileUV > aa/bb/fileUV.in
echo fileX.txt > aa/bb/fileX.txt
ln -s aa/bb/fileX.txt aa/bb/fileX.txt.2.0
echo fileT.txt > aa/bb/fileY.txt

# files in dir aa
echo file_in_aa > aa/file_in_aa.txt
echo fileZ.in > aa/fileZ.in
ln -s aa/parent_link aa
ln -s invalid_link aa/invalid_link