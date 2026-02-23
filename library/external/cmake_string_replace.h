// Simple cmake variable string replace
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

// Simple cmake variable string replace, support both ${VAR} and $<VAR> format, without eval any expression.
// @param dict: the dict for variable replace, key should be in format of "${VAR}" or "$<VAR>".
// @param input: the input string to be replaced, should be in format of "abc${VAR}def" or "abc$<VAR>def".
// @return: the replaced string, if the input string is invalid or the variable is not found in dict, an exception will be thrown.
static std::string cmake_string_replace(
    const std::unordered_map<std::string, std::string> &dict, 
    const std::string &input
) {
    std::string result;
    result.reserve(input.size());

    constexpr std::size_t flag_angle_branket = (1UL<<63);
    std::vector<std::size_t> bracket_pos;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\'){
            if (i + 1 >= input.size())
                throw std::runtime_error{"invalid cmake string(end with \\):" + input};
            result.push_back(input[i+1]);
            i++;
            continue;
        }
        if (input[i] == '$' && i+1 < input.size()) {
            if (input[i+1] == '{')
                bracket_pos.push_back(i);
            else if (input[i+1] == '<')
                bracket_pos.push_back(i | flag_angle_branket);
            else
                throw std::runtime_error{"invalid cmake string(invalid $expression): " + input};
        }
        else if (input[i] == '}' || input[i] == '>') {
            if (bracket_pos.empty())
                throw std::runtime_error{"invalid cmake string(missing left bracket): " + input};
            if (input[i] == '>' && (bracket_pos.back() & flag_angle_branket) == 0)
                throw std::runtime_error{"invalid cmake string(missing <): " + input};
            if (input[i] == '}' && (bracket_pos.back() & flag_angle_branket) != 0)
                throw std::runtime_error{"invalid cmake string(missing {): " + input};
            
            // if that is the last bracket.
            if (bracket_pos.size() == 1) {
                std::size_t start = bracket_pos[0] & ~flag_angle_branket;
                std::string strkey = input.substr(start, i-start+1);
                auto fd = dict.find(strkey);
                if (fd != dict.end())
                    result += fd->second;
                else
                    throw std::runtime_error{"invalid cmake string(" + strkey + "): " + input};
            }
            bracket_pos.pop_back();
        }
        else if (bracket_pos.empty())
            result.push_back(input[i]);
    }
    return result;
}