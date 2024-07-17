// https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#import-library-format
//  Archive (Library) File Format

#include <iostream>
#include <fstream>
#include <sstream>
#include <array>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cassert>

// Open the COFF archive format (.lib)
//  https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#import-header
//  Format in .lib
//  <Magic: 8 bytes>
//  <Header: 60 bytes>
//  <1st Linker Member>
//  <Header: 60 bytes>
//  <2nd Linker Member>
//  
class ArchiveFile
{
    constexpr static std::string_view file_signature = {"!<arch>\n"};

    static std::size_t ascii_dec_to_ul(const char *in, std::size_t len) {
        std::size_t rv = 0;
        for (std::size_t i=0; i<len; i++)
            if ('0' <= in[i] && in[i] <= '9')
                rv = rv * 10 + (in[i] -'0');
            else
                break;
        return rv;
    }

    static bool is_cpu_big_endian() {
        constexpr static uint16_t a = 0x0001;
        return reinterpret_cast<const char*>(&a)[0] != (char)0x01;
    }
    static uint32_t u32be_to_host(const std::byte *src)
    {
        if (is_cpu_big_endian())
            return *(uint32_t*)src;
        return ((uint32_t)src[3])     + ((uint32_t)src[2]<<8)
            + ((uint32_t)src[1]<<16) + ((uint32_t)src[0]<<24);
    } 

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
            if (rv == "/               ")
                return "IMAGE_ARCHIVE_LINKER_MEMBER";
            if (rv == "//              ")
                return "IMAGE_ARCHIVE_LONGNAMES_MEMBER";
            if (rv == "/<HYBRIDMAP>/   ")
                return "IMAGE_ARCHIVE_HYBRIDMAP_MEMBER";
            for (size_t i=0; i<sizeof(_name); i++)
                if (_name[i] == '/') { rv = rv.substr(0, i); break; }
            return rv;
        }
        std::size_t size() const {
            return ascii_dec_to_ul(_body_size, sizeof(_body_size));
        }
        bool valid() const {
            return _end_of_header[0] == 0x60 && _end_of_header[1] == 0x0A;
        }
    };
    static_assert(sizeof(MemberHeader) == 60);

public:
    // FirstLinkerMember[symbol string] = file offsets to archive member headers
    using FirstLinkerMember = std::unordered_map<std::string, uint32_t>;

    // 'F'irst 
    char load_stat = 'F';

    bool load(const std::string_view &content)
    {
        if (content.substr(0, 8) != file_signature)
            return false;

        std::cout<<"Content size "<< content.size()<<std::endl;

        std::size_t off=8;
        while (off < content.size()) {
            //read title
            if (off + sizeof(MemberHeader) > content.size())
                return false;
            MemberHeader *hdr = (MemberHeader *)(content.data() + off);
            std::cout<<std::hex<<"0x"<<off;
            if (!hdr->valid()) {
                std::cout<<" Header Invalid"<<std::endl;
                return false;
            }
            std::cout<<std::dec<<" == "<<hdr->name()<<" body_size:"<<hdr->size()<<std::endl;
            off += sizeof(MemberHeader);

            //read body
            if (load_stat == 'F') {
                assert(hdr->name() == "IMAGE_ARCHIVE_LINKER_MEMBER");
                uint32_t body_n = u32be_to_host((const std::byte *)content.data() + off);
                off += 4;

                // Offsets[]
                off += body_n * 4;
                
                std::cout<<"\t FIRST entry_count = "<<body_n<<" strs_off="<<off<<std::endl;

                for (std::size_t i=0, b=off; i<body_n; i++) {
                    while(content[off] != 0)
                        off++;
                    std::string_view sym{content.data() + b, off-b};
                    std::cout<<"\t0x"<<std::hex<<b
                             <<" | "<< sym <<std::endl;
                    b = ++off;
                }

                load_stat = 'S';
            }else
                off += hdr->size();

            // Each member header starts on the first even address after the end of the previous archive member, one byte '\n' (IMAGE_ARCHIVE_PAD) may be inserted after an archive member to make the following member start on an even address.
            off += off%2;
        }

        return true;
    }
};

class ObjFile {
    struct COFFFileHeader {
        char _machine[2];
        char _n_sections[2];
        char _timestamp[4];
        char _off_symtable[4];
        char _nums_in_symtable[2];
        char _size_opthdr[2];
        char _attr[2];

        std::size_t section_nums() const {
            return ((uint16_t)_n_sections[1]<<8) + (uint16_t)_n_sections[0];
        }
    };

    struct SectionHeader {
        char _name[8];
    };
public:
    bool load() {}
};

// #include "windows.h"
// #include "winnt.h"
// #include "imagehlp.h"
// #pragma comment(lib, "Imagehlp")
// void use_imageload() {
//     // Errorcode 11 : ERROR_BAD_FORMAT
//     auto ptr = ImageLoad("MSVCRT.lib", "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.37.32822\\lib\\x64");
//     if (ptr)
//         std::cout<<"Image loaded."<<std::endl;
//     else
//         std::cout<<"Imageload errorcode: "<<GetLastError()<<std::endl;
// }

int main(int argc, char **argv)
{
    // std::ifstream fin("C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.37.32822\\lib\\x64\\MSVCRT.lib", std::ios::binary);
    std::ifstream fin("../lazyload/func2/fake2.lib", std::ios::binary);
    std::stringstream ss; ss<<fin.rdbuf();
    std::string content = ss.str();
    ArchiveFile file;
    file.load(content);

    return 0;
}
