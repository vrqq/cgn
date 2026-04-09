# CGN Architecture — AI Agent Reference

> **Purpose**: This document is the single authoritative reference for AI agents working in CGN monorepos. It covers every concept, type, interpreter, and pattern needed to read, write, and debug `BUILD.cgn.cc` files.

---

## 1. What Is CGN?

**CGN** (CPP Generate Ninja) is a modern, object-oriented meta-build system. It uses C++ as its build language (replacing Starlark / Python / Makefile), compiles `BUILD.cgn.cc` files into shared libraries on demand, and generates `build.ninja` files that Ninja then executes for actual compilation.

**Lineage**: Makefile → CMake → Bazel/Buck → Chrome-GN → **CGN**

**Key innovation over Chrome-GN**: factory functions return typed `CGNTarget` objects, enabling multi-layer dependencies and cross-configuration builds without re-compilation.

---

## 2. Build Pipeline

```
BUILD.cgn.cc  (user-written C++ build definitions)
      │
      ▼  Stage 1 – Script compilation
   cgn compiles BUILD.cgn.cc → .so / .dll
   (incremental: only rebuilds if .cgn.cc changed)
      │
      ▼  Stage 2 – Factory registration (dlopen)
   Factory functions registered in memory via CGN_RULE_DEFINE
      │
      ▼  Stage 3 – Analysis / ninja generation
   cgn calls each requested factory with a Configuration
   → each factory configures an interpreter
   → interpreter writes build.ninja to cgn-out/obj/…/
      │
      ▼  Stage 4 – Real compilation
   ninja reads all generated build.ninja files
   → produces final .o / .a / .so / .exe artifacts
```

---

## 3. Monorepo Layout

```
monorepo_root/
├── @cgn.d/                    # CGN framework (git submodule, do not modify)
│   ├── cgn.h                  # Main include for BUILD.cgn.cc files
│   ├── BUILD.cgn.cc           # Builds the cgn executable itself
│   ├── v1/                    # Core C++ implementation
│   │   ├── cgn_api.h/.cpp     # Public API: class CGN (dlopen'd)
│   │   ├── cgn_type.h/.cpp    # Core types: Configuration, CGNPath, CGNTarget*
│   │   ├── cli.cpp            # CLI entry point
│   │   ├── rule_macro.h       # CGN_RULE_DEFINE macro
│   │   ├── configuration.h    # Configuration class
│   │   ├── graph.h/.cpp       # Dependency graph (GraphNode)
│   │   ├── ninja_file.h/.cpp  # Ninja file writer
│   │   └── logger.h/.cpp      # Logging
│   ├── library/               # Built-in interpreters
│   │   ├── cgn_library_all.cgn.h   # Include ALL interpreters
│   │   ├── cgn_default_setup.cgn.h # --target parsing into Configuration
│   │   ├── cxx/               # C++ compiler interpreter
│   │   ├── utility/           # git, shell_script, alias, group, run_exec, copy
│   │   └── external/          # cmake, nmake
│   ├── root_example/          # Template scripts for monorepo root
│   │   ├── debug.sh / debug.bat
│   │   ├── release.sh / release.bat
│   │   ├── query.sh / query.bat
│   │   ├── run.sh
│   │   └── cgn_setup.cgn.cc   # Default: #include cgn_default_setup.cgn.hxx
│   ├── doc/
│   │   └── ARCHITECTURE.md    # This file
│   └── mcp/                   # MCP server for AI agent tool use
│       ├── BUILD.cgn.cc
│       ├── mcp_server.cpp
│       └── cgn_mcp_config.example.json
│
├── @third_party/              # External dependencies (git submodule)
│   ├── asio/BUILD.cgn.cc      # header-only lib example
│   ├── grpc/BUILD.cgn.cc      # complex multi-target example
│   ├── openssl/BUILD.cgn.cc   # shell_script + cxx_prebuilt example
│   └── …
│
├── @happybase/                # Example user cell (C++ utility library)
│   └── BUILD.cgn.cc
│
├── cgn_setup.cgn.cc           # Root setup (copies root_example template)
├── debug.sh / debug.bat       # Debug build entry point
├── release.sh / release.bat   # Release build entry point
└── cgn-out/                   # All build output (git-ignored)
    └── obj/
        └── @cell_/dir_/name_HASHID/
            ├── build.ninja
            └── *.o / *.a / *.so / …
```

