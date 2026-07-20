---
name: cgn-build-system
description: "Use when writing, debugging, or reviewing BUILD.cgn.cc and other CGN scripts. Covers labels, configurations, C/C++ rules, dependencies, and common failures."
applyTo:
  - "**/BUILD.cgn.cc"
  - "**/*.cgn.cc"
  - "**/*.cgn.h"
  - "**/*.cgn.hxx"
---

# CGN Build Script Skill

Use this guide for CGN build definitions. Read
`@cgn.d/doc/ARCHITECTURE.md` for the system overview and `cgn_setup.cgn.cc` for
the configurations available in this monorepo.

## Rules

1. Prefer the smallest built-in interpreter that fits the artifact.
2. Use full labels for cross-cell dependencies: `@cell//dir:name`.
3. Use `x.*` for private settings and `x.pub.*` for consumer-facing settings.
4. Read every `x.cfg[...]` key before `x.opt->confirm()`.
5. After `confirm()`, return immediately when it returns `nullptr`.
6. Propagate dependency errors with `return x.opt->set_fail(dep.errmsg)`.

## Labels And Configurations

```text
@cell//dir:name  full target label
:name            current BUILD.cgn.cc directory
//dir:name        current cell
```

`x.cfg` is the current configuration. A target output hash contains only the
configuration keys read by that target. Use a named configuration through
`x.load_named_config("host_release")` when an alias needs a host tool.

## Core C++ Pattern

```cpp
#include <cgn>

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

| Rule | Output | Main fields |
|---|---|---|
| `cxx_sources` | objects | `srcs`, compile settings |
| `cxx_static` | `.a` or `.lib` | `srcs`, `pub`, `archive` dependencies |
| `cxx_shared` | shared library | `srcs`, link dependencies |
| `cxx_executable` | executable | `srcs`, dependencies |
| `cxx_prebuilt` | dependency metadata | `files`, `pub.include_dirs`, `system_libs` |

| Dependency flag | Effect |
|---|---|
| `cxx::private_dep` | consume dependency only here |
| `cxx::inherit` | consume and export dependency information |
| `cxx::archive` | include dependency objects in a static library |
| `cxx::order_dep` | establish build order only |

## External Source Or Build

```cpp
git("lib.git", x) {
    x.repo = "https://example.invalid/lib.git";
    x.commit_id = "full-40-character-commit-id";
    x.dest_dir = "repo";
}

cmake("lib.build", x) {
    x.sources_dir = "repo";
    x.outputs = {"install/lib/libexample.a"};
}
```

Use `shell_script` for external build systems that do not have an interpreter.
It must declare `analysis_outputs` so consumers can find the produced files.

## Platform Or Cross-Config Alias

```cpp
alias("host_tool", x) {
    x.actual_label = "@third_party//tool";
    x.load_named_config("host_release");
}
```

Set `x.actual_label` in every alias. Do not create aliases that resolve back to
themselves.

## Error Pattern

```cpp
cxx_executable("app", x) {
    auto core = x.add_dep(":core", cxx::private_dep);
    if (!core.errmsg.empty()) return x.opt->set_fail(core.errmsg);

    std::string mode = x.cfg["optimization"];
    auto *maker = x.opt->confirm();
    if (!maker) return;
}
```

| Problem | Repair |
|---|---|
| `Configuration locked` | Move config reads before `confirm()`. |
| target cache hit | Return when `confirm()` returns `nullptr`. |
| missing generated output | Set `analysis_outputs`. |
| source glob does not work | List sources explicitly or use `api.file_glob()`. |
| `dlopen` fails | Run `cgn analyse <label>` and repair the script compile error. |
