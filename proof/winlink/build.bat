mkdir dir1

cl.exe c.cpp /MD /link /DLL /OUT:dir1/c.dll
del c.obj

cl.exe b.cpp dir1/c.lib /MD /link /DLL /OUT:dir1/b.dll
del b.obj

cl.exe exe1.cpp dir1/b.lib /DWINVER=0x0A00 /D_WIN32_WINNT=0x0603 /D_CONSOLE /D_AMD64_ /MD /EHsc /utf-8