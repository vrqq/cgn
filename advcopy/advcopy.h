// This target should compiled by c++17 standard, 
// so it can be part of cgn, but not as the part
// of Interpreter
//
#pragma once
#include <string>
#include <vector>

namespace cgnv1 {
struct AdvanceCopy
{

static std::vector<std::string> file_match(
    const std::string &pattern,
    std::string *errmsg
);

static void match_debug(const std::string &pattern);

static std::string copy_to_dir(
    const std::vector<std::string> &src, 
    const std::string &src_base, 
    const std::string &dst_dir,
    const std::string depfile,
    const std::string stampfile
);


static std::string flatcopy_to_dir(
    const std::vector<std::string> &src, 
    const std::string &dst_dir,
    const std::string depfile,
    const std::string stampfile,
    bool cout_log = false
);


};
} //namspace cgnv1
