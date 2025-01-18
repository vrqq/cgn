// copied from https://source.chromium.org/chromium/chromium/src/+/main:third_party/nasm/nasm_assemble.gni
#define NASM_CGN_IMPL
#include "nasm.cgn.h"

void NasmInterpreter::interpret(context_type &x)
{
    cgn::CGNTarget nasm = x.opt->quick_dep_namedcfg("@third_party//nasm:nasm", "host_release");
    if (nasm.errmsg.size()) {
        x.opt->confirm_with_error(nasm.errmsg);
        return ;
    }

    x.include_dirs += {"."};

    std::vector<std::string> &flags = x.nasm_flags;

    // cross compile
    std::string obj_ext;
    if (x.cfg["os"] == "mac") {
        if (x.cfg["cpu"] == "x86")
            flags += {"-fmacho32"};
        if (x.cfg["cpu"] == "x64")
            flags += {"-fmacho64"};
        
        flags += {"--macho-min-os=macos10.5"};
        obj_ext = ".o";
    }
    else if (x.cfg["os"] == "linux" && x.cfg["cpu"] == "x86") {
        flags += {"-felf32"};
        obj_ext = ".o";
    }
    else if (x.cfg["os"] == "linux" && x.cfg["cpu"] == "x86_64") {
        flags += {"-DPIC", "-felf64"};
        obj_ext = ".o";
    }
    else if (x.cfg["os"] == "win" && x.cfg["cpu"] == "x86") {
        flags += {"-DPREFIX", "-fwin32"};
        obj_ext = ".obj";
    }
    else if (x.cfg["os"] == "win" && x.cfg["cpu"] == "x86_64") {
        flags += {"-fwin64"};
        obj_ext = ".obj";
    }
    else {
        x.opt->confirm_with_error("unsupported target OS and CPU");
        return ;
    }

    if (x.cfg["optimization"] == "debug")
        flags += {"-O0"}; // "-g" may cause nasm.exe halt?
    else
        flags += {"-Ox"};

    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // helper func
    auto twoesc = [](const std::string &in){ 
        return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
    };

    // # NASM does not append path separators when processing the -I flags, so
    // # -Ifoo means includes of bar look up "foobar" rather than "foo/bar".
    // # Add the trailing slash for it.
    for (auto dir : x.include_dirs)
        flags += {"-I" + api.rebase_path(dir, ".", x.opt->src_prefix) + opt->path_separator};
    
    for (auto def : x.defines)
        flags += {"-D" + def};
    
    opt->ninja->append_include(api.get_filepath("@third_party//nasm/nasm_rule.ninja"));
    
    //convert flags to string
    std::string njflags;
    for (auto it : flags)
        njflags += twoesc(it) + " ";
    opt->ninja->append_variable("nasmflags", njflags);

    std::vector<std::string> njtargets, objrv;
    for (auto it : x.srcs) {
        std::string src = api.rebase_path(it, ".", x.opt->src_prefix);
        std::string dst = api.mangle_path(src, opt->out_prefix);
        std::string ext = api.extension_of_path(dst);
        if (ext.size())
            dst = dst.substr(0, dst.size() - ext.size());
        dst += obj_ext;
        if (src[0] == '@')
            src = "." + opt->path_separator + src;
        if (dst[0] == '@')
            dst = "." + opt->path_separator + dst;

        auto *field = opt->ninja->append_build();
        field->rule = "run_gccdep";
        field->inputs = {opt->ninja->escape_path(nasm.outputs[0])};
        field->implicit_inputs = {opt->ninja->escape_path(src)};
        field->outputs = {opt->ninja->escape_path(dst)};
        field->variables["args"] = 
            "-MD " + twoesc(dst + ".d") + " ${nasmflags} -o " + twoesc(dst) + " " + twoesc(src);
        field->implicit_inputs += opt->quickdep_ninja_full;
        field->order_only      += opt->quickdep_ninja_dynhdr;
        njtargets.push_back(field->outputs[0]);
        objrv.push_back(dst);
    }

    auto *phony = opt->ninja->append_build();
    phony->rule    = "phony";
    phony->inputs  = njtargets;
    phony->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};

    auto *lrinfo = opt->result.get<cgn::LinkAndRunInfo>(true);
    lrinfo->object_files = objrv;
}