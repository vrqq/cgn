#include <filesystem>
#include "ninja_file.h"

static std::string varlist_to_string(
    const std::unordered_map<std::string, std::string> &ls
) {
    std::string rv;
    for (auto [k, v] : ls)
        rv += "\n    " + NinjaFile::escape_path(k) 
            + " = " + NinjaFile::escape_path(v);
    return rv;
}

std::string NinjaFile::BuildSection::to_string() {
    if (outputs.empty() || rule.empty())
        throw std::runtime_error{"NinjaFile: empty outputs or rule."};
    std::string rv = "build ";
    for (auto &it: outputs)
        rv += it + " ";
    if (implicit_outputs.size()) {
        rv += "|";
        for (auto &it: implicit_outputs)
            rv += it + " ";
    }

    rv += ": " + rule + " ";
    for (auto &it: inputs)
        rv += it + " ";
    if (implicit_inputs.size()) {
        rv += "|";
        for (auto &it: implicit_inputs)
            rv += it + " ";
    }
    if (order_only.size()) {
        rv += "||";
        for (auto &it: order_only)
            rv += it + " ";
    }
    return rv + varlist_to_string(variables);
}

std::string NinjaFile::RuleSection::to_string() {
    return "rule " + name 
        + "\n    command = " + command
        + varlist_to_string(variables);
}

std::string NinjaFile::CommentSection::to_string() {
    if (word_warp < 0)
        return comment;
    auto escape = [](char c) { return c==' ' || c=='\n' || c=='\t'; };

    std::string rv, line;
    auto commit = [&]() {
        if (rv.size())
            rv += "\n";
        rv += "# " + line;
        line.clear();
    };
    for (int i=0, j=0; i < comment.size(); i = ++j) {
        while(!escape(comment[j]) && j<comment[j])
            j++;
        if (line.size() && line.size() + j-i > word_warp)
            commit();
        line += comment.substr(i, j-i+1);
    }
    if (line.size())
        commit();
    return rv;
}

std::string NinjaFile::GlobalVariable::to_string() {
    return escape_path(key) + " = " + escape_path(value);
}

NinjaFile::BuildSection *NinjaFile::append_build() {
    return append_section<BuildSection>();
}

NinjaFile::RuleSection *NinjaFile::append_rule() {
    return append_section<RuleSection>();
}

NinjaFile::CommentSection *NinjaFile::append_comment(const std::string &comment) {
    auto ptr = append_section<CommentSection>();
    return ptr->comment = comment, ptr;
}

NinjaFile::IncludeSection *NinjaFile::append_include(const std::string &file) {
    auto ptr = append_section<IncludeSection>();
    return ptr->file = file, ptr;
}

NinjaFile::Subninja *NinjaFile::append_subninja(const std::string &file) {
    auto ptr = append_section<Subninja>();
    return ptr->file = file, ptr;
}

NinjaFile::GlobalVariable *
NinjaFile::append_variable(const std::string &k, const std::string &v) {
    auto ptr = append_section<GlobalVariable>();
    return ptr->key = k, ptr->value = v, ptr;
}

template<typename T> T *NinjaFile::append_section() {
    return (T*)sections.emplace_back(new T).get();
}

void NinjaFile::flush() {
    std::filesystem::path p(filepath);
    if (std::filesystem::exists(p.parent_path()) == false)
        std::filesystem::create_directories(p.parent_path());
    std::ofstream fout(p);
    if (fout)
        for (auto &sect : sections)
            fout<<sect->to_string()<<"\n\n";
}

std::string NinjaFile::parse_ninja_str(const std::string &in)
{
    std::string rv;
    for (std::size_t i=0; i<in.size(); i++)
        if (in[i] == '$' && (in[i+1] == '\n' || in[i+1] == ' ' || in[i+1] == '$'))
            rv.push_back(in[++i]);
        else
            rv += in[i];
    return rv;
}

std::string NinjaFile::escape_path(const std::string &in)
{
    std::string out;
    for (auto ch : in)
        if (ch == '\n' || ch == ' ' || ch == '$')
            out += "$" + std::string{ch};
        else
            out += ch;
    return out;
}

NinjaFile::NinjaFile(const std::string &filepath) : filepath(filepath) {}

NinjaFile::~NinjaFile() { flush(); }