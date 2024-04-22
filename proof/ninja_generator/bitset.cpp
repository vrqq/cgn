#include <iostream>
#include <cstdint>

int main() {
    uint32_t a = 0x12345678;
    for (int i=31; i>=0; i--){
        if ((a & (1UL<<i)) != 0)
            std::cout<<"1";
        else
            std::cout<<"0";
        if (i%4 == 0)
            std::cout<<" ";
    }
    std::cout<<std::endl;
    return 0;
}