---

## 4. Core Types

### `Configuration`
A lazy `unordered_map<string, string>` with access tracking. The set of keys actually **read** during a factory call determines the config hash — unread keys are stripped ("trimmed"). Two calls that read the same set of keys with the same values share one output directory.

```cpp
cfg["os"]           // read key "os" → moves it to visited set
cfg["os"] = "linux" // write key "os"
cfg.trim_lock()     // compute hash, freeze; no more writes or new reads
cfg.get_id()        // returns the 8-char hex hash used in directory names
```

**Important**: Writing a key or reading a unreaded-key after `opt->confirm()` (which calls `trim_lock()`) throws `"Configuration locked."`.

### `GraphNode`
An opaque node in the analysis dependency graph. Every script file, factory, and target has a node. Edges model dependency relationships used to detect cycles and determine when to rebuild.

### `CGNPath`
A tagged path for use in build definitions:

| Tag constant | Meaning | `to_string()` prefix |
|---|---|---|
| `BASE_ON_SCRIPT` | Relative to the `BUILD.cgn.cc` directory | `$(SCRIPT_DIR)` |
| `BASE_ON_OUTPUT` | Relative to the current target's output directory | `$(OUT_PREFIX)` |
| `BASE_ON_WORKINGROOT` | Relative to the monorepo root (where `cgn` runs) | _(none)_ |

```cpp
cgn::make_path_base_script("src/foo.cpp")      // BASE_ON_SCRIPT
cgn::make_path_base_out("install/lib/foo.a")   // BASE_ON_OUTPUT
cgn::make_path_base_working("cgn-out/bin/foo") // BASE_ON_WORKINGROOT
```

Use `api.rebase_path(path, new_base, maker)` to convert to a concrete string.

### `CGNTargetOpt`
The input specification for creating a target. Filled in automatically when using `CGN_RULE_DEFINE`. Key fields:

| Field | Description |
|---|---|
| `name` | Target name (last segment of the label) |
| `cfg` | Input `Configuration` |
| `src_prefix` | OS-sep path to the directory containing `BUILD.cgn.cc` |
| `out_parent_prefix` | OS-sep path to `cgn-out/obj/cell_/dir_/` (parent of the target dir) |
| `confirm()` | Locks the config, computes the hash, returns `CGNTargetMaker*` (or `nullptr` if cached) |
| `set_fail(msg)` | Sets an error on the target; factory should return immediately after |
| `get_cfg0_out_prefix()` | Returns path with config hash `00000000`, for platform-independent outputs |

### `CGNTargetMaker`
Returned by `opt->confirm()`. Only created the first time a given target is computed; subsequent identical calls return `nullptr` (cache hit).

| Field | Description |
|---|---|
| `ninja` | Pointer to `NinjaFile` for writing build rules (nullptr in analyse-only mode) |
| `out_prefix` | OS-sep path to this target's output directory |
| `out_prefix_unixsep` | Same but with `/` separators (for use in ninja files) |
| `outputs` | `vector<string>` — primary outputs (e.g. path to `.a` or `.exe`) |
| `file_unchanged` | If true, output files are identical to last build; skip re-writing ninja |

### `CGNTarget`
Returned by `api.create_target()` and `x.add_dep()`. Represents a completed analysis result.

| Field | Description |
|---|---|
| `errmsg` | Non-empty if the target failed |
| `ninja_entry` | The ninja build entry filename (for order-only deps) |
| `outputs` | Primary output files |
| `anode` | `GraphNode*` for this target |
| `get<T>()` | Retrieve a typed info object (e.g. `get<cxx::CxxInfo>()`, `get<cgn::LinkAndRunInfo>()`) |

