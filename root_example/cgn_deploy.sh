#!/bin/sh

MONOREPO_DEPLOY="false"

## 0. OS_DETECT
OS_TYPE=$(uname)
NINJA_FILE=""
if [[ "$OS_TYPE" == "Linux" ]]; then
    NINJA_FILE=build_linuxrel.ninja
elif [[ "$OS_TYPE" == "Darwin" ]]; then
    NINJA_FILE=build_macrel.ninja
else
    echo "Unsupported operating system: $OS_TYPE"
    exit 1
fi

## 1. Add @cgn.d @third_party as submodule or subtree
if [[$MONOREPO_DEPLOY == "true"]]; then
    git subtree add --prefix=@cgn.d https://github.com/vrqq/cgn-cgn.d.git main
    git subtree add --prefix=@third_party https://github.com/vrqq/cgn-third_party.git main
else
    git submodule add -b cell/cgn.d git@github.com:vrqq/cgn.git @cgn.d
    git submodule add -b cell/third_party git@github.com:vrqq/cgn.git @third_party
fi

## 2. build cgn
pushd @cgn.d
ninja -f $NINJA_FILE
popd

## 3. copy default script
cp @cgn.d/root_example/cgn .
cp @cgn.d/root_example/cgn_setup.cgn.cc .
cp @cgn.d/root_example/debug.sh .
