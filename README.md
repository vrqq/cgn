# Announcement
[Why Not Recommand](WHY_NOT_RECOMMAND.md)

目前正在改动 `@cell` 以及 configuration 处理：拟增加 remove unnecessary entry in cfg[] for interpreter

# CGN
受GN和bazel/buck启发，用 C++11 编写编译脚本(BUILD.cgn.cc)，并由cgn.exe扫描目录并自动将每一个`BUILD.cgn.cc` 编译为独立dll，然后由cgn.exe依次dlopen后，自动解析target然后生成ninja脚本。 
之后使用`cgn build @cell//folder:target`编译  

**conceptions**  
* factory label: `@third_party//protobuf:protoc`
    * 
* script label: `@cgn.d//library/cxx.cgn.bundle`
    * CGN支持3种script表述方式: 以.bundle结尾的文件夹, 以.rsp结尾的文件, 和以 .cc结尾的单脚本文件
    * CGN利用类似ninja的 `/showIncludes` 处理 .h的引入
* WorkingRoot: CWD where to run cgn.exe
* cgn-out: CGN的输出位置 含build.ninja，compile_commands.json等
* Target输出路径:    `cgn-out/obj/<FOLDER-NAME>_/<FACTORY-NAME>_<ConfigID>`
    * 例如 `cgn-out/obj/folder1_/factoryname_1A2B3C4D`
    * target-ninja: `cgn-out/obj/folder1_/factoryname_1A2B3C4D/build.ninja`
    * entry:        `cgn-out/obj/main.ninja`
    * $OUT in-ninja: `cgn-out/obj/folder1_/factoryname_1A2B3C4D/hello.o`
    * $IN in-ninja:  `folder1/hello.cpp`
* Configuration存储路径: `cgn-out/configurations/<ConfigID>.cfg`
    * 例如 `cgn-out/configurations/1A2B3C4D.cfg`
* CGN-Script中间路径:  `cgn-out/analysis/FOLDERNAME/libSCRIPTFILENAME.cgn.so`
    * 例如 cgn-out/analysis/folder1/libBUILD.cgn.so 编译于 `folder1/BUILD.cgn.cc`
    * script-include: `cgn-out/cell_include`
    * -DCGN_VAR_PREFIX: `folder1_20_20`
    * -DCGN_ULABEL_PREFIX: `//folder1:`
* 所有有关路径的string 无论在ninja文件中还是任何地方 都和系统相关
    * windows使用backslash'\'，unix 使用 slash'/'
* 所有label均使用slash'/'分割
    * @cell//folder/file.cpp
    * @cell//folder:label
    

**Pros**
* 受buck2启发，既然buck2仅是starlark解释器，那么c++语言就是天然的解释器，只要规定好脚本习惯，即可扩充其他支持的语言
* 无需考虑语法解释器 和新的语言语法，c++大多数系统自带，同时也有语法提示
* ninja做了依赖，文件检查，并行编译

**Cons**
* 造轮子成本
* 分布式编译协议 可引入分布式编译器
* cgn-script 的c++编译链接速度令人发指

**RoadMap**
* cgn改为后台常驻模式，动态dlopen/dlclose，避免每次启动重新加载，中型工程约400个target对应400次dlopen，开销很大

## 拟支持的 interpreter 第一期
**PUBLIC-RULE**  
* `@cgn.d//library/shell.cgn.rsp`
* filegroup, sh_binary

**C, C++**  
* `@cgn.d//library/cxx.cgn.bundle`
* cxx_executable / cxx_shared / cxx_static
* clang ThinLTO
* msvc incremental build

**Protobuf, gRPC**  
* `@cgn.d//third_party/proto.cgn.cc`
* proto_cxx / proto_py / proto_js / grpc_cxx

**NodeJS**  
* system npm/yarn required
* node-modules inside CGN-WORKING-DIR

**NASM**  
* `@third_party//nasm`

## 从这里开始
**working-root**
整个project的根目录

