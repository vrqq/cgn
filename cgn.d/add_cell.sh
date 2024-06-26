#!/bin/sh

if [ "$#" -ne 2 ]; then
    echo "Usage: " $0 "<cell_name> <target_destination>"
fi

if [ ! -d "cgn-out" ]; then
    echo "Please run this script in project root (with cgn-out folder inside)"
    exit 1
fi

echo "Add cell" $1 "to" $2
ln -s ../../$2 cgn-out/cell_include/@$1
