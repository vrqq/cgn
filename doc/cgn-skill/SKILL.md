---
name: cgn-build-system
description: |
  Expert guide for writing, debugging, and understanding BUILD.cgn.cc files in the CGN
  (CPP Generate Ninja) build system. Activates when working with BUILD.cgn.cc files,
  *.cgn.cc scripts, or any file in the @cgn.d, @third_party, or user @cell directories.
  Provides interpreter reference, templates, error diagnosis, proper noun definitions,
  and LLM notes for each built-in interpreter.
applyTo:
  - "**/BUILD.cgn.cc"
  - "**/*.cgn.cc"
  - "**/*.cgn.h"
  - "**/*.cgn.hxx"
---

# CGN Build System — Expert Skill

## Quick Reference

**Full architecture reference**: `@cgn.d/doc/ARCHITECTURE.md`
**Main include**: `#include <cgn>` or `#include "@cgn.d/library/cgn_library_all.cgn.h"`

---

## Proper Noun Glossary

| Term | Definition |
|------|-----------|
| **cell** | Top-level monorepo folder prefixed with `@` (e.g. `@myproject`, `@third_party`). Each cell is a logical module. |
| **script** | A `.cgn.cc` file compiled into a shared library (`.so`/`.dll`) by CGN. |
| **factory** | A named function registered in a script via `CGN_RULE_DEFINE`. Identified by a label. |
| **interpreter** | A pre-defined C++ struct implementing build logic (e.g. `CxxInterpreter`). Factories configure interpreters; interpreters write ninja files. |
| **target** | The result of calling a factory with a specific `Configuration`. Stored in `cgn-out/obj/cell_/dir_/name_HASHID/`. |
| **config** / `Configuration` | A key-value map tracking which keys were read. Only read keys affect the output directory hash — unread keys are "trimmed". |
| **trimmed config** | The subset of `Configuration` keys actually read by the factory. Determines the 8-char hex hash in the output directory name. |
| **label** | Identifies a factory: `@cell//dir/subdir:name`. Two-slash separates cell from path; colon separates path from name. |
| **CGNPath** | A tagged path: `BASE_ON_SCRIPT` (relative to `BUILD.cgn.cc`), `BASE_ON_OUTPUT` (relative to target output dir), `BASE_ON_WORKINGROOT` (relative to monorepo root). |
| **named config** | A `Configuration` pre-built in `cgn_setup.cgn.cc` and retrieved by name (e.g. `"DEFAULT"`, `"host_release"`). |
| **`pub`** | Fields on a context struct (e.g. `x.pub.include_dirs`) that are exported to consumers via `inherit`. Contrast with private fields that only affect the current target. |
| **`cgn-out`** | The build output directory, containing `obj/`, `bin/`, `lib/`. Never commit this directory. |
| **`out_prefix`** | The concrete filesystem path to a specific target's output directory (e.g. `cgn-out/obj/cell_/dir_/name_ABCD1234/`). |
| **InfoTable** | A typed map stored in `CGNTarget`. Interpreters write info objects into it; consumers read via `target.get<InfoType>()`. |
| **`opt->confirm()`** | Locks the `Configuration`, computes the hash, registers the target, and returns `CGNTargetMaker*`. Returns `nullptr` on cache hit (target already built). |
| **`opt->set_fail(msg)`** | Marks the target as failed. Factory must return immediately after calling this. |
| **adep** | Analysis dependency: an edge in the CGN dependency graph between `GraphNode`s. Set automatically when `add_dep()` or `create_target()` is called. |
| **`host_release`** | Named config that always targets the host machine in release mode. Used when building host tools (e.g. `perl`, `nasm`, `protoc`) during cross-compilation. |

---

## Label Syntax

```
@cell//dir/subdir:target_name   Full label (always unambiguous)
@cell//dir/subdir               Shorthand: target_name == last dir segment
:target_name                    Same cell + directory as current BUILD.cgn.cc
//dir:name                      Same cell, different directory
../other:name                   Relative to current BUILD.cgn.cc directory
```

---

## `--target` Token Parsing

`--target llvm,debug,asan` → comma-separated tokens consumed by `config_guessor()`:

| Token(s) | Sets config key | Default |
|---|---|---|
| `linux` / `mac` / `win` | `os` | host OS |
| `x86_64` / `x86` / `arm64` | `cpu` | host CPU |
| `llvm` / `gcc` / `msvc` / `xcode` | `cxx_toolchain` | platform default |
| `debug` / `release` | `optimization` | `release` |
| `asan` / `tsan` / `ubsan` / `msan` / `lsan` | `cxx_asan` etc. | `""` |
| `msvc_MD` / `msvc_MT` / `msvc_MDd` / `msvc_MTd` | `msvc_runtime` | `MD`/`MDd` |

