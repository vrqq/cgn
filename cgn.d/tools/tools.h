#include <string>

struct HostInfo {
    //os : win, linux, mac
    //cpu: x86, x64, arm64
    std::string os, cpu;

    // gnu_get_libc_version() : 2.8
    // gnu_get_libc_release() : stable
    std::string glibc_version, glibc_release;

};

// void get_host_info();