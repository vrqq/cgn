#!/bin/sh

target_file=sources.cgn.h

cp repo/gen/sources.gni $target_file

# commit '#' => '//'
sed -i 's/^#\s*/\/\/&/' $target_file

# add include
sed -i '1i#include<vector>\n#include<string>\n' $target_file

# define variable 'var=[' => 'std::vector<std::string> var={'
# sed -i 's/^([a-zA-Z0-9_]+)\s*=\s*\[/std::vector<std::string> \1 = {/' $target_file
sed -Ei 's/^([a-zA-Z0-9_]+)\s*=\s*\[/std::vector<std::string> \1 = {/' $target_file

# ']' => '};'
sed -i 's/\]/};/' $target_file
