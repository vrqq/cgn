#include <fstream>
#include <iostream>
#include <filesystem>
#include "../pe_file.h"

int debug_read_lib(std::string libname)
{
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

int debug_obj(std::string filename)
{
    std::ifstream fin(filename, std::ios::binary);
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

int debug_diff(std::string filename)
{
    std::cout<<"==> .obj"<<std::endl;
    std::ifstream fin(filename, std::ios::binary);
    auto [rv1, elog1] = cgn::COFFFile::extract_somedata(fin);
    if (elog1.size()) {
        std::cout<<"\n=== ERROR ===\n"<<elog1<<std::endl;
        return 1;
    }

    // Previouse returned value only contain /DEFAULTLIB from cl.exe
    //      for example "MSVCRT" and "OLDNAMES"
    // Here, the lib added and removed by link.exe
    std::cout<<"==> Add some default libs\n";
    rv1.defaultlib.insert({"kernel32", "vcruntime", "ucrt"});
    // rv1.defaultlib.erase({""}); //TODO

    std::vector<std::string> dll_search_paths = {
        R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.37.32822\ATLMFC\lib\x64)",
        R"(C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.37.32822\lib\x64)",
        R"(C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8\lib\um\x64)",
        R"(C:\Program Files (x86)\Windows Kits\10\lib\10.0.22621.0\ucrt\x64)",
        R"(C:\Program Files (x86)\Windows Kits\10\lib\10.0.22621.0\um\x64)",
    };

    std::unordered_set<std::string> def_syms;
    for (auto libname : rv1.defaultlib) {
        std::string path_found;
        for (auto it : dll_search_paths) {
            std::string tmp = it + "\\" + libname;
            if (libname.size() < 4 || libname.substr(libname.size()-4) != ".lib")
                tmp += + ".lib";
            if (std::filesystem::is_regular_file(tmp)) {
                path_found = tmp;
                break;
            }
            // std::cout<<"  -- "<<tmp<<" not exist.\n";
        }
        if (path_found.empty()) {
            std::cout<<"=> "<<libname<<" NOT FOUND!\n";
            continue;
        }
        std::cout<<"=> "<<path_found<<"\n";
        std::ifstream fin2(path_found, std::ios::binary);
        
        auto [rv2, elog2] = cgn::LibraryFile::extract_exported_symbols(fin2);

        if (elog2.size()) {
            std::cout<<"\n=== elog2 ERROR ===\n"<<elog2<<std::endl;
            return 1;
        }

        def_syms.insert(rv2.begin(), rv2.end());
    }
    std::cout<<"=== DEFAULT SYMBOL LOADED: "<<std::dec<<def_syms.size()<<std::endl;

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
    if (argc == 3) {
        if (argv[1][0] == 'L')
            return debug_read_lib(argv[2]);
        if (argv[1][0] == 'O')
            return debug_obj(argv[2]);
        if (argv[1][0] == 'D')
            return debug_diff(argv[2]);
        std::cerr<<argv[0]<<"+ L/O/D + file\n";
        return 1;
    }
    return debug_diff("objmaker.obj");
}