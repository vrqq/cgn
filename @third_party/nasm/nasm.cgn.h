
// NASM interpreter
//
// see also : https://source.chromium.org/chromium/chromium/src/+/main:third_party/nasm/nasm_assemble.gni
#ifdef _WIN32
    #ifdef NASM_CGN_IMPL
        #define NASM_CGN_API  __declspec(dllexport)
    #else
        #define NASM_CGN_API
    #endif
#else
    #define NASM_CGN_API __attribute__((visibility("default")))
#endif

#include <cgn>

// Files to be assembled with NASM should have an extension of .asm.
// Example
//
//   nasm_assemble("my_nasm_target") {
//     sources = [
//       "ultra_optimized_awesome.asm",
//     ]
//     include_dirs = [ "assembly_include" ]
//   }
struct NasmContext
{
    const std::string &name;
    cgn::Configuration &cfg;

    std::vector<std::string> srcs;

    // nasm_flags (optional)
    //   [list of strings] Pass additional flags into NASM. These are appended
    //   to the command line. Note that the output format is already set up
    //   based on the current toolchain so you don't need to specify these
    //   things (see below).
    //   Example: nasm_flags = {"-Wall"}
    std::vector<std::string> nasm_flags;


    // include_dirs (optional)
    //   [list of dir names] List of additional include dirs. Note that the
    //   source root and the root generated file dir is always added, just like
    //   our C++ build sets up.
    std::vector<std::string> include_dirs;


    // defines (optional)
    //   [list of strings] List of defines, as with the native code defines.
    //   Example: defines = {"FOO", "BAR=1"}
    std::vector<std::string> defines;

    NasmContext(cgn::CGNTargetOptIn *opt)
    : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

private: friend struct NasmInterpreter;
    cgn::CGNTargetOptIn *opt;
};

struct NasmInterpreter
{
    using context_type = NasmContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@third_party//nasm/nasm.cgn.cc"};
    }

    NASM_CGN_API static void interpret(context_type &x);
};

#define nasm_object(name, x) CGN_RULE_DEFINE(NasmInterpreter, name, x)