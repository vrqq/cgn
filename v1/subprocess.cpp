#ifdef _WIN32
    #include "../ninjabuild/src/subprocess-win32.cc"
#else
    #include "../ninjabuild/src/subprocess-posix.cc"
#endif