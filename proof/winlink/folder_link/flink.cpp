// The example to test folder symbolic link in Linux and windows
// Result:
//   For windows, failed but no exception throw, permission denied when 
//   using mklink.exe
//
// compile on linux gcc
//   g++ flink.cpp -o flink
//   ./flink ./src1 ./dst1
// compile on windows msvc
//   cl.exe flink.cpp /std:c++17 /DWINVER=0x0A00 /D_WIN32_WINNT=0x0603 /D_CONSOLE /D_AMD64_ /MD /EHsc /utf-8
//   flink.exe src1 dstw
//
// running result:
//   Z:\> flink.exe c:\wwd\smart c:\wwd\smart-link
//   mklink : (DIR) "c:\\wwd\\smart" -> (DIR) "c:\\wwd\\smart-link"
//   DONE: 1314 unknown error
//
//   Z:\fable\cgn-design\proof\winlink\folder_link> flink.exe src1 dst2
//   Current path:"Z:\\fable\\cgn-design\\proof\\winlink\\folder_link"
//   mklink : (DIR) "src1" -> (DIR) "dst2"
//   DONE: 5 unknown error
//
#include <filesystem>
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 3)
        return 1;
    std::cout<<"Current path:"<<std::filesystem::current_path()<<std::endl;
    auto u = std::filesystem::path(argv[1]);
    auto v = std::filesystem::path(argv[2]);
    if (!std::filesystem::is_directory(u)) {
        std::cerr<<u<<" is not directory."<<std::endl;
        return 1;
    }
    if (std::filesystem::exists(v)) {
        std::cerr<<v<<" existed."<<std::endl;
        return 1;
    }

    std::cout<<"mklink : (DIR) "<<u<<" -> (DIR) "<<v<<std::endl;
    std::error_code ec;
    std::filesystem::create_directory_symlink(u, v, ec);
    std::cout<<"DONE: "<<ec.value()<<" "<<ec.message()<<std::endl;
    return 0;
}
