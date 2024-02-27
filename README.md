## 写在最前面
这是一个比较失败的烂尾项目: 无论gcc还是clang 编译dll的速度实在太慢(0.5秒)，若一个小型项目(几十个BUILD文件) 每次修改几个 都需要数秒analyze，首次生成甚至需要数十秒analyze 很难接受。几个改进方向:  
* JIT: llvm IR JIT
* PCH
* 保留其思想 使用python/js重写

# CGN
受GN和bazel/buck启发，用 C++11 编写编译脚本(BUILD.cgn)，并由cgn.exe扫描目录并自动将每一个BUILD.cgn编译为独立dll，然后由cgn.exe依次dlopen后，自动解析target然后生成ninja脚本。
之后使用`ninja -C xx ...`或`cgn build cell://folder:target`编译  
* label: `@third_party//protobuf:protoc`
* WorkingRoot: CWD where to run cgn.exe
* cgn-out: CGN的输出位置 含build.ninja，compile_commands.json等
* Target输出路径:    `cgn-out/obj/folder_/factoryname_1A2B3C`
    * target-ninja: `cgn-out/obj/folder_/factoryname_1A2B3C/build.ninja`
    * entry:        `cgn-out/obj/main.ninja`
    * $OUT in-ninja: `cgn-out/obj/folder_/factoryname_1A2B3C/hello.o`
    * $IN in-ninja:  `folder/hello.cpp`
* Configuration存储路径: `cgn-out/configurations/1A2B3C.cfg`
* CGN-Script中间路径:  `cgn-out/analysis/folder/libSCRIPTFILENAME.cgn.so`
    * cgn.so builder: `cgn-out/analysys/folder/BUILD.cgn.ninja`
    * .rsp savepath:  `cgn-out/analysys/folder/SCRIPTFILENAME.rsp`
    * scriptc:        `cgn-out/analysis/.cgn.ninja`
    * script-include: `cgn-out/cell_include`
    * -DCGN_VAR_PREFIX: `FOLDER1_20_20__`
    * -DCGN_ULABEL_PREFIX: `//folder:`
* 所有有关路径的string 无论在ninja文件中还是任何地方 都和系统相关
    * windows使用backslash'\'，unix 使用 slash'/'
* 所有label均使用slash'/'分割
    * @cell//folder/file.cpp
    * @cell//folder:label
    

**Pros**
* 受buck2启发，既然buck2仅是starlark解释器，那么c++语言就是天然的解释器，只要规定好脚本习惯，即可扩充其他支持的语言
* 无需考虑语法解释器 和新的语言语法，c++大多数系统自带，同时也有语法提示
* ninja做了依赖，文件检查，并行

**Cons**
* 造轮子成本
* 分布式编译协议 可引入分布式编译器
* 编译链接速度

**RoadMap**
* cgn改为后台常驻模式，动态dlopen/dlclose，避免每次启动重新加载，中型工程约400个target对应400次dlopen，开销很大

## PUBLIC-RULE
* filegroup, sh_binary

## C, C++
* cxx_executable / cxx_shared / cxx_static
* clang ThinLTO
* msvc incremental build

## Protobuf, gRPC
* proto_cxx / proto_py / proto_js / grpc_cxx

## NodeJS
* system npm/yarn required
* node-modules inside CGN-WORKING-DIR

## NASM
* `@third_party//nasm`

## 从这里开始
**BUILD**
`ninja -C cgn.d` 编译输出到 cgn.d/build/cgn

demo: `./cgn.d/build/cgn build //hello:demo`

**对比 Chrome GN**
* CGN 支持函数, dep返回值
* 支持C++全部语法

**对比 Bazel/Buck**
* bzl注册函数，没法在analyze阶段先分析后返回结果
* bzl把分析表达式当作lambda代入target声明
* bzl在每个RULE未进入前, 不能立刻拿到结果, 而CGN可以 (实时结果)

**cgn**
```cpp
rust_library("mylib", x) {
    x.srcs = ["lib1.rs"]
    if (x.cfg["os"] == "win")
        x.srcs.append("lib1_win.rs");
}
```

* *target factory* (类似于其他编译工具的'target') 在 BUILD.cgn.cc 中定义, cell在 .cgn_init 文件中定义 该文件定义了 *SETUP字段* 该字段指向的文件中 定义了初始 *configuration* ，作为`x.cfg`传入所有 BUILD.cgn.cc 的代码片段
* *Interpreter* 在 xxx.cgn.h / .cc 中: 生成  obj/target_dir/build.ninja 用来编译特定target
    * *interpreter*一般分为 header和implement, header例如 *//cgn.d/library/rust.h* 定义了结构体，供所有人依赖
* "Interpreter脚本 *//cgn.d/library/shell.cgn.rsp*" 和 "定义具体*target factory*的 BUILD.cgn.cc" 都称为 *CGN script*

**analyze**
* 所有 `.cgn.cc` / `.cgn.h` 文件均运行在analyze阶段
* 有一入口程序cgn.exe (cgn.d/entry/cli.cpp) , 每个 `BUILD.cgn.cc` 会被cgn.exe 实时编译并 `dlopen(libBUILD.cgn.so)`, 然后运行其中的 *target factory function* 代码段, 该代码段指导 obj/target_dir/build.ninja 生成, 供后续 *build* 使用

**build**
例如运行 `//hello:demo` 是shell-script (//cgn.d/library/shell.cgn.cc) 其configID为 `FFFF9B7C`
相当于`ninja -f cgn-out/obj/main cgn-out/obj/hello_/demo_FFFF9B7C/demo.stamp`
