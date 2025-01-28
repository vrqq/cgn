#pragma once
#include <string>

namespace cgnv1 {

struct CGNPath {

    constexpr static char BASE_ON_OUTPUT      = 0;
    constexpr static char BASE_ON_SCRIPT_SRC  = 1;
    constexpr static char BASE_ON_WORKINGROOT = 2;
    char type = 2;

    std::string rpath;

    CGNPath(const std::string &rel = "") {}
    CGNPath(char t, const std::string &rel) : type(t), rpath(rel) {}

    std::string to_string() const {
        if (type == BASE_ON_OUTPUT)
            return "<OUT_PREFIX>" + rpath;
        if (type == BASE_ON_SCRIPT_SRC)
            return "<SCRIPTDIR>" + rpath;
        return rpath.size()?rpath:".";
    }
};

inline CGNPath make_path_base_out(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_OUTPUT, rel};
}
inline CGNPath make_path_base_script(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_SCRIPT_SRC, rel};
}
inline CGNPath make_path_base_working(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_WORKINGROOT, rel};
}

} //namespace
