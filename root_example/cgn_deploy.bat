@echo off

echo CGN deployment scriptcc_debug
echo init monorepo

REM 1. create empty git repository
git init

REM 2. Add @cgn.d @third_party as submodule or subtree
echo Using submodule mode
git submodule add -b cell/cgn.d https://github.com/vrqq/cgn.git @cgn.d
git submodule add -b cell/third_party https://github.com/vrqq/cgn.git @third_party
git submodule update --init --recursive

echo Building cgn
pushd @cgn.d
ninja -f build_msvc.ninja
ninja -f build_msvcrel.ninja
popd

REM 3. copy default script to monorepo root
echo copy basic files...
copy @cgn.d\root_example\cgn .
copy @cgn.d\root_example\cgn_setup.cgn.cc .
copy @cgn.d\root_example\debug.bat .
copy @cgn.d\root_example\query.bat .
