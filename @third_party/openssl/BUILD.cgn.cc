#include <cgn>

cxx_sources("openssl", x) {
    if (x.cfg["os"] == "linux")
        x.pub.ldflags = {"-lssl", "-lcrypto"};
}
