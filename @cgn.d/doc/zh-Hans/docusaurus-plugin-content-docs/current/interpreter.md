
# Rule-Interpreter
`@cgn.d//library/` 以及来自 `@third_party//` 的规则列表

* Interpreter 规则解释器: 解析 `target-factory` + `config` 组合并将其翻译成 `.ninja` 文件


## cxx_sources(), cxx_shared(), cxx_static(), cxx_executable() cxx_interperter
`@cgn.d//library/cxx.cgn.bundle` 生成执行c++编译器的ninja 文件  
`include_dirs[]`搜索顺序: 当前`context.include_dirs[]`排在前面, 其余根据`context.add_dep()`调用顺序依次排列.
`ldflags, cflags`在编译命令的参数顺序: 先是`context.add_dep()`, 最后才是当前`context.include_dirs[]`, 因为针对大部分编译器 这个参数都是后面覆盖前面.

**config**
* `os`, `cpu` and `optimization` : see also 通用规则
* `cxx_toolchain`: `xcode`, `llvm`, `gcc`, `msvc`
    * Host=MacOS cxx_toolchain == `xcode` : XCode.app/clang and OS bsd linker       (UP!)
    * Host=MacOS cxx_toolchain == `llvm`  : clang and llvm-linker
    * Host=Linux cxx_toolchain == `llvm`  : clang and llvm linker (ld.lld)          (UP!)
    * Host=Linux cxx_toolchain == `gcc`   : gcc and binutil-ld                      (UP!)
    * Host=Win   cxx_toolchain == `llvm`  : VS-Inside clang-cl.exe and lld-link.exe (UP!)
    * Host=Win   cxx_toolchain == `msvc`  : VS-Inside cl.exe and link.exe           (UP!)
    * Host=Win   cxx_toolchain == `gcc`   : gcc.exe and ld.exe
* `cxx_prefix`: 编译工具链前缀 默认为空
* `cxx_sysroot` : 制定target OS的sysroot, 通常用于交叉编译
* `llvm_stl` : `libc++` or empty
* `msvc_runtime` : `MD`, `MDd`, `MT`, `MTd`
* `msvc_subsystem` : `CONSOLE`, `WINDOW`

**props**
* `srcs[]` : 源代码, 相对于当前BUILD.cgn.cc的相对路径, 或直接指定绝对路径.
* `perferred_binary_name`: 针对 cxx_sources() 没有作用, 对其他有效果.
* `include_dirs[]` : 仅作用于当前target的 include_dir, 靠前的优先.
* `cflags[], ldflags[]` : 仅作用于当前target的 cflags, 后面的覆盖前面的.
* `pub.*` 仅提供给外部 不作用于当前target.

**Interpreter Returns**
* `.outputs[]` : 当前target输出的文件
* `CxxInfo[]` : 来自 inherit继承 和 `context.pub`
* `LinkAndRunInfo[]` : 来自 当前target的产物 以及 非order_only继承减去当前target消耗的剩余

## cmake(), cmake_config(): cmake_interpreter
`@cgn.d//library/cmake.cgn.cc` 执行cmake.exe编译外部工程

**props**
* `sources_dir` : CMakeLists.txt 所在的目录 (基于当前文件夹的相对路径)
* `vars` : 传给 cmake 的变量 例如 `zlib_STATIC=OFF`
* `outputs` : cmake install 预期产生的输出文件 (基于install_dir的相对路径)，在interpreter 内解析该列表用于指导返回值，例如 `lib/libz.a` 将生成 `TargetInfos[LinkAndRunInfo].static = {"<install_dir>/lib/libz.a"}`
* `pub` : 作用于interpreter返回值的 `TargetInfos[CxxInfo]` 注意其中 `CxxInfo.include_dirs[]` 将自动新增一条 `<install_dir>/include`

**config**
* `os`, `cpu` and `optimization` : see also 通用规则, 对应cmake变量 `CMAKE_C_COMPILER_TARGET`, `CMAKE_CXX_COMPILER_TARGET`
* `msvc_runtime` : `MD`, `MDd`, `MT`, `MTd`, 对应cmake变量 `CMAKE_MSVC_RUNTIME_LIBRARY`
* `sysroot` : 对应cmake变量 `CMAKE_SYSROOT`
* `cmake_exe` : fullpath of cmake.exe binary file

**Interpreter Return**
* `DefaultInfo.output` : 仅一项 `["<install_dir>"]`
* `CxxInfo` : 来自 `Context.pub`
* `BinDevelInfo`

## nmake()
windows only `@cgn.d//library/nmake.cgn.h` 以`nmake.exe`编译外部项目
example: `@third_party//perl:perl_win`

## GitFetcher
从git拉制定版本的repo, 需要依赖外部程序 `git.exe`, 在成功pull后 会创建`<repo>.stamp`作为命令执行标记, 如需重新pull, 需要手工删除此文件.

**props**
* dest_dir 目标文件夹
* repo GIT地址 例如 `https://github.com/fmtlib/fmt.git`
* commit_id 要拉取的版本 例如`df249d8ad3a9375f54919bdfa10a33ae5cba99a2`
* post_script 在成功pull之后, 需要执行的本地命令(例如patch)


## filegroup()
把文件收集起来 `@cgn.d//library/general.cgn.bundle/bin_devel.cgn.cc`

**props**
* `add()` 将src_files的文件, 保持src_files[]相对于src_dir的相对路径, 放入dst_dir内
* `flat_add()` 将src_files[] 平放进 dst_dir 内, 无任何子目录
* `flat_add_rootbase()`
* `add_target_dep()` 

**Interpreter Return**
* `.outputs[]` 仅一项，为目标文件夹


## bin_devel()
`@cgn.d//library/general.cgn.bundle/bin_devel.cgn.h` 收集文件和target编译结果，生成`BinDevelInfo` 

**Interpreter Return**
* `.outputs[]` 仅一项，为目标文件夹
* `BinDevelInfo`


