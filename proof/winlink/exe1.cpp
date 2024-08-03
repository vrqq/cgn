#pragma comment(linker, "/manifestdependency:\"type='win32' name='bc' version='1.0.0.0' processorArchitecture='*' language='*'\"")

__declspec( dllimport ) int b();

int main() {
    return b();
}
