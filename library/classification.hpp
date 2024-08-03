#pragma once
#include <string>
#include <unordered_map>
#include "../provider.h"

namespace cgn {

// For MacOS (i)
//      LinkAndRunInfo.shared : .so folder.kext folder.dylib
//      LinkAndRunInfo.static : .a
//      LinkAndRunInfo.object : .o
// For Linux (l)
//      LinkAndRunInfo.shared : .so
//      LinkAndRunInfo.static : .a
//      LinkAndRunInfo.object : .o
// For Windows (w)
//      LinkAndRunInfo.shared : .lib (shared lib only)
//      LinkAndRunInfo.static : .lib (static lib only)
//      LinkAndRunInfo.object : .obj
inline cgn::LinkAndRunInfo LinkAndRunInfo_classification(
    const std::vector<std::string> &in,
    const std::string file_base
) {
    cgn::LinkAndRunInfo inf;

    std::unordered_set<std::string> dllstem;
    std::vector<std::pair<std::string,std::string>> dotlib;
    for (auto &file : in) {
        auto fd1   = file.rfind('/');
        auto fddot = file.rfind('.');
        fd1 = (fd1 == file.npos? 0: fd1+1);
        if (fddot == file.npos || fddot < fd1)
            continue; //invalid file
        std::string stem  = file.substr(fd1, fddot-fd1);
        std::string ext   = file.substr(fddot);
        std::string fullp = file_base + "/" + file;
        if (ext == ".so" || ext == ".dylib" || ext == ".kext")
            inf.shared_files.push_back(fullp);
        else if (ext == ".a")
            inf.static_files.push_back(fullp);
        else if (ext == ".dll") {
            inf.runtime_files[file] = fullp;
            dllstem.insert(stem);
        }
        else if (ext == ".lib")
            dotlib.push_back({stem, fullp});
        else
            continue;
    }
    for (auto item : dotlib)
        if (dllstem.count(item.first) != 0)
            inf.shared_files.push_back(item.second);
        else
            inf.static_files.push_back(item.second);

    return inf;
}

};