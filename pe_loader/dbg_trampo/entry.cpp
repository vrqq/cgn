// The source file of tr_debug.exe
//
#include <iostream>
#include "../pe_file.h"
#include "publ.h"
namespace cgn = cgnv1;

int load_libuser_dll(const std::string &userdll) {
    // Load libfunc1.dll
    std::string dllname = "./libfunc1.dll";
    std::cout<<"gen_asm_for_libuser"<<std::endl;
    auto rv = cgn::GlobalSymbol::WinLoadLibrary(dllname);
    std::cout<<"WinLoadLibrary("<< dllname << ") m_ptr = "<<rv.m_ptr<<" sym_export_count="<< rv.sym_exports.size() << std::endl;
    
    // Show all registered table
    std::cout << "=== Global Symbol Table ===\n";
    for (auto& [sym, addr] : *cgn::GlobalSymbol::get_symbol_table())
        std::cout<<"   0x" << std::hex << addr << " " << sym << "\n";

    // Load libuser.dll (within ASM trampo)
    auto rv2 = cgn::GlobalSymbol::WinLoadLibrary(userdll);
    if (rv2.sym_exports.empty()) {
        std::cerr << "Cannot load " << userdll << std::endl;
        return 1;
    }else {
        std::cout << "\n=== Library "<< userdll <<" Exports ===\n";
        for (auto sym : rv2.sym_exports)
            std::cout<<"  "<<sym<<"\n";
    }
    std::cout << "\n=== AfterLoad "<< userdll <<" Global Symbols ===\n";
    for (auto& [sym, addr] : *cgn::GlobalSymbol::get_symbol_table())
        std::cout << "   0x" << std::hex << addr << " " << sym << "\n";

    // Call function inside libuser.dll
    using TypeFuncuser = int(*)(int);
    TypeFuncuser fnptr = (TypeFuncuser)cgn::GlobalSymbol::find("?funcuser@@YAHH@Z");
    std::cout << "=== Before call funcuser 0x" << std::hex << (void*)fnptr << std::endl;
    try {
        int rv_funcuser = fnptr(1);
        std::cout << "=== funcuser return " << std::dec << rv_funcuser << std::endl;
    }
    catch (std::exception& e) {
        std::cout << "=== Catched! ===\n";
    }

    // Call another function inside libuser.dll
    TypeFuncuser ptrX = (TypeFuncuser)cgn::GlobalSymbol::find("?funcuser_X@@YAHH@Z");
    std::cout << "=== Before call funcuserX 0x" << std::hex << (void*)ptrX << std::endl;
    int rv_funcX = ptrX(1);
    std::cout << "=== funcuserX return " << std::dec << rv_funcX << std::endl;
    return 0;
}

int gen_asm()
{
    std::cout << "=== GEN ASM file ===\n";
    cgn::MSVCTrampo helper;
    std::cout << "=== add objfile\n";
    helper.add_objfile("libuser.obj");
    std::cout << "=== output asm\n";
    std::string libfile = "libuser_trampo_autogen.lib";
    helper.make_asmfile("libuser_trampo_autogen.asm", libfile);
    //libfile should be cleared here since there's no long function symbol required.
    std::cout << "=== done\n";
    return 0;
}

// Run "tr_debug.exe T libuser_autogen.dll"
//  to test dll with auto generated ASM trampoline
int main(int argc, char** argv)
{
    if (argc == 2 && argv[1][0] == 'A')
        return gen_asm();
    if (argc == 3 && argv[1][0] == 'T')
        return load_libuser_dll(argv[2]);
    return load_libuser_dll("libuser_manualgen.dll");
}
