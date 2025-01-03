---
sidebar_position: 1
---

# CGN
受GN和bazel/buck启发，用 C++11 编写编译脚本(BUILD.cgn.cc)，并由cgn.exe扫描目录并自动将每一个`BUILD.cgn.cc` 编译为独立dll，然后由cgn.exe依次dlopen后，自动解析target然后生成ninja脚本。 
使用`cgn build @cell//folder:target` 执行编译

## Getting Started
进入 @cgn.d 文件夹 根据当前系统编译相应版本的 cgn
* linux系统 debug模式: `ninja -f build_linux.ninja`
* windows系统 debug模式: `ninja -f build_msvc.ninja`

然后回到上层目录 通过样例脚本编译(用户请自行修改参数)
* linux: `./debug.sh //hello/cpp0`
* windows: `dbgwin.bat //hello/cpp0`

用户可自行修改 `cgn_setup.cgn.cc` 配置预设编译参数集

## 一些优点和难以解决的问题
**Pros**
* 受buck2启发，既然buck2仅是starlark解释器，那么c++语言就是天然的解释器，只要规定好脚本习惯，即可扩充其他支持的语言
* 无需考虑语法解释器 和新的语言语法，c++大多数系统自带，同时也有语法提示
* 依赖及函数实时解析 类似chrome GN, 比 buck2/bazel 有更强的灵活性.
* ninja做了依赖，文件检查，并行编译

**Cons**
* 造轮子成本
* 分布式编译协议 可引入分布式编译器
* cgn-script 的c++编译链接速度令人发指 (当然和cpp target比就不算什么了)

