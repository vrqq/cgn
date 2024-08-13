
#ifdef _WIN32
    decltype(__dllexport)
#else
    __attribute__((visibility("default")))
#endif
int sum(int a, int b) { return a+b; }