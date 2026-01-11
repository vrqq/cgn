## CGN V1.1 roadmap

**Conception**
- interpreter: a predefined procedure used with a `factory` to produce a `target`.
- factory: a function that generates arguments for an `interpreter` to create a `target`.
- target: an actual build target with specific build arguments such as OS, CPU, etc.
- script: a `.cgn.cc` file that can be built and `dlopen`-ed by CGN.
- cgn_out: the base output directory for CGN.
- target_dir: the base directory for a `target`, e.g. `cgn-out/dir_/target_FFFFABAB`.
- config: a key-value map represent for the request argument to `factory` when generating a `target`.
- CGNPath: storage path base on one of output, script_dir or working_root.
- script label : a file label of BUILD.cgn.cc, like '@cgn.d//library/example.cgn.cc'
- factory label: a label which transform from path of BUILD.cgn.cc, composed with '@cell//dir:factory_name', like '@proj1//src:myproto'.
- factory_name: the last part of factory_label string without '@', '/', '\', ':'
- XXX_prefix : OS-sep dir path string, trailling with '/' or '\'.
    - src_prefix : string, like '@cell/src/', 'C:\external_code\'
    - out_prefix : string, like 'cgn-out/obj/src_/myproto_FFFFAABB/'
- target_name: factory_name + '_' + config_hashid. (fname_FFFFAAAA, fname_FFFFAAAA:name2_FFFFBBBB)

**The requirement to make target**
- Procedure : opt -> (in factory / interpreter)(TrimmedConfig -> write build.ninja)
- Here we record the calling stack of `api.create_target()` for adep_edge relation, for example if create_target(B) called inside and before create_target(A) return, we know there's should be add_adep(early=B, late=A).
- case: factory_has_label registered along with BUILD.cgn.so loaded
    - The factory has a label to locate, the source_dir, name and out_parent_prefix can be inferred.
    - `opt.description = label`
    - `opt.cfg = input_argument`
    - `opt.name = computed from label`
    - `opt.src_prefix = computed from label`
    - `opt.out_parent_prefix = computed from label`
    - `opt.get_cfg0_out_prefix()` returns `$out_parent_prefix + name + "00000000"`.
- case: anonymous target (created by user code):
    - Called via `api.create_target` without factory label, only a name is available to identify the output directory.
    - Check for conflicts with `impl.TargetCache[output_dir()]`.
    - `opt.cfg = input_argument`
    - `opt.description = USER_SHOULD_FILL_THIS` (default: `(anonymous) + source_code_line in stack`)
    - `opt.name = USER_SHOULD_FILL_THIS` (api will check its validate)
    - `opt.src_prefix = USER_SHOULD_FILL_THIS` (must exist)
    - `opt.out_parent_prefix = USER_SHOULD_FILL_THIS`

```cpp
// Make 2 anode deps: this_target->script_file and calling_from->this_target
CGNTarget api.create_target(factory_label, cfg) {
    infinite_loop_check(opt.out_parent_prefix, opt.name, opt.cfg.id, TLS.TLRuntime);
    CGNFactoryOpt opt{cfg, [name,src_prefix,out_parent_prefix]=factory_entry[factory_label]};
    TLRuntime now_rt;
    now_rt.call_from = TLS.TLRuntime;
    now_rt.deps_anode += factory_entry[factory_label].script_anode;
    now_rt.hint_label = factory_label;
    TLS.TLRuntime = now_rt;
    factory_entry[factory_label](opt) {
        // user factory and interpreter code
        opt.confirm();
    }

    now_rt.call_from.deps_anode += now_rt.rv.anode;
    TLS.TLRuntime = now_rt.call_from;
    return now_rt.rv;
}

// Make one anode dep: calling_from->this_target
CGNTarget api.create_target(opt, fn_entry) {
    infinite_loop_check(opt.out_parent_prefix, opt.name, opt.cfg.id, TLS.TLRuntime);
    TLRuntime now_rt;
    now_rt.call_from = TLS.TLRuntime;
    now_rt.hint_label = now_rt.call_from.hint_label + ":" + opt.name;
    TLS.TLRuntime = now_rt;
    fn_entry(opt) {
        maker = opt.confirm() -> TargetMaker* || nullptr {
            assert(&opt == TLRuntime.active_opt);
            cache_label = opt.out_parent_prefix + opt.name + opt.cfg.lock();
            return nullptr if now_rt.rv = api.target_cache[cache_label] found;

            api.add_adep(early=now_rt.deps_anode[], late=this_anode);
            return now_rt.rv = api.target_cache[cache_label] = new TargetMaker;
        }
        if (maker)
            maker.result[XXInfo] = "string_data";
            if (maker.ninja)
                maker.ninja.add_build();
    }
    now_rt.call_from.deps_anode += now_rt.rv.anode;
    TLS.TLRuntime = now_rt.call_from;
    return now_rt.rv;
}

api.async_create_target(args...) -> promise<CGNTarget> {
    _rt_source = TLS.TLRuntime;
    new thread {
        TLS.TLRuntime = _rt_source;
        api.create_target(args...);
    };
};

ThreadLocal struct TLRuntime {
    active_opt,
    call_from : StackItem*,
    deps_anode[],
    rv : Target
    hint_label,
}

struct CGNFactoryOpt {
    name,
    cfg,
    src_prefix,
    out_parent_prefix,
    get_cfg0_out_prefix_os(),
    confirm() -> CGNTargetMaker*,
    set_fail(errmsg),
};

struct CGNTarget : InfoTable {
    desc,
    trimmed_cfg,
    anode,
    ninja_entry,
};

struct CGNTargetMaker : CGNTarget, CGNFactoryOpt {
    ninja,
    out_prefix,
    out_prefix_unix,
    is_file_unchanged(),
};
```

**CGN public API**
- `api.debug_tlruntime()`: returns the current thread-local-storage with runtime context of `api.create_target()`.
    - TLS.cell_name     : string, like '@mycell'
    - TLS.factory_label : string, 
    - TLS.src_dir       : string, like '@cell/src'
    - TLS.dst_dir       : string, like 'cgn-out/obj/src_/myproto_FFFFAABB'

- `<GraphNode*, error_msg> api.active_script(script_label)`: build and load a `.cgn.cc` script (dynamic library).
- `error_msg api.offline_script(script_label)`: dlclose the specified dynamic library.
- `error_msg api.add_factory(factory_label, fn_factory)`: register a factory and compute its source and output base directories.
- `error_msg api.remove_factory(factory_label)`
- `CGNTarget api.create_target(factory_label, cfg_in)`: create a target using a factory loaded by `active_script`.
    - `opt = convert_from(factory_label)`
    - call `impl.factory[factory_label].fn(cfg_in)`
    - (in factory or interpreter) after config lock:
        - `opt.out_dir = opt.out_parent_dir / {opt.name + cfg.get_hash()}`

- `CGNTarget api.create_target(opt, fn_entry = [](opt){ return XInterpreter::interpret(XInterpreter::Context(opt)); })`: create a target by custom option.


## Example: protobuf

* Preferred output structure
```txt
- /cgn-out/obj/third_party_/protobuf_FFFF1111
    - protoc
- /cgn-out/obj/src_/hello_00000000
    - build.ninja
    - protoc_gen.stamp
    - cpp/hello_phase1.pb.cc
    - cpp/hello_phase1.pb.h
- /cgn-out/obj/src_/hello_FFFF8086
    - build.ninja
    - cpp/hello_phase1.pb.o
- /cgn-out/obj/src_/hello_FFFFAA64
    - build.ninja
    - cpp/hello_phase1.pb.o
```

* //src/BUILD.cgn.cc
```cpp
protobuf("hello", x) { x.src = {"hello_phase1.proto"}; x.lang_cxx = true }
```

```ninja
## cgn-out/src_/hello_00000000/build.ninja
## ---------------------------------------
build cgn-out/src_/hello_00000000/cpp_00000000/hello_phase1.pb.cc : phony
build cgn-out/src_/hello_00000000/cpp_00000000/hello_phase1.pb.h : phony

## cgn-out/src_/hello_FFFFAA64/build.ninja
## ---------------------------------------
build cgn-out/src_/hello_FFFFAA64/hello_phase1.o : cc cgn-out/src_/hello_00000000/cpp/hello_phase1.pb.cc || cgn-out/src_/hello_FFFFAA64/proto_gen.stamp

build cgn-out/src_/hello_FFFFAA64/proto_gen.stamp : run cgn-out/cgn.d_/library/checker_11111111/checker
    rspfile_content = --stamp cgn-out/src_/hello_00000000/hello_phase1.stamp $
    --check src/hello_phase1.proto $
    --check cgn-out/src_/hello_00000000/hello_phase1.pb.cc $
    --check cgn-out/src_/hello_00000000/hello_phase1.pb.h $
    cgn-out/third_party/protobuf_FFFF1111/protoc src/hello_phase1.proto --cpp-out=cgn-out/src_/hello_00000000
```

* ProtobufInterpreter.cgn.cc
```cpp
interpreter(ctx) {
    // anonymous target to generate .pb.cc placeholder
    // 'cgn-out/src_/hello_00000000/cpp_00000000/hello_phase1.pb.cc'
    pbopt = {
        .name = "cpp";
        .src_prefix = ctx.opt.src_prefix;
        .out_parent_dir = ctx.opt.get_out_prefix_cfg0();
    }
    pbtarget = api.create_target(pbopt, [](TargetOptIn op2){
        op2.cfg.clear();
        maker2 = op2.confirm();
        maker2.ninja.append_build("$out_prefix/hello_phase1.pb.cc : phony");
    });

    // anonymous target to generate .o
    // 'cgn-out/src_/hello_00000000/cppobj_FFFFAA64/hello_phase1.pb.cc'
    cxxopt = {
        .name = "cppobj";
        .src_prefix = ctx.opt.get_out_prefix_cfg0() + "/cpp";
        .out_parent_dir = ctx.opt.get_out_prefix_cfg0();
    }
    cxxtarget = api.create_target<CxxInterpreter>(cxxopt, [](CxxInfo &x){
        x.src = {"*.pb.cc"};
        x.opt.add_dep(pbtarget.anode);
    });
    
    // target entry to call protoc
    // cgn-out/src_/hello_FFFFAA64/.entry
    maker.ninja.append_build("$out_prefix/.entry: checker protoc ... ");
}
```

**Design ideas**
- The three files `hello_phase1.pb.cc`, `hello_phase1.pb.h`, and `hello_phase1.proto` should not appear in the protoc ninja target output zone.
- Different platforms use different `protoc` executables to generate or update these files. The appropriate `protoc` should be invoked when a file is missing or its mtime has changed.
- No single command behaves identically on Linux, Windows, and macOS, so the stamp checker must be platform-aware.

**Explanation**
`hello_phase1.pb.cc` is a platform-independent source file generated from a `.proto` file, but the `protoc` binary and produced `.obj` files differ by platform. Therefore, the `.obj` file ninja target should depend on a platform-specific target that consumes the `.proto` source and generates the intermediary `.pb.cc` file.

To avoid unnecessary cross-platform regeneration, validate a platform-independent mtime stamp file before running `protoc`. (The same applies when `.pb.cc` is generated directly into the source directory.)

````