### `TLRuntime` (Thread-Local Runtime)
Tracks the current call stack during `api.create_target()`. Used for cycle detection and automatic `adep` edge recording. Access via `api.get_debug_runtime()`.

### `InfoTable`
A typed map stored in `CGNTarget`. Interpreters write output info into it (e.g. `CxxInfo`, `LinkAndRunInfo`, `BinDevelInfo`). Consumers read it via `target.get<InfoType>()`.

---

## 5. Configuration Key Reference

All keys are set by `config_guessor()` in `library/cgn_default_setup.cgn.h`, parsed from the `--target` argument.

| Key | Example values | Default |
|-----|---------------|---------|
| `os` | `linux`, `mac`, `win` | Host OS |
| `cpu` | `x86_64`, `x86`, `arm64`, `ia64`, `mips64` | Host CPU |
| `host_os` | same as `os` | Always host |
| `host_cpu` | same as `cpu` | Always host |
| `host_shell` | `bash`, `zsh`, `cmd`, `powershell` | Auto-detected |
| `cxx_toolchain` | `gcc`, `llvm`, `msvc`, `xcode` | Platform default |
| `optimization` | `debug`, `release` | `release` |
| `cxx_asan` | `asan` or `""` | `""` |
| `cxx_tsan` | `tsan` or `""` | `""` |
| `cxx_ubsan` | `ubsan` or `""` | `""` |
| `cxx_msan` | `msan` or `""` | `""` |
| `cxx_lsan` | `lsan` or `""` | `""` |
| `msvc_runtime` | `MD`, `MDd`, `MT`, `MTd` | `MD`/`MDd` |
| `msvc_subsystem` | `CONSOLE`, `WINDOW` | `CONSOLE` |
| `cxx_winapi_winver` | `0x0A00` (Win10), `0x0601` (Win7) | `0x0A00` |
| `llvm_stl` | `libc++`, `libstdc++` | `libstdc++` |

**Named configs** (defined in `cgn_setup.cgn.cc`):
- `DEFAULT` — the configuration requested via `--target` (used by default)
- `host_release` — always the host OS + CPU in release mode; used when building host tools (e.g. `protoc`, `perl`) during cross-compilation

---

## 6. Label Syntax

```
@cell//dir/subdir:target_name    Full label
@cell//dir/subdir                Shorthand: target_name == last dir segment ("subdir")
:target_name                     Same cell + directory as current BUILD.cgn.cc
//dir:name                       Same cell, different directory
../other:name                    Relative to current BUILD.cgn.cc directory
```

**Cell** is the `@xxx` prefix, mapping to a top-level folder in the monorepo.
**`//`** separates the cell from the directory path.
**`:`** separates the directory from the target name.

---

## 7. Interpreter Catalogue

### 7.1 C++ Interpreters (`library/cxx/cxx.cgn.h`)

Include via: `#include "@cgn.d/library/cxx/cxx.cgn.h"` (or `cgn_library_all.cgn.h`)

#### `cxx_sources(name, x)`
- **Context**: `cxx::CxxSourcesContext` (role=`'o'`)
- **Output**: Object files only. No linkable artifact.
- **Key fields**: `x.srcs`, `x.defines`, `x.include_dirs`, `x.cflags`, `x.pub.*`
- **Use when**: Sharing compile flags/headers across multiple targets without creating a library.
- **Propagation**: `inherit` propagates `CxxInfo` and object files to dependents.

#### `cxx_static(name, x)`
- **Context**: `cxx::CxxStaticContext` (role=`'a'`)
- **Output**: `.a` (Unix) or `.lib` (Windows)
- **Key fields**: same as `cxx_sources` + `x.perferred_binary_name`
- **Use `cxx::archive`** to pack dep's object files into this `.a`.
- **Note**: Does NOT copy runtime deps.

#### `cxx_shared(name, x)`
- **Context**: `cxx::CxxSharedContext` (role=`'s'`)
- **Output**: `.so` (Linux), `.dylib` (macOS), `.dll` (Windows)
- **Copies `LinkAndRunInfo.runtime`** of all deps to its own output folder (for `dep.shared`).
- **Use `cxx::_no_whole`** to avoid whole-archiving a specific static dep.

