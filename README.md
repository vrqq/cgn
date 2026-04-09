# What is CGN
`cgn` is a modern, object-oriented build system that supports multiple languages (TODO) and complex multi-platform builds.  
参考了chrome-gn, bazel 和 buck 的设计思路, 仅使用C++ compiler做build target管理, 不引入其他语言, 可以理解为在 Chrome-GN 的基础上增加target返回值 以及多编程语言支持.

## evolution
1. 早先使用Makefile, 通过环境变量控制编译参数
2. 之后使用CMake, 可以做很多复杂的环境判断, 从而完成工程编译. CMake仍然是 procedure oriented的, 大量函数以及新老版本的交替, 让人很难用好它, 只能靠外部脚本扩展
3. 下一代是bazel/buck, 我认为其只是把cmake结构化了, 依旧是procedure oriented, 通过调用函数声明target, 或者调用函数声明target+config的组合. 其混乱的概念 我觉得不是好的设计. 为了速度使用starlark, 导致扩展也只能靠外部脚本. 一切的起点都是函数调用.
4. 下一代是chrome gn, 我觉得划时代的设计来了, object oriented的魅力, 每个 ninja target 通过 `user_function(arg: config)` 生成, 简单明了.

一直在使用chrome gn, 但是仍决定自己造一套轮子, 这些工具链的缺点:
* chrome gn 的`user_function`没有返回值, 很难优雅的多层依赖, 尤其跨config时重复编译. (例如在debug环境编译release 版本的perl.exe)
* bazel 的predefined c++ rule 命名混乱, 可能是历史遗留问题, 遇到打包需求很难扩展, 并且其面向过程(procedure oriented) 的声明式定义, 可以看作是cmake的python dialect, 用完gn再看它真的不是一个好的设计.

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

**Project Structure**
```
your_monorepo/
├── @cgn.d/                   # CGN framework (git submodule)
│   ├── library/              # Built-in interpreters
│   │   ├── cxx/              # C++ compiler integration
│   │   ├── external/         # CMake, NMake support
│   │   └── utility/          # Custom commands, etc.
│   ├── cgn.h                 # Main header file
│   └── v1/                   # Core implementation
├── @third_party/             # External dependencies (git submodule)
├── @myproject/               # Your project cell
│   └── src/
│       ├── BUILD.cgn.cc      # A build definition
│       └── main.cc           # Source code
├── debug.sh                  # Debug build script
├── release.sh                # Release build script
├── cgn-out/                  # Build output directory
│   ├── obj/                  # Object files & intermediate outputs
│   ├── bin/                  # Executables
│   └── lib/                  # Libraries
└── .git/                     # Monorepo git repository
```

---

## Architecture

CGN processes build definitions in four stages:

```
BUILD.cgn.cc files
      │
      ▼  (1) ScriptCC: compiled by cgn's built-in C++ compiler
    .so / .dll  (one per BUILD.cgn.cc)
      │
      ▼  (2) dlopen + factory registration via CGN_RULE_DEFINE
    Factory functions in memory
      │
      ▼  (3) Factory called with Configuration → generates build.ninja
    cgn-out/obj/.../build.ninja
      │
      ▼  (4) ninja executes actual compilation
    Final binaries / libraries
```

**Key insight**: unlike Chrome-GN, each factory function returns a `CGNTarget` with typed `InfoTable` data. This allows multi-layer dependencies and cross-config builds (e.g. build-and-run a host-release-mode `protoc` in `debug` mode project).

### Core Concepts

