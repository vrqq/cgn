// This target should compiled by c++17 standard, 
// so it can be part of cgn, but not as the part
// of Interpreter
//
#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace cgnv1 {

struct AdvanceCopy
{
    struct SearchRecord {
        using fspath = std::filesystem::path;

        // only regular_file, symlink_file or empty_dir
        // pair<path matched pattern, the relative path of matched path (empty allowed)>
        // the full path need to copy: {pair.first / pair.second}
        std::vector<std::pair<fspath, fspath>> file_need_copy;

        // regular_file, symlink_file or directory
        // node add to depfile
        std::vector<fspath> node_need_watch;

        // std::vector<std::string> result;
        std::string errmsg;
    };

    static SearchRecord path_search(const std::string &pattern);

    static std::vector<std::string> file_match(
        const std::string &pattern,
        std::string *errmsg
    );

    static void match_debug(const std::string &pattern);

    static std::string copy_to_dir(
        const std::vector<std::string> &src_rel, 
        const std::vector<std::string> &src_rel_exclude, 
        const std::string &src_base, 
        const std::string &dst_dir,
        const std::string depfile,
        const std::string stampfile,
        bool print_log = false
    );

    static std::string flatcopy_to_dir(
        const std::vector<std::string> &src, 
        const std::vector<std::string> &src_exclude, 
        const std::string &dst_dir,
        const std::string depfile,
        const std::string stampfile,
        bool print_log = false
    );

    static std::string copy_rename(
        const std::string &src,
        const std::string &dst,
        const std::string &depfile,
        const std::string &stampfile,
        bool print_log = false
    );

}; //struct AdvanceCopy

} //namspace cgnv1
