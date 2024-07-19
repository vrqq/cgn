#include <cstring>

extern int base();

__declspec(dllexport) int func1() {
    return strlen("eeffg") + base();
}