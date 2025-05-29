#include <cgn>

// v0.22.0
git("thrift.git", x) {
    x.repo = "https://github.com/apache/thrift.git";
    x.commit_id = "af9ac170f4de895266de4b6f9f3e68a58f113760";
    x.dest_dir = "repo";
}

custom_command("_prep_thrift_compiler", x) {
    auto cpath = [&](cgn::CGNPath p){
        return api.shell_escape(x.rebase_path(p));
    };

    x.opt_confirm_cached();
    x.append_escaped_cmd("flex" 
        " -o " + cpath(cgn::make_path_base_out("src/thrift/thriftl.cc"))
        + " " + cpath(cgn::make_path_base_script("repo/compiler/cpp/src/thrift/thriftl.ll"))
    );
    x.append_escaped_cmd("bison -y"
        " -o " + cpath(cgn::make_path_base_out("src/thrift/thrifty.cc"))
        + " --defines=" + cpath(cgn::make_path_base_out("src/thrift/thrifty.hh"))
        + " " + cpath(cgn::make_path_base_script("repo/compiler/cpp/src/thrift/thrifty.yy"))
    );
    
    x.watch_inputs = {
        cgn::make_path_base_script("repo/compiler/cpp/src/thrift/thriftl.ll"),
        cgn::make_path_base_script("repo/compiler/cpp/src/thrift/thrifty.yy")
    };
    x.watch_outputs = {
        cgn::make_path_base_out("src/thrift/thriftl.cc"), 
        cgn::make_path_base_out("src/thrift/thrifty.cc"),
        cgn::make_path_base_out("src/thrift/thrifty.hh"),
    };
    x.analysis_outputs = {cgn::make_path_base_out("src")};
}

// it depends on bison and flex
// for Unix: using OS internal
// for Windows: using @third_party//winbisonflex (todo)
cxx_executable("compiler", x) {
    const std::string SRCP = "repo/compiler/cpp/";
    cgn::CGNTarget src1 = x.add_dep(":_prep_thrift_compiler", cxx::order_dep);
    std::string src1base = src1.outputs[0] + "/";
    x.srcs += {
        cgn::make_path_base_working(src1base + "thrift/thriftl.cc"),
        cgn::make_path_base_working(src1base + "thrift/thrifty.cc"),
        cgn::make_path_base_script(SRCP + "src/thrift/generate/*.cc"),
        cgn::make_path_base_script(SRCP + "src/thrift/parse/*.cc"),
        cgn::make_path_base_script(SRCP + "src/thrift/audit/t_audit.cpp"),
        cgn::make_path_base_script(SRCP + "src/thrift/common.cc"),
        cgn::make_path_base_script(SRCP + "src/thrift/main.cc"),
    };
    x.include_dirs = {cgn::make_path_base_script(SRCP + "src"), 
                      cgn::make_path_base_working(src1base)};
}

alias("host_compiler", x) {
    x.actual_label = ":compiler";
    x.load_named_config("host_release");
}

static std::vector<std::string> add_prefix(const std::string &prefix, std::vector<std::string> ls)
{
    for (auto &it : ls)
        it = prefix + it;
    return ls;
}

cxx_static("thrift", x) {
    std::vector<std::string> thriftcpp_SOURCES = {
        "src/thrift/TApplicationException.cpp",
        "src/thrift/TOutput.cpp",
        "src/thrift/TUuid.cpp",
        "src/thrift/async/TAsyncChannel.cpp",
        "src/thrift/async/TAsyncProtocolProcessor.cpp",
        "src/thrift/async/TConcurrentClientSyncInfo.h",
        "src/thrift/async/TConcurrentClientSyncInfo.cpp",
        "src/thrift/concurrency/ThreadManager.cpp",
        "src/thrift/concurrency/TimerManager.cpp",
        "src/thrift/processor/PeekProcessor.cpp",
        "src/thrift/protocol/TBase64Utils.cpp",
        "src/thrift/protocol/TDebugProtocol.cpp",
        "src/thrift/protocol/TJSONProtocol.cpp",
        "src/thrift/protocol/TMultiplexedProtocol.cpp",
        "src/thrift/protocol/TProtocol.cpp",
        "src/thrift/transport/TTransportException.cpp",
        "src/thrift/transport/TFDTransport.cpp",
        "src/thrift/transport/TSimpleFileTransport.cpp",
        "src/thrift/transport/THttpTransport.cpp",
        "src/thrift/transport/THttpClient.cpp",
        "src/thrift/transport/THttpServer.cpp",
        "src/thrift/transport/TSocket.cpp",
        "src/thrift/transport/TSocketPool.cpp",
        "src/thrift/transport/TServerSocket.cpp",
        "src/thrift/transport/TTransportUtils.cpp",
        "src/thrift/transport/TBufferTransports.cpp",
        "src/thrift/transport/SocketCommon.cpp",
        "src/thrift/server/TConnectedClient.cpp",
        "src/thrift/server/TServerFramework.cpp",
        "src/thrift/server/TSimpleServer.cpp",
        "src/thrift/server/TThreadPoolServer.cpp",
        "src/thrift/server/TThreadedServer.cpp",

        // IF not WINCE
        "src/thrift/transport/TPipe.cpp",
        "src/thrift/transport/TPipeServer.cpp",
        "src/thrift/transport/TFileTransport.cpp",
    };

    if (x.cfg["os"] == "win")
        thriftcpp_SOURCES += {
            "src/thrift/windows/TWinsockSingleton.cpp",
            "src/thrift/windows/SocketPair.cpp",
            "src/thrift/windows/GetTimeOfDay.cpp",
            "src/thrift/windows/WinFcntl.cpp",

            // IF not WINCE
            "src/thrift/windows/OverlappedSubmissionThread.cpp"
        };
    else
        thriftcpp_SOURCES += {
            "src/thrift/VirtualProfiling.cpp",
            "src/thrift/server/TServer.cpp"
        };
    
    // openssl dependency
    if (0) {
        x.add_dep("@third_party//openssl", cxx::private_dep);
        thriftcpp_SOURCES += {
            "src/thrift/transport/TSSLSocket.cpp",
            "src/thrift/transport/TSSLServerSocket.cpp",
            "src/thrift/transport/TWebSocketServer.h",
            "src/thrift/transport/TWebSocketServer.cpp"
        };
    }

    std::vector<std::string> thriftcpp_threads_SOURCES = {
        "src/thrift/concurrency/ThreadFactory.cpp",
        "src/thrift/concurrency/Thread.cpp",
        "src/thrift/concurrency/Monitor.cpp",
        "src/thrift/concurrency/Mutex.cpp"
    };

    x.srcs = add_prefix("repo/lib/cpp/", thriftcpp_SOURCES + thriftcpp_threads_SOURCES);
    x.defines = {"THRIFT_STATIC_DEFINE"};
    x.include_dirs = {"repo/lib/cpp/src", "configured_include_rhel"};
}

cxx_static("thriftz", x) {
    std::vector<std::string> thriftcppz_SOURCES = {
        "src/thrift/transport/TZlibTransport.cpp",
        "src/thrift/protocol/THeaderProtocol.cpp",
        "src/thrift/transport/THeaderTransport.cpp",
        "src/thrift/protocol/THeaderProtocol.cpp",
        "src/thrift/transport/THeaderTransport.cpp"
    };
    x.srcs = add_prefix("repo/lib/cpp/", thriftcppz_SOURCES);
    x.add_dep("@third_party//zlib", cxx::private_dep);
}
