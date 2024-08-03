#include <iostream>
#include <dlfcn.h>
#include <unistd.h>
#include <linux/limits.h>

int main() {
    auto *m_ptr = dlopen("libdll1.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!m_ptr)
        std::cout<<dlerror()<<std::endl;
    return 0;
}
