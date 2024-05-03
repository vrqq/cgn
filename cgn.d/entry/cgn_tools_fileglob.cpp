//
// Match rules:
// '**' match all include FILE-SEPARATOR
// '*' match all exclude FILE-SEPARATOR
//
// Conding suggestion:
// if '**' was meet, all paths after star-pattern would expand.
// 
#include <regex>
#include <iostream>
#include <filesystem>
#include <cassert>
#include "../cgn.h"

namespace {

#ifdef _WIN32
constexpr char SEP = '\\';
#endif

#ifdef __linux__
constexpr char SEP = '/';
#endif

bool match(std::string section_name, std::string_view want)
{
    std::string_view tgt{section_name};
    if (want.back() != '*') {
        if (auto fd = want.rfind('*'); fd != want.npos) {
            std::string_view wtail = want.substr(fd+1);
            if (tgt.size() < wtail.size())
                return false;
            if (tgt.substr(tgt.size() - wtail.size()) != wtail)
                return false;
            want = want.substr(0, fd+1);
            tgt = tgt.substr(0, tgt.size() - wtail.size());
        }
        else
            return tgt == want;
    }

    if (want.front() != '*') {
        if (auto fd = want.find('*'); fd != want.npos) {
            if (tgt.size() < fd || tgt.substr(0, fd) != want.substr(0, fd))
                return false;
            tgt = tgt.substr(fd+1);
            want = want.substr(fd+1);
        }
        else
            return tgt == want;
    }

    // the 'want' must begin and end with '*' here.
    // then pop front '*'
    want = want.substr(1);
    for (size_t i=0, j=0; i<tgt.size() && j<want.size(); ) {
        auto fdj = want.find('*', j);
        std::string_view part = want.substr(j, fdj - j);
        if (part.empty()) {
            j = fdj + 1;
            continue;
        }
        if (auto fdi = tgt.find(part, i); fdi != tgt.npos)
            j = fdj + 1, i = fdi + part.size();
        else
            return false;
    }

    return true;
}

void recursive(std::vector<std::string> &out, std::filesystem::path dir, 
               std::string_view want_tail, const std::string &base_path)
{
    assert(std::filesystem::is_directory(dir));

    for (auto ff : std::filesystem::directory_iterator(dir)) {
        if (ff.is_directory())
            recursive(out, ff, want_tail, base_path);
        if (ff.is_regular_file()) {
            std::string name = ff.path().filename().string();
            if (name.size() > want_tail.size()) {
                auto off = name.size() - want_tail.size();
                std::string_view now{name.c_str() + off, want_tail.size()};
                if (now == want_tail)
                    out.push_back(base_path.empty()? 
                        std::filesystem::absolute(ff).string() :
                        std::filesystem::proximate(ff, base_path).string()
                    );
            }
        }
    }
}

void work(std::vector<std::string> &out, std::filesystem::path prefix, 
          std::string_view tail, const std::string &base_path
) {
    if (!std::filesystem::exists(prefix))
        return ;
    if (tail.empty() && std::filesystem::is_regular_file(prefix)) {
        out.push_back(base_path.empty()?
            std::filesystem::absolute(prefix).string() : 
            std::filesystem::proximate(prefix, base_path).string()
        );
        return ;
    }
    if (!std::filesystem::is_directory(prefix))
        return ;
    // std::cout<<"[DBG] "<<prefix<<std::endl;

    std::string_view next_arg2 = "";
    std::string_view want_word = tail;
    enum { DIR, FILE}want_type = FILE;
    if (auto fd = tail.find(SEP); fd != tail.npos) { //dir
        want_word = tail.substr(0, fd);
        want_type = DIR;
        next_arg2 = tail.substr(fd+1);
    }

    //special case: expend all file recursively inside this path
    if (auto fd = tail.find("**"); want_type == FILE && fd != tail.npos)
        return recursive(out, prefix, tail.substr(2), base_path);

    for (auto subf : std::filesystem::directory_iterator(prefix)) {
        if (subf.is_symlink()) //skip all symlink (include invalid symlink)
            continue;
        if (subf.is_directory() && want_type == DIR) {
            std::string dir_name = subf.path().filename().string();
            // std::cout<<"[DBG-DIRNAME] "<<subf<<" "<<dir_name<<std::endl;
            if (match(dir_name, want_word))
                work(out, subf, next_arg2, base_path);
        }
        if (subf.is_regular_file() && want_type == FILE) {
            std::string file_name = subf.path().filename().string();
            if (match(file_name, want_word))
                out.push_back(base_path.empty()?
                    std::filesystem::absolute(subf).string() : 
                    std::filesystem::proximate(subf, base_path).string()
                );
        }
    }
} //work()
   
} //namespace <>

namespace cgn {

std::vector<std::string> Tools::file_glob(const std::string &dir, const std::string &base)
{
    std::string s_raw = dir;
    //convert to unix path-delimiter '/'
    #ifdef _WIN32
    for (auto &ch : s_raw){
        if (ch == '/')
            ch = '\\';
    }
    #endif

    std::string_view ss{s_raw};
    
    if (auto fd = ss.find("**"); fd != ss.npos) {
        if (auto fd2 = ss.find(SEP, fd); fd2 != ss.npos)
            throw std::runtime_error{
                "** must be in the last part."
            };
    }
    if (auto fd = ss.rfind("**"); fd != ss.npos) {
        if (fd > 0 && ss[fd-1] != SEP)
            throw std::runtime_error{
                "** must be the prefix of last part."
            };
    }

    std::vector<std::string> rv;
    if (auto fd = ss.find('*'); fd != ss.npos) {
        auto fd2 = ss.rfind(SEP, fd);
        work(rv, ss.substr(0, fd2), ss.substr(fd2+1), base);
    }
    else
        work(rv, ss, "", base);
    
    return rv;
} //file_glob()

} //namespace cgn