---

## Template Library

### Template 1 — Header-Only Library (e.g. nlohmann/json, asio)

```cpp
#include <cgn>

git("mylib.git", x) {
    x.repo      = "https://github.com/example/mylib.git";
    x.commit_id = "abc123def456abc123def456abc123def456abc12";
    x.dest_dir  = "repo";
}

cxx_sources("mylib", x) {
    x.pub.include_dirs = {"repo/include"};
    // No srcs needed for header-only.
}
```

### Template 2 — Static Library

```cpp
#include <cgn>

cxx_static("mylib", x) {
    x.srcs             = {"src/a.cpp", "src/b.cpp"};
    x.include_dirs     = {"src"};          // private: only for compiling mylib itself
    x.pub.include_dirs = {"include"};      // exported: available to all consumers
    x.defines          = {"BUILDING_MYLIB"};
    x.pub.defines      = {"MYLIB_STATIC"};
}
```

### Template 3 — Executable with Dependencies

```cpp
#include <cgn>

cxx_executable("myapp", x) {
    x.srcs = {"main.cpp"};
    x.add_dep("@third_party//asio",        cxx::inherit);     // header-only, inherit pub fields
    x.add_dep("@third_party//openssl",     cxx::inherit);     // static lib, inherit and link
    x.add_dep(":mylib",                    cxx::private_dep); // local dep, private
}
```

### Template 4 — Fetch + Shell Build + Prebuilt Wrapper

```cpp
#include <cgn>

// Step 1: fetch source
git("mylib.git", x) {
    x.repo = "https://github.com/example/mylib.git";
    x.commit_id = "...";
    x.dest_dir = "repo";
}

// Step 2: build with shell script
shell_script("mylib_build", x) {
    auto git_dep = x.add_dep(":mylib.git", x.cfg, false);

    // Read config before confirm()
    std::string build_type = (x.cfg["optimization"] == "debug") ? "Debug" : "Release";

    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk) return;

    std::string install_dir = api.rebase_path(cgn::make_path_base_out("install"), "", mk);
    std::string src_dir     = api.rebase_path("repo", ".", x.opt->src_prefix);
    api.mkdir(install_dir);

    x.worker.append_pushd(cgn::make_path_base_out("build"), mk);
    x.worker.append_cmd({"cmake", src_dir,
        "-DCMAKE_BUILD_TYPE=" + build_type,
        "-DCMAKE_INSTALL_PREFIX=" + install_dir});
    x.worker.append_cmd({"cmake", "--build", ".", "--target", "install"});
    x.worker.append_popd();

    x.analysis_outputs = {cgn::make_path_base_out("install")};
}

// Step 3: expose as cxx_prebuilt
cxx_prebuilt("mylib", x) {
    auto build = x.add_dep(":mylib_build"); // implicit x.cfg
    auto instdir = build.outputs[0];

    x.pub.include_dirs = {cgn::make_path_base_working(instdir + "/include")};
    x.files = {
        cgn::make_path_base_working(instdir + "/lib/libmylib.a"),
    };
}
```

### Template 5 — CMake External Project

```cpp
#include <cgn>
#include "@cgn.d/library/external/cmake.cgn.h"

git("mylib.git", x) { x.repo = "..."; x.commit_id = "..."; x.dest_dir = "repo"; }

cmake("mylib", x) {
    x.sources_dir = "repo";
    x.vars["BUILD_SHARED_LIBS"] = "OFF";
    x.vars["BUILD_TESTING"]     = "OFF";
    x.outputs = {"install/lib/libmylib.a"};
    x.pub.include_dirs = {x.vars["CMAKE_INSTALL_PREFIX"] + "/include"};
}
```

### Template 6 — Platform-Conditional Alias

```cpp
alias("openssl", x) {
    if (x.cfg["cxx_toolchain"] == "msvc")
        x.actual_label = ":openssl_shared";
    else
        x.actual_label = ":openssl_static";
}
```

### Template 7 — Cross-Config Host Tool

```cpp
// Always build perl in host release mode regardless of the current config.
alias("host_perl", x) {
    x.actual_label = "@third_party//perl";
    x.load_named_config("host_release");
}
```

### Template 8 — Group (Multiple Targets)

```cpp
#include "@cgn.d/library/utility/general.cgn.h"

group("all_git", x) {
    x.add_deps({
        "@third_party//asio:asio.git",
        "@third_party//grpc:grpc.git",
        "@third_party//openssl:openssl.git",
    });
}
```