**cell**
一个独立的package例如 `@cgn.d`，名称以 `@`开头，外部库通常以 cell 的形式引入。所有cell必须要在`working-root`下，无论是直接放置还是mklink。
* 某个文件也可以不属于任何cell，这样的话就不能供给其他cell调用，例如`//hello/cpp1/main.cpp`。
* 跨cell引用必须使用带`@cell`的label，例如`@third_party//spdlog`。
* 接上一条，不允许出现`//@`开头的label，也就是说working-root下所有以@开头的文件夹都是imported cell，working-root下不允许出现以@开头的普通文件。
* 如果`@`出现在label的中段，则认为他是普通的folder_name，例如 `//proj1/@folderX/fileY`
* cgn为monorepo设计，通常建议任何新开工程都有自己的project folder。

**BUILD**
`ninja -C cgn.d` 编译输出到 cgn.d/build/cgn

shdemo: `./cgn.d/build/cgn build //hello:demo`
cppdemo: `./cgn.d/build/cgn build //hello:cpp`

**对比 Chrome GN**
* CGN 支持函数, dep返回值
* 支持C++全部语法

**对比 Bazel/Buck**
* bzl注册函数，没法在analyze阶段先分析后返回结果
* bzl把分析表达式当作lambda代入target声明
* bzl在每个RULE未进入前, 不能立刻拿到结果, 而CGN可以 (实时结果)

**BUILD.cgn.cc example**
```cpp
// 'mylib' is 'target factory'
rust_library("mylib", x) {
    x.srcs = {"lib1.rs"};
    if (x.cfg["os"] == "win")
        x.srcs += {"lib1_win.rs"};
}
```

* *target factory* (类似于其他编译工具的'target') 在 BUILD.cgn.cc 中定义, cell在 .cgn_init 文件中定义 该文件定义了 *SETUP字段* 该字段指向的文件中 定义了初始 *configuration* ，作为`x.cfg`传入所有 BUILD.cgn.cc 的代码片段
* *Interpreter* 在 xxx.cgn.h / .cc 中: 生成  obj/target_dir/build.ninja 用来编译特定target
    * *interpreter*一般分为 header和implement, header例如 *//cgn.d/library/rust.h* 定义了结构体，供所有人依赖
* "Interpreter脚本 *//cgn.d/library/shell.cgn.rsp*" 和 "定义具体*target factory*的 BUILD.cgn.cc" 都称为 *CGN script*

**analyze**
* 所有 `.cgn.cc` / `.cgn.h` 文件均运行在analyze阶段
* 有一入口程序cgn.exe (cgn.d/entry/cli.cpp) , 每个 `BUILD.cgn.cc` 会被cgn.exe 实时编译并 `dlopen(libBUILD.cgn.so)`, 然后运行其中的 *target factory function* 代码段, 该代码段指导 `obj/target_dir/build.ninja` 生成, 供后续 *build* 使用
* analyse时需指定 target_factory_label + configuration
    * 通过cli运行时 configuration 默认为 DEFAULT

