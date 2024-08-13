#pragma once
#include <string>

static std::string get_ext(const std::string &fpath)
{
    auto fd = fpath.rfind('.');
    if (fd == fpath.npos)
        return "";
    
    std::string ext = fpath.substr(fd+1);
    for (char &c : ext)  // to lower case
        if ('A' <= c && c <= 'Z')
            c = c - 'A' + 'a';
    
    return ext;
} //std::string get_ext
