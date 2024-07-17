
#include <vector>
#include "pe_file.h"

#ifdef DEV_DEBUG
    #include <iostream>
    #define devout std::cout
#else
    struct DEVOUT : std::ostream{
        template<typename T> DEVOUT& operator<<(T&&) { return *this; }
    }devout;
#endif

namespace cgn {

static std::string strhex(char in) {
    constexpr char tbl[16] = {'0', '1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    uint8_t* ptr = (uint8_t*)&in;
    std::string rv = {"0x.."};
    rv[2] = tbl[(*ptr) >> 4];
    rv[3] = tbl[(*ptr) & 0x0f];
    return rv;
}

std::pair<std::unordered_set<std::string>, std::string>
LibraryFile::extract_exported_symbols(std::istream &in)
{
    std::string file_signature{"        "};
    in.read(file_signature.data(), 8);
    if (in.eof() || file_signature != "!<arch>\n")
        return {{}, "Invalid signature: "+ file_signature};
    
    // FirstLinkerMember
    MemberHeader hdr;
    in.read((char*)&hdr, sizeof(hdr));
    if (in.eof() || hdr.name() != IMAGE_ARCHIVE_LINKER_MEMBER)
        return {{}, "Header of FirstLinkerMember"};
    
    devout<<std::hex<<"0x"<<in.tellg()<<std::dec
        <<" | 1st Body: body_size="<<hdr.body_size();
    
    uint32_t sym_count;
    in.read((char*)&sym_count, sizeof(sym_count));
    sym_count = Tools::u32be_to_host(sym_count);

    devout<<" sym_count="<<sym_count<<std::endl;

    in.seekg(sym_count*4, in.cur);

    std::vector<char> body(hdr.body_size());
    in.read(body.data(), body.size());
    if (in.eof())
        return {{}, "FirstLinkerMember Stringtable"};

    std::unordered_set<std::string> rv;
    for (std::size_t b=0, i=0; i<sym_count && b<body.size(); i++) {
        std::string sym{body.data()+b};
        b += sym.size()+1;
        rv.insert(sym);
    }
    return {rv, ""};
} //LibraryFile::extract_exported_symbols()

std::pair<COFFFile::SomeData, std::string>
COFFFile::extract_somedata(std::istream &in)
{
    COFFFile::SomeData rv;
    in.read((char *)&rv.coff_hdr, sizeof(rv.coff_hdr));
    if (in.eof() || rv.coff_hdr.symtable_offset() == 0)
        return {{}, "COFFFileHeader"};
    
    devout<<" CoffHeader: symtbl_off=0x"<<std::hex<<rv.coff_hdr.symtable_offset()
          <<" symtbl_ncount="<<std::dec<<rv.coff_hdr.symtable_entry_count()<<std::endl;

    // extract '.drectve' section
    std::string drectve_body;
    for (std::size_t i=0; i<rv.coff_hdr.section_nums(); i++) {
        SectionHeader hdr;
        in.read((char*)&hdr, sizeof(hdr));
        if (in.eof())
            return {{}, "need more SectionHeader"};
        if (hdr.name() == ".drectve") {
            devout<<"  .drectve off=0x"<<std::hex<<hdr.body_offset()
                <<" size="<<std::dec<<hdr.body_size()<<std::endl;
            drectve_body.resize(hdr.body_size());
            in.seekg(hdr.body_offset(), in.beg);
            in.read(drectve_body.data(), drectve_body.size());
            if (in.eof())
                return {{}, "'.drectve' body read failure"};
            devout<<"  .drectve "<<drectve_body<<std::endl;
            break;
        }
    }
    for (std::size_t i=0; i<drectve_body.size();) {
        auto fd = drectve_body.find("/DEFAULTLIB:", i);
        if (fd == drectve_body.npos)
            break;
        std::size_t strbegin = fd + 13; // sizeof(/DEFAULTLIB:\") == 13
        auto fd2 = drectve_body.find('\"', strbegin);
        if (fd2 == drectve_body.npos)
            return {{}, "Unsupported '/DEFAULTLIB:\"...\"' argument"};
        rv.defaultlib.insert(drectve_body.substr(strbegin, fd2 - strbegin));
        i = fd2+1;
    }

    // After iterator COFF Symbol Table
    //  if symbol-name can be extract directly ( <= 8 bytes)
    //      save it into rv.undef_symbols directly
    //  else
    //      save the str-offset into pending_symbol_offset
    //      and then read from 'COFF String Table' then save it.
    std::unordered_set<std::size_t> pending_symbol_offset;

    // extract 'COFF SYMTABLE'
    in.seekg(rv.coff_hdr.symtable_offset(), in.beg);
    if (in.eof())
        return {{}, "COFF SYMTABLE (EOF)"};
    for (std::size_t i=0; i<rv.coff_hdr.symtable_entry_count(); i++) {
        devout<<"0x"<<std::hex<<in.tellg()<<" "<<i<<"-th";

        COFFSymbolTable symrow;
        in.read((char*)&symrow, sizeof(symrow));
        if (in.eof())
            return {{}, "COFF SYMTABLE [" + std::to_string(i) + "]"};

        std::size_t aux_count = (std::size_t)symrow._num_of_auxsym[0];    
            
        devout<<" | ";
        auto selname = symrow.name();
        if (selname.first.size()) {
            if (symrow.section_no() == COFFSymbolTable::IMAGE_SYM_UNDEFINED)
                rv.undef_symbols.insert(selname.first);
            devout<<selname.first<<" ";
        }
        else {
            if (symrow.section_no() == COFFSymbolTable::IMAGE_SYM_UNDEFINED)
                pending_symbol_offset.insert(selname.second);
            devout<<"STR[0x"<<std::hex<<selname.second<<"] ";
        }
        
        devout<<" SECT=0x"<<std::hex<<symrow.section_no();

        devout<<std::hex<<" TYPE="<<strhex(symrow._type[0])
                <<","<<strhex(symrow._type[1]);
        
        devout<<std::hex<<" StorageClass="<<strhex(symrow._storage_class[0])<<"\n";
        
        if (aux_count) {
            devout<<"0x"<<std::hex<<in.tellg()<<"          - skip "
                  <<std::dec<<aux_count<<" AUX fields"<<"\n";
            in.seekg(18 * aux_count, in.cur);
            i += aux_count;
        }
    }

    // COFF String Table
    // https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-string-table
    uint32_t strtbl_size;
    in.read((char*)&strtbl_size, 4);
    devout<<std::hex<<"0x"<<in.tellg();
    if (in.eof() || strtbl_size < 4)
        return {{}, "String Table size field: " + std::to_string(strtbl_size)};
    devout<<std::hex<<"  COFF String Table (size=0x"<<strtbl_size<<")";

    std::vector<char> strtable(strtbl_size);
    in.read(strtable.data() + 4, strtable.size() - 4);
    if (in.eof())
        return {{}, "String Table TOO LARGE: " + std::to_string(strtbl_size)};

    devout<<"--- COFF String Table---\n";
    for (auto it : pending_symbol_offset) {
        devout<<std::hex<<"  +0x"<<it<<" : "<<std::string{strtable.data() + it}<<"\n";
        rv.undef_symbols.insert(std::string{strtable.data() + it});
    }

    return {rv, ""};
}

} //namespace