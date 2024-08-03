#include <cgn>

git("libuv.git", x) {
    x.repo = "https://github.com/libuv/libuv.git";
    x.commit_id = "e9f29cb984231524e3931aa0ae2c5dae1a32884e";
    x.dest_dir = "repo";
}


cxx_static("libuv", x) {
    const static std::string base = "repo/";

    x.include_dirs = x.pub.include_dirs = {base + "include"};
    x.include_dirs += {base + "src"};
    
    if (x.cfg["asan"] == "T")
        x.defines += {"__ASAN__=1"};
    if (x.cfg["msan"] == "T")
        x.defines += {"__MSAN__=1"};
    if (x.cfg["tsan"] == "T")
        x.defines += {"__TSAN__=1"};
    if (x.cfg["ubsan"] == "T")
        x.defines += {"__UBSAN__=1"};
    
    if (x.cfg["os"] == "win") {
        x.cflags = {
            "/W4",
            "/wd4100",
            "/wd4127",
            "/wd4201",
            "/wd4206",
            "/wd4210",
            "/wd4232",
            "/wd4456",
            "/wd4457",
            "/wd4459",
            "/wd4706",
            "/wd4996",
            "/wd4146"
        };
        x.ldflags += {
            "/L:psapi.lib",
            "/L:user32.lib",
            "/L:advapi32.lib",
            "/L:iphlpapi.lib",
            "/L:userenv.lib",
            "/L:ws2_32.lib"
        };
        x.defines += {"WIN32_LEAN_AND_MEAN"};
        x.srcs    += {base + "src/win/async.c", 
                      base + "src/win/core.c", 
                      base + "src/win/detect-wakeup.c", 
                      base + "src/win/dl.c", 
                      base + "src/win/error.c", 
                      base + "src/win/fs.c", 
                      base + "src/win/fs-event.c", 
                      base + "src/win/getaddrinfo.c", 
                      base + "src/win/getnameinfo.c", 
                      base + "src/win/handle.c", 
                      base + "src/win/loop-watcher.c", 
                      base + "src/win/pipe.c", 
                      base + "src/win/thread.c", 
                      base + "src/win/poll.c", 
                      base + "src/win/process.c", 
                      base + "src/win/process-stdio.c", 
                      base + "src/win/signal.c", 
                      base + "src/win/snprintf.c", 
                      base + "src/win/stream.c", 
                      base + "src/win/tcp.c", 
                      base + "src/win/tty.c", 
                      base + "src/win/udp.c", 
                      base + "src/win/util.c", 
                      base + "src/win/winapi.c", 
                      base + "src/win/winsock.c"};
    } // (win)
    else { // (NOT win)
        x.srcs += { base + "src/unix/async.c", 
                    base + "src/unix/core.c", 
                    base + "src/unix/dl.c", 
                    base + "src/unix/fs.c", 
                    base + "src/unix/getaddrinfo.c", 
                    base + "src/unix/getnameinfo.c", 
                    base + "src/unix/loop-watcher.c", 
                    base + "src/unix/loop.c", 
                    base + "src/unix/pipe.c", 
                    base + "src/unix/poll.c", 
                    base + "src/unix/process.c", 
                    base + "src/unix/random-devurandom.c", 
                    base + "src/unix/signal.c", 
                    base + "src/unix/stream.c", 
                    base + "src/unix/tcp.c", 
                    base + "src/unix/thread.c", 
                    base + "src/unix/tty.c", 
                    base + "src/unix/udp.c"};
    } // endif(win)

    if (x.cfg["os"] == "linux") {
        x.defines += {"_FILE_OFFSET_BITS=64", "_LARGEFILE_SOURCE",
                      "_GNU_SOURCE", "_POSIX_C_SOURCE=200112"};
        x.cflags  += {"-Wno-sign-compare"};
        x.ldflags += {"-lpthread", "-ldl", "-lrt"};
        x.srcs  += {base + "src/unix/proctitle.c",
                    base + "src/unix/linux.c", 
                    base + "src/unix/procfs-exepath.c", 
                    base + "src/unix/random-getrandom.c", 
                    base + "src/unix/random-sysctl-linux.c"};
    }

    if (x.cfg["os"] == "mac") {
        x.defines += {"_DARWIN_UNLIMITED_SELECT=1", "_DARWIN_USE_64_BIT_INODE=1"};
        x.srcs += {
            base + "/src/unix/darwin-proctitle.c",
            base + "/src/unix/darwin.c",
            base + "/src/unix/fsevents.c"
        };
    }

} //cxx_static(libuv)
