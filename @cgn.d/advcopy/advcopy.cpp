#include <iostream>
#include <fstream>
#include <unordered_set>
#include <filesystem>
#include <cassert>
#include "advcopy.h"

namespace cgnv1 {

namespace fs = std::filesystem;

namespace {

struct SearchRecord {
    bool opt_skip_invalid_symlink;

    // only regular_file, symlink_file or empty_dir
    // pair<path matched pattern, the relative path of matched path (empty allowed)>
    // the full path need to copy: {pair.first / pair.second}
    std::vector<std::pair<fs::path, fs::path>> file_need_copy;

    // regular_file, symlink_file or directory
    // node add to depfile
    std::vector<fs::path> node_need_watch;

    // std::vector<std::string> result;
    std::string errmsg;
};

// for example 'aa/bb/str1**str2'
//      prefix : 'aa/bb'
//      before : 'str1'
//      after  : 'str2'
// @param is_prefix_matched:
//      true : 'aa/bb/*/' matched with 'aa/bb/cc/'
//     false : 'aa/bb/**' matched with 'aa/bb/cc/dd/ee' or any of 'aa/bb/.....'
static void case_2stars(
    const fs::path &prefix,
    std::string_view before_2star,
    std::string_view after_2star,
    SearchRecord &rec,
    bool is_prefix_matched
) {
    assert(fs::is_directory(prefix));

    for (auto const &iter : fs::recursive_directory_iterator(prefix)) {
        //skip invalid symlink
        // if (rec.opt_skip_invalid_symlink && iter.is_symlink()) {
        //     if (std::error_code ec; iter.is_regular_file(ec), ec)
        //         continue;
        // }

        if (!iter.is_symlink() && iter.is_directory()) {
            rec.node_need_watch.push_back(iter.path());
            continue;
        }
        if (!iter.is_symlink() && !iter.is_regular_file())
            continue;
        
        std::string filename = iter.path().filename().string();
        if (filename.size() < before_2star.size() + after_2star.size())
            continue;
        bool matched = (before_2star == filename.substr(0, before_2star.size()))
                    && (after_2star == filename.substr(filename.size()-after_2star.size()));
        if (matched){
            if (is_prefix_matched)
                rec.file_need_copy.push_back({prefix, iter.path().lexically_relative(prefix)});
            else
                rec.file_need_copy.push_back({iter.path(), {}});
            rec.node_need_watch.push_back(iter.path());
        }
    }
} //case_2stars()

// Match rules:
//   if '*' or '**' is not the last section, this section must be folder (var 'now')
//   if '*' in last section, both file and folder accepted.
//   '**' only accepted in the last section
//
// Note that 'remain' can be end with '/' to indicate that the end section is folder.
// @param prefix: dir searching currently
// @param remain: empty for current dir of 'prefix'
static void path_search_impl(
    const fs::path &prefix,
    std::string remain,
    SearchRecord &rec
) {
    assert(!fs::is_symlink(prefix) && fs::is_directory(prefix));
    if (rec.errmsg.size())
        return ;

    bool pattern_last_section;
    std::string now;
    if (auto fdslash = remain.find(fs::path::preferred_separator); 
        fdslash != remain.npos
    ) {
        now = remain.substr(0, fdslash);
        remain = remain.substr(fdslash+1);
        pattern_last_section = false;
    }
    else {
        now = remain;
        remain = "";
        pattern_last_section = true;
    }

    // case: current dir
    if (now.empty() && remain.empty()) {
        case_2stars(prefix, "", "", rec, true);
        return;
    }
    
    // check if there's more than one '*' in a section
    auto fdstar = now.find('*');
    if (fdstar != now.npos) {
        if (fdstar+2 < now.size() && now.find('*', fdstar+2) != now.npos) {
            rec.errmsg = "'*' can only appear once in one section.";
            return ;
        }    
        if (fdstar+1 < now.size() && now[fdstar+1] == '*') {
            if (!pattern_last_section)
                rec.errmsg = "'**' can only appear in the last section.";
            else // case: '**' in last section
                case_2stars(prefix, now.substr(0, fdstar), now.substr(fdstar+2), rec, false);
            return ;
        }
    }

    // if '*' appear in this_section, watch folder 'prefix'
    // e.g.: 'aa/bb/*/...'  ==> watch folder 'aa/bb'
    if (fdstar != now.npos)
        rec.node_need_watch.push_back(prefix);
    
    // iterator all files in current dir and match
    // symlink is seen as 'normal-file' and do not forward it.
    // condition to match:
    //       ( pattern_last_section && (is_symlink || is_regular_file) )
    //   || ( !pattern_last_section && !is_symlink && is_directory )
    std::string_view sprefix, ssuffix;
    if (fdstar != now.npos) {
        sprefix = std::string_view{now}.substr(0, fdstar);
        ssuffix = std::string_view{now}.substr(fdstar+1);
    }
    for (auto ff : fs::directory_iterator(prefix)) {
        // Symlinks are handled by the OS filesystem, and in some operating systems,
        // invalid symlinks cannot be copied.
        // if (rec.opt_skip_invalid_symlink && ff.is_symlink()) {
        //     if (std::error_code ec; ff.is_regular_file(ec), ec)
        //         continue;
        // }
        std::string ffname = ff.path().filename().string();
        bool ff_is_endnode = ff.is_symlink() || ff.is_regular_file();
        bool ff_is_dir = !ff.is_symlink() && ff.is_directory();
        // bool can_match = (pattern_last_section && (ff.is_symlink() || ff.is_regular_file()))
        //              || (!pattern_last_section && !ff.is_symlink() && ff.is_directory());
        bool matched = false;
        if (pattern_last_section || ff_is_dir) {
            if (fdstar == now.npos)
                matched = (now == ffname);
            else if (now.size()-1 > ffname.size())
                matched = false;
            else
                matched = (sprefix == ffname.substr(0, sprefix.size()))
                       && (ssuffix == ffname.substr(ffname.size() - ssuffix.size()));
        }
        if (!matched || rec.errmsg.size())
            continue;
        
        if (remain.empty()) {
            if (ff_is_dir) //same as path_search_impl(ff, "");
                case_2stars(ff, "", "", rec, true);
            else if (ff_is_endnode) {
                rec.file_need_copy.push_back({ff.path(), ""});
                rec.node_need_watch.push_back(ff.path());
            }
        }
        else if (ff_is_dir)
            path_search_impl(ff.path(), remain, rec);
    } //endfor(dir_iterator{prefix})

}  //void path_search_impl()

SearchRecord path_search(const std::string &pattern, bool skip_invalid_symlink = false)
{
    SearchRecord rec;
    rec.opt_skip_invalid_symlink = skip_invalid_symlink;
    fs::path fp{pattern};
    
    fp = fp.lexically_normal().make_preferred();
    while (fp.has_filename() && fp.filename() == ".")
        fp = fp.parent_path();
    
    // case: current dir
    if (fp.has_root_path())
        path_search_impl(fp.root_path(), fp.relative_path().string(), rec);
    else
        path_search_impl(fs::path{"."}, fp.string(), rec);

    return rec;
}

std::string makefile_escape(const std::string &in)
{
    std::string rv;
    bool have_colon = false;
    for (auto ch : in) {
        if (ch==' ' || ch=='\\' || ch=='(' || ch==')')
            rv.push_back('\\');
        else if (ch == '$')
            rv.push_back('$');
        rv.push_back(ch);
        if (ch == ':')
            have_colon = true;
    }
    if (have_colon)
        return "'" + rv + "'";
    return rv;
}

// @return errmsg, is_updated
std::pair<std::string, bool> copy_impl(const fs::path &src, const fs::path &dst)
{
    bool updated = false;
    try{
        if (!fs::is_symlink(dst) && fs::exists(dst) && !fs::is_regular_file(dst) && !fs::is_directory(dst))
            return {dst.string() + " unsupported type.", false};

        if (fs::is_symlink(src)) {
            if (!fs::is_symlink(dst) && fs::is_directory(dst))
                return {src.string() + " is symlink, but " + dst.string() + " is directory.", false};
            if (!fs::is_symlink(dst) || fs::read_symlink(src) != fs::read_symlink(dst)) {
                fs::remove(dst);
                fs::create_symlink(fs::read_symlink(src), dst);
                updated = true;
            }
        }
        else if (fs::is_regular_file(src)) {
            if (fs::is_symlink(dst))
                fs::remove(dst);
            else if (fs::is_directory(dst))
                return {src.string() + " is regular file, but " + dst.string() + " is directory.", false};
            if (!fs::exists(dst) || fs::last_write_time(src) != fs::last_write_time(dst)) {
                fs::create_directories(dst.parent_path());
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing), updated = true;
            }
        }
        else if (fs::is_directory(src)) {
            if (fs::is_symlink(dst) || fs::is_regular_file(dst))
                fs::remove(dst);
            if (!fs::exists(dst) || fs::last_write_time(src) != fs::last_write_time(dst))
                fs::copy(src, dst, fs::copy_options::recursive), updated = true;
        }

        // set mtime after copy
        if (!fs::is_symlink(src))
            fs::last_write_time(dst, fs::last_write_time(src));
    }catch(std::filesystem::filesystem_error &e) {
        return {src.string() + " --> " + dst.string() + " : " + e.what(), false};
    }