**build**
例如 运行 `//hello:demo` with `cfg["DEFAULT"]` 是 shell-script (//cgn.d/library/shell.cgn.cc) 其configID为 `FFFF9B7C`
相当于`ninja -f cgn-out/obj/main.ninja cgn-out/obj/hello_/demo_FFFF9B7C/demo.stamp`

**build argument**
* `cgn build //some/label --target=target_str` 其中 target_str 是由逗号分隔的字符串, 该字符串由 `CGN_SETUP()` 解析至 `named_configs["DEFAULT"]` 从而指导各interpreter生成ninja代码
* OneOf(gcc, msvc, llvm) : OPT, linux默认gcc, win默认msvc
* OneOf(debug, release) : OPT, 默认release
* OneOf(win, mac, linux), OneOf(x86, x86_64, arm64) : OPT, 默认host
* OneOf(msvc_MD, msvc_MD) : OPT, 仅windows下自动填充, debug to MDd, release to MD
* OneOf(CONSOLE, WINDOW) : OPT, 仅windows下自动填充 CONSOLE

**named config**
* DEFAULT: 应由 `cgn_setup()` 生成, 作为默认的编译参数
* host_release: 建议由 `cgn_setup()` 生成, 通常在跨平台时编译本地toolchain

## Configurations
* os(win, mac, linux)
* cpu(x86, x86_64, arm64, ia64, mips64)
* optimization(debug, release)
* msvc_subsystem(UNDEFINED, WINDOW, CONSOLE), msvc_runtime(UNDEFINED, MD, MDd), llvm_stl(UNDEFINED, libstdc++, libc++), march(UNDEFINED, skylake, skylake-avx512, native, ...)
* c/c++: toolchain(gcc, llvm, msvc), asan(T, F), msan(T, F), ubsan(T, F), cxx_prefix(STRING), cxx_sysroot(STRING)

**Configuration Generator**
* default entry: //cgn.d/library/cgn_default_setup.cgn.cc
* user can create custom entry and include the default entry to prepare custom config.

## TargetInfos (Provider)

**DefaultInfo**
* .outputs[] 仅当前target的输出文件 例如 a.exe b.dll c.so d.txt
* User: Package() target 会收集指定target的 outputs[] 并打包到当前输出

**LinkAndRunInfo**
* 由于接近系统的语言为c/cpp系列 此处暂不考虑其他语言的中间输出
* .object 编译半成品 通常 windows下.obj(PE-format)  linux下.o(ELF)
* .shared 动态库 windows下.lib(COFF) 注意不含.dll(PE), linux下.so(ELF)
* .static 静态库 windows下.lib(COFF), linux下.a
* .runtime 运行时 例如 win动态库带的.dll, exe需要读取的配置文件.ini
    * win动态库.dll/.exe等等 默认情况放在'/' 和exe同位置, 他们会根据例如 cfg['pkg-mode'] 改变输出的位置
* dll不放shared原因: 这个info给编译器看的，若target-os是windows, 即使gcc.exe可以直接链接dll 但我们选择遵循windows习惯. 注意这个习惯面向target-os, 故不包含例如arduino的cross-compile情况.

**CxxInfo**
* .ldflags 仅针对由 cxx_toolchain linking时用的flag，若有其他interpreter的linker 需要在其 lang_toolchain 额外选择。.so / .dll / .lib / .obj 在各个语言有可能通用，也可以使用cxx_interpeter支持的linker链接其他的语言编译出的中间文件，但这里的.ldflags 也只针对 cxx_interpreter，我们可以有很多个interpreter 都带各种各样的linker，他们的linker侧重点也不同，虽然他们的input/output可以重合。

**BinDevelInfo**
收集文件至 `include_dir`, `lib_dir`，而非CxxInfo内散落的路径，一般按照xxx-devel install后的目录样子。给例如 cmake() nmake() 引入依赖用，这些外部project编译时指定某个第三方库的include_dir只能指定一个目录，不能指定多个。当然也可以把所有路径都加到INCLUDE_DIR中假装是默认的。

## 常用rules说明 (所有cgn.d/library的rule)
简易使用说明/目录再此更新，dev-implement说明在bundle内README
部分rule开发中

**dir_package()**
`@cgn.d//library/general.cgn.bundle`
将其内部提到的target的outputs[]全部copy到当前target的输出文件夹

**zip_package()**
`@cgn.d//library/general.cgn.bundle`
与dir_package()类似 将其输出的文件夹打包成zip，需要依赖系统zip.exe 或者在target内部指定archiver，可指定例如 `@third_party//zlib:exe`
支持 zip, 7z, gzip, bzip2, tar, tar.gz

**git_fetch()**
`@cgn.d//library/general.cgn.bundle`
从git拉指定版本的repo，需要依赖系统git.exe 亦可指定 `@third_party//libgit2:exe` 但此处涉及鸡生蛋蛋生鸡问题。

