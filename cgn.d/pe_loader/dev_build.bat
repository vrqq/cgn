
cl.exe /DEBUG /DDEV_DEBUG /std:c++17 /MD /EHsc /Fe:pe_debug.exe pe_loader_debug.cpp pe_file.cpp ../entry/cgn_tools.cpp ../ninjabuild/src/line_printer.cc ../ninjabuild/src/util.cc ../ninjabuild/src/string_piece_util.cc ../ninjabuild/src/edit_distance.cc 
