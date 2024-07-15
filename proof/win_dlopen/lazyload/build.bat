del /S *.dll *.exp *.lib *.exe *.obj

cl.exe func2/x.cpp /EHsc /MD /link /DLL /OUT:"func2/x.dll"

cl.exe func1/x.cpp func2/x.lib /EHsc /MD /link /DLL /OUT:"func1/x.dll"

cl.exe rootfn.cpp /EHsc /MD /link /DLL /OUT:"rootfn.dll"

cl.exe main.cpp func1/x.lib rootfn.lib /EHsc /MD /Fe:"main.exe" /link /DELAYLOAD:"rootfn.dll"