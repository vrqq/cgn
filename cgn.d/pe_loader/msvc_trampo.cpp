#include <filesystem>
#include <cstdlib>
#include "pe_file.h"
#include "../entry/debug.h"

namespace cgn {

std::unordered_set<std::string> MSVCTrampo::sys_libs;
std::unordered_set<std::string> MSVCTrampo::msvcrt_symbols;

MSVCTrampo::MSVCTrampo() {
    // since we always use /MD in msvc script
    // here we needn't to switch to vcruntimed
    static int init_default_lib = [](){
        add_msvcrt_lib("kernel32");
        add_msvcrt_lib("vcruntime");
        add_msvcrt_lib("ucrt");
        return 1;
    }();
}

void MSVCTrampo::add_msvcrt_lib(std::string libname)
{
    if (libname.size() < 4 || libname.substr(libname.size()-4) != ".lib")
        libname += + ".lib";

    static std::vector<std::string> libpath = [](){
        std::string estr = std::getenv("LIB");
        if (estr.empty()) {
            logger.paragraph("cannot get environment 'LIB'");
            throw std::runtime_error{"getenv('LIB')"};
        }
        std::vector<std::string> rv;
        for (size_t i=0; i<estr.size();) {
            auto fd = estr.find(';', i);
            if (fd == estr.npos)
                fd = estr.size();
            if (fd != i)
                rv.push_back(estr.substr(i, fd-i));
            i = fd+1;
        }
        return rv;
    }();

    if (sys_libs.insert(libname).second == false)
        return ;

    std::string path_found;
    for (auto prefix : libpath) {
        std::string p1 = prefix + "\\" + libname;
        if (std::filesystem::is_regular_file(p1)) {
            path_found = p1;
            break;
        }
    }

    if (path_found.empty())
        throw std::runtime_error{libname + " not found."};
    
    std::ifstream fin(path_found, std::ios::binary);
    auto [rv, error_log] = cgn::LibraryFile::extract_exported_symbols(fin);
    if (error_log.size())
        throw std::runtime_error{"Load library " + path_found + ": " + error_log};
    
    msvcrt_symbols.insert(rv.begin(), rv.end());
} // MSVCTrampo::add_msvcrt_lib()

void MSVCTrampo::add_objfile(const std::string &filename)
{
    std::ifstream fin(filename, std::ios::binary);
    auto [rv1, elog1] = cgn::COFFFile::extract_somedata(fin);
    if (elog1.size())
        throw std::runtime_error{"Objfile " + filename + ": " + elog1};
    
    for (auto &libname : rv1.defaultlib)
        add_msvcrt_lib(libname);
    undef_syms.insert(rv1.undef_symbols.begin(), rv1.undef_symbols.end());
}

void MSVCTrampo::make_asmfile(const std::string &file_out)
{
    for (auto iter = undef_syms.begin(); iter != undef_syms.end();)
        if (msvcrt_symbols.count(*iter))
            iter = undef_syms.erase(iter);
        else
            iter++;
    
    // ASM format:
    //  .const
    //      msvc_trampo_1 DB "...",0
    //  end
    //  .code
    //  func_xxx PROC
    //      push msvc_trampo_1
    //      call ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z
    //      add rsp, 4
    //      jmp rax
    //  func_xxx ENDP
    //  .end
    std::ofstream fout(file_out);
    std::string code_section;
    int i = 0;
    fout << ".const\n";
    for (auto &sym : undef_syms) {
        std::string varname = "msvc_trampo_" + std::to_string(i++);
        fout << "  " << varname << " DB \"" + sym + "\",0\n";
        code_section +=
            "  " + sym + " PROC\n"
            "    push " + varname + "\n"
            "    call ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z\n"
            "    add rsp, 4\n"
            "    jmp rax\n"
            "  " + sym + " ENDP\n\n";
    }
    fout << "end\n\n"
         << ".code\n"
         << code_section
         << "end\n";
    fout.close();
}



} //namespace