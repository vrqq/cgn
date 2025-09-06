#!/bin/sh

MONOREPO_DEPLOY="false"
echo "cgn deployment script"

## 0. OS_DETECT
OS_TYPE=$(uname)

# 1. create empty git repository
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Already inside a git repo, skip init."
else
    git init
fi

## 2. Add @cgn.d @third_party as submodule or subtree
if [[ "$MONOREPO_DEPLOY" == "true" ]]; then
    echo "Using monorepo subtree mode"
    git subtree add --prefix=@cgn.d https://github.com/vrqq/cgn-cgn.d.git main
    git subtree add --prefix=@third_party https://github.com/vrqq/cgn-third_party.git main
else
    echo "Using submodule mode"
    git submodule add -b cell/cgn.d git@github.com:vrqq/cgn.git @cgn.d
    git submodule add -b cell/third_party git@github.com:vrqq/cgn.git @third_party
    git submodule update --init --recursive
fi

## 3. build cgn
pushd @cgn.d
if [[ "$OS_TYPE" == "Linux" ]]; then
    ninja -f build_linux.ninja
    ninja -f build_linuxrel.ninja
elif [[ "$OS_TYPE" == "Darwin" ]]; then
    ninja -f build_macrel.ninja
else
    echo "Unsupported operating system: $OS_TYPE"
    popd
    exit 1
fi
popd

## 4. copy default script
cp @cgn.d/root_example/cgn .
cp @cgn.d/root_example/cgn_setup.cgn.cc .
cp @cgn.d/root_example/debug.sh .
cp @cgn.d/root_example/release.sh .
cp @cgn.d/root_example/query.sh .
cp @cgn.d/root_example/run.sh .
