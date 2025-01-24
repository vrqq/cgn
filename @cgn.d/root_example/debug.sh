#!/bin/sh

# Detect the operating system
OS_TYPE=$(uname)

if [[ "$OS_TYPE" == "Linux" ]]; then
    ./@cgn.d/build_linux/cgn --halt_on_error --target llvm,debug,asan build $@
elif [[ "$OS_TYPE" == "Darwin" ]]; then
    ./@cgn.d/build_mac/cgn --halt_on_error --target xcode,debug,asan build $@
else
    echo "Unsupported operating system: $OS_TYPE"
    exit 1
fi