    return {"", updated};
}

} //namespace<>


std::vector<std::string> AdvanceCopy::file_match(
    const std::string &pattern,
    std::string *errmsg
) {
    auto resp = path_search(pattern);
    if (resp.errmsg.size()) {
        if (errmsg)
            *errmsg = resp.errmsg;
        return {};
    }
    std::vector<std::string> rv;
    for (auto it : resp.file_need_copy)
        rv.push_back((it.first / it.second).string());
    return rv;
}

void AdvanceCopy::match_debug(const std::string &pattern)
{
    std::cout<<"--- Match Debug ---\n";
    auto resp = path_search(pattern);
    if (resp.errmsg.size()) {
        std::cout<<pattern<<" : "<<resp.errmsg<<"\n";
        return ;
    }
    std::cout<<"- Pattern: "<<pattern<<"\n"
             <<"- Matched files:\n";
    std::filesystem::path last_p1;
    for (auto &[p1, p2] : resp.file_need_copy) {
        if (p2.empty())
            std::cout<<"  "<<p1.string()<<"\n";
        else {
            if (p1 != last_p1)
                std::cout<<p1.string()<<"\n";
            std::cout<<"    "<<(p1/p2).string()<<"\n";
        }
        last_p1 = p1;
    }
    
    std::cout<<"- Watch nodes:\n";
    for (auto it : resp.node_need_watch)
        std::cout<<"  "<<it.string()<<"\n";
    std::cout<<"--------------\n";
}

