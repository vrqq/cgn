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
