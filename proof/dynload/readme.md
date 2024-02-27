
测试当修改dll重新加载，能否再次触发变量更新

可通过 static variable in class, global variable, static global variable 触发dlopen/dlclose时候的注册和取消注册
linux下亦支持`__attribute__((destructor))` 详见proof/dynload

**on Linux**
`RTLD_NODELETE (since glibc 2.2)`
https://man7.org/linux/man-pages/man3/dlopen.3.html

