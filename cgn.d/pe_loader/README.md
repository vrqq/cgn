https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

**.obj File**
Common Object File Format (COFF)

https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-file-header-object-and-image
https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-table-section-headers

File format
```
<COFF File Header : 20 bytes>
n * <Section Headers : 40 bytes>
m * <DataBlock : aligned on a 4-byte boundary> .....
```
* First section : `.drectve` contain the build arg `/DEFAULTLIB`
* section `COFF symbol table` : all symbols used in this obj file
* Example: `func1.obj` and `func1.obj.txt` (git ignored)

**.lib File**

File format
```
<Magic: 8 bytes>
<Header: 60 bytes> <1st Linker Member>
<Header: 60 bytes> <2nd Linker Member>
n * {<Header: 60 bytes> <...>}
```

* The `1st Linker Member` contains all symbols exported.
* Example: `msvcrt.lib` (git ignored)
