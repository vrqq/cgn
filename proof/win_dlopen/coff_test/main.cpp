#include <stdio.h>

__declspec(dllimport) int func1();

int main(int argc, char **argv)
{
    printf("BEFORE func1()\n");
    printf("%d\n", func1());
    printf("AFTER func1()\n");
    return 0;
}
