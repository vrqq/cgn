#include <iostream>
#include <dlfcn.h>

// ===fail===
// extern int cgn_setup(int x) __attribute__((weak_import, visibility("default")));
// extern int cgn_setup(int x) __attribute__((weak, visibility("default")));
// extern int cgn_setup(int x) __attribute__((weak));

//succ
// int cgn_setup(int x);
// extern int cgn_setup(int x);
extern int cgn_setup(int x) __attribute__((visibility("default")));

int main()
{
    std::cout << "Loading mydl.so ..." << std::endl;
    void* handle = dlopen("./mydl.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "Failed to load mydl.so: " << dlerror() << std::endl;
        return 1;
    }


    std::cout<< "call func()..." <<std::endl;
    // getchar();
    typedef int (*FuncType)();
    FuncType func = (FuncType)dlsym(handle, "_Z4funcv");
    if (!func) {
        std::cerr << "Failed to find symbol 'func': " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }
    std::cout<< func() <<std::endl;


    std::cout<< "call setup()..." <<std::endl;
    // getchar();
    std::cout<<cgn_setup(42)<<std::endl;

    dlclose(handle);
    return 0;
}
