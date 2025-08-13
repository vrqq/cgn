
## Feature
    * `cxx_sources(cxx::CxxSourcesContext)`
    * `cxx_static(cxx::CxxStaticContext)`
    * `cxx_shared(cxx::CxxSharedContext)`
    * `cxx_executable(cxx::CxxExecutableContext)`
	* `cxx_prebuilt(cxx::PrebuiltContext)`
    * clang ThinLTO
    * TODO: msvc incremental build

**.add_dep()**
* `cxx::private_dep` 仅对自己有效 : 依赖项不向上传递
* `cxx::inherit` 对自己和有效 且public : 依赖项尽可能向上传递
* `cxx::pack_obj` 特殊flag 尽可能的将从dep来的obj/static_lib打包
	* 尽量在当前target消费掉 `LinkAndRunInfo.static` 和 `LinkAndRunInfo.object` 不向上传递, 若消费不掉 再遵循`private_dep` 和 `inherit` 两个flag决定是否传递.
* `LinkAndRunInfo.runtime` 无论哪种flag, 均截止至`cxx_executable()`并复制到同文件夹

**(CxxInfo)this 和 this.pub**
* `(CxxInfo)this` 仅对自己有效 不对外public
* `this.pub` 仅对外有效 不对自己生效

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
    * TODO: Linux 系统可 symbolic link, Windows 因权限问题只能copy

## C/C++ interpreter 处理流程
仅处理dep上游传来的 `BuildAndRunInfo` 和 `CxxInfo`, 其余一律转发.
下文缩写: `dep` from dep-input, `rv` 当前target的输出, `self_buildarg` 作用于当前target的编译参数, `move()`表示将该项目移走并清空

```
// cxx::pack_obj 不起作用
cxx_sources(x) {
	for dep.BuildAndRunInfo
		dep => rv
	for priv_dep dep.CxxInfo
		dep => self_buildarg
	for inherit dep.CxxInfo
		dep => self_buildarg + rv[CxxInfo]
	gen-ninja: x.src => x.obj => rv[BuildAndRunInfo].obj 
}


// Option : pack_obj 优先控制 是否将dep.obj打包进当前.a
cxx_static(x) {
	for pack_obj dep.BuildAndRunInfo
		move(dep.obj) => self.src
	for inherit dep.BuildAndRunInfo
		dep => rv[BuildAndRunInfo]
	for priv_dep dep.CxxInfo
		dep => self_buildarg
	for inherit dep.CxxInfo
		dep => self_buildarg + rv[cxxInfo]
	gen-ninja: x.src => x.a => rv[BuildAndRunInfo].a
}

// Option : pack_obj 优先控制 是否将dep.obj + wholearchive(dep.static) 打包进当前.so
cxx_shared/cxx_executable(x) {
	for priv_dep dep.CxxInfo
		dep => self_buildarg
	for inherit dep.CxxInfo
		dep => self_buildarg + rv[CxxInfo]
	for pack_obj dep.BuildAndRunInfo
		move(dep.static) => wholearchive into x.so
		move(dep.object) => x.src
	for private/inherit dep.BuildAndRunInfo
		self_buildarg.ldflags += "rpath=dep.shared" (UNIX and NOT-PKG)
		self_buildarg.ldflags += dep.shared + dep.object + dep.static
		dep.runtime => rv[BuildAndRunInfo].runtime (for SHARED target)
		exec("cp dep.runtime => x.out_folder") && clear(dep.runtime) (for EXECUTABLE target)
	for inherit dep.BuildAndRunInfo
		dep => rv[BuildAndRunInfo]
	x.src => x.so/x.exe => rv[BuildAndRunInfo].so + rv[BuildAndRunInfo].rt
	if target==WIN and x.src.contain(".mainfest"): (both PKG and NOT-PKG)
		x.so => target_out/{manifest_name}/x.so
		rv[BuildAndRunInfo].rt = {"manifest_pkg_name/x.dll"}
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
}
```

**可能的改进 (TODO) **
* `cxx_sources(x)`
	* inherit : 暴露 dep.CxxInfo
	* privcfg : 隐藏 dep.CxxInfo 例如从当前target间接调用dep内函数
* `cxx_static(x)`
	* packobj + inherit : dep.obj -> thisrv.static && 暴露 dep.CxxInfo
	* nopack  + inherit : dep.obj -> thisrv.obj    && 暴露 dep.CxxInfo
	* packobj + privcfg : dep.obj -> thisrv.static && 不暴露 dep.CxxInfo && 削减 ranlib
	* nopack  + privcfg : dep.obj -> thisrv.obj    && 不暴露 dep.CxxInfo
* `cxx_shared/cxx_executable(x)`
	* privdep : dep.obj 抹掉导出表
	* inherit : dep.obj 正常link
