## Features

	* `cxx_sources(name, x)` — Compiles source files to object files. No standalone artifact.
	* `cxx_static(name, x)` — Creates a static library (`.a` / `.lib`).
	* `cxx_shared(name, x)` — Creates a shared library (`.so` / `.dll`).
	* `cxx_executable(name, x)` — Creates an executable. Copies all `.runtime` deps to the output folder.
	* `cxx_prebuilt(name, x)` — Wraps pre-built binaries/headers into the CGN dependency graph.
	* clang ThinLTO
	* TODO: MSVC incremental build
	* TODO: Linux symbolic link for .runtime (Windows 因权限问题只能copy)
	* private语义: 生成dll，是否在打包时 obj/.a 同时 抹掉内部的dllexport？目前dep.static和dep.obj内自己定义是否export （例如使用ranlib）

## `.add_dep()` flags

| Flag | Meaning |
|------|---------|
| `cxx::private_dep` | (default) Consume dep's `CxxInfo` and `LinkAndRunInfo` for the current target only; do not propagate upstream. (仅对自己有效, 依赖项不向上传递)|
| `cxx::inherit` | Consume dep's info for the current target AND propagate it upstream to all consumers. (对自己和有效 且依赖项尽可能向上传递)|
| `cxx::archive` | *(static lib only)* Pack dep's object files / static libs into the current target. Prefers consuming `LinkAndRunInfo.static` and `LinkAndRunInfo.object` at this level; any remainder follows `private_dep`/`inherit` rules. |
| `cxx::order_dep` | Build-order dependency only; all `TargetInfo` from dep is discarded. |
| `cxx::_no_whole` | *(shared lib only)* Do not use whole-archive linking on dep's static library. |

Flags can be combined: `cxx::inherit | cxx::archive`.

`LinkAndRunInfo.runtime` entries are always stopped at `cxx_executable()` and copied to the same output folder, regardless of which flag is used.

## `this` vs `this.pub` (CxxInfo fields)

| Field | Scope |
|-------|-------|
| `x.srcs`, `x.defines`, `x.include_dirs`, `x.cflags`, `x.ldflags` | Applied **only** to the current target; not visible to consumers. (对自己有效) |
| `x.pub.defines`, `x.pub.include_dirs`, `x.pub.cflags`, `x.pub.ldflags` | Exported to all consumers via `inherit`; **do not include ** to the current target itself. (对外有效)|

* 若需要同时对自己和对外有效则需要同时设置这两组值

## Feature
    * `cxx_sources(cxx::CxxSourcesContext)`
    * `cxx_static(cxx::CxxStaticContext)`
    * `cxx_shared(cxx::CxxSharedContext)`
    * `cxx_executable(cxx::CxxExecutableContext)`
	* `cxx_prebuilt(cxx::PrebuiltContext)`

## interpreter 接受的 config
见 cgn.d/README.md

**ROADMAP**
* MacOS cxx_toolchain == xcode : XCode.app/clang and OS bsd linker       (UP!)
* MacOS cxx_toolchain == llvm  : clang and llvm-linker
* Linux cxx_toolchain == llvm : clang and llvm linker (ld.lld)  		 (UP!)
* Linux cxx_toolchain == gcc  : gcc and binutil-ld  					 (UP!)
* Win   cxx_toolchain == llvm : VS-Inside clang-cl.exe and lld-link.exe  (UP!)
* Win   cxx_toolchain == msvc : VS-Inside cl.exe and link.exe  			 (UP!)
* Win   cxx_toolchain == gcc  : gcc.exe and ld.exe

## interpreter 输出规范
* 所有 target 均直接在 `out_prefix` 中输出 .o / .a / .so / .lib 等
* 仅 cxx_executable() 才将 `deps_info[BuildAndRunInfo].runtime` 复制到 `out_prefix` 中
    * 

## interpreter 接受的 config
见 cgn.d/README.md

**ROADMAP**
* MacOS cxx_toolchain == xcode : XCode.app/clang and OS bsd linker       (UP!)
* MacOS cxx_toolchain == llvm  : clang and llvm-linker
* Linux cxx_toolchain == llvm : clang and llvm linker (ld.lld)  		 (UP!)
* Linux cxx_toolchain == gcc  : gcc and binutil-ld  					 (UP!)
* Win   cxx_toolchain == llvm : VS-Inside clang-cl.exe and lld-link.exe  (UP!)
* Win   cxx_toolchain == msvc : VS-Inside cl.exe and link.exe  			 (UP!)
* Win   cxx_toolchain == gcc  : gcc.exe and ld.exe

## C/C++ interpreter 处理流程
仅处理dep上游传来的 `BuildAndRunInfo` 和 `CxxInfo`, 其余一律转发.
下文缩写: `pub` from dep-input, `rv` 当前target的输出, `self_buildarg` 作用于当前target的编译参数, `move()`表示将该项目移走并清空

```
cxx_sources(x) {
	for all_dep.BuildAndRunInfo
		all_dep => rv[BuildAndRunInfo]
	for priv_dep.CxxInfo
		priv_dep => self_buildarg
	for inherit_dep.CxxInfo
		inherit_dep => self_buildarg + rv[CxxInfo]
	gen-ninja: x.src => x.obj => rv[BuildAndRunInfo].obj 
}

cxx_static(x) {
	for _archive_dep.BuildAndRunInfo
		move(dep.obj) => self.src
	for inherit_dep.BuildAndRunInfo
		inherit_dep => rv[BuildAndRunInfo]
	for priv_dep.CxxInfo
		priv_dep => self_buildarg
	for inherit_dep.CxxInfo
		inherit_dep => self_buildarg + rv[cxxInfo]
	gen-ninja: x.src => x.a => rv[BuildAndRunInfo].a
}

cxx_shared/cxx_executable(x) {
	for priv_dep.CxxInfo
		dep => self_buildarg
	for inherit.CxxInfo
		dep => self_buildarg + rv[CxxInfo]
	for _no_whole_dep.BuildAndRunInfo
		move(dep.static) => x._self_no_whole_archive
	for all_dep.BuildAndRunInfo
		move(dep.obj) => self.src (for linking)
		move(dep.static) => wholearchive into x.so (for linking)
		self_buildarg.ldflags += "rpath=dep.shared" (UNIX and NOT-PKG)
		self_buildarg.ldflags += dep.shared (for linking)
		dep.runtime => rv[BuildAndRunInfo].runtime (for SHARED target)
		exec("cp dep.runtime => x.out_folder") && clear(dep.runtime) (for EXECUTABLE target)
	for inherit dep.BuildAndRunInfo
		dep => rv[BuildAndRunInfo]
	gen-ninja: x.src => x.so/x.exe => rv[BuildAndRunInfo].so + rv[BuildAndRunInfo].rt
	if target==WIN and x.src.contain(".mainfest"): (both PKG and NOT-PKG)
		pack manifest into current output.
}
```