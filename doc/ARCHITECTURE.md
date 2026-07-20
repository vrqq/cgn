# CGN Architecture

This is the source-oriented reference for CGN build definitions and the MCP
server. Read the nearby source when this guide and behavior differ.

## Build Flow

```text
BUILD.cgn.cc
  -> ScriptCC compiles and loads the script
  -> registered factory receives a Configuration
  -> interpreter writes cgn-out/obj/.../build.ninja
  -> Ninja builds the requested artifact
```

A factory returns a `CGNTarget`. That return value carries outputs and typed
information to dependencies, including dependencies built with another
configuration.

## Source Map

| Area | Source |
|---|---|
| CLI commands | `v1/cli.cpp` |
| public API and target types | `v1/cgn_api.h`, `v1/cgn_type.h` |
| configuration parsing | `library/cgn_default_setup.cgn.h` |
| C/C++ rules | `library/cxx/cxx.cgn.h` |
| utility rules | `library/utility/` |
| MCP protocol and tools | `mcp/mcp_server.cpp` |

## Labels And Paths

```text
@cell//dir:name  fully-qualified target
@cell//dir       target named dir
:name            target in the current script directory
//dir:name        target in the current cell
```

`CGNPath` identifies a path base. Prefer the helper that matches ownership:

| Helper | Base |
|---|---|
| `make_path_base_script()` | source directory containing `BUILD.cgn.cc` |
| `make_path_base_out()` | current target output directory |
| `make_path_base_working()` | monorepo root |

## Configurations

`cgn_setup.cgn.cc` creates named configurations. The built-in helper can turn
tokens such as `llvm,debug,asan` into a configuration.

```cpp
#include "@cgn.d/library/cgn_default_setup.cgn.h"

CGN_SETUP_IF void cgn_setup(cgn::CGNInitSetup &x) {
    auto debug = str_to_set("llvm,debug,asan");
    auto release = str_to_set("llvm,release");
    x.configs["debug"] = config_guessor(debug);
    x.configs["release"] = config_guessor(release);
}
```

`--target` is a CLI convenience that creates or changes `DEFAULT`. Named
configurations are the stable interface for aliases, cross-configuration
dependencies, and MCP `cgn_build` calls.

During factory execution, CGN records every configuration key read. The values
of those keys determine the target output hash. Read every needed `x.cfg[...]`
key before `x.opt->confirm()`; after confirmation the configuration is locked.

## Factory Rules

Use `#include <cgn>` for the common rules or include an interpreter header.

```cpp
cxx_static("core", x) {
    x.srcs = {"src/core.cpp"};
    x.include_dirs = {"src"};
    x.pub.include_dirs = {"include"};
}

cxx_executable("app", x) {
    x.srcs = {"src/main.cpp"};
    x.add_dep(":core", cxx::inherit);
}
```

| Rule | Produces | Use for |
|---|---|---|
| `cxx_sources` | object files | shared compilation settings or objects |
| `cxx_static` | static library | reusable C/C++ library |
| `cxx_shared` | shared library | dynamically loaded library |
| `cxx_executable` | executable | final program; runtime files are copied beside it |
| `cxx_prebuilt` | dependency metadata | externally built or system binary |
| `git` | fetched source directory | pinned external source |
| `shell_script` | declared outputs | custom external build step |
| `cmake` | declared outputs | external CMake project |
| `alias` | another target | platform or configuration selection |
| `group` | dependency collection | aggregate targets |

Dependency flags:

| Flag | Meaning |
|---|---|
| `cxx::private_dep` | consume dependency only in this target |
| `cxx::inherit` | consume and export dependency information |
| `cxx::archive` | add dependency object files to a static library |
| `cxx::order_dep` | build ordering only |
| `cxx::_no_whole` | do not whole-archive a static library in a shared link |

For custom rules, report failures and stop. Treat a null maker as a cache hit.

```cpp
auto dep = x.add_dep(":tool", cxx::private_dep);
if (!dep.errmsg.empty()) return x.opt->set_fail(dep.errmsg);

auto *maker = x.opt->confirm();
if (!maker) return;
```

## CLI

```sh
cgn [options] analyse @cell//dir:name [config_name]
cgn [options] build   @cell//dir:name [config_name]
cgn [options] query   @cell//dir:name [config_name]
cgn [options] run     @cell//dir:name [config_name]
```

Key options: `--cgn_out <dir>` (or `-C`; legacy `--cgn-out` is accepted), `--target <tokens>`,
`--halt_on_error`, `--verbose`, `--scriptcc <compiler>`, and `--winenv`.

`analyse` does not invoke Ninja. `build` invokes Ninja and returns the primary
executable path when the target is executable. `query` prints target and
configuration data.

## MCP

Start the server with the existing `cgn` executable, not a separate binary:

```sh
./@cgn.d/build_linuxd/cgn --cgn_out cgn-out mcp
```

| Tool | Required fields | Purpose |
|---|---|---|
| `cgn_list_configs` | none | discover every named configuration |
| `cgn_analyse` | `target_label` | analyse without building |
| `cgn_query` | `target_label` | show full target information |
| `cgn_build` | `target_label`, `config_name` | build with an explicit named configuration |

For MCP builds, call `cgn_list_configs`, then pass the chosen `config_name` to
`cgn_build`. Do not use server command-line `--target` to alter `DEFAULT`.
See [MCP server](../mcp/README.md).

## Common Failures

| Symptom | Action |
|---|---|
| `Configuration locked` | Move all `x.cfg[...]` reads before `confirm()`. |
| config not found | Define the named configuration in `cgn_setup.cgn.cc`. |
| cycle dependency | Remove the circular target or alias dependency. |
| `dlopen` or script compile error | Run `analyse` and repair the affected `.cgn.cc` script. |
| no executable found | Use `cxx_executable` or build the correct final target. |
