#include "../cgn.h"
#include "cgn_impl.h"

void CGN::load_cgn_script(const std::string &label)
{
    pimpl->load_cgn_script(label);
}

void CGN::unload_cgn_script(const std::string &label)
{
    pimpl->unload_cgn_script(label);
}

TargetInfos CGN::analyse(
    const std::string &tf_label, 
    const Configuration &cfg
) {
    return pimpl->analyse(tf_label, cfg);
}

#ifdef _WIN32
    #include <sysinfoapi.h>
#elif __linux__
    #include <sys/utsname.h>
#endif
HostInfo CGN::get_host_info()
{
    HostInfo rv;
    #ifdef _WIN32
        // https://stackoverflow.com/questions/47023477/how-to-get-system-information-in-windows-with-c
        rv.os = "win";
        SYSTEM_INFO siSysInfo;
        GetSystemInfo(&siSysInfo); 
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            rv.cpu = "x86_64";
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM)
            rv.cpu = "arm";
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
            rv.cpu = "arm64";
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64)
            rv.cpu = "ia64";
        if (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
            rv.cpu = "x86";
    #elif __APPLE__
        rv.os = "mac";
    #elif __linux__
        rv.os = "linux";
        struct utsname osInfo{};
        uname(&osInfo);
        rv.cpu = osInfo.machine;
    #endif
    return rv;
}

//$ : Variable substitution.
// # : Comment.
// & : Background job.
// * : Matches any string in filename expansion.
// ? : Matches any single character in filename expansion.
// ; : Command separator.
// | : Pipe, command chaining.
// > : Redirect output.
// < : Redirect input.
// () : Command grouping.
// {} : Command block.
// [] : Character classes in filename expansion.
// ‘ : Command substitution.
// “ : Partial quote.
// ‘ : Full quote.
// ~ : Home directory.
// ! : History substitution.
// \ : Escape character.
constexpr auto gen_base_special() {
    std::array<bool, 256> rv{0};
    for (std::size_t i=0; i<rv.size(); i++) rv[i] = 0;
    rv['$'] = rv['#'] = rv['&'] = rv['*'] = rv['?'] = rv['|'] = rv['>'] 
    = rv['<'] = rv['('] = rv[')'] = rv['{'] = rv['}'] = rv['['] = rv[']']
    = rv['\''] = rv['"'] = rv['~'] = rv['!'] = rv['\\'] = rv[' '] = 1;
    return rv;
}

//TODO
// https://github.com/diatche/shell-encode
std::string CGN::shell_escape(const std::string &in)
{
    constexpr static auto bash_special = gen_base_special();

    std::string rv;
    #ifdef _WIN32
        #error "TODO!"
    #else
        for (auto c: in)
            if (bash_special[c])
                rv.append({' ', c});
            else
                rv.push_back(c);
    #endif
    return rv;
}

std::vector<std::string> CGN::get_path(const std::string &label)
{
    return pimpl->get_script_filepath(label);
}

std::string CGN::get_output_folder(
    const std::string &tf_label, 
    ConfigurationID cfg_hash_id
) {
    std::string rv = pimpl->get_target_dir(tf_label, cfg_hash_id).fout_prefix;
    return rv.pop_back(), rv;
}

ConfigurationID CGN::commit_config(const Configuration &plat_cfg)
{
    return pimpl->cfg_mgr->commit(plat_cfg);
}

const Configuration *CGN::query_config(const std::string &name)
{
    return pimpl->cfg_mgr->get(name);
}

void CGN::register_ninjafile(const std::string &ninja_file_path)
{
    pimpl->register_ninjafile(ninja_file_path);
}

std::shared_ptr<void> CGN::bind_factory_part2(
    const std::string &label,
    std::function<TargetInfos(const Configuration&, TargetOpt)> fn_apply
) {
    return pimpl->auto_target_factory(label, fn_apply);
}

CGN::CGN(CGNImpl *impl)
: cmd_kvargs(impl->cmd_kvargs), pimpl(impl) {}