#### `cxx_executable(name, x)`
- **Context**: `cxx::CxxExecutableContext` (role=`'x'`)
- **Output**: Executable binary; copies ALL `.runtime` deps to the same folder.
- **Terminates** the dependency chain: does not propagate `CxxInfo` or `LinkAndRunInfo` upstream.

#### `cxx_prebuilt(name, x)`
- **Context**: `cxx::PrebuiltContext`
- **Key fields**: `x.files` (list of `.a`/`.so`/`.dll` files), `x.pub.include_dirs`, `x.system_libs`
- **Use when**: Wrapping system libraries or outputs of `shell_script`/`cmake` builds.
- **Pattern**: Typically receives output dir from a `shell_script` or `cmake` dep via `x.add_dep(":build_target")`, then sets `x.files` from `target.outputs[0]`.

---

### 7.2 Utility Interpreters

#### `git(name, x)` — `library/utility/git_fetch.cgn.h`
- **Context**: `GitContext`
- **Key fields**:
  - `x.repo` — git remote URL
  - `x.commit_id` — exact commit hash (SHA-1)
  - `x.dest_dir` — destination directory (default: `"repo"`)
  - `x.fetch_submodule` — whether to also clone submodules
  - `x.post_script.command` / `x.post_script.cwd` — optional post-clone command
- **Output**: The `dest_dir` path (string).
- **Idempotent**: Uses mtime stamp; only re-fetches if commit changes.
- **Config**: Only reads `host_os` and `host_shell`; config hash is effectively `00000000`.

#### `shell_script(name, x)` — `library/utility/shell_script.cgn.h`
- **Context**: `ShellScript::context_type`
- **Key fields**:
  - `x.worker` — `ShellScriptWorker` instance for building the script
  - `x.extra_watch_files` — files that trigger a re-run when changed
  - `x.script_outputs` — files produced during the ninja build step
  - `x.analysis_outputs` — directories/files exposed as the target's `outputs`
- **`x.worker` methods**:
  - `append_setenv(key, value)` — set environment variable
  - `append_pushd(path, maker)` — cd into directory
  - `append_popd()` — cd back
  - `append_cmd(args_vec)` — add a command (args are shell-escaped automatically)
  - `append_escaped_cmd(line)` — add a pre-escaped command string
  - `append_stamp(path)` — write a stamp file (done automatically by interpreter)
- **Use when**: Building external projects (automake, custom scripts) that CGN doesn't natively support.
- **Important**: Call `x.opt->confirm()` AFTER building all shell commands, then fill `script_outputs` and `analysis_outputs`.

#### `alias(name, x)` — `library/utility/general.cgn.h`
- **Context**: `AliasInterpreter::AliasContext`
- **Key fields**:
  - `x.actual_label` — the label to redirect to
  - `x.cfg` — can be modified to redirect with a different config
  - `x.load_named_config("host_release")` — loads a named config from `cgn_setup.cgn.cc`
- **Use when**: Providing a platform-aware entry point (e.g. `alias("openssl", x)` picks static or shared based on `x.cfg["cxx_toolchain"]`), or cross-config builds.

#### `group(name, x)` — `library/utility/general.cgn.h`
- **Context**: `GroupInterpreter::GroupContext`
- **Key fields**: `x.add_deps({list_of_labels})` — build all listed targets
- **Use when**: Aggregating multiple targets (e.g. `all_git` group that fetches all repos).

#### `run_exec(name, x)` — `library/utility/general.cgn.h`
- **Context**: `RunExecInterpreter::context_type`
- **Key fields**: `x.cmd_build`, `x.inputs`, `x.outputs`
- **Use when**: Running a prebuilt executable as a build step (simpler than `shell_script`).

---

### 7.3 External Build Interpreters

