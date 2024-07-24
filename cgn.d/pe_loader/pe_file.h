#pragma once

#include <istream>
#include <string>
#include <cstring>
#include "../cgn.h"

#ifdef _WIN32
#include <WinDef.h>
#endif

namespace cgn {

//
struct LibraryFile
{
    static std::size_t ascii_dec_to_ul(const char *in, std::size_t len) {
        std::size_t rv = 0;
        for (std::size_t i=0; i<len; i++)
            if ('0' <= in[i] && in[i] <= '9')
                rv = rv * 10 + (in[i] -'0');
            else
                break;
        return rv;
    }

    constexpr static std::string_view 
        IMAGE_ARCHIVE_LINKER_MEMBER_{"/               "},
        IMAGE_ARCHIVE_LONGNAMES_MEMBER_{"//              "},
        IMAGE_ARCHIVE_HYBRIDMAP_MEMBER_{"/<HYBRIDMAP>/   "};

    struct MemberHeader {
        char _name[16];
        char _date_time[12];
        char _user_id[6];
        char _group_id[6];
        char _mode[8];
        char _body_size[10];
        char _end_of_header[2];

        std::string_view name() const {
            std::string_view rv{_name, sizeof(_name)};
            if (rv == IMAGE_ARCHIVE_LINKER_MEMBER_ 
                || rv == IMAGE_ARCHIVE_LINKER_MEMBER_
                || rv == IMAGE_ARCHIVE_HYBRIDMAP_MEMBER_
            ) return rv;
            for (std::size_t i=0; i<sizeof(_name); i++)
                if (_name[i] == '/') { rv = rv.substr(0, i); break; }
            return rv;
        }
        std::size_t body_size() const {
            return ascii_dec_to_ul(_body_size, sizeof(_body_size));
        }
        bool valid() const {
            return _end_of_header[0] == 0x60 && _end_of_header[1] == 0x0A;
        }
    };
    static_assert(sizeof(MemberHeader) == 60);

    // symbols from FirstLinkerMember
    static std::pair<std::unordered_set<std::string>, std::string>
    extract_exported_symbols(std::istream &in);
}; //LibraryFile


// 
struct COFFFile
{
    struct COFFFileHeader {
        char _machine[2];
        char _n_sections[2];
        char _timestamp[4];
        char _pointer_symtable[4];
        char _n_in_symtable[4];
        char _size_opthdr[2];
        char _attr[2];

        std::size_t section_nums() const {
            return ((uint16_t)_n_sections[1]<<8) + (uint16_t)_n_sections[0];
        }

        std::size_t symtable_offset() const {
            return *(uint32_t*)&_pointer_symtable;
            // return Tools::u32be_to_host(*(uint32_t*)&_pointer_symtable);
        }

        std::size_t symtable_entry_count() const {
            return *(uint32_t*)&_n_in_symtable;
            // return Tools::u32be_to_host(*(uint32_t*)&_n_in_symtable);
        }
    };
    static_assert(sizeof(COFFFileHeader) == 20);

    struct SectionHeader {
        char _name[8];
        char _virtual_size[4];
        char _virtual_address[4];
        char _data_size[4];
        char _pointer_data[4];
        char _pointer_reloc[4];
        char _pointer_linenum[4];
        char _n_relocs[2];
        char _n_linenums[2];
        char _attr[4];

        std::string_view name() const {
            return std::string_view{_name, strnlen(_name, 8)};
        }
        std::size_t body_offset() const {
            return *(uint32_t*)_pointer_data;
            // return Tools::u32be_to_host(*(uint32_t*)_pointer_data);
        };
        std::size_t body_size() const {
            return *(uint32_t*)_data_size;
            // return Tools::u32be_to_host(*(uint32_t*)_data_size);
        };
    };
    static_assert(sizeof(SectionHeader) == 40);

    // struct AuxiliaryWeakExternal
    // {
    //     char _tag_index[4];
    //     char _attrs[4];
    //     char _padding[10];
    //     std::size_t tag_index() const {
    //         return *(uint32_t*)_tag_index;
    //     };
    // };
    // static_assert(sizeof(AuxiliaryWeakExternal) == 18);


    struct COFFSymbolTable {
        char _name[8];
        char _value[4];
        char _sect_num[2];
        char _type[2];
        char _storage_class[1];
        char _num_of_auxsym[1];

        std::pair<std::string, std::size_t> name() const {
            if (_name[0]==0 && _name[1]==0 && _name[2]==0 && _name[3]==0)
                return {"", *(uint32_t*)(_name + 4)};
            return {std::string{_name, strnlen(_name, 8)}, 0};
        }

        constexpr static int16_t IMAGE_SYM_UNDEFINED_ = 0;
        constexpr static int16_t IMAGE_SYM_ABSOLUTE_ = -1;
        constexpr static int16_t IMAGE_SYM_DEBUG_ = -2;
        int16_t section_no() const {
            return *(int16_t*)_sect_num;
        }
        int8_t storage_class() const {
            return (int8_t)_storage_class[0];
        }
    };
    static_assert(sizeof(COFFSymbolTable) == 18);

    // The data required for CGN
    struct SomeData {
        COFFFileHeader coff_hdr;

        // .rective linker_directives
        std::unordered_set<std::string> defaultlib;

        // 'UNDEF' in 'COFF SYMBOL TABLE'
        std::unordered_set<std::string> undef_symbols;

        // WeakExternal replacement
        // alt_symbols[want-symbol] = alternative-symbol
        // std::unordered_map<std::string, std::string> alt_symbols;
    };

    static std::pair<SomeData, std::string> 
    extract_somedata(std::istream &in);
}; //COFFFile

// Class for create .asm file to .cgn.cc script in cgn executable
struct MSVCTrampo
{
    static std::unordered_set<std::string> sys_libs, msvcrt_symbols;

    static void add_msvcrt_lib(std::string libname);
    static void add_lib(std::string libname);

    std::unordered_set<std::string> undef_syms;

public:
    MSVCTrampo();

    void add_objfile(const std::string &filename);
    // void add_jmpfunc(const std::string &func_name);

    void make_asmfile(const std::string &file_out);
};

// Class for create cgn API for calling from .asm file
#ifdef _WIN32
struct GlobalSymbol
{
    struct DllHandle{
        std::vector<std::string> sym_exports;
        HMODULE m_ptr;
        operator bool() const { return m_ptr; }
    };
    static std::unordered_map<std::string, void*> symbol_table;

    // get the address for 'jmp xxx' instruction in .asm file
    __declspec(dllexport) static void* find(const char *sym);

    // load .cgn.dll file in cgn-impl to replace WINAPI LoadLibrary()
    static DllHandle WinLoadLibrary(const std::string &dllpath);

    // replace for WINAPI UnloadLibrary()
    static void WinUnloadLibrary(DllHandle &handle);
};

#endif

} //namespace
