#include <fstream>
#include <iostream>
#include "pe_file.h"

int debug_read_lib()
{
    std::string libname = "msvcprt";
    std::string path2 = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.37.32822\\lib\\x64\\" + libname + ".lib";
    std::ifstream fin(path2, std::ios::binary);

    // std::ifstream fin("x.lib", std::ios::binary);
    auto [rv, errorlog] = cgn::LibraryFile::extract_exported_symbols(fin);
    if (errorlog.size()) {
        std::cout<<"\n=== ERROR ===\n"<<errorlog<<"\n";
        return 1;
    }

    std::cout<<"\n======\n";
    for (auto sym : rv)
        std::cout<<" "<<sym<<"\n";

    std::cout<<"-- "<<rv.size()<<" symbols providered"<<std::endl;
    return 0;
}

int debug_obj()
{
    std::ifstream fin("func1.obj", std::ios::binary);
    auto [rv, elog] = cgn::COFFFile::extract_somedata(fin);

    if (elog.size()) {
        std::cout<<"\n=== ERROR ===\n"<<elog<<std::endl;
        return 1;
    }
    std::cout<<"\n======\n";

    for (auto it : rv.defaultlib)
        std::cout<<" | DEFLIB: "<<it<<"\n";
    
    for (auto it : rv.undef_symbols)
        std::cout<<" | UNDEF: "<<it<<"\n";

    std::cout<<"sizeof_undef:"<<std::dec<<rv.undef_symbols.size()<<std::endl;
    return 0;
}

int debug_diff()
{
    std::ifstream fin("func1.obj", std::ios::binary);
    auto [rv1, elog1] = cgn::COFFFile::extract_somedata(fin);
    if (elog1.size()) {
        std::cout<<"\n=== ERROR ===\n"<<elog1<<std::endl;
        return 1;
    }

    std::unordered_set<std::string> def_syms;
    for (auto libname : rv1.defaultlib) {
        std::string path2 = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.37.32822\\lib\\x64\\" + libname + ".lib";
        std::ifstream fin2(path2, std::ios::binary);
        if (!fin2) {
            std::cout<<"=> "<<path2<<" NOT FOUND!\n";
            continue;
        }
        std::cout<<"=> "<<path2<<"\n";
        
        auto [rv2, elog2] = cgn::LibraryFile::extract_exported_symbols(fin2);

        if (elog2.size()) {
            std::cout<<"\n=== elog2 ERROR ===\n"<<elog2<<std::endl;
            return 1;
        }

        def_syms.insert(rv2.begin(), rv2.end());
    }
    std::cout<<" * DEFAULT SYMBOL LOADED: "<<std::dec<<def_syms.size()<<std::endl;

    for (auto iter = rv1.undef_symbols.begin(); iter != rv1.undef_symbols.end();)
        if (def_syms.count(*iter))
            iter = rv1.undef_symbols.erase(iter);
        else
            iter++;
    
    std::cout<<"=== REMAIN UNDEFINED ===\n";
    for (auto it : rv1.undef_symbols)
        std::cout<<it<<"\n";

    std::cout<<std::endl;
    return 0;
}

int main(int argc, char **argv)
{
    // return debug_read_lib();
    // return debug_obj();
    return debug_diff();
}