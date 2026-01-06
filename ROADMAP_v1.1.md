## CGN V1.1 roadmap

**Conception**
- interpreter: a predefined procedure used with a `factory` to produce a `target`.
- factory: a function that generates arguments for an `interpreter` to create a `target`.
- target: an actual build target with specific build arguments such as OS, CPU, etc.
- script: a `.cgn.cc` file that can be built and `dlopen`-ed by CGN.
- cgn_out: the base output directory for CGN.
- target_dir: the base directory for a `target`, e.g. `cgn-out/dir_/target_FFFFABAB`.
- config: a key-value map that guides a `factory` when generating a `target`.

**CGN public API**
- `api.current_env()`: returns the current environment (thread-local) which contains runtime state and context.
- `api.load_script(script_label)`: build and load a specific `.cgn.cc` script.
- `api.register_factory(factory_label, fn_factory)`
- `api.add_target<Interpreter>(factory_label, config, fn_factory)`

```cpp
struct Env {
};
```

## Example: protobuf
* //src/BUILD.cgn.cc
```cpp
protobuf("hello", x) { x.src = {"hello_phase1.proto"}; x.lang_cxx = true }
```

* cgn-out/src_/hello_00000000/build.ninja
```ninja
build cgn-out/src_/hello_00000000/hello_phase1.pb.cc : phony
build cgn-out/src_/hello_00000000/hello_phase1.pb.h : phony
```

* cgn-out/src_/hello_FFFF1A1B/build.ninja
```ninja
build cgn-out/src_/hello_FFFF1A1B/hello_phase1.o : cc cgn-out/src_/hello_00000000/hello_phase1.pb.cc || cgn-out/src_/hello_FFFF1A1B/host_proto_gen.stamp

build cgn-out/src_/hello_FFFF1A1B/host_proto_gen.stamp : run cgn-out/cgn.d_/library/checker_11111111/checker
    rspfile_content = --stamp cgn-out/src_/hello_00000000/hello_phase1.stamp $
    --src src/hello_phase1.proto $
    --src cgn-out/src_/hello_00000000/hello_phase1.pb.cc $
    --src cgn-out/src_/hello_00000000/hello_phase1.pb.h $
    cgn-out/third_party/protobuf_FFFF1111/protoc src/hello_phase1.proto --cpp-out=cgn-out/src_/hello_00000000
```

**Design ideas**
- The three files `hello_phase1.pb.cc`, `hello_phase1.pb.h`, and `hello_phase1.proto` should not appear in the Ninja target output zone.
- Different platforms use different `protoc` executables to generate/update these files. The appropriate `protoc` should be invoked when one of the files is missing or mtime has changed.
- There is no one command that works identically on Linux, Windows, and macOS, so the stamp checker must be platform-aware.

**Explanation**
`hello_phase1.pb.cc` is platform-independent source generated from `.proto`, but the `protoc` binary and the produced `.obj` files differ by platform. Therefore, the `.obj` ninja target should depend on a platform-specific target that reads the `.proto` source and generates the intermediary `.pb.cc` file.
To avoid unnecessary regeneration across platforms, It should validate a platform-independent mtime stampfile before `protoc` run. (This also applies when `.pb.cc` is generated into the source directory.)
