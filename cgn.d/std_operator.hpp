#pragma once

#include <vector>
#include <string>
#include <unordered_set>
using StrList = std::vector<std::string>;
using StrSet  = std::unordered_set<std::string>;

StrList operator+(const StrList &lhs, const StrList &rhs) {
    StrList rv{lhs};
    rv.insert(rv.end(), rhs.begin(), rhs.end());
    return rv;
}
StrList &operator+=(StrList &lhs, const StrList &rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}

StrSet operator+(const StrSet &lhs, const StrSet &rhs) {
    StrSet rv{lhs};
    rv.insert(rhs.begin(), rhs.end());
    return rv;
}
StrSet &operator+=(StrSet &lhs, const StrSet &rhs) {
    lhs.insert(rhs.begin(), rhs.end());
    return lhs;
}


template<typename T> StrList&
operator+=(StrList &lhs, std::initializer_list<T> rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}
template<typename T> StrSet&
operator+=(StrSet &lhs, std::initializer_list<T> rhs) {
    lhs.insert(rhs);
    return lhs;
}
