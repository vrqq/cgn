#!/bin/sh

# git clone https://github.com/libgit2/libgit2.git repo --depth=1
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

mkdir -p $SCRIPT_DIR/repo
cd $SCRIPT_DIR/repo
git init
git remote add origin https://github.com/libgit2/libgit2.git
git fetch --depth=1 origin d74d491481831ddcd23575d376e56d2197e95910
git reset --hard d74d491481831ddcd23575d376e56d2197e95910
