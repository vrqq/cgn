del /S *.dll *.exp *.lib *.exe *.obj

cl.exe func2/x.cpp /EHsc /MD /link /DLL /OUT:"func2/x.dll"

lib.exe /def:func2/fake2.def /nologo /machine:x64 /OUT:"func2/fake2.lib"

@rem it cause "func1/x.dll load failure 126" with linking with func2/fake2.lib
@rem for LoadLibrary("func1/x.dll") in main.exe
cl.exe func1/x.cpp func2/fake2.lib /EHsc /MD /link /DLL /OUT:"func1/x.dll" /ALLOWBIND:NO

@rem This is the right way to run program
cl.exe func1/x.cpp func2/x.lib /EHsc /MD /link /DLL /OUT:"func1/x.dll" /DELAYLOAD:"x.dll"

cl.exe rootfn.cpp /EHsc /MD /link /DLL /OUT:"rootfn.dll"

cl.exe main.cpp func1/x.lib rootfn.lib /EHsc /MD /Fe:"main.exe" /link /DELAYLOAD:"rootfn.dll" /ALLOWBIND:NO