---

## `add_dep()` Flag Reference

| Flag | Type | Effect |
|------|------|--------|
| `cxx::private_dep` | default | Consume dep's `CxxInfo` and `LinkAndRunInfo` for current target only. Not propagated upstream. |
| `cxx::inherit` | public | Consume AND propagate dep's info to all consumers of the current target. |
| `cxx::archive` | packing | Pack dep's object files / static libs into the current static library. |
| `cxx::order_dep` | ordering | Build-order dep only; all `TargetInfo` from dep is discarded. |
| `cxx::_no_whole` | linking | (shared lib only) Do not use whole-archive linking for this dep's static library. |

Flags can be combined with `|`:
```cpp
x.add_dep(":sublib", cxx::inherit | cxx::archive);
```

---

## `this` vs `this.pub`

```cpp
cxx_static("example", x) {
    // Applied ONLY when compiling "example" itself:
    x.srcs         = {"src/impl.cpp"};
    x.include_dirs = {"src/private"};
    x.defines      = {"BUILDING_EXAMPLE"};
    x.cflags       = {"-Wno-deprecated"};
    x.ldflags      = {};  // usually empty for static libs

    // Exported to ALL consumers of "example" via cxx::inherit:
    x.pub.include_dirs = {"include"};
    x.pub.defines      = {"EXAMPLE_STATIC"};
    x.pub.cflags       = {};
    x.pub.ldflags      = {};
}
```

---

## Error Diagnosis Guide

### `'DEFAULT' config not found`
- **Cause**: `cgn_setup.cgn.cc` did not define a config named `"DEFAULT"`.
- **Fix**: Ensure `cgn_setup.cgn.cc` contains `#include "@cgn.d/library/cgn_default_setup.cgn.hxx"` or manually defines `x.configs["DEFAULT"]`.

### `cycle-dependency`
- **Cause**: Target A depends on B, B depends on... A.
- **Fix**: Use `alias` with a different `cfg` to break the self-dependency. Common pattern: have a host tool alias use `load_named_config("host_release")`.

### `Configuration locked.`
- **Cause**: Code accessed `x.cfg["some_key"]` after calling `x.opt->confirm()`.
- **Fix**: Move ALL `x.cfg[...]` reads to BEFORE the `opt->confirm()` call, or using `x.cfg.visit_keys(...)` before.

### Build script fails silently after `opt->confirm()`
- **Cause**: `opt->confirm()` returned `nullptr` (cache hit) — but the factory continued executing.
- **Fix**: Always check: `if (!mk) return;` immediately after `opt->confirm()`.

### `dlopen failed` / script compile error
- **Cause**: Syntax error in a `.cgn.cc` file.
- **Fix**: Run `cgn analyse @cell//dir:target` and check stderr for compiler errors. The script compilation error appears before any analysis output.

### Missing include / unresolved symbol in script
- **Cause**: Forgot to include the interpreter header.
- **Fix**: Add the appropriate `#include` (see Interpreter Reference below). Most users can just use `#include "@cgn.d/library/cgn_library_all.cgn.h"`.

### CMake build fails
- **Cause**: Usually missing CMake variables or wrong source dir.
- **Fix**: Check `x.sources_dir` points to the directory containing `CMakeLists.txt`. Add any needed `x.vars["CMAKE_XXX"] = "..."`.

---

## LLM Notes per Interpreter

### `cxx_sources` — Compile to Objects Only

**Intent**: Compile C/C++ source files to object files without creating any standalone artifact. Use as a building block to share compile flags and headers across related targets.

**Required fields**:
- `x.srcs` — list of `.c`/`.cpp`/`.cc`/`.s` files

**Optional fields**:
- `x.pub.include_dirs` — headers exported to consumers
- `x.pub.defines` — macros exported to consumers
- `x.include_dirs`, `x.defines`, `x.cflags` — private compile options

**Output info**: `LinkAndRunInfo.object_files[]` — consumed by `cxx_static` (via `archive`) or `cxx_shared`/`cxx_executable` (via `inherit` / `private_dep`).

**Gotcha**: `cxx_sources` outputs object files, not a `.a`. If you use `cxx::inherit` on a `cxx_sources` dep from a `cxx_static`, the objects are passed through — they are NOT automatically archived. Use `cxx::archive` explicitly if you want them packed.

---

### `cxx_static` — Static Library

**Intent**: Compile sources and package them into a `.a` (Unix) or `.lib` (Windows). The canonical reusable library type.

**Key fields**:
- `x.srcs` — source files
- `x.pub.include_dirs` — exported headers
- `x.add_dep(label, cxx::archive)` — pack dep's objects into this `.a`
- `x.perferred_binary_name` — override the default `lib<name>.a` filename