std::string AdvanceCopy::flatcopy_to_dir(
    const std::vector<std::string> &src_list, 
    const std::string &dst_dir,
    const std::string depfile,
    const std::string stampfile,
    bool print_log
) {
    if (print_log)
        std::cout<<"--- Flat Copy ---\n";
    std::ofstream fdep(depfile);
    fdep<<makefile_escape(stampfile)<<" : ";

    for (auto pattern : src_list) {
        auto rec = path_search(pattern, true);
        if (rec.errmsg.size())
            return pattern + " : " + rec.errmsg;
        
        if (print_log)
            std::cout<<pattern<<"\n";
        for (auto [_src1, _src2] : rec.file_need_copy) {
            fs::path src = _src2.empty()? _src1 : (_src1 / _src2);
            fs::path dst = fs::path{dst_dir} / _src1.filename();
            if (!_src2.empty())
                dst /= _src2;
            auto [errmsg, updated] = copy_impl(src, dst);
            if (errmsg.size())
                return errmsg;
            if (print_log)
                std::cout<<"  "<<src.string()<<"\n  └--> "<<(updated?dst.string():"skipped")<<"\n";
        } //end_for(path_search().file_need_copy)

        for (auto it : rec.node_need_watch)
            fdep<<makefile_escape(it.string())<<" ";
    } //end_for(pattern : arg[src_list])

    std::ofstream fstamp(stampfile);
    return "";
}

std::string AdvanceCopy::copy_to_dir(
    const std::vector<std::string> &srcs_part2, 
    const std::string &src_base, 
    const std::string &dst_dir,
    const std::string depfile,
    const std::string stampfile,
    bool print_log
) {
    if (print_log)
        std::cout<<"--- Copy ---\n";
    std::ofstream fdep(depfile);
    fdep<<makefile_escape(stampfile)<<" : ";

    for (auto pattern : srcs_part2) {
        fs::path src_pattern = fs::path{src_base} / pattern;
        auto rec = path_search(src_pattern.string(), true);
        if (rec.errmsg.size())
            return pattern + " : " + rec.errmsg;
        
        if (print_log)
            std::cout<<src_pattern<<"\n";
        for (auto [_src1, _src2] : rec.file_need_copy) {
            fs::path src = _src2.empty()? _src1 : (_src1 / _src2);
            src = fs::proximate(src, "./");
            fs::path dst = fs::path{dst_dir} / src.lexically_proximate(src_base);
            auto [errmsg, updated] = copy_impl(src, dst);
            if (errmsg.size())
                return errmsg;
            if (print_log)
                std::cout<<"  "<<src.string()<<"\n"
                         <<"  └--> "<<(updated?dst.string():"skipped")<<"\n";
        } //end_for(path_search().file_need_copy)

        for (auto it : rec.node_need_watch)
            fdep<<makefile_escape(it.string())<<" ";
    } //end_for(pattern : arg[src_list])

    std::ofstream fstamp(stampfile);
    return "";
}

} //namespace
