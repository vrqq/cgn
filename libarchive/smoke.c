#include <archive.h>

int main(void) {
    const char *v = archive_version_details();
    return (v && *v) ? 0 : 1;
}
