#pragma once
#include <vector>
#include <string>


struct MyTypeA
{
    std::string str1, str2;
};

using MyTypeB = std::vector<int>;

using MyTypeC = std::vector<MyTypeA>;