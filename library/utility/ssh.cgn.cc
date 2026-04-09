#define CGN_UTILITY_SSH_IMPL
#include "ssh.cgn.h"

CGN_UTILITY_SSH cgn::CGNTarget SCPTransmit::context_type::add_files_from_output(
    const std::string &label, cgn::Configuration &cfg
) {
    auto tmp = quick_dep(label, cfg, false);
    if (tmp.errmsg.size())
        return tmp;
    for (auto it : tmp.outputs)
        local_files.push_back(cgn::make_path_base_working(it));
    return tmp;
}

CGN_UTILITY_SSH void SCPTransmit::interpret(SCPTransmit::context_type &x)
{
    if (x.local_files.empty() || x.remote_path.empty() || x.identity_file.empty())
        return x.opt->set_fail("Missing fields");

    // host_os for rule 'win_run_and_stamp' or 'unix_run_and_stamp'
    std::string ninja_rule = (x.cfg["host_os"]=="win"? "win_run_and_stamp" : "unix_run_and_stamp");

    auto *mk = x.opt->confirm();
    if (!mk)
        return;

    std::vector<std::string> src_files;
    for (auto it : x.local_files)
        src_files.push_back(api.rebase_path(it, ".", mk));

    mk->outputs = src_files;

    // skip if ninja file unchanged
    if (mk->ninja == nullptr)
        return ;


    // Make ninja targets
    mk->ninja->append_include(api.get_filepath("@cgn.d//library/utility/quick_run.ninja"));

    std::string scp_args;
    if (x.enable_compress)
        scp_args += "-C ";
    if (!x.identity_file.empty())
        scp_args += "-i " + api.rebase_path(x.identity_file, "", mk) + " ";

    // foreach src in src_files[] write a command:
    // 'scp -i ${x.identity_file} $src $remote_path'
    auto *entry = mk->ninja->append_build();
    entry->rule = "phony";
    entry->outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry)};

    std::size_t i=0;
    for (auto src: src_files) {
        auto *field = mk->ninja->append_build();
        field->inputs = {cgn::NinjaFile::escape_path(src)};
        field->outputs = {cgn::NinjaFile::escape_path(mk->out_prefix + std::to_string(i++) + ".stamp")};
        field->rule = ninja_rule;
        field->variables["exe"] = std::string{"scp"} + (x.cfg["host_os"]=="win"?".exe":"");
        field->variables["arg0"] = cgn::NinjaFile::escape_path(scp_args);
        field->variables["args"] = cgn::NinjaFile::escape_path(x.remote_path);
        field->variables["desc"] = "SCP " + src + " " + x.remote_path;

        entry->inputs += field->outputs;
    }

} //interpreter()