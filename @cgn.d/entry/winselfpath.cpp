
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

// unused in xnix system
#include <string>
namespace cgn {
    std::string self_realpath();
};

#endif