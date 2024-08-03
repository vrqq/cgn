#pragma once
#include <string>

namespace cgn {

    inline std::string _elem2str(const std::string &in) {
        return in + ", ";
    };
    inline std::string _elem2str(const std::pair<std::string, std::string> &in) {
        return in.first + ": " + in.second + ", ";
    }

    template<typename Type1> std::string 
    list2str_h(const Type1 &ls, const std::string &indent, std::size_t maxlen = 5)
    {

        using Type = typename std::decay<Type1>::type;
        std::string ss;
        auto iter = ls.begin();
        for (size_t i=0; i<ls.size() && i<maxlen; i++, iter++) {
            if (!ss.empty())
                ss += "\n" + indent;
            ss += _elem2str(*iter);
            // if constexpr(std::is_same<std::vector<std::string>, Type>::value)
            //     ss += elem2str(*iter);
            // else
            //     ss += iter->first + ": " + iter->second + ", ";
        }
        ss += "(" + std::to_string(ls.size()) + " element" + (ls.size()>1? "s)":")");
        return ss;
    };

} //namespace cgn