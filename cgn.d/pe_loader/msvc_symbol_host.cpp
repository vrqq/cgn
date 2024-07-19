// GlobalSymbol register table
//  only loaded in cgn.exe host process

#include "pe_file.h"

#ifdef _WIN32
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <Libloaderapi.h>
#include <errhandlingapi.h>

namespace cgn {

std::unordered_map<std::string, void*> GlobalSymbol::symbol_table;

void *GlobalSymbol::find(const std::string &sym)
{
    if (auto fd = symbol_table.find(sym); fd != symbol_table.end())
        return fd->second;
    return nullptr;
}

GlobalSymbol::DllHandle GlobalSymbol::WinLoadLibrary(const std::string &dllpath)
{
    // GetProcAddress cannot iterator the exported symbol
    auto libpath = dllpath.substr(dllpath.size()-4) + ".lib";
    std::ifstream fin(libpath, std::ios::binary);
    if (!fin)
        throw std::runtime_error{"Cannot open " + libpath};
    auto [exp, elog] = LibraryFile::extract_exported_symbols(fin);
    if (elog.size())
        throw std::runtime_error{"Parse filure " + libpath};

    DllHandle rv;
    rv.m_ptr = ::LoadLibrary(dllpath.c_str());
    if (rv.m_ptr)
        rv.sym_exports.insert(rv.sym_exports.end(), exp.begin(), exp.end());
    return rv;
}

void GlobalSymbol::WinUnloadLibrary(DllHandle &handle)
{
    if (handle.m_ptr) {
        for (auto &it : handle.sym_exports)
            symbol_table.erase(it);
        ::FreeLibrary(handle.m_ptr);
    }
}

} //namespace

#endif