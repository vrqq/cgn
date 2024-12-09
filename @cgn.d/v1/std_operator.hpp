#pragma once

#include <vector>
#include <string>
#include <unordered_set>
using StrList = std::vector<std::string>;
using StrSet  = std::unordered_set<std::string>;

inline StrList operator+(const StrList &lhs, const StrList &rhs) {
    StrList rv{lhs};
    rv.insert(rv.end(), rhs.begin(), rhs.end());
    return rv;
}
inline StrList operator+(const StrList &lhs, StrList &&rhs) {
    StrList rv{lhs};
    rv.insert(rv.end(), 
        std::make_move_iterator(rhs.begin()), 
        std::make_move_iterator(rhs.end()));
    rhs.clear();
    return rv;
}

inline StrList &operator+=(StrList &lhs, const StrList &rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}
inline StrList &operator+=(StrList &lhs, StrList &&rhs) {
    lhs.insert(lhs.end(), 
        std::make_move_iterator(rhs.begin()), 
        std::make_move_iterator(rhs.end()));
    rhs.clear();
    return lhs;
}

inline StrSet operator+(const StrSet &lhs, const StrSet &rhs) {
    StrSet rv{lhs};
    rv.insert(rhs.begin(), rhs.end());
    return rv;
}
inline StrSet operator+(const StrSet &lhs, StrSet &&rhs) {
    StrSet rv{lhs};
    rv.insert(std::make_move_iterator(rhs.begin()), 
              std::make_move_iterator(rhs.end()));
    rhs.clear();
    return rv;
}

inline StrSet &operator+=(StrSet &lhs, const StrSet &rhs) {
    lhs.insert(rhs.begin(), rhs.end());
    return lhs;
}
inline StrSet &operator+=(StrSet &lhs, StrSet &&rhs) {
    lhs.insert(std::make_move_iterator(rhs.begin()), 
               std::make_move_iterator(rhs.end()));
    rhs.clear();
    return lhs;
}

template<typename T> StrList&
operator+=(StrList &lhs, std::initializer_list<T> &&rhs) {
    lhs.insert(lhs.end(), 
        std::make_move_iterator(rhs.begin()), 
        std::make_move_iterator(rhs.end()));
    return lhs;
}
template<typename T> StrSet&
operator+=(StrSet &lhs, std::initializer_list<T> &&rhs) {
    lhs.insert(std::make_move_iterator(rhs.begin()), 
               std::make_move_iterator(rhs.end()));
    return lhs;
}
