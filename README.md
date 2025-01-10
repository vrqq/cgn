# CGN
CPP Generate Ninja V0.9
Still in development

受GN和bazel/buck启发，用 C++11 编写编译脚本(BUILD.cgn.cc)，并由cgn.exe扫描目录并自动将每一个`BUILD.cgn.cc` 编译为独立dll，然后由cgn.exe依次dlopen后，自动解析target然后生成ninja脚本, 最后由ninja完成实际编译。  

**中文参考文档如下**

[Why Not Recommand](WHY_NOT_RECOMMAND.md) 

[Intro](@cgn.d/doc/zh-Hans/docusaurus-plugin-content-docs/current/intro.md)

[API](@cgn.d/doc/zh-Hans/docusaurus-plugin-content-docs/current/cgn-api.md)

[Interpreter](@cgn.d/doc/zh-Hans/docusaurus-plugin-content-docs/current/interpreter.md)

[DevNote](@cgn.d/doc/zh-Hans/docusaurus-plugin-content-docs/current/devnote.md)

## How to start

1. build cgn itself  
For linux: ```pushd @cgn.d && ninja -f build_linux.ninja && popd```  
For windows:  ```pushd @cgn.d && ninja -f build_msvc.ninja && popd```  

2. (Linux LLVM) build hello-world  
(CWD is the project root dir), see also debug.sh  
```./@cgn.d/build_linux/cgn --halt_on_error --target llvm,debug,asan build //hello/cpp0```  

2 (Windows MSVC) build hello-world  
(CWD is the project root dir), see also dbgwin.sh  
```"@cgn.d\build_win\cgn.exe" --halt_on_error --target msvc,debug build //hello/cpp0```  

More examples can be found in the 'hello' folder.

## Advance
**Query target info**
```"@cgn.d\build_win\cgn.exe" --halt_on_error --target msvc,debug query //hello/cpp0```  

**Assign cgn-out folder**
```"@cgn.d\build_win\cgn.exe" --cgn-out c:\\workingdir\\cgntmp --target msvc,debug build //hello/cpp0```  

**Customize relation of cmdline and the real 'configuration'**
Imitate '@cgn.d/library/cgn_default_setup.cgn.hxx' to write your own `CGNSetup()` function in 'cgn_setup.cgn.cc'
