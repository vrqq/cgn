// GlobalSymbol register table
//  only loaded in cgn.exe host process

#include "pe_file.h"

#ifdef _WIN32
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <iostream>
#include <Windows.h>
#include <imagehlp.h>
#include <Libloaderapi.h>
#include <errhandlingapi.h>
#pragma comment(lib,"ImageHlp")

namespace cgnv1 {

// https://stackoverflow.com/a/4354755/12529885
static std::vector<std::string> ListDLLFunctions(std::string sADllName)
{
    std::vector<std::string> slListOfDllFunctions;
    DWORD* dNameRVAs(0);
    _IMAGE_EXPORT_DIRECTORY* ImageExportDirectory;
    unsigned long cDirSize;
    _LOADED_IMAGE LoadedImage;
    std::string sName;
    slListOfDllFunctions.clear();
    if (MapAndLoad(sADllName.c_str(), NULL, &LoadedImage, TRUE, TRUE))
    {
        ImageExportDirectory = (_IMAGE_EXPORT_DIRECTORY*)
            ImageDirectoryEntryToData(LoadedImage.MappedAddress,
                false, IMAGE_DIRECTORY_ENTRY_EXPORT, &cDirSize);
        if (ImageExportDirectory != NULL)
        {
            dNameRVAs = (DWORD*)ImageRvaToVa(LoadedImage.FileHeader,
                LoadedImage.MappedAddress,
                ImageExportDirectory->AddressOfNames, NULL);
            for (size_t i = 0; i < ImageExportDirectory->NumberOfNames; i++)
            {
                sName = (char*)ImageRvaToVa(LoadedImage.FileHeader,
                    LoadedImage.MappedAddress,
                    dNameRVAs[i], NULL);
                slListOfDllFunctions.push_back(sName);
            }
        }
        UnMapAndLoad(&LoadedImage);
    }

    return slListOfDllFunctions;
}

// https://stackoverflow.com/a/16016958/12529885
// wrong!
static std::vector<std::string> EnumerateExportedFunctions(HMODULE hModule)
{
    std::vector<std::string> slListOfDllFunctions;
    DWORD* dNameRVAs(0);
    _IMAGE_EXPORT_DIRECTORY* ImageExportDirectory;
    unsigned long cDirSize;
    _LOADED_IMAGE LoadedImage;
    ZeroMemory(&LoadedImage, sizeof(LoadedImage));
    std::string sName;
    slListOfDllFunctions.clear();

    ImageExportDirectory = (_IMAGE_EXPORT_DIRECTORY*)
        ImageDirectoryEntryToData(hModule,
            TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &cDirSize);
    if (ImageExportDirectory != NULL)
    {
        dNameRVAs = (DWORD*)ImageRvaToVa(LoadedImage.FileHeader,
            LoadedImage.MappedAddress,
            ImageExportDirectory->AddressOfNames, NULL);
        for (size_t i = 0; i < ImageExportDirectory->NumberOfNames; i++)
        {
            sName = (char*)ImageRvaToVa(LoadedImage.FileHeader,
                LoadedImage.MappedAddress,
                dNameRVAs[i], NULL);
            slListOfDllFunctions.push_back(sName);
        }
    }
    return slListOfDllFunctions;
}

// std::unordered_map<std::string, void*> GlobalSymbol::symbol_table;

std::unordered_map<std::string, void*> *GlobalSymbol::get_symbol_table()
{
    static std::unordered_map<std::string, void*> symbol_table;
    return &symbol_table;
    // using T = std::unordered_map<std::string, void*>;
    // static bool valid = true;
    // static std::shared_ptr<T>
    //     tbl = std::shared_ptr<T>(new T, [](T *ptr) {
    //         std::cout<<"Table delete !"<<std::endl;
    //         delete ptr;
    //         valid = false;
    //     });
    
    // if (valid && tbl)
    //     return tbl.get();
    // return nullptr;
}

void* GlobalSymbol::find(const char *sym)
{
    if (auto tbl = get_symbol_table(); tbl)
        if (auto fd = tbl->find(sym); fd != tbl->end())
            return fd->second;
    
    throw std::runtime_error{"GlobalSymbol::find(" + std::string{sym} +") not found."};
    return nullptr;
}

GlobalSymbol::DllHandle GlobalSymbol::WinLoadLibrary(const std::string& dllpath)
{
    // GetProcAddress cannot iterator the exported symbol
    // So we load .lib here
    //if (dllpath.size() < 4)
    //    throw std::runtime_error{ "Dllpath must end with .dll :" + dllpath };
    //auto libpath = dllpath.substr(0, dllpath.size() - 4) + ".lib";
    //std::ifstream fin(libpath, std::ios::binary);
    //if (!fin)
    //    throw std::runtime_error{ "Cannot open " + libpath };
    //auto [exp, elog] = LibraryFile::extract_exported_symbols(fin);
    //if (elog.size())
    //    throw std::runtime_error{ "Parse failure " + libpath };
    
    auto *tbl = get_symbol_table();
    if (tbl == nullptr)
        throw std::runtime_error{"GlobalSymbol::symbol_table destroyed."};

    std::vector<std::string> exp = ListDLLFunctions(dllpath.c_str());
    // 
    // Load .dll and GetProcAddress() for all exported symbols
    DllHandle rv;
    rv.m_ptr = ::LoadLibrary(dllpath.c_str());
    //std::vector<std::string> exp = EnumerateExportedFunctions(rv.m_ptr);
    if (rv.m_ptr) {
        for (auto& sym : exp)
            if (auto ptr = ::GetProcAddress(rv.m_ptr, sym.c_str()); ptr) {
                (*tbl)[sym] = ptr;
                rv.sym_exports.push_back(sym);
            }
            else
                throw std::runtime_error{
                    "GetProcAddress(" + dllpath + ", " + sym + "): " + std::to_string(GetLastError())};
    }else
        throw std::runtime_error{
            "Cannot load library " + dllpath + ": " + std::to_string(GetLastError())};
    return rv;
}

void GlobalSymbol::WinUnloadLibrary(DllHandle &handle)
{
    if (auto tbl = get_symbol_table(); tbl && handle.m_ptr) {
        for (auto &it : handle.sym_exports)
            tbl->erase(it);
        ::FreeLibrary(handle.m_ptr);
    }
}

} //namespace

#endif //_WIN32