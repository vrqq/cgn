cl.exe dll.cpp /MD /link /DLL /OUT:"dll.dll"
cl.exe main.cpp /MD /EHsc /Fe: main.exe /link /DELAY:UNLOAD /DELAY:NOBIND /FORCE:UNRESOLVED
