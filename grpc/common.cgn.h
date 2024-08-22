#pragma once
#include <vector>
#include <string>

std::string repo_dir = "repo";

static std::vector<std::string> 
add_prefix(const std::string &prefix, std::vector<std::string> in) {
    for (auto &it : in)
        it = prefix + it;
    return in;
}