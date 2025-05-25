# File: win32/Makefile

# Configuration
CXX = cl
LD = link
CXXFLAGS = /nologo /EHsc
DLLFLAGS = /LD /D "BUILDING_DLL"
EXEFLAGS = /nologo
OUTDIR = ..\build
INSTALLDIR = ..\install

# Source files
FN1_SRC = ..\fn1.cpp
MAIN_SRC = ..\main.cpp

# Targets
FN1_OBJ = $(OUTDIR)\fn1.obj
FN1_DLL = $(OUTDIR)\fn1.dll
FN1_LIB = $(OUTDIR)\fn1.lib
MAIN_OBJ = $(OUTDIR)\main.obj
MAIN_EXE = $(OUTDIR)\main.exe

# Default target
all: $(MAIN_EXE)

# Create output dir
$(OUTDIR):
	if not exist $(OUTDIR) mkdir $(OUTDIR)

# Build fn1.dll and fn1.lib
$(FN1_DLL): $(OUTDIR) $(FN1_SRC)
	$(CXX) $(CXXFLAGS) $(DLLFLAGS) /Fo$(FN1_OBJ) /Fe$(FN1_DLL) $(FN1_SRC)

# Build main.exe (link against fn1.lib)
$(MAIN_EXE): $(FN1_DLL) $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) /Fo$(MAIN_OBJ) /Fe$(MAIN_EXE) $(MAIN_SRC) $(FN1_LIB)

# Install
install: all
	if not exist $(INSTALLDIR) mkdir $(INSTALLDIR)
	copy /Y $(MAIN_EXE) $(INSTALLDIR)
	copy /Y $(FN1_DLL) $(INSTALLDIR)

# Clean
clean:
	if exist $(OUTDIR) rmdir /S /Q $(OUTDIR)
	if exist $(INSTALLDIR) rmdir /S /Q $(INSTALLDIR)