| Term | Meaning |
|------|---------|
| **cell** | A top-level folder prefixed with `@`, e.g. `@myproject`, `@third_party`. Each cell is an sub-repo in the monorepo in usually. |
| **script** | A `.cgn.cc` file (or `.cgn.bundle` directory) that is compiled into a shared library by CGN. |
| **factory** | A named function registered inside a script via `api.bind_target_factory`. Identified by a label like `@cell//dir:name`. |
| **interpreter** | A pre-defined C++ struct (e.g. `CxxInterpreter`) that implements the actual build logic. Factories configure interpreters. |
| **target** | The result of calling a factory with a specific `Configuration`. Stored in `cgn-out/obj/…/name_HASHID/`. |
| **config** | A `std::unordered_map<string,string>`. |
| **label** | Identifies a factory: `@cell//dir/subdir:name`. The `//` separates the cell root from the path. Omitting `:name` defaults to the last path segment. |
| **CGNPath** | A path tagged with a base: `BASE_ON_SCRIPT` (relative to BUILD.cgn.cc), `BASE_ON_OUTPUT` (relative to target's output dir), or `BASE_ON_WORKINGROOT` (relative to monorepo root). |

### Label Syntax

```
@cell//dir/subdir:target_name   full label
@cell//dir/subdir               shorthand: target_name == "subdir"
:target_name                    same cell and directory as current script
//dir:name                      same cell as current script
../other:name                   relative to the current script directory
```

### Configuration and `--target`

The `--target` argument (e.g. `--target llvm,debug,asan`) is parsed by `cgn_default_setup.cgn.hxx` into a `Configuration` named `DEFAULT`. If you don't use 'cgn_default_setup.cgn.hxx', you can use any argument to cgn cli instead of `--target`.

The `cgn_default_setup.cgn.hxx` provide the `--target` to `config` below:

| Token | Config key | Possible values |
|-------|-----------|-----------------|
| `linux` / `mac` / `win` | `os` | auto-detected if omitted |
| `x86_64` / `x86` / `arm64` | `cpu` | auto-detected if omitted |
| `llvm` / `gcc` / `msvc` / `xcode` | `cxx_toolchain` | platform default if omitted |
| `debug` / `release` | `optimization` | `release` if omitted |
| `asan` / `tsan` / `ubsan` / `msan` / `lsan` | `cxx_asan` etc. | empty if omitted |
| `msvc_MD` / `msvc_MT` / `msvc_MDd` / `msvc_MTd` | `msvc_runtime` | `MD`(rel)/`MDd`(dbg) |
| `CONSOLE` / `WINDOW` | `msvc_subsystem` | `CONSOLE` |
| `libc++` | `llvm_stl` | `libstdc++` |

Additional auto-set keys: `host_os`, `host_cpu`, `host_shell`.

The `host_release` named config always targets the host machine at `release` optimization, used by cross-compilation scenarios (e.g. building a host `protoc` during cross-compilation).

### Interpreter Reference

Include `@cgn.d/library/cgn_library_all.cgn.h` (or individual headers) to access these rules:

| Rule | Header | Description |
|------|--------|-------------|
| `cxx_sources(name, x)` | `library/cxx/cxx.cgn.h` | Compiles .c/.cpp to object files only; no standalone artifact. |
| `cxx_static(name, x)` | `library/cxx/cxx.cgn.h` | Creates a static library (`.a` / `.lib`). |
| `cxx_shared(name, x)` | `library/cxx/cxx.cgn.h` | Creates a shared library (`.so` / `.dll`). |
| `cxx_executable(name, x)` | `library/cxx/cxx.cgn.h` | Creates an executable. Copies all `.runtime` deps to the output directory. |
| `cxx_prebuilt(name, x)` | `library/cxx/cxx.cgn.h` | Wraps pre-built binaries/headers into the CGN dep graph. |
| `git(name, x)` | `library/utility/git_fetch.cgn.h` | Clones/updates a git repository at a specific commit. Idempotent via mtime stamp. |
| `shell_script(name, x)` | `library/utility/shell_script.cgn.h` | Runs arbitrary shell commands as a build step. Generates a stamp file. |
| `alias(name, x)` | `library/utility/general.cgn.h` | Redirects to another label; can override `cfg` for cross-config builds. |
| `group(name, x)` | `library/utility/general.cgn.h` | Aggregates multiple targets under one label. |
| `run_exec(name, x)` | `library/utility/general.cgn.h` | Runs a built executable as a build step. |
| `cmake(name, x)` | `library/external/cmake.cgn.h` | Wraps an external CMake project. Returns `CxxInfo` + `LinkAndRunInfo`. |
| `nmake(name, x)` | `library/external/nmake.cgn.h` | Wraps an external NMake project (Windows). |

### Developer Guide

**Minimal BUILD.cgn.cc**

```cpp
#include <cgn>   // or #include "@cgn.d/library/cgn_library_all.cgn.h"

// Fetch the source
git("mylib.git", x) {
    x.repo = "https://github.com/example/mylib.git";
    x.commit_id = "abc123def456...";
    x.dest_dir = "repo";
}

// Build a static library
cxx_static("mylib", x) {
    x.srcs = {"repo/src/mylib.cpp"};
    x.pub.include_dirs = {"repo/include"};  // exported to consumers
    x.include_dirs = {"repo/src"};          // private, only for this target
    x.add_dep(":mylib.git", cxx::order_dep);
}

// C/C++ Shared Library
cxx_shared("myshared", x) {
    x.srcs = {"src/impl.cc"};
    x.pub.include_dirs = {"include"};
    x.add_dep("@third_party//openssl", cxx::private_dep);
}

// Build an executable that uses it
cxx_executable("myapp", x) {
    x.srcs = {"main.cpp"};
    x.add_dep(":mylib", cxx::inherit);
}
```


**Cross-config (enforce config name)**

```cpp
// Build dep_tool always in host release mode, regardless of the current x.cfg[]
alias("host_tool", x) {
    x.actual_label = "@third_party//perl";
    x.load_named_config("host_release");
}
```

**Referencing the output of another target**

```cpp
shell_script("my_build", x) {
    auto tool = x.quick_dep_namedcfg("@third_party//perl", "host_release", false);
    std::string perl_exe = tool.outputs[0];
    // ... use perl_exe in shell commands
    // Here we got host-release version perl.exe, even if the current config is debug or cross-compiling for another platform.
}
```

**Error handling in factories**

```cpp
shell_script("winhook", x) {
    if (x.cfg["os"] != "win")
        return x.opt->set_fail("Only windows supported.");
    // ...
}
```

---

# Appendix
**Proxy guide for Linux (for V2RayA)**
Edit ~/.ssh/config with below (which 192.168.122.1:20170 is V2RayA socks proxy port)
```
Host github.com
    User git
    ProxyCommand nc --proxy-type socks4 --proxy 192.168.122.1:20170 %h %p
```
Then call `git config --global http.proxy http://192.168.122.1:20172` to set a proxy for https://github.com. (192.168.122.1:20172 is the V2RayA http proxy address and port)

