#pragma once
#include <unordered_set>
#include <list>
#include <string>
// #include <functional>

// template<typename T> inline std::function<T()> process(T data) {

//     return [data](){
//         T v = data;
//         for (T i=0; i<100; i++)
//             v += i;
//         return v;
//     };
// }

template<typename T> void process_string(std::unordered_set<T> &data)
{
    std::list<T> helper;
    for (auto iter = data.begin(); iter != data.end(); iter++)
        helper.push_back(*iter + ".suffix");
    
    for (auto iter = helper.begin(); iter != helper.end(); iter++)
        for (size_t j=0; j<iter->size(); j++)
            *iter[j]++;
    
    for (auto iter = helper.begin(); iter != helper.end(); iter++)
        data.insert(*iter);
}

#ifdef ENABLE_SPECIALIZATION
template<> void process_string<std::string>(std::unordered_set<std::string> &);
// template<> std::function<int()> process<int>(int);
#endif
