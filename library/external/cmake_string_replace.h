// Simple CMake variable string replacement utility.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

// Simple CMake variable string replacement. Supports both ${VAR} and $<VAR> forms.
// Does not evaluate expressions.
// @param dict: mapping from token (including delimiters) to replacement, e.g. "${VAR}" -> "value".
// @param input: input string containing tokens like "abc${VAR}def" or "abc$<VAR>def".
// @return: the replaced string. Throws std::runtime_error on malformed input or missing variables.
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
                throw std::runtime_error{"invalid CMake string(end with \\):" + input};
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
                throw std::runtime_error{"invalid CMake string(invalid $expression): " + input};
        }
        else if (input[i] == '}' || input[i] == '>') {
            if (bracket_pos.empty())
                throw std::runtime_error{"invalid CMake string(missing opening bracket): " + input};
            if (input[i] == '>' && (bracket_pos.back() & flag_angle_branket) == 0)
                throw std::runtime_error{"invalid CMake string(expected '<' before closing '>' in): " + input};
            if (input[i] == '}' && (bracket_pos.back() & flag_angle_branket) != 0)
                throw std::runtime_error{"invalid CMake string(missing {): " + input};
            
            // if that is the last bracket.
            if (bracket_pos.size() == 1) {
                std::size_t start = bracket_pos[0] & ~flag_angle_branket;
                std::string strkey = input.substr(start, i-start+1);
                auto fd = dict.find(strkey);
                if (fd != dict.end())
                    result += fd->second;
                else
                    throw std::runtime_error{"Undefined CMake variable " + strkey + " in: " + input};
            }
            bracket_pos.pop_back();
        }
        else if (bracket_pos.empty())
            result.push_back(input[i]);
    }
    return result;
}