#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <initializer_list>
// using StrList = std::vector<std::string>;
using StrSet  = std::unordered_set<std::string>;

template<typename T> inline std::vector<T> 
operator+(const std::vector<T> &lhs, std::vector<T> &&rhs) {
    std::vector<T> rv{lhs};
    rv.insert(rv.end(), 
        std::make_move_iterator(rhs.begin()), 
        std::make_move_iterator(rhs.end()));
    rhs.clear();
    return rv;
}

template<typename T> inline std::vector<T> 
operator+(const std::vector<T> &lhs, const std::vector<T> &rhs) {
    std::vector<T> rv{lhs};
    rv.insert(rv.end(), rhs.begin(), rhs.end());
    return rv;
}

template<typename T> inline std::vector<T> &
operator+=(std::vector<T> &lhs, const std::vector<T> &rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}

template<typename T> inline std::vector<T> &
operator+=(std::vector<T> &lhs, std::vector<T> &&rhs) {
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

template<typename T> std::vector<T>&
operator+=(std::vector<T> &lhs, std::initializer_list<T> &&rhs) {
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
