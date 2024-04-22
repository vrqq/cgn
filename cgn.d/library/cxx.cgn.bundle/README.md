
## Feature
    * `cxx_sources(cxx::CxxSourcesContext)`
    * `cxx_static(cxx::CxxStaticContext)`
    * `cxx_shared(cxx::CxxSharedContext)`
    * `cxx_executable(cxx::CxxExecutableContext)`
    * clang ThinLTO
    * TODO: msvc incremental build

## interpreter 接受的 config
见 cgn.d/README.md

## interpreter 输出规范
* 所有 target 均直接在 `out_prefix` 中输出 .o / .a / .so / .lib 等
* 仅 cxx_executable() 才将 `deps_info[BuildAndRunInfo].runtime` 复制到 `out_prefix` 中
    * TODO: Linux 系统可 symbolic link, Windows 因权限问题只能copy

## C/C++ interpreter 处理流程
仅处理dep上游传来的 `BuildAndRunInfo` 和 `CxxInfo`, 其余一律转发.
下文缩写 `brin => BuildAndRunInfo from dep-input`, `brin.rt => [BuildAndRunInfo].runtime`, `cxin => CxxInfo from dep-input` 等等

```
cxx_sources(x)
	for private/inherit BuildAndRunInfo：
		brin => rv[BrInfo]
	for private CxxInfo
		cxin => self_buildarg
	for cxin as public CxxInfo
		cxin => rv[CxxInfo] + self_buildarg
	x.src => x.obj => rv[BrInfo].obj

cxx_static(x)
	for private/inherit BuildAndRunInfo:
		move(brin.obj) => x.src
		brin.a / brin.so / brin.rt => rv[brInfo]
	for private CxxInfo
		cxin => self_buildarg
	for inherit CxxInfo
		cxin => self_buildarg + rv[cxxInfo]
	x.src => x.a => rv[brInfo].a

cxx_shared/cxx_executable(x)
	for private CxxInfo:
		cxin => self_buildarg
	for inherit CxxInfo:
		cxin => self_buildarg + rv[CxxInfo]
	for private/inherit BuildAndRunInfo：
		self_buildarg.ldflags += "rpath=brin.so" (UNIX and NOT-PKG)
		self_buildarg.ldflags += brin.so + move(brin.obj + brin.a)
		brin.rt => rv[brInfo].rt (SHARED ONLY)
		exec("cp brin.rt => x.out_folder") (EXECUTABLE ONLY)
	for inherit BuildAndRunInfo：
		brin.so => rv[brInfo].so
		self_buildarg.ldflags += "/wholearchive:brin.a"
	x.src => x.so/x.exe => rv[brInfo].so + rv[brInfo].rt
	if target==WIN and x.src.contain(".mainfest"): (both PKG and NOT-PKG)
		x.so => target_out/{manifest_name}/x.so
		rv[brInfo].rt = {"manifest_pkg_name/x.dll"}
		rv[CxxInfo].ldflags += "/manifestdependency:x.manifest"
	if target==WIN: (both PKG and NOT-PKG)
		x.so => target_out/x.so
	if target==UNIX and PKG-mode:
		x.so => target_out/{target_name}/x.so
		rv[CxxInfo].ldflags += "rpath={target_name}"
		rv[brInfo].rt += "x.so -> {target_name}/x.so"
```