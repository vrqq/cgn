@REM https://learn.microsoft.com/en-us/cpp/build/reference/d-preprocessor-definitions?view=msvc-170
@REM The /D option doesn't support function-like macro definitions. 
@REM To insert definitions that can't be defined on the command line, 
@REM consider the /FI (Name forced include file) compiler option.
cl.exe /nologo main.cpp /EHsc /Fe: main.exe /DDEFVAR=var /D DEFSTRING=\"DEF~!@^#$\"