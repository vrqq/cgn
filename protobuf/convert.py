
import re
import sys

def convert_cmake_to_cpp(cmake_file_path, cpp_output_path):
    with open(cmake_file_path, 'r') as file:
        lines = file.readlines()

    cpp_code_lines = []
    capturing = False
    var_name = ""
    collected_lines = []

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("set(") and not capturing:
            capturing = True
            match = re.match(r"set\(([-\w]+)", stripped)
            if match:
                var_name = match.group(1)
                cpp_var_name = var_name.replace("-", "_")
                collected_lines = []
                rest = stripped[len(f"set({var_name}"):].strip()
                if rest.endswith(")"):
                    rest = rest[:-1].strip()
                    if rest:
                        collected_lines.append(rest)
                    capturing = False
        elif capturing:
            if stripped == ")":
                capturing = False
            else:
                collected_lines.append(stripped)

        if not capturing and var_name and collected_lines:
            cpp_var_name = var_name.replace("-", "_")
            cpp_var = f"std::vector<std::string> {cpp_var_name} = {{"
            formatted_lines = []
            for line in collected_lines:
                line = re.sub(r"\$\{([-\w]+)\}", lambda m: m.group(1).replace("-", "_") + ' + "', line.strip())
                formatted_lines.append(f'    {line}",')
            cpp_var += "\n" + "\n".join(formatted_lines) + "\n};\n"
            cpp_code_lines.append(cpp_var)
            var_name = ""
            collected_lines = []

    with open(cpp_output_path, 'w') as f:
        f.write("#include<vector>\n#include<string>\n")
        f.write("static std::string protobuf_SOURCE_DIR = \"\";\n\n")
        f.write("\n".join(cpp_code_lines))

if __name__ == "__main__":
    convert_cmake_to_cpp("repo/src/file_lists.cmake", "file_list.cgn.h")
