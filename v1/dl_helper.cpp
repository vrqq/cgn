#include <stdexcept>
#include <filesystem>
#include "dl_helper.h"

#ifdef _WIN32
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <Windows.h>
namespace cgnv1 {
    DLHelper::DLHelper(const std::string &file) {
        // std::filesystem::path p{file};
        // if (!::SetDllDirectory(p.parent_path().string().c_str()))
        //     throw std::runtime_error{"Cannot set DLL search path"};
        hnd = GlobalSymbol::WinLoadLibrary(file);
    }
    DLHelper::~DLHelper() {
        GlobalSymbol::WinUnloadLibrary(hnd);
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
                "(dlopen)Cannot load library " + file + ": " + dlerror()};
    }
    DLHelper::~DLHelper() {
        dlclose(m_ptr);
    }
}
#endif