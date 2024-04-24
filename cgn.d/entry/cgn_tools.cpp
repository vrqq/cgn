#include <algorithm>
#include <filesystem>
#include <sstream>
#include <fstream>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
    #include <direct.h>  // _mkdir
    #include <windows.h>
    #include <sysinfoapi.h>
#else
    #include <unistd.h>
#endif

#ifdef __linux__
    #include <sys/utsname.h>
#endif

#include "../cgn.h"

namespace cgn {

#ifdef _WIN32
static std::string GetLastErrorString() {
    DWORD err = GetLastError();

    char* msg_buf;
    FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            err,
            MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
            (char*)&msg_buf,
            0,
            NULL);

    if (msg_buf == nullptr) {
        char fallback_msg[128] = {0};
        snprintf(fallback_msg, sizeof(fallback_msg), "GetLastError() = %d", err);
        return fallback_msg;
    }

    string msg = msg_buf;
    LocalFree(msg_buf);
    return msg;
} //GetLastErrorString()

static int64_t TimeStampFromFileTime(const FILETIME& filetime) {
    // FILETIME is in 100-nanosecond increments since the Windows epoch.
    // We don't much care about epoch correctness but we do want the
    // resulting value to fit in a 64-bit integer.
    uint64_t mtime = ((uint64_t)filetime.dwHighDateTime << 32) |
        ((uint64_t)filetime.dwLowDateTime);
    // 1600 epoch -> 2000 epoch (subtract 400 years).
    return (TimeStamp)mtime - 12622770400LL * (1000000000LL / 100);
} //TimeStampFromFileTime()

#endif //_WIN32

static bool is_cpu_big_endian() {
    constexpr static uint16_t a = 0x0001;
    return reinterpret_cast<const char*>(&a)[0] != (char)0x01;
}

// Tools implements
// --------------------

uint32_t Tools::host_to_u32be(uint32_t in)
{
    std::byte dst[4];
    memcpy(dst, &in, 4);
    if (is_cpu_big_endian() == false) {
        dst[0]^=dst[3]; dst[3]^=dst[0]; dst[0]^=dst[3];
        dst[1]^=dst[2]; dst[2]^=dst[1]; dst[1]^=dst[2];
    }
    return *((uint32_t*)dst);
}

uint32_t Tools::u32be_to_host(uint32_t in)
{
    if (is_cpu_big_endian())
        return in;
    std::byte* src = (std::byte*)&in;
    return ((uint32_t)src[3])     + ((uint32_t)src[2]<<8)
         + ((uint32_t)src[1]<<16) + ((uint32_t)src[0]<<24);
}

// Bash Escape
// $ : Variable substitution.
// # : Comment.
// & : Background job.
// * : Matches any string in filename expansion.
// ? : Matches any single character in filename expansion.
// ; : Command separator.
// | : Pipe, command chaining.
// > : Redirect output.
// < : Redirect input.
// () : Command grouping.
// {} : Command block.
// [] : Character classes in filename expansion.
// ‘ : Command substitution.
// “ : Partial quote.
// ‘ : Full quote.
// ~ : Home directory.
// ! : History substitution.
// \ : Escape character.
std::string Tools::shell_escape(
    const std::string &in
) {
    constexpr static auto bash_special = [](){
        std::array<bool, 256> rv{0};
        for (std::size_t i=0; i<rv.size(); i++)
            rv[i] = 0;
        rv['$'] = rv['#'] = rv['&'] = rv['*'] = rv['?'] = rv['|'] = rv['>'] 
        = rv['<'] = rv['('] = rv[')'] = rv['{'] = rv['}'] = rv['['] = rv[']']
        = rv['\''] = rv['"'] = rv['~'] = rv['!'] = rv['\\'] = rv[' '] = 1;
        return rv;
    }();

    std::string rv;
    #ifdef _WIN32
        #error "TODO!"
    #else
        for (auto c: in)
            if (bash_special[c])
                rv.append({'\\', c});
            else
                rv.push_back(c);
    #endif
    return rv;
}

