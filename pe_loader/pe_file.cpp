
#include <iostream>
#include <vector>
#include "pe_file.h"
#include "../v1/cgn_api.h"

// #define devout std::cout

namespace cgnv1 {

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
    
    // devout<<"LibraryFile\n";

    // FirstLinkerMember
    MemberHeader hdr;
    in.read((char*)&hdr, sizeof(hdr));
    if (in.eof() || hdr.name() != IMAGE_ARCHIVE_LINKER_MEMBER_)
        return {{}, "Header of FirstLinkerMember"};
    
    // devout<<" 0x"<<std::hex<<in.tellg()<<std::dec
    //     <<" | FirstLinkerMember Body: body_size="<<hdr.body_size();
    
    uint32_t sym_count;
    in.read((char*)&sym_count, sizeof(sym_count));
    sym_count = Tools::u32be_to_host(sym_count);

    // devout<<" sym_count="<<sym_count<<std::endl;

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


// The undefined variable won't expose here.
std::pair<COFFFile::SomeData, std::string>
COFFFile::extract_somedata(std::istream &in)
{
    if (!in)
        return {{}, "ISTREAM ERROR"};

    // COFF File Header (Object and Image)
    // https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-file-header-object-and-image
    COFFFile::SomeData rv;
    in.read((char *)&rv.coff_hdr, sizeof(rv.coff_hdr));
    if (in.eof() || rv.coff_hdr.symtable_offset() == 0)
        return {{}, "COFFFileHeader"};
    
    // devout<<" CoffHeader: symtbl_off=0x"<<std::hex<<rv.coff_hdr.symtable_offset()<<"\n";

    // extract '.drectve' section
    // https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-table-section-headers
    std::string drectve_body;
    for (std::size_t i=0; i<rv.coff_hdr.section_nums(); i++) {
        SectionHeader hdr;
        in.read((char*)&hdr, sizeof(hdr));
        if (in.eof())
            return {{}, "need more SectionHeader"};
        if (hdr.name() == ".drectve") {
            // devout<<"  .drectve off=0x"<<std::hex<<hdr.body_offset()
            //     <<" size="<<std::dec<<hdr.body_size()<<std::endl;
            drectve_body.resize(hdr.body_size());
            in.seekg(hdr.body_offset(), in.beg);
            in.read(drectve_body.data(), drectve_body.size());
            if (in.eof())
                return {{}, "'.drectve' body read failure"};
            // devout<<"  .drectve "<<drectve_body<<std::endl;
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

    // COFF Symbol Table
    // https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-symbol-table
    // devout<<" COFF Symbol Table (off 0x"
    //     <<std::hex<<rv.coff_hdr.symtable_offset()
    //     <<" count="<<std::dec<<rv.coff_hdr.symtable_entry_count()<<") UNDEF\n";
    in.seekg(rv.coff_hdr.symtable_offset(), in.beg);
    if (in.eof())
        return {{}, "COFF SYMTABLE (EOF)"};

    for (std::size_t i=0; i<rv.coff_hdr.symtable_entry_count(); i++) {
        auto hdr_off = in.tellg();

        COFFSymbolTable symrow;
        in.read((char*)&symrow, sizeof(symrow));
        if (in.eof())
            return {{}, "COFF SYMTABLE [" + std::to_string(i) 
                        + "-th] off=" + std::to_string(hdr_off)};
        auto selname = symrow.name();

        std::size_t aux_count = (std::size_t)symrow._num_of_auxsym[0];
        if (symrow.section_no() == COFFSymbolTable::IMAGE_SYM_UNDEFINED_ 
            && symrow._type[0] == 0x20  // notype ()
        ) {
            // devout<<" 0x"<<std::hex<<hdr_off<<" "<<i<<"-th | ";
            if (symrow.storage_class() == 105 && aux_count == 1) {
                // devout<<"HAVE_WEAK_EXTERNAL "; // IMAGE_SYM_CLASS_WEAK_EXTERNAL
            }else if (selname.first.size()) {
                // devout<<selname.first<<" ";
                rv.undef_symbols.insert(selname.first);
            }else{
                // devout<<"STR'0x"<<std::hex<<selname.second<<" ";
                pending_symbol_offset.insert(selname.second);
            }
            // devout<<std::hex<<" SECT=0x"<<symrow.section_no()
            //     <<" TYPE="<<strhex(symrow._type[0])
            //     <<","<<strhex(symrow._type[1])
            //     <<" StorClass="<<strhex(symrow._storage_class[0])<<"\n";
            // if (aux_count)
            //     devout<<std::hex<<" 0x"<<in.tellg()<<"          - skip "
            //         <<std::dec<<aux_count<<" AUX fields"<<"\n";
        }
        
        if (aux_count) {
            in.seekg(18 * aux_count, in.cur);
            i += aux_count;
        }
    } //end for each entry in symbol table

    // COFF String Table
    // https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-string-table
    // devout<<" COFF String Table (off=0x"<<std::hex<<in.tellg();
    uint32_t strtbl_size;
    in.read((char*)&strtbl_size, 4);
    if (in.eof() || strtbl_size < 4)
        return {{}, "String Table size field: " + std::to_string(strtbl_size)};
    // devout<<std::hex<<" size=0x"<<strtbl_size<<") UNDEF\n";

    std::vector<char> strtable(strtbl_size); // the whole string table
    in.read(strtable.data() + 4, strtable.size() - 4);
    if (in.eof())
        return {{}, "String Table TOO LARGE: " + std::to_string(strtbl_size)};

    for (auto it : pending_symbol_offset) {
        // devout<<std::hex<<"  +0x"<<it<<" : "<<std::string{strtable.data() + it}<<"\n";
        rv.undef_symbols.insert(std::string{strtable.data() + it});
    }

    return {rv, ""};
}

} //namespace