
#ifdef _WIN32
#include <string>
#include <windows.h>
#include <shlwapi.h>

namespace cgn {
    std::string self_realpath() {
        TCHAR name[4096];
        ZeroMemory(name, sizeof(name));
        if (auto len = GetModuleFileNameA(NULL, name, sizeof(name)); len > 0)
            return name;
        return "";
    }
};

#else
#include <string>
#include <cstring>
#include <unistd.h>         // readlink
#include <linux/limits.h>   // PATH_MAX

namespace cgn {
    std::string self_realpath() {
        char result[PATH_MAX];
        memset(result, 0, sizeof(result));
        //readlink() does not append a terminating null byte to buf.
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        if (count != -1)
            return result;
        return "";
    }
};

#endif