//
// cl.exe cp3.cpp /EHsc /std:c++17
#include <filesystem>
#include <iostream>
#include <string>
#include <cstdlib>

#ifdef _WIN32
    constexpr char DELIM = '\\';
#else
    constexpr char DELIM = '/';
#endif

int wincp(std::string src, std::string dst) {
    for (auto &c : src)
        if (c == '\\' || c == '/')
            c = DELIM;
    for (auto &c : dst)
        if (c == '\\' || c == '/')
            c = DELIM;
    
    try {
        std::filesystem::path pth1(src);
        std::string cmd;
        if (std::filesystem::is_directory(pth1))
            cmd = "xcopy /S /E /H /Y /F /I \"" + src + "\" \"" + dst + "\"";
        else
            cmd = "copy /Y \"" + src + "\" \"" + dst + "\"";
        return system(cmd.c_str());
    }catch(...) {
        return 1;
    }
    return 0;
}
