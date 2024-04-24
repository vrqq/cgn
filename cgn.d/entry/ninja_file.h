// CGN public header, implement in cli
#pragma once
#include <fstream>
#include <string>
#include <memory>
#include <list>
#include <vector>
#include <unordered_map>

namespace cgn {

// All sections would not auto escape any string!
// see also //proof/ninja/syntax/build.ninja
class NinjaFile {
public:
    struct Section {
        virtual std::string to_string() = 0;
        virtual ~Section() {}
    };

    //
    // Build <outputs> | <implicit_outputs> : <rule> <inputs> | <implicit_inputs> || <order-only>
    //     [variables].key = [variables].value
    struct BuildSection final : Section {
        std::string rule;
        std::vector<std::string> 
            outputs, implicit_outputs, 
            inputs, implicit_inputs, 
            order_only;
        
        // variables["pool"]
        // variables["dyndep"]
        std::unordered_map<std::string, std::string> variables;

        virtual std::string to_string();
        virtual ~BuildSection() {}
    };

    struct RuleSection final : Section {
        std::string name, command;
        bool generator = false;

        // variables[]
        //      description, depfile, generator,
        //      pool, restat, rspfile, rspfile_content,
        //      deps
        std::unordered_map<std::string, std::string> variables;

        virtual std::string to_string();
        virtual ~RuleSection() {}
    };

    struct CommentSection final : Section {
        std::string comment;
        int word_warp = 80;
        virtual std::string to_string();
        virtual ~CommentSection() {}
    };

    struct IncludeSection final : Section {
        std::string file;
        virtual std::string to_string() { return "include " + file; }
        virtual ~IncludeSection() {}
    };

    struct Subninja final : Section {
        std::string file;
        virtual std::string to_string() { return "subninja " + file; }
        virtual ~Subninja() {}
    };

    struct GlobalVariable final : Section {
        std::string key;
        std::string value;
        virtual std::string to_string();
        virtual ~GlobalVariable() {}
    };

    BuildSection *append_build();
    RuleSection  *append_rule();
    CommentSection *append_comment(const std::string &comment = "");
    IncludeSection *append_include(const std::string &file = "");
    Subninja       *append_subninja(const std::string &file = "");

    //Two variables are significant when declared in the outermost file scope.
    //builddir and ninja_required_version
    GlobalVariable *append_variable(const std::string &k="", const std::string &v="");

    void flush();

    static std::string parse_ninja_str(const std::string &in);

    static std::string escape_path(const std::string &in);
    
    static std::vector<std::string> escape_path(
        const std::vector<std::string> &in
    );

    // static std::string escape(const std::vector<std::string> &in);

    NinjaFile(const std::string &filepath);// : filepath(filepath) {}

    ~NinjaFile();// { flush(); }

    // void set_builddir(const std::string &in) {
    //     builddir = in;
    // }
    // void set_ninja_required_version(const std::string &in) {
    //     ninja_required_version = in;
    // }

private:
    const std::string filepath;
    std::list<std::unique_ptr<Section>> sections;
    template<typename T> T *append_section();

    // std::string builddir;
    // std::string ninja_required_version;
};

} //namespace