#include <stdexcept>
#include "dl_helper.h"

#ifdef _WIN32
    
namespace cgnv1 {
    DLHelper::DLHelper(const std::string &file) {
        hnd = ::cgn::GlobalSymbol::WinLoadLibrary(file);
    }
    DLHelper::~DLHelper() {
        ::cgn::GlobalSymbol::WinUnloadLibrary(hnd);
    }
}

#else

#include <dlfcn.h>
#include <unistd.h>
    
namespace cgnv1 {
    DLHelper::DLHelper(const std::string &file) {
        m_ptr = dlopen(file.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (!m_ptr)
            throw std::runtime_error{
                "Cannot load library " + file + ": " + dlerror()};
    }
    DLHelper::~DLHelper() {
        dlclose(m_ptr);
    }
}
#endif