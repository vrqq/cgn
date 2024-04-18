#include <stdexcept>
#include "dl_helper.h"

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
        #define NOMINMAX_DEFINED_OUTSIDE_DLLLOADER
    #endif
    #include <Libloaderapi.h>
    #include <errhandlingapi.h>
    // #include <winbase.h>
    #ifndef NOMINMAX_DEFINED_OUTSIDE_DLLLOADER
        #undef NOMINMAX
    #endif
    
namespace cgn {
    DLHelper::DLHelper(const std::string &file) {
        m_ptr = LoadLibrary(file);
    }
    DLHelper::~DLHelper() {
        FreeLibrary(m_ptr);
    }
}
#endif

#ifdef __linux__
    #include <dlfcn.h>
    #include <unistd.h>
    #include <linux/limits.h>
    
namespace cgn {
    DLHelper::DLHelper(const std::string &file) {
        m_ptr = dlopen(file.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (!m_ptr)
            throw std::runtime_error{
                "Cannot load library: " + file + ": " + dlerror()};
    }
    DLHelper::~DLHelper() {
        dlclose(m_ptr);
    }
}
#endif