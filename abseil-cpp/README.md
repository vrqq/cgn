# Abseil CMakeLists Parser

This directory contains a Node.js script to parse Abseil's CMakeLists.txt files and generate a C++ header file with target definitions.

## Usage

### Default usage (generates `abseil-src.h` in current directory):
```bash
node generate-abseil-targets.js
```

### With custom repo path and output path:
```bash
node generate-abseil-targets.js <repo-path> <output-path>
```

### Examples:
```bash
# Default behavior
./generate-abseil-targets.js

# Specify custom paths
node generate-abseil-targets.js /path/to/abseil-cpp/repo ./abseil-src.h

# From different directory
node /path/to/abseil-cpp/generate-abseil-targets.js
```

## Output

The script generates a C++ header file (`abseil-src.h`) containing:

1. **AbseilTarget struct** with fields:
   - `basedir`: The base directory relative to `repo/` (e.g., `"absl/base"`)
   - `name`: Target name (e.g., `"atomic_hook"`)
   - `hdrs`: List of header files
   - `srcs`: List of source files
   - `copts`: List of compile options
   - `deps`: List of dependencies (targets and external libs)
   - `linkopts`: List of linker options
   - `testonly`: Boolean flag indicating if this is test-only target
   - `public_`: Boolean flag indicating if this is a public target

2. **get_all_targets()** function that returns a vector of all discovered targets

## Example Output

```cpp
#pragma once

#include <vector>
#include <string>

struct AbseilTarget {
  std::string basedir;
  std::string name;
  std::vector<std::string> hdrs;
  std::vector<std::string> srcs;
  std::vector<std::string> copts;
  std::vector<std::string> deps;
  std::vector<std::string> linkopts;
  bool testonly;
  bool public_;
};

inline std::vector<AbseilTarget> get_all_targets() {
  return {
    AbseilTarget{
      "absl/base",
      "atomic_hook",
      {"internal/atomic_hook.h"},
      {},
      {},
      {"absl::config", "absl::core_headers"},
      {},
      false,
      false
    },
    // ... more targets ...
  };
}
```

## Implementation Details

- Recursively discovers all CMakeLists.txt files under `repo/absl/`
- Parses `absl_cc_library()` and `absl_cc_test()` macro calls
- Extracts all target metadata: NAME, HDRS, SRCS, COPTS, DEPS, LINKOPTS, PUBLIC, TESTONLY
- Skips CMake variable references (e.g., `${ABSL_DEFAULT_COPTS}`)
- Generates valid C++ header with proper syntax
- Note: `public_` field is used instead of `public` to avoid C++ keyword conflicts

