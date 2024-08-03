// https://inbox.sourceware.org/gcc-help/CAAxjCEzY31LuebN_Y-w3p=eNmA0vikOPtCtMSUqSy6nhnN5Viw@mail.gmail.com/T/

#include <iostream>
#include <dlfcn.h>
#include <unistd.h>
#include <linux/limits.h>

extern int get() __attribute__((weak));
extern void pusherror() __attribute__((weak));
 
int main() {
    dlopen("libfn.so", RTLD_LAZY | RTLD_GLOBAL);
    dlopen("libfn2.so", RTLD_LAZY | RTLD_GLOBAL);
    // try{
        std::cout<<"Hello!"<<get()<<std::endl;
        pusherror();
    // }catch(std::logic_error &e) {
    //     std::cout<<"CATCHED: "<<e.what()<<std::endl;
    // }
    std::cout<<"DONE"<<std::endl;
    return 0;
}