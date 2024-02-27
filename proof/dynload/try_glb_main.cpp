#include <iostream>

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
    HMODULE base_ptr = 0;
    void load() {
        base_ptr = LoadLibrary("try_glb_ext.dll");
    }
    void unload() {
        FreeLibrary(base_ptr);
    }
#endif

#ifdef __linux__
    #include <dlfcn.h>
    #include <unistd.h>
    #include <linux/limits.h>
    void *base_ptr = nullptr;
    void load() {
        base_ptr = dlopen("libtry_glb_ext.so", RTLD_LAZY | RTLD_LOCAL);
    }
    void unload() {
        dlclose(base_ptr);
    }
#endif

int main()
{
    std::cout<<"BEFORE-DLOPEN"<<std::endl;
    load();
    std::cout<<"END-DLOPEN"<<std::endl;

    std::cout<<"BEFORE-DLCLOSE"<<std::endl;
    unload();
    std::cout<<"END-DLCLOSE"<<std::endl;

    std::cout<<"BEFORE-RELOAD"<<std::endl;
    load();
    std::cout<<"END-RELOAD"<<std::endl;

    return 0;
}