// TBD: currently unused
// From CMake source code: Tests/RunCMake/showIncludes.c 
#ifdef _WIN32

#include <string>
#include <cstdlib>
#include <windows.h>

std::string guess_ninja_msvc_deps_prefix()
{
  /* 'cl /showIncludes' encodes output in the console output code page.  */
  unsigned int cp = GetConsoleOutputCP();

  /* 'cl /showIncludes' prints output in the VS language.  */
  const char* vslang = getenv("VSLANG");

  std::string rv;

  if (strcmp(vslang, "1031") == 0) {
    if (cp == 437 || cp == 65001) {
      return "Hinweis: Einlesen der Datei:";
    }
  }

  if (strcmp(vslang, "1033") == 0) {
    if (cp == 437 || cp == 65001) {
      return "Note: including file:";
    }
  }

  if (strcmp(vslang, "1036") == 0) {
    if (cp == 437 || cp == 863) {
      return "Remarque\xff: inclusion du fichier\xff";
    }
    if (cp == 65001) {
      return "Remarque\xc2\xa0: inclusion du fichier\xc2\xa0:";
    }
  }

  if (strcmp(vslang, "1040") == 0) {
    if (cp == 437 || cp == 65001) {
      return "Nota: file incluso  C:\\foo.h\n");
    }
  }

  if (strcmp(vslang, "1041") == 0) {
    if (cp == 932) {
      return "\x83\x81\x83\x82: "
             "\x83\x43\x83\x93\x83\x4e\x83\x8b\x81\x5b\x83\x68 "
             "\x83\x74\x83\x40\x83\x43\x83\x8b:";
    }
    if (cp == 65001) {
      return "\xe3\x83\xa1\xe3\x83\xa2: \xe3\x82\xa4\xe3\x83\xb3"
             "\xe3\x82\xaf\xe3\x83\xab\xe3\x83\xbc\xe3\x83\x89 "
             "\xe3\x83\x95\xe3\x82\xa1\xe3\x82\xa4\xe3\x83\xab:";
    }
  }

  if (strcmp(vslang, "2052") == 0) {
    if (cp == 54936 || cp == 936) {
      return "\xd7\xa2\xd2\xe2: "
             "\xb0\xfc\xba\xac\xce\xc4\xbc\xfe:";
    }

    if (cp == 65001) {
      return "\xe6\xb3\xa8\xe6\x84\x8f: "
             "\xe5\x8c\x85\xe5\x90\xab\xe6\x96\x87\xe4\xbb\xb6:";
    }
  }

  return "";
}

#else
std::string guess_ninja_msvc_deps_prefix() {
  return "";
}
#endif