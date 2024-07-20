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

void* __cdecl GlobalSymbol::find(const char *sym)
{
    if (auto fd = symbol_table.find(sym); fd != symbol_table.end())
        return fd->second;
    return nullptr;
}

GlobalSymbol::DllHandle GlobalSymbol::WinLoadLibrary(const std::string& dllpath)
{
    // GetProcAddress cannot iterator the exported symbol
    // So we load .lib here
    if (dllpath.size() < 4)
        throw std::runtime_error{ "Dllpath must end with .dll :" + dllpath };
    auto libpath = dllpath.substr(0, dllpath.size() - 4) + ".lib";
    std::ifstream fin(libpath, std::ios::binary);
    if (!fin)
        throw std::runtime_error{ "Cannot open " + libpath };
    auto [exp, elog] = LibraryFile::extract_exported_symbols(fin);
    if (elog.size())
        throw std::runtime_error{ "Parse filure " + libpath };

    // Load .dll and GetProcAddress() for all exported symbols
    DllHandle rv;
    rv.m_ptr = ::LoadLibrary(dllpath.c_str());
    if (rv.m_ptr) {
        for (auto& sym : exp)
            if (auto ptr = ::GetProcAddress(rv.m_ptr, sym.c_str()); ptr) {
                symbol_table[sym] = ptr;
                rv.sym_exports.push_back(sym);
            }
    }
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