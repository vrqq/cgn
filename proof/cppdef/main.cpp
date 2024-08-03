// In terminal, we define things below
//   DEFVAR = var
//   DEFSTRING = "string1"

#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)

#include <iostream>

int CONCAT(DEFVAR, 1) = 10;

int main() {
    var1 = 99;
    std::cout<<"DEFSTRING: " << DEFSTRING <<"\n";
    std::cout<<"DEFVAR: " << var1 <<"\n";
    return 0;
}