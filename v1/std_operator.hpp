#pragma once
#include <vector>
#include <string>
#include <unordered_set>
#include <initializer_list>

namespace cgnv1 {
    using SList = std::vector<std::string>;
    using SSet  = std::unordered_set<std::string>;
}

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

inline std::unordered_set<std::string> operator+(const std::unordered_set<std::string> &lhs, const std::unordered_set<std::string> &rhs) {
    std::unordered_set<std::string> rv{lhs};
    rv.insert(rhs.begin(), rhs.end());
    return rv;
}
inline std::unordered_set<std::string> operator+(const std::unordered_set<std::string> &lhs, std::unordered_set<std::string> &&rhs) {
    std::unordered_set<std::string> rv{lhs};
    rv.insert(std::make_move_iterator(rhs.begin()), 
              std::make_move_iterator(rhs.end()));
    rhs.clear();
    return rv;
}

inline std::unordered_set<std::string> &operator+=(std::unordered_set<std::string> &lhs, const std::unordered_set<std::string> &rhs) {
    lhs.insert(rhs.begin(), rhs.end());
    return lhs;
}
inline std::unordered_set<std::string> &operator+=(std::unordered_set<std::string> &lhs, std::unordered_set<std::string> &&rhs) {
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

template<typename T> std::unordered_set<std::string>&
operator+=(std::unordered_set<std::string> &lhs, std::initializer_list<T> &&rhs) {
    lhs.insert(std::make_move_iterator(rhs.begin()), 
               std::make_move_iterator(rhs.end()));
    return lhs;
}

// initializer_list<T> + vector<T>  (T deduced from vector side)
template<typename T> inline std::vector<T>
operator+(std::initializer_list<T> lhs, const std::vector<T> &rhs) {
    std::vector<T> rv(lhs);
    rv.insert(rv.end(), rhs.begin(), rhs.end());
    return rv;
}

// vector<T> + initializer_list<T>  (T deduced from vector side)
template<typename T> inline std::vector<T>
operator+(const std::vector<T> &lhs, std::initializer_list<T> rhs) {
    std::vector<T> rv(lhs);
    rv.insert(rv.end(), rhs.begin(), rhs.end());
    return rv;
}

// initializer_list<string> + initializer_list<string>
// Non-template: braced-init-lists cannot deduce T on their own, but they
// can implicitly construct std::string from const char*, so this overload
// covers {"a"} + {"b"} expressions.
inline std::vector<std::string>
operator+(std::initializer_list<std::string> lhs,
          std::initializer_list<std::string> rhs) {
    std::vector<std::string> rv(lhs);
    rv.insert(rv.end(), rhs.begin(), rhs.end());
    return rv;
}

// // vector<string> += single std::string
// inline std::vector<std::string>&
// operator+=(std::vector<std::string> &lhs, const std::string &rhs) {
//     lhs.push_back(rhs);
//     return lhs;
// }

// // vector<string> + single std::string
// inline std::vector<std::string>
// operator+(const std::vector<std::string> &lhs, const std::string &rhs) {
//     std::vector<std::string> rv(lhs);
//     rv.push_back(rhs);
//     return rv;
// }

// // single std::string + vector<string>
// inline std::vector<std::string>
// operator+(const std::string &lhs, const std::vector<std::string> &rhs) {
//     std::vector<std::string> rv{lhs};
//     rv.insert(rv.end(), rhs.begin(), rhs.end());
//     return rv;
// }
