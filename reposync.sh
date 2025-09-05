#!/bin/sh
pushd "$(dirname "$(readlink -f "$0")")"
echo == cgn monorepo update script ==

cd ..
echo GIT PULL all submodules...
git submodule foreach git pull

echo Recompile cgn...
cd @cgn.d
ninja -f build_linux.ninja
ninja -f build_linuxrel.ninja
 
echo ✅Done
popd
