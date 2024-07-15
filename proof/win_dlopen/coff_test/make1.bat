del func1.obj func1.obj.txt fake.obj fake.obj.txt dlout.* main.obj main.exe main.exe.txt

cl.exe /c func1.cpp /EHsc /Fo: func1.obj
dumpbin /ALL func1.obj > func1.obj.txt

ml64.exe /c /Cx fake.S
dumpbin /ALL fake.obj > fake.obj.txt

link.exe /DLL fake.obj func1.obj /OUT:dlout.dll
dumpbin /ALL dlout.dll > dlout.dll.txt

cl.exe main.cpp dlout.lib /Fe: main.exe
dumpbin /ALL main.exe > main.exe.txt