#### `cmake(name, x)` — `library/external/cmake.cgn.h`
- **Context**: `CMakeContext`
- **Key fields**:
  - `x.sources_dir` — path to the directory containing `CMakeLists.txt`
  - `x.vars` — CMake variables (pre-populated with compiler, build type, sysroot)
  - `x.outputs` — list of expected output files (`.a`, `.so`, `.dll`)
  - `x.pub` — `CxxInfo` to expose to consumers
- **Auto-set CMake vars**: `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`, `CMAKE_BUILD_TYPE`, `CMAKE_INSTALL_PREFIX`, `CMAKE_SYSROOT` (for cross-compilation)
- **Returns**: `CMakeInfo` (binary/source/install dirs), `CxxInfo`, `LinkAndRunInfo`

#### `nmake(name, x)` — `library/external/nmake.cgn.h`
- Windows-only. Similar pattern to `cmake` but for NMake-based projects.

---

## 8. `add_dep()` Flag Semantics

### In `cxx_sources(x)`:
- `private_dep`: dep's `CxxInfo` → apply to `self_buildarg` only
- `inherit`: dep's `CxxInfo` → `self_buildarg` + propagate in `rv[CxxInfo]`

### In `cxx_static(x)`:
- `archive` (primary control): pack `dep.obj` → `self.src` (i.e. archive into `.a`)
- `inherit`: dep's `BuildAndRunInfo` → propagate via `rv[BuildAndRunInfo]`
- `private_dep`: dep's `CxxInfo` → `self_buildarg` only

### In `cxx_shared(x)` / `cxx_executable(x)`:
- `private_dep`: dep's `CxxInfo` → `self_buildarg`
- `inherit`: dep's `CxxInfo` → `self_buildarg` + propagate in `rv[CxxInfo]`
- `archive`: `move(dep.static)` → wholearchive into `.so`; `move(dep.obj)` → `self.src`
- `_no_whole`: dep's `.a` linked without whole-archive
- `LinkAndRunInfo.runtime` is **always** stopped at `cxx_executable()` and copied to its output folder

---

## 9. QuickDepContext — Convenience Dependency API

Most interpreter context structs inherit from `cgn::QuickDepContext`, which provides:

```cpp
// Get a dep using the current target's config
cgn::CGNTarget quick_dep(const std::string &label, const cgn::Configuration &cfg, bool keep_anode=true);

// Get a dep using a named config from cgn_setup.cgn.cc
cgn::CGNTarget quick_dep_namedcfg(const std::string &label, const std::string &cfg_name, bool keep_anode=true);
```

The `ShellScript::context_type::add_dep()` wraps `quick_dep` and also adds the dep's ninja entry to `extra_watch_files` automatically.

---

## 10. Common Patterns

### Pattern 1: Fetch + Build + Prebuilt
```cpp
git("mylib.git", x) {
    x.repo = "..."; x.commit_id = "..."; x.dest_dir = "repo";
}
shell_script("mylib_build", x) {
    auto git = x.add_dep(":mylib.git", x.cfg, false);
    // ... build commands ...
    x.analysis_outputs = {cgn::make_path_base_out("install")};
}
cxx_prebuilt("mylib", x) {
    auto build = x.add_dep(":mylib_build"); // uses x.cfg
    x.pub.include_dirs = {cgn::make_path_base_working(build.outputs[0] + "/include")};
    x.files = {cgn::make_path_base_working(build.outputs[0] + "/lib/libmylib.a")};
}
```

### Pattern 2: Platform-Conditional build
```cpp
alias("openssl", x) {
    if (x.cfg["cxx_toolchain"] == "msvc")
        x.actual_label = ":openssl_shared";
    else
        x.actual_label = ":openssl_static";
}
```

### Pattern 3: Cross-config host tool
```cpp
alias("host_perl", x) {
    x.actual_label = "@third_party//perl";
    x.load_named_config("host_release"); // always builds for host in release
}
```

### Pattern 4: Reading tool output in shell_script
```cpp
shell_script("my_build", x) {
    auto perl = x.quick_dep_namedcfg("@third_party//perl", "host_release", false);
    if (perl.outputs.empty()) return x.opt->set_fail("perl not found");
    std::string perl_exe = perl.outputs[0];
    x.extra_watch_files += {cgn::make_path_base_working(perl.ninja_entry)};
    // ... use perl_exe ...
    cgn::CGNTargetMaker *mk = x.opt->confirm(); // MUST call after reading all deps
    if (!mk) return;
    // ... add ninja rules ...
}
```

