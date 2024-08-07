@cd /D "%~dp0"

@REM del /S *.dll *.obj

@REM base.dll
cl.exe /nologo /std:c++17 /MD /EHsc /Fo:base.obj /c base.cpp
dumpbin /ALL base.obj > base.obj.txt
link.exe /nologo /DLL base.obj /OUT:base.dll /VERBOSE > base.dll.txt

cl.exe /nologo /std:c++17 /MD /EHsc /Fo:objmaker.obj /c objmaker.cpp
dumpbin /ALL objmaker.obj > objmaker.obj.txt
link.exe /nologo /DLL objmaker.obj base.lib /OUT:objmaker.dll /VERBOSE > objmaker.dll.txt

cl.exe /nologo /std:c++17 /MD /EHsc /Fo:entry0.obj /c entry0.cpp
dumpbin /ALL entry0.obj > entry0.obj.txt
link.exe /nologo /DLL entry0.obj objmaker.lib /OUT:entry0.dll /VERBOSE > entry0.dll.txt