HostInfo Tools::get_host_info()
{
    HostInfo rv;
    #ifdef _WIN32
        // https://stackoverflow.com/questions/47023477/how-to-get-system-information-in-windows-with-c
        rv.os = "win";
        SYSTEM_INFO siSysInfo;
        GetSystemInfo(&siSysInfo); 
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            rv.cpu = "x86_64";
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM)
            rv.cpu = "arm";
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
            rv.cpu = "arm64";
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64)
            rv.cpu = "ia64";
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
            rv.cpu = "x86";
    #elif __APPLE__
        rv.os = "mac";
    #elif __linux__
        rv.os = "linux";
        struct utsname osInfo{};
        uname(&osInfo);
        rv.cpu = osInfo.machine;
    #endif
    return rv;
}

std::string Tools::rebase_path(const std::string &p, const std::string &base)
{
    return std::filesystem::proximate(p, base).string();
}

std::string Tools::locale_path(const std::string &in)
{
    return std::filesystem::path{in}.make_preferred().string();
}

std::string Tools::parent_path(const std::string &in)
{
    auto p = std::filesystem::path{in}.parent_path();
    return p.has_parent_path()? p.parent_path() : ".";
}

bool Tools::win32_long_paths_enabled() 
{
#ifdef _WIN32
    setlocale(LC_ALL, "");

    // Probe ntdll.dll for RtlAreLongPathsEnabled, and call it if it exists.
    HINSTANCE ntdll_lib = ::GetModuleHandleW(L"ntdll");
    if (ntdll_lib) {
        typedef BOOLEAN(WINAPI FunctionType)();
        auto* func_ptr = reinterpret_cast<FunctionType*>(
            ::GetProcAddress(ntdll_lib, "RtlAreLongPathsEnabled"));
        if (func_ptr)
            return (*func_ptr)();
    }
#endif
    return false;
} //Tools::win32_long_paths_enabled()

bool Tools::is_win7_or_later() {
#ifdef _WIN32
  OSVERSIONINFOEX version_info =
      { sizeof(OSVERSIONINFOEX), 6, 1, 0, 0, {0}, 0, 0, 0, 0, 0};
  DWORDLONG comparison = 0;
  VER_SET_CONDITION(comparison, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(comparison, VER_MINORVERSION, VER_GREATER_EQUAL);
  return VerifyVersionInfo(
      &version_info, VER_MAJORVERSION | VER_MINORVERSION, comparison);
#else
    return false;
#endif
} //Tools::is_win7_or_later()


std::unordered_map<std::string, int64_t> Tools::win32_stat_folder(
    const std::string &folder_path
) {
#ifdef _WIN32
    std::unordered_map<std::string, int64_t> stamps;
    // FindExInfoBasic is 30% faster than FindExInfoStandard.
    static bool can_use_basic_info = is_win7_or_later();
    // This is not in earlier SDKs.
    const FINDEX_INFO_LEVELS kFindExInfoBasic =
        static_cast<FINDEX_INFO_LEVELS>(1);
    FINDEX_INFO_LEVELS level =
        can_use_basic_info ? kFindExInfoBasic : FindExInfoStandard;
    WIN32_FIND_DATAA ffd;
    HANDLE find_handle = FindFirstFileExA((folder_path + "\\*").c_str(), level, &ffd,
                                        FindExSearchNameMatch, NULL, 0);

    if (find_handle == INVALID_HANDLE_VALUE) {
        DWORD win_err = GetLastError();
        if (win_err == ERROR_FILE_NOT_FOUND || win_err == ERROR_PATH_NOT_FOUND ||
            win_err == ERROR_DIRECTORY)
            return {};
        throw std::runtime_error{"FindFirstFileExA(" + folder_path + "): " 
                + GetLastErrorString()};
    }
    do {
        std::string fname{ffd.cFileName};
        if (fname == "..") {
            // Seems to just copy the timestamp for ".." from ".", which is wrong.
            // This is the case at least on NTFS under Windows 7.
            continue;
        }
        stamps[fname] = TimeStampFromFileTime(ffd.ftLastWriteTime);
    } while (FindNextFileA(find_handle, &ffd));
    FindClose(find_handle);
    return stamps;
#else
    return {};
#endif
} //Tools::win32_stat_folder()


int64_t Tools::stat(const std::string &path)
{
#ifdef _WIN32
    // MSDN: "Naming Files, Paths, and Namespaces"
    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
    if (!path.empty() && !win32_long_paths_enabled() && path[0] != '\\' &&
        path.size() > MAX_PATH) {
            std::string err = "Stat(" + path + "): Filename longer than "
                            + std::to_string(MAX_PATH) + " characters";
            throw std::runtime_error{err};
    }
    
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attrs)) {
        DWORD win_err = GetLastError();
        if (win_err == ERROR_FILE_NOT_FOUND || win_err == ERROR_PATH_NOT_FOUND)
            return 0;
        throw std::runtime_error{
            "GetFileAttributesEx(" + path + "): " + GetLastErrorString()
        };
    }
    return TimeStampFromFileTime(attrs.ftLastWriteTime);

