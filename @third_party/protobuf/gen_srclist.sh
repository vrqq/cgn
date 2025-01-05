#!/bin/bash

# Define the input and output files
input_file="repo/src/file_lists.cmake"
output_file="protobuf_src_list.cgn.h"

# Check if the input file exists
if [[ ! -f $input_file ]]; then
    echo "Error: Input file '$input_file' not found."
    exit 1
fi

# Start writing to the output C++ file
echo "// Generated C++ source code from file_lists.cmake" > $output_file
echo "// This file contains std::vector definitions generated from CMake 'set' commands" >> $output_file
echo "#include <vector>" >> $output_file
echo "#include <string>" >> $output_file
echo "" >> $output_file

# Function to convert CMake variable names to valid C++ variable names
convert_variable_name() {
    local var_name="$1"
    # Replace hyphens with underscores and convert to snake_case
    echo "$var_name" | sed -E 's/[-]/_/g; s/([A-Z])/_\L\1/g; s/^_//'
}

# Process the input file line by line
while read -r line; do
    # Detect the start of a 'set' command
    if [[ "$line" =~ ^set\(([A-Za-z0-9_-]+) ]]; then
        # Extract the variable name
        original_name="${BASH_REMATCH[1]}"
        # Convert to valid C++ variable name
        cpp_name=$(convert_variable_name "$original_name")

        # Start the vector definition in the output file
        echo "// Definition for $cpp_name" >> $output_file
        echo "std::vector<std::string> $cpp_name = {" >> $output_file
    elif [[ "$line" =~ \) ]]; then
        # End of the 'set' command
        echo "};" >> $output_file
        echo "" >> $output_file
    elif [[ "$line" =~ ^\s*# ]]; then
        # Skip comment lines
        continue
    else
        # Handle individual paths in the set command
        item=$(echo "$line" | sed -E 's/\$\{protobuf_SOURCE_DIR\}/./g' | xargs)
        if [[ -n "$item" ]]; then
            echo "    \"$item\"," >> $output_file
        fi
    fi
done < "$input_file"

echo "Conversion completed. Output written to $output_file."
