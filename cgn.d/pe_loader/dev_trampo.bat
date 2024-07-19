
cl.exe /nologo /DEBUG /DWINVER=0x0A00 /D_WIN32_WINNT=0x0603 /D_CONSOLE /D_AMD64_ /std:c++17 /MD /EHsc /Fe:pe_debug.exe trampo_debug.cpp pe_file.cpp msvc_symbol_host.cpp msvc_trampo.cpp ../entry/cgn_tools.cpp ../ninjabuild/src/line_printer.cc ../ninjabuild/src/util.cc ../ninjabuild/src/string_piece_util.cc ../ninjabuild/src/edit_distance.cc 

del *.exp *.lib *.obj