### Pattern 5: Conditional sources
```cpp
cxx_static("mylib", x) {
    x.srcs = {"src/common.cpp"};
    if (x.cfg["os"] == "win")
        x.srcs += {"src/win32.cpp"};
    else
        x.srcs += {"src/posix.cpp"};
}
```

---

## 11. Error Handling

```cpp
// Signal a build error and return immediately:
return x.opt->set_fail("Reason why this target cannot be built.");

// After add_dep, check for errors:
auto dep = x.add_dep(":some_target", cxx::private_dep);
if (dep.errmsg.size()) return x.opt->set_fail(dep.errmsg);

// After confirm(), check for cache hit:
cgn::CGNTargetMaker *mk = x.opt->confirm();
if (!mk) return; // cache hit — nothing more to do
```

**Common error messages**:

| Error | Cause | Fix |
|---|---|---|
| `'xxx' config not found` | Named config not in `cgn_setup.cgn.cc` | Add config or use `DEFAULT` |
| `cycle-dependency` | Circular `add_dep()` chain | Tell user to fix BUILD.cgn.cc |
| `Configuration locked` | Accessing a new cfg key after `opt->confirm()` | Move all `x.cfg[...]` reads before `opt->confirm()` |
| `No executable found` | `build` command on a non-executable target | Use `cxx_executable`, not `cxx_static`/`cxx_shared` |
| `dlopen failed` | Script `.so` failed to compile | Check compiler output; look for syntax errors in `.cgn.cc` |

---

## 12. CLI Reference

```sh
# Build a target
cgn --halt_on_error --target llvm,debug,asan build @cell//dir:target

# Query target info without building
cgn --halt_on_error --target llvm,debug,asan query @cell//dir:target [cfgname]

# Analyse only (no ninja, no build)
cgn --halt_on_error --target llvm,debug,asan analyse @cell//dir:target

# Run an executable target
cgn --halt_on_error --target llvm,debug,asan run @cell//dir:target

# Key options
--cgn-out <dir>      Output directory (default: cgn-out)
-C <dir>             Alias for --cgn-out
--halt_on_error      Exit immediately on any analysis error
--verbose / -V       Verbose output
--target <tokens>    Comma-separated config tokens (see §5)
--scriptcc <path>    Use a specific C++ compiler for .cgn.cc files
--scriptcc_debug     Enable debug mode for script compilation
--winenv             Load MSVC environment before script compilation (Windows)
```

---

## 13. The `cgn_setup.cgn.cc` File

Located at the monorepo root. Called once at startup to define named configurations.

```cpp
// Minimal setup (uses cgn_default_setup.cgn.hxx which parses --target):
#include "@cgn.d/library/cgn_default_setup.cgn.hxx"

// Custom setup example:
void cgn_setup(cgn::CGNInitSetup &x) {
    x.configs["DEFAULT"] = config_guessor(str_to_set(api.get_kvargs()["target"]));
    x.configs["host_release"] = generate_host_release();
    // add more named configs:
    x.configs["arm64_linux"] = config_guessor(str_to_set("linux,arm64,gcc,release"));
}
```

---

## 14. MCP Server

The `@cgn.d/mcp/` directory contains a Model Context Protocol server (`cgn_mcp`) that wraps the CGN API for AI agent tool use.

**Configuration** (in your MCP client config JSON):
```json
{
  "mcpServers": {
    "cgn": {
      "command": "./@cgn.d/build_linuxd/cgn_mcp",
      "args": ["--target", "llvm,debug,asan", "--cgn-out", "cgn-out"]
    }
  }
}
```

**Available tools**: `cgn_analyse`, `cgn_build`, `cgn_query`, `cgn_list_configs`

See `@cgn.d/mcp/README.md` for build and usage instructions.
