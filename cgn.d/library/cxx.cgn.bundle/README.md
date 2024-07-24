
## Feature
    * `cxx_sources(cxx::CxxSourcesContext)`
    * `cxx_static(cxx::CxxStaticContext)`
    * `cxx_shared(cxx::CxxSharedContext)`
    * `cxx_executable(cxx::CxxExecutableContext)`
    * clang ThinLTO
    * TODO: msvc incremental build

**.add_dep()**
* `cxx::private_dep` 仅对自己有效
* `cxx::inherit` 对自己和有效 且public

**(CxxInfo)this 和 this.pub**
* `(CxxInfo)this` 仅对自己有效 不对外public
* `this.pub` 仅对外有效 不对自己生效

## interpreter 接受的 config
见 cgn.d/README.md

**ROADMAP**
* MacOS cxx_toolchain == xcode : XCode.app/clang and OS bsd linker  (UP!)
* MacOS cxx_toolchain == llvm  : clang and llvm-linker
* Linux cxx_toolchain == llvm : clang and llvm linker (ld.lld)  (UP!)
* Linux cxx_toolchain == gcc  : gcc and binutil-ld  (UP!)
* Win   cxx_toolchain == llvm : VS-Inside clang-cl.exe and lld-link.exe  (UP!)
* Win   cxx_toolchain == msvc : VS-Inside cl.exe and link.exe  (UP!)
* Win   cxx_toolchain == gcc  : gcc.exe and ld.exe

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
	// Option : 控制CxxInfo是否暴露

cxx_static(x)
	for private/inherit BuildAndRunInfo:
		move(brin.obj) => x.src
		brin.a / brin.so / brin.rt => rv[brInfo]
	for private CxxInfo
		cxin => self_buildarg
	for inherit CxxInfo
		cxin => self_buildarg + rv[cxxInfo]
	x.src => x.a => rv[brInfo].a
	// Option 1: CxxInfo是否向上暴露
	// Option 2: 是否将dep的.obj打包进当前.a

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
	// Option : 控制CxxInfo是否暴露 + 是否 /Wholearchive:.a
	//
	// Reason : 由于interpreter不晓得源码内写了哪些dllexport 故
	//			CxxInfo暴露时 就认为 dep.static 的函数也需要暴露
	//			dep.shared 同理 暴露给上游 link
	//			即使 obj 没打进当前dll 他也会随着TargetInfo 到上游
	//			从而引发潜在的 symbol-collection
	//			一般 dll/exe 还独立发布 (处理全部
	//		    DYNDEP-DLL 之后就不含 undefined symbol 了)
	//
	//			故private语义 需打包 obj/.a 同时 抹掉内部的dllexport

```

**可能的改进**
* `cxx_sources(x)`
	* inherit : 暴露 dep.CxxInfo
	* privcfg : 隐藏 dep.CxxInfo 例如从当前target间接调用dep内函数
* `cxx_static(x)`
	* packobj + inherit : dep.obj -> this.static && 暴露 dep.CxxInfo
	* nopack  + inherit : dep.obj -> this.obj    && 暴露 dep.CxxInfo
	* packobj + privcfg : dep.obj -> this.static && 不暴露 dep.CxxInfo && 削减 ranlib
	* nopack  + privcfg : dep.obj -> this.obj    && 不暴露 dep.CxxInfo
* `cxx_shared/cxx_executable(x)`
	* privdep : dep.obj 抹掉导出表
	* inherit : dep.obj 正常link  && /wholearchive:pub.a