**Output info**: `LinkAndRunInfo.static_files[]`

**Chain behavior with `add_dep` flags**:
- `private_dep`: dep's info used when compiling self, not propagated
- `inherit`: dep's info propagated to consumers of this static lib
- `archive`: dep's objects packed into this `.a` (consumed from `LinkAndRunInfo.object_files`)

**Gotcha**: Static libs do NOT copy runtime files (`.so`, `.dll`) to the output folder. Only `cxx_executable` does that.

---

### `cxx_shared` — Shared Library

**Intent**: Build a dynamically-linked shared library (`.so`/`.dylib`/`.dll`). Copies `runtime` deps (other `.so`/`.dll` files) to its output folder.

**Key fields**:
- `x.srcs` — source files
- `x.pub.include_dirs`, `x.pub.defines` — exported to consumers
- `x.add_dep(label, cxx::_no_whole)` — link specific static dep without whole-archive

**Output info**: `LinkAndRunInfo.shared_files[]` + `LinkAndRunInfo.runtime[]`

**Gotcha**: On Linux, uses `rpath` pointing to the output dir. On Windows, `.dll` imports use `__declspec(dllexport)` — symbols not explicitly exported from deps compiled into the shared lib may not be visible. This is why `private_dep` semantics strip the export table.

---

### `cxx_executable` — Executable Binary

**Intent**: Link everything into an executable. Terminates the dependency chain: nothing propagates upstream from an executable.

**Key fields**:
- `x.srcs` — source files (usually just `{"main.cpp"}`)
- `x.add_dep(...)` with any flag — all are consumed

**Special behavior**: Copies ALL `LinkAndRunInfo.runtime` entries (collected via `inherit` from deps) to the executable's output directory. This ensures DLLs/`.so` files are co-located with the executable.

**Output**: The executable binary path, available in `CGNTarget.outputs[0]`.

---

### `cxx_prebuilt` — Pre-Built Binary Wrapper

**Intent**: Integrate already-built binaries (from `shell_script`, `cmake`, system packages) into the CGN dependency graph.

**Key fields**:
- `x.files` — list of `.a`/`.so`/`.dll`/`.lib` files (as `CGNPath`)
- `x.pub.include_dirs` — header directories exported to consumers
- `x.system_libs` — system library names (e.g. `{"dl", "pthread"}`)
- `x.add_dep(label)` — add a build dep (e.g. the `shell_script` that produced the files)

**Pattern**: Add dep first (to get `outputs[0]` = install dir), then set `x.files` relative to that dir using `cgn::make_path_base_working(instdir + "/lib/...")`.

**Gotcha**: `x.files` must use absolute paths or `BASE_ON_WORKINGROOT` paths — not `BASE_ON_SCRIPT` paths — because the files are in the build output, not the source tree.

---

### `git` — Git Repository Fetch

**Intent**: Clone a git repository at a specific commit. Idempotent: if the repo already exists at the correct commit, does nothing.

**Required fields**:
- `x.repo` — full git remote URL (https or git+ssh)
- `x.commit_id` — full 40-char SHA-1 commit hash

**Optional fields**:
- `x.dest_dir` — destination directory (default: `"repo"`)
- `x.fetch_submodule` — fetch git submodules too
- `x.post_script.command` + `x.post_script.cwd` — run a command after fetch

**Output**: `CGNTarget.outputs[0]` = path to `dest_dir`.

**Config dependency**: Only reads `host_os` and `host_shell`. Config hash is effectively always `00000000` (platform-independent).

**Gotcha**: The git dep is needed for build ordering, but does NOT need explicit `add_dep` in the user factory — the subsequent `cxx_*` target will discover source files relative to `x.opt->src_prefix` which already contains the fetched dir.

---

### `shell_script` — Arbitrary Shell Build

**Intent**: Run a sequence of shell commands as a ninja build step. Output is a stamp file; the actual build outputs are declared in `x.analysis_outputs`.

**Key fields**:
- `x.worker.append_setenv(key, val)` — `export KEY=val`
- `x.worker.append_pushd(path, mk)` — `cd path`
- `x.worker.append_popd()` — `cd -`
- `x.worker.append_cmd(args_vec)` — add one command (each arg shell-escaped)
- `x.worker.append_escaped_cmd(line)` — add pre-escaped command lines
- `x.extra_watch_files` — trigger re-run if these files change
- `x.script_outputs` — files produced during the ninja run (used as ninja outputs)
- `x.analysis_outputs` — directories/files that become `CGNTarget.outputs[]`

