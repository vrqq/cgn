// Load configuration in these order:
//  1. load from cgn-out/configurations/*.cfg
//  2. run function setup() in file "cgn" in root
//     rewrite .cfg inside cgn-out
//
// Note:
//  Using ninja.deps=gcc/msvc to compile BUILD.cgn.cc to keep update 
//  with root 'cgn' file.
//
#include <filesystem>
#include <fstream>
#include <cstdint>
#include "configuration.h"

namespace cgn {

ConfigurationManager::ConfigurationManager(const std::string &storage_dir)
: storage_dir(storage_dir)
{
    std::filesystem::path p{storage_dir};

    //create dir
    if (!std::filesystem::exists(p))
        std::filesystem::create_directory(p);
    else if (!std::filesystem::is_directory(p))
        throw std::runtime_error{"CfgMgr: dir required " + storage_dir};

    //string strip function
    auto strip = [](const std::string &ss) -> std::string {
        int i=0, j=ss.size()-1;
        while(ss[i] == ' ' && i<ss.size()) i++;
        while(ss[j] == ' ' && j>=i) j--;
        if (i <= j)
            return ss.substr(i, j-i+1);
        return "";
    };

    //iterate and read cfg files.
    for (auto &ff : std::filesystem::directory_iterator{p})
        if (ff.is_regular_file() && ff.path().extension() == ".cfg") {
            std::string name = ff.path().filename().stem();
            Configuration cfg;

            std::ifstream fin(ff.path().string());
            for (std::string ss; std::getline(fin, ss);) {
                if (fin.eof())
                    break;
                if (ss.empty())
                    continue;
                if (auto fd = ss.find('='); fd != ss.npos) {
                    std::string key = strip(ss.substr(0, fd-1));
                    std::string val = strip(ss.substr(fd+1));
                    if (key.size() && val.size())
                        cfg[key] = val;
                }
            }

            //Note: calling cfg[key]=xxx would clear cfg.hashid and calc .hash_hlp
            //      so set name later.
            cfg.hashid = name;
            auto iter = cfg_hashs.emplace(name, cfg).first;
            cfg_indexs[CDataRef{&iter->second.data, iter->second.hash_hlp}] = name;
        }
} //ConfigurationManager()


void ConfigurationManager::set_name(
    const std::string &name, const ConfigurationID &hash
) {
    if (auto fd = cfg_hashs.find(hash); fd != cfg_hashs.end())
        named_cfgs[name] = &(fd->second);
}
const Configuration *ConfigurationManager::get(const std::string name) const {
    if (auto fd = named_cfgs.find(name); fd != named_cfgs.end())
        return fd->second;
    return nullptr;
}

ConfigurationID ConfigurationManager::commit(Configuration cfg)
{
    //return if unchanged.
    if (cfg.hashid.size())
        return cfg.hashid;

    //[entry]: find by content
    CDataRef lastref{&cfg.data, cfg.hash_hlp};
    if (auto fd = cfg_indexs.find(lastref); fd != cfg_indexs.end())
        return fd->second;

    //[entry]: create new
    std::string new_id = get_hash(CHasher()(lastref));
    cfg.hashid = new_id;
    auto iter = cfg_hashs.emplace(new_id, std::move(cfg));
    cfg_indexs[CDataRef{&iter.first->second}] = new_id;

    //[.cfg file]: create new
    std::ofstream fout(std::filesystem::path{storage_dir} / (new_id+".cfg"));
    for (auto &[k, v]: iter.first->second.data)
        fout<<k<<" = "<<v<<"\n";

    return new_id;
} //ConfigurationManager::commit()

static void UlongToHexString(uint64_t num, char *s, bool lowerAlpha);

std::string ConfigurationManager::get_hash(size_t want) {
    std::string rv = "PlaceHol\0\0\0\0\0der"; //sizeof(rv) == 16
    for (size_t i=0; i<20; i++) {
        size_t j = (i + want)%0xFFffFFfful;
        UlongToHexString(j, rv.data(), false);
        if (cfg_hashs.count(rv.c_str()) == 0)
            return rv.c_str();
    }
    throw std::runtime_error{"Too Many conflictions, please update the hash algorithm."};
    return "";
} //ConfigurationManager::get_hash()

std::string Configuration::empty_string;

// https://johnnylee-sde.github.io/Fast-unsigned-integer-to-hex-string/
static void UlongToHexString(uint64_t num, char *s, bool lowerAlpha)
{
	uint64_t x = num;

	// use bitwise-ANDs and bit-shifts to isolate
	// each nibble into its own byte
	// also need to position relevant nibble/byte into
	// proper location for little-endian copy
	x = ((x & 0xFFFF) << 32) | ((x & 0xFFFF0000) >> 16);
	x = ((x & 0x0000FF000000FF00) >> 8) | (x & 0x000000FF000000FF) << 16;
	x = ((x & 0x00F000F000F000F0) >> 4) | (x & 0x000F000F000F000F) << 8;

	// Now isolated hex digit in each byte
	// Ex: 0x1234FACE => 0x0E0C0A0F04030201

	// Create bitmask of bytes containing alpha hex digits
	// - add 6 to each digit
	// - if the digit is a high alpha hex digit, then the addition
	//   will overflow to the high nibble of the byte
	// - shift the high nibble down to the low nibble and mask
	//   to create the relevant bitmask
	//
	// Using above example:
	// 0x0E0C0A0F04030201 + 0x0606060606060606 = 0x141210150a090807
	// >> 4 == 0x0141210150a09080 & 0x0101010101010101
	// == 0x0101010100000000
	//
	uint64_t mask = ((x + 0x0606060606060606) >> 4) & 0x0101010101010101;

	// convert to ASCII numeral characters
	x |= 0x3030303030303030;

	// if there are high hexadecimal characters, need to adjust
	// for uppercase alpha hex digits, need to add 0x07
	//   to move 0x3A-0x3F to 0x41-0x46 (A-F)
	// for lowercase alpha hex digits, need to add 0x27
	//   to move 0x3A-0x3F to 0x61-0x66 (a-f)
	// it's actually more expensive to test if mask non-null
	//   and then run the following stmt
	x += ((lowerAlpha) ? 0x27 : 0x07) * mask;

	//copy string to output buffer
	*(uint64_t *)s = x;
} //UlongToHexString()

} //namespace