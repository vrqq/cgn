@echo off
color
pushd %~dp0
echo == cgn monorepo update script ==

@REM DIR: monorepo root
echo [40mGIT PULL all submodules ...[0m
cd ..
git submodule foreach git pull

@REM DIR: cgn.d
echo [40mRecompile cgn ...[0m
cd @cgn.d
ninja -f build_msvc.ninja
ninja -f build_msvcrel.ninja

echo [40mDone[0m
popd