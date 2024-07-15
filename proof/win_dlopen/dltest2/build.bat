cl.exe dll1.cpp main1.lib /EHsc /link /DLL /OUT:"dll1.dll"
cl.exe main1.cpp /EHsc /Fe: main1.exe /EHsc /link /DELAY:UNLOAD /DELAY:NOBIND /FORCE:UNRESOLVED