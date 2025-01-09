#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdlib>
#include "pe_file.h"
#include "../v1/cgn_api.h"

namespace cgnv1 {

std::unordered_set<std::string> MSVCTrampo::sys_libs;
std::unordered_set<std::string> MSVCTrampo::msvcrt_symbols;

MSVCTrampo::MSVCTrampo() {
    // since we always use /MD in msvc script
    // here we needn't to switch to vcruntimed
    #ifdef _WIN32
        static int init_default_lib = [](){
            add_msvcrt_lib("kernel32");
            add_msvcrt_lib("vcruntime");
            add_msvcrt_lib("ucrt");
            return 1;
        }();
    #endif
}

void MSVCTrampo::add_msvcrt_lib(std::string libname)
{
    if (libname.size() < 4 || libname.substr(libname.size()-4) != ".lib")
        libname += + ".lib";

    static std::vector<std::string> libpath = [](){
        std::string estr = Tools::getenv("LIB");
        if (estr.empty()) {
            // logger.paragraph("cannot get environment 'LIB'");
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
    
    add_lib(path_found);
} // MSVCTrampo::add_msvcrt_lib()

void MSVCTrampo::add_lib(std::string libname)
{
    std::ifstream fin(libname, std::ios::binary);
    auto [rv, error_log] = LibraryFile::extract_exported_symbols(fin);
    if (error_log.size())
        throw std::runtime_error{"Load library " + libname + ": " + error_log};
    
    msvcrt_symbols.insert(rv.begin(), rv.end());
}

void MSVCTrampo::add_objfile(const std::string &filename)
{
    std::ifstream fin(filename, std::ios::binary);
    auto [rv1, elog1] = COFFFile::extract_somedata(fin);
    if (elog1.size())
        throw std::runtime_error{"Objfile " + filename + ": " + elog1};
    
    for (auto &libname : rv1.defaultlib)
        add_msvcrt_lib(libname);
    undef_syms.insert(rv1.undef_symbols.begin(), rv1.undef_symbols.end());
}

// === ASM output ===
//  .const ;only for long function
//      msvc_trampo_1 DB "...",0
//  end
//  .code
//      <asm_proc_label> PROC
//          lea RCX, <asm_arg_str>
//          call find_symbol()
//      <asm_proc_label> ENDP
//      ??func2@@YAXXZ PROC
//          lea RCX, msvc_trampo_1
//          call find_symbol()
//      ??func2@@YAXXZ ENDP
//      msvc_trampo_long_function_1 PROC
//          ...
//      msvc_trampo_long_function_1 ENDP
//  end
//
// === DEF output ===
//  EXPORTS
//  msvc_trampo_1
//  ?the_long_function_1234567890 = msvc_trampo_long_function_2
//
void MSVCTrampo::make_asmfile(const std::string &asm_path, std::string &def_path)
{
    for (auto iter = undef_syms.begin(); iter != undef_syms.end();)
        if (msvcrt_symbols.count(*iter))
            iter = undef_syms.erase(iter);
        else
            iter++;
    
    std::string const_section, code_section;
    std::string def_shortname, def_longname;
    
    int varcount = 0;
    for (auto &sym : undef_syms) {
        std::string asm_proc_label = sym;
        std::string asm_arg_str = "msvc_trampo_" + std::to_string(varcount++);
        { // .const section DB "..."
            std::string tmp;
            for (std::size_t i=0, j=200; i<sym.size(); i=j, j=j+200) {
                if (j > sym.size())
                    j = sym.size();
                tmp += (tmp.size()?("\n  " + std::string(asm_arg_str.size(), ' ')):"") 
                    +  " DB \"" + sym.substr(i, j-i) + "\"";
            }
            const_section += "  " + asm_arg_str + tmp + ",0\n";
        }

        // check symbol need replace or not.
        if (sym.size() < 246)
            def_shortname += "  " + asm_proc_label + "\n";
        else {
            asm_proc_label = "msvc_trampo_" + std::to_string(varcount++);
            def_longname  += "  " + sym + " = " + asm_proc_label + "\n";
        }

        code_section += 
            "  " + asm_proc_label + " PROC\n"
            "    sub rsp, 30h\n"
            "    mov [rsp + 10h], rcx\n"
            "    mov [rsp + 18h], rdx\n"
            "    mov [rsp + 20h], r8\n"
            "    mov [rsp + 28h], r9\n"
            "    lea rcx, " + asm_arg_str + "\n"
            "    call ?find@GlobalSymbol@cgnv1@@SAPEAXPEBD@Z\n"
            "    mov rcx, [rsp + 10h]\n"
            "    mov rdx, [rsp + 18h]\n"
            "    mov r8, [rsp + 20h]\n"
            "    mov r9, [rsp + 28h]\n"
            "    add rsp, 30h\n"
            "    jmp rax\n"
            "  " + asm_proc_label + " ENDP\n\n";
    }

    if (def_longname.size()) {
        std::ofstream fdef(def_path);
        fdef<<"EXPORTS\n" + def_shortname + def_longname;
        code_section += 
        "  DllMain proc hinst:QWORD, reason:DWORD, reserved:QWORD\n"
        "    mov rax, 1    ; Return TRUE (non-zero) for successful initialization\n"
        "    ret\n"
        "  DllMain endp\n";
    }
    else
        def_path.clear();

    std::ofstream fasm(asm_path);
    fasm << "; This file is auto generated by cgn::MSVCTrampo\n"
         << "; If a function name is excessively long (247), a .def file will be created.\n\n"
         << "EXTERN ?find@GlobalSymbol@cgnv1@@SAPEAXPEBD@Z :PROC\n"
         << ".const\n" << const_section << "\n\n"
         << ".code\n" << code_section << "end\n";
    fasm.close();
}



} //namespace