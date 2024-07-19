#include <string>
#include <cstring>

static std::string glb1 = "1234567890";
__declspec(dllexport) int base() {
    return 10 + strlen("aabbccd") + glb1.size();
}