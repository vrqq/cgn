cl.exe try_glb_ext.cpp /MD /link /DLL /OUT:try_glb_ext.dll
cl.exe try_glb_main.cpp /DWINVER=0x0A00 /D_WIN32_WINNT=0x0603 /D_CONSOLE /D_AMD64_ /MD /EHsc /utf-8