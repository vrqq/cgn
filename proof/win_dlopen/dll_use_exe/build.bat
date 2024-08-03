del *.exe *.exp *.obj *.lib *.dll

lib.exe /def:main_manual.def /out:main_manual.lib

cl.exe func1.cpp /MD /link /DLL main_manual.lib /OUT:"func1.dll" /ALLOWBIND:NO

cl.exe main.cpp /DWINVER=0x0A00 /D_WIN32_WINNT=0x0603 /D_CONSOLE /D_AMD64_ /MD /EHsc /Fe: main.exe
