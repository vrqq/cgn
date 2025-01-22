#include <cgn>

cxx_executable("advcopy", x)
{
    x.srcs = {"advcopy.cpp", "cli.cpp"};
}
