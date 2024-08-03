
__declspec( dllimport ) int c();

__declspec( dllexport ) int b() { return c() + 1; }