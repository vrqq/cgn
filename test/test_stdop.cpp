#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "../v1/std_operator.hpp"

using StrList = std::vector<std::string>;

int main()
{
    // --- vector += initializer_list ---
    // C++ allows a braced-init-list on the right-hand side of assignment
    // operators (+=, =, etc.) but NOT as a free operand of binary '+'.
    StrList list1 = {"hello"};
    list1 += {"aa"};
    assert(list1.size() == 2);
    assert(list1[0] == "hello" && list1[1] == "aa");

    // --- (initializer_list / vector) + vector + (initializer_list / vector) ---
    // Braced-init-lists must be explicitly constructed to be used with '+'.
    StrList list2;
    list2 = StrList{"mm"} + list1 + StrList{"aa"};
    // {"mm"} + {"hello","aa"} + {"aa"} => {"mm","hello","aa","aa"}
    assert(list2.size() == 4);
    assert(list2[0] == "mm" && list2[1] == "hello"
        && list2[2] == "aa"  && list2[3] == "aa");

    // --- initializer_list<string> + operator ---
    // Named initializer_list variables CAN be passed to our operator+.
    std::initializer_list<std::string> il = {"mm"};
    StrList list2b = il + list1;
    assert(list2b.size() == 3 && list2b[0] == "mm");

    // --- chained initializer_list + initializer_list + initializer_list ---
    StrList list3;
    list3 = StrList{"aa"} + StrList{"bb"} + StrList{"cc", "dd"};
    assert(list3.size() == 4);
    assert(list3[0] == "aa" && list3[1] == "bb"
        && list3[2] == "cc" && list3[3] == "dd");

    // Also works via named initializer_list for the first operand:
    std::initializer_list<std::string> il2 = {"aa"};
    StrList list3b = il2 + StrList{"bb"} + StrList{"cc", "dd"};
    assert(list3b == list3);

    // --- vector += {} (single element) ---
    StrList list4 = {"x"};
    list4 += {"y"};
    assert(list4.size() == 2 && list4[1] == "y");

    // --- vector + typed initializer_list ---
    // C++11 does not allow `list4 + {"xx", "yy"}` directly. The shortest
    // operator form without `StrList` on the right is a typed initializer_list.
    StrList list5 = list4 + std::initializer_list<std::string>{"z"};
    assert(list5.size() == 3 && list5[2] == "z");

    StrList list5b = list4 + std::initializer_list<std::string>{"xx", "yy"};
    assert(list5b.size() == 4);
    assert(list5b[0] == "x" && list5b[1] == "y"
        && list5b[2] == "xx" && list5b[3] == "yy");

    // --- vector temporary + vector ---
    StrList list6 = StrList{"w"} + list4;
    assert(list6.size() == 3 && list6[0] == "w");

    // --- vector + vector (pre-existing) ---
    StrList list7 = list4 + list5;
    assert(list7.size() == list4.size() + list5.size());

    // --- vector += vector (pre-existing) ---
    StrList list8 = {"p"};
    list8 += list4;
    assert(list8.size() == 1 + list4.size() && list8[0] == "p");

    std::cout << "All tests passed!" << std::endl;
    return 0;
}