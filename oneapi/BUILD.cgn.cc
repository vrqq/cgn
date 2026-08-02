#include "cgn"

// oneTBB 2022.2.0 (Jun 30, 2025)
git("tbb.git", x) {
    x.repo = "https://github.com/uxlfoundation/oneTBB.git";
    x.commit_id = "06ce6212da6710f4bb2d20a1904b018aa44069bf";
    x.dest_dir = "repo";
}

static std::string _tbb_def_file(const cgn::Configuration cfg) {
    if (cfg["cxx_toolchain"] == "llvm" && cfg["os"] == "linux")
        return "expose_for_lld.def";
    
    auto _rv = [](std::string mid){ return "repo/src/tbb/def/" + mid + "-tbb.def"; };
    // repo/cmake/compiler/Clang.cmake
    // script for for llvm linker (lld) is not same as GNU ld
    if (cfg["cxx_toolchain"] == "llvm" && cfg["os"] == "mac")
        return _rv("mac64");

    if (cfg["cxx_toolchain"] == "msvc") {
        if (cfg["os"] == "win" && cfg["cpu"] == "x86")
            return _rv("win32");
        if (cfg["os"] == "win" && cfg["cpu"] == "x86_64")
            return _rv("win64");
    }

    if (cfg["os"] == "linux" && cfg["cpu"] == "x86")
        return _rv("lin32");
    if (cfg["os"] == "linux" && cfg["cpu"] == "x86_64")
        return _rv("lin64");

    return "";
}

cxx_shared("tbb", x) {
    x.pub.include_dirs = x.include_dirs = {"repo/include"};
    x.srcs = {
        "repo/src/tbb/address_waiter.cpp",
        "repo/src/tbb/allocator.cpp",
        "repo/src/tbb/arena.cpp",
        "repo/src/tbb/arena_slot.cpp",
        "repo/src/tbb/concurrent_bounded_queue.cpp",
        "repo/src/tbb/dynamic_link.cpp",
        "repo/src/tbb/exception.cpp",
        "repo/src/tbb/global_control.cpp",
        "repo/src/tbb/governor.cpp",
        "repo/src/tbb/itt_notify.cpp",
        "repo/src/tbb/main.cpp",
        "repo/src/tbb/market.cpp",
        "repo/src/tbb/misc.cpp",
        "repo/src/tbb/misc_ex.cpp",
        "repo/src/tbb/observer_proxy.cpp",
        "repo/src/tbb/parallel_pipeline.cpp",
        "repo/src/tbb/private_server.cpp",
        "repo/src/tbb/profiling.cpp",
        "repo/src/tbb/queuing_rw_mutex.cpp",
        "repo/src/tbb/rml_tbb.cpp",
        "repo/src/tbb/rtm_mutex.cpp",
        "repo/src/tbb/rtm_rw_mutex.cpp",
        "repo/src/tbb/semaphore.cpp",
        "repo/src/tbb/small_object_pool.cpp",
        "repo/src/tbb/task.cpp",
        "repo/src/tbb/task_dispatcher.cpp",
        "repo/src/tbb/task_group_context.cpp",
        "repo/src/tbb/tcm_adaptor.cpp",
        "repo/src/tbb/thread_dispatcher.cpp",
        "repo/src/tbb/threading_control.cpp",
        "repo/src/tbb/thread_request_serializer.cpp",
        "repo/src/tbb/version.cpp",
    };
    if (x.cfg["os"] == "linux")
        x.ldflags += {"-Wl,--version-script=@third_party/oneapi/" + _tbb_def_file(x.cfg)};
    else
        x.srcs += {_tbb_def_file(x.cfg)};

    if (x.cfg["cxx_toolchain"] == "gcc" || x.cfg["cxx_toolchain"] == "llvm") {
        if (x.cfg["cpu"] == "x86" || x.cfg["cpu"] == "x86_64")
            x.cflags += {"-w", "-mwaitpkg"};
        // if (x.cfg["optimization"] == "release")
        //     x.defines += {"_FORTIFY_SOURCE=2"};
    }

    if (x.cfg["cxx_toolchain"] == "gcc")
        x.cflags += {"-flifetime-dse", "-fstack-clash-protection"};
    if (x.cfg["cxx_toolchain"] == "llvm")
        x.cflags += {"-fexceptions"};

    if (x.cfg["os"] == "win"){
        x.defines += {"WIN32", "__TBB_SKIP_DEPENDENCY_SIGNATURE_VERIFICATION=1"};
        x.cflags += {"/external:W4", "/TP"};
    }
    else {//not win
        x.defines += {"_REENTRANT"};
        x.ldflags += {"-lpthread"};
    }
    
    if (x.cfg["os"] == "linux") {
        x.cflags += {"-fvisibility=default"};
        x.ldflags += {"-ldl", "-lrt"};
    }
    if (x.cfg["os"] == "mac")
        x.pub.defines += {"_XOPEN_SOURCE"};
    if (x.cfg["cpu"] == "x86_64"){
        x.pub.defines += {"__TBB_NO_IMPLICIT_LINKAGE"};
        x.defines += {"__TBB_USE_ITT_NOTIFY"};
    }
    if (x.cfg["optimization"] == "debug")
        x.defines += {"TBB_USE_DEBUG"};
    x.pub.defines += {"USE_PTHREAD"};
    x.defines += {"__TBB_BUILD"};
}

cxx_shared("tbbmalloc", x) {
    x.pub.include_dirs =  x.include_dirs = {"repo/include"};
    x.srcs = {"repo/src/tbbmalloc/backend.cpp",
              "repo/src/tbbmalloc/backref.cpp",
              "repo/src/tbbmalloc/frontend.cpp",
              "repo/src/tbbmalloc/large_objects.cpp",
              "repo/src/tbbmalloc/tbbmalloc.cpp"};
    if (x.cfg["os"] == "linux")
        x.ldflags += {"-Wl,--version-script=@third_party/oneapi/" + _tbb_def_file(x.cfg)};
    else
        x.srcs += {_tbb_def_file(x.cfg)};
    x.defines = {"__TBBMALLOC_BUILD"};
}