#else //_WIN32 above, others below

    #ifdef __USE_LARGEFILE64
    struct stat64 st;
    if (stat64(path.c_str(), &st) < 0) {
    #else
    struct stat st;
    if (stat(path.c_str(), &st) < 0) {
    #endif
        if (errno == ENOENT || errno == ENOTDIR)
            return 0;
        throw std::runtime_error{"stat(" + path + "): " + strerror(errno)};
        return -1;
    }
    // Some users (Flatpak) set mtime to 0, this should be harmless
    // and avoids conflicting with our return value of 0 meaning
    // that it doesn't exist.
    if (st.st_mtime == 0)
        return 1;
    #if defined(_AIX)
        return (int64_t)st.st_mtime * 1000000000LL + st.st_mtime_n;
    #elif defined(__APPLE__)
        return ((int64_t)st.st_mtimespec.tv_sec * 1000000000LL +
                st.st_mtimespec.tv_nsec);
    #elif defined(st_mtime) // A macro, so we're likely on modern POSIX.
        return (int64_t)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
    #else
        return (int64_t)st.st_mtime * 1000000000LL + st.st_mtimensec;
    #endif

#endif //_WIN32 or others

} //Tools::stat()


std::string Tools::rebase_label(
    const std::string &p, std::string base
) {
    if (p.size() && p[0] == '@')
        return p;
    if (base.size() && base.back() == '/' && p.size() && p.front() == ':')
        base.pop_back();
    auto tmp = std::filesystem::proximate(
        std::filesystem::path{base} / p
    );
    return "//" + tmp.string();
}

std::unordered_map<std::string, std::string> 
Tools::read_kvfile(const std::string &fname)
{
    //string strip function
    auto strip = [](const std::string &ss) -> std::string {
        int i=0, j=ss.size()-1;
        while(ss[i] == ' ' && i<ss.size()) i++;
        while(ss[j] == ' ' && j>=i) j--;
        if (i <= j)
            return ss.substr(i, j-i+1);
        return "";
    };

    std::unordered_map<std::string, std::string> rv;
    std::ifstream fin(fname);
    for (std::string ss; !fin.eof() && std::getline(fin, ss);) {
        if (ss.empty())
            continue;
        if (auto fd = ss.find('='); fd != ss.npos) {
            std::string key = strip(ss.substr(0, fd-1));
            std::string val = strip(ss.substr(fd+1));
            if (key.size() && val.size())
                rv[key] = val;
        }
    }

    return rv;
}


} //namespace