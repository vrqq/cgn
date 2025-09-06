# What is CGN
`cgn` 是一个Build system, 参考了chrome-gn, bazel 和 buck 的设计思路, 仅使用C++ compiler做build target管理, 不引入其他语言.

## evolution
1. 早先使用Makefile, 通过环境变量控制编译参数
2. 之后使用CMake, 可以做很多复杂的环境判断, 从而完成工程编译. CMake仍然是 procedure oriented的, 大量函数以及新老版本的交替, 让人很难用好它, 只能靠外部脚本扩展
3. 下一代是bazel/buck, 我认为其只是把cmake结构化了, 依旧是procedure oriented, 通过调用函数声明target, 或者调用函数声明target+config的组合. 其混乱的概念 我觉得不是好的设计. 为了速度使用starlark, 导致扩展也只能靠外部脚本. 一切的起点都是函数调用.
4. 下一代是chrome gn, 我觉得划时代的设计来了, object oriented的魅力, 每个 ninja target 通过 `user_function(arg: config)` 生成, 简单明了.

一直在使用chrome gn, 但是仍决定自己造一套轮子, 这些工具链的缺点:
* chrome gn 的`user_function`没有返回值, 很难优雅的多层依赖, 尤其跨config时重复编译. (例如在debug环境编译release 版本的perl.exe)
* bazel 的predefined c++ rule 命名混乱, 可能是历史遗留问题, 遇到打包需求很难扩展, 并且其面向过程的声明式定义 用完gn再看它真的不是一个好的设计.

所以自制了当前工具 称之为 `CPP Generate Ninja`.
对于小型工程, 尤其很少外部依赖的, 首选推荐GN. (https://github.com/vrqq/gn_catalina)

## pros and cons
* 通过cgn编译user_target_factory.cpp, 然后dlopen后call it, 由其 生成build.ninja 再用ninja 完成实际编译.
* 编译cpp的过程 目前仍未并行化, 首次编译速度堪忧. 之后由于都是dll, 只需dlopen
* 期待后续以常驻线程, 并行编译等特性优化性能.

# Startup
1. create your own monorepo root folder in local

2. Download https://raw.githubusercontent.com/vrqq/cgn/refs/heads/cell/cgn.d/root_example/cgn_deploy.sh (linux), or https://raw.githubusercontent.com/vrqq/cgn/refs/heads/cell/cgn.d/root_example/cgn_deploy.bat (windows) to your monorepo root and run it

3. run these command in necessary (if you want to use all third party dependencies)
```./debug.sh @third_party//:all_git```

4. create a folder like `@myproject` as your own cell, see `@cgndemo` folder for all examples.
    NOW `@cgndemo` folder is under construction

# Appendix
**Proxy guide for Linux (for V2RayA)**
Edit ~/.ssh/config with below (which 192.168.122.1:20170 is V2RayA socks proxy port)
```
Host github.com
    User git
    ProxyCommand nc --proxy-type socks4 --proxy 192.168.122.1:20170 %h %p
```
Then call `git config --global http.proxy http://192.168.122.1:20172` to set a proxy for https://github.com. (192.168.122.1:20172 is the V2RayA http proxy address and port)