**Critical ordering**:
1. Call `x.add_dep(...)` and `x.quick_dep_namedcfg(...)` to get tool paths
2. Read ALL `x.cfg[...]` keys
3. Call `x.opt->confirm()` — AFTER this, config is locked
4. Set up `x.worker` commands using paths from step 1 and mk->out_prefix
5. Set `x.analysis_outputs`

**Gotcha**: Tool executables obtained via `quick_dep_namedcfg("tool", "host_release")` are host-native paths. Escape them with `api.shell_escape(path, x.cfg["host_shell"])` before embedding in command strings.

---

### `alias` — Label Redirect

**Intent**: Redirect one label to another, optionally with a different configuration. The primary way to implement platform-conditional selection and cross-config builds.

**Key fields**:
- `x.actual_label` — the label to redirect to (MUST be set)
- `x.cfg` — can be modified freely before setting `actual_label`
- `x.load_named_config("host_release")` — load a named config, returns false if not found

**Patterns**:
```cpp
// Platform selection
alias("mylib", x) {
    x.actual_label = (x.cfg["os"] == "win") ? ":mylib_win" : ":mylib_posix";
}

// Cross-config: always host release
alias("host_protoc", x) {
    x.actual_label = "@third_party//protobuf:protoc";
    x.load_named_config("host_release");
}

// Config override
alias("mylib_debug", x) {
    x.actual_label = ":mylib";
    x.cfg["optimization"] = "debug";
}
```

**Gotcha**: `x.actual_label` must be set — if left empty, the alias fails silently. Always validate the label is non-empty.

---

### `cmake` — CMake External Build

**Intent**: Build an external project that uses CMake. Pre-populates CMake variables from the CGN `Configuration` (compiler, build type, sysroot for cross-compilation).

**Key fields**:
- `x.sources_dir` — path to the directory containing `CMakeLists.txt` (relative to `src_prefix`)
- `x.vars` — extra CMake variables to set (string→string map)
- `x.outputs` — expected output files relative to `out_prefix`; used as ninja outputs
- `x.pub` — `CxxInfo` to export to consumers
- `x.add_dep(label)` — additional deps (order included automatically if `enforce_havedep_mode=true`)

**Auto-set CMake vars**: `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`, `CMAKE_BUILD_TYPE`, `CMAKE_INSTALL_PREFIX`, and cross-compilation vars when `cfg["os"]` != `cfg["host_os"]`.

**Output info**: `CMakeInfo` (install/binary/source dirs), `CxxInfo`, `LinkAndRunInfo`

**Gotcha**: `enforce_havedep_mode = true` (default) adds all dep ninja entries as explicit deps in the CMake build rule. Set to `false` only if CMake generates its own dep tracking.

---

### `group` — Target Aggregator

**Intent**: Build multiple targets via a single label. Useful for `all_git`, `all_libs` aggregation targets.

**Key fields**:
- `x.add_deps({"label1", "label2", ...})` — adds all listed targets using current config
- `x.add_deps(labels, cfg)` — adds all targets using a specific config

**Output**: None directly — output is the union of all dep outputs.

---

### `run_exec` — Run Executable as Build Step

**Intent**: Execute a built binary as part of the build. Simpler alternative to `shell_script` when you just need to run one program.

**Key fields**:
- `x.cmd_build` — command line as a vector of strings
- `x.inputs` — input files (ninja deps)
- `x.outputs` — files produced by the run

---

## Common Mistakes

1. **Accessing `x.cfg[]` after `opt->confirm()`**: Always read all cfg keys before confirming.

2. **Using `BASE_ON_SCRIPT` paths for build outputs**: Output files are not in the source dir. Use `BASE_ON_OUTPUT` or `BASE_ON_WORKINGROOT`.

3. **Forgetting to set `x.analysis_outputs`** in `shell_script`: Without this, the target has no outputs and dependent targets cannot find files.

4. **Circular `alias` chains**: Don't have alias A point to alias B which points back to alias A with a different cfg — this is still a cycle.

5. **Using `add_dep` inside `cxx_prebuilt` with the wrong label format**: The `cxx_prebuilt::add_dep` uses `quick_dep` which respects `x.cfg`. Use `add_dep(label)` (no cfg arg) to inherit current cfg.

6. **Globbing sources with `**/*.cpp`**: CGN does NOT expand globs automatically. Use `api.file_glob(dir)` or list each file explicitly.

7. **Missing `x.extra_watch_files`** in `shell_script`: If you depend on an external tool (perl, cmake), add its ninja entry to `x.extra_watch_files` so the build reruns when the tool changes.
