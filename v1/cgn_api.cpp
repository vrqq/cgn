
#include "cgn_impl.h"
#include "cgn_api.h"

namespace cgnv1
{

std::string CGN::get_filepath(const std::string &file_label) const
{
    return pimpl->expand_filelabel_to_filepath(file_label);
}

// Clear all mtime cache, rescan all files to check which is changed and reload them.
// void CGN::start_new_round()
// {
//     return pimpl->start_new_round();
// }

// load CGNScript, auto rebuild if necessary.
std::pair<GraphNode*, std::string>
CGN::active_script(const std::string &label)
{
    return pimpl->active_script(label);
}

// unload CGNScript, it's safe to delete dll file after return.
std::string CGN::offline_script(const std::string &label)
{
    return pimpl->offline_script(label);
}

std::string CGN::add_factory(
    const std::string &factory_label,
    std::function<void(CGNTargetOpt*)> loader
) {
    return pimpl->add_factory(factory_label, loader);
}

std::string CGN::remove_factory(
    const std::string &factory_label
) {
    return pimpl->remove_factory(factory_label);
}

// Analyse specific target
CGNTarget CGN::create_target(
    const std::string &label, const Configuration &cfg
) {
    return pimpl->create_target(label, cfg);
}

CGNTarget CGN::create_target(
    CGNTargetOpt *opt_in, const std::function<void(CGNTargetOpt*)> &loader
) {
    return pimpl->create_target(opt_in, loader);
}

// Build specific target
std::string CGN::build(const std::string &label, const Configuration &cfg)
{
    auto [tgt, code] = pimpl->create_and_build_target(label, cfg);
    if (tgt.errmsg.size())
        throw std::runtime_error{tgt.errmsg};
    return (tgt.outputs.size() && code == 0)?tgt.outputs[0]:"";
}


// ConfigurationID commit_config(const Configuration &plat_cfg);

// Query named configuration assigned in cgn_setup.cgn.cc
std::pair<Configuration, GraphNode *>
CGN::query_config(const std::string &name) const
{
    auto [cfg, anode] = pimpl->cfg_mgr->get(name);
    if (pimpl->tls_runtime)
        pimpl->tls_runtime->dep_anodes.insert(anode);
    return {cfg, anode};
}

void CGN::add_adep_edge(GraphNode *early, GraphNode *late)
{
    return pimpl->add_adep(early, late);
}

// The init function must be called before others.
void CGN::init(const std::unordered_map<std::string, std::string> &kvargs)
{
    if (pimpl)
        throw std::runtime_error{"Inited"};

    // https://timsong-cpp.github.io/cppwp/n4659/expr.new#19
    // If any part of the object initialization throws an exception, the allocation
    // function’s deallocation function is called (if present).
    // … Thus, when the constructor of an object created by a new expression throws
    // an exception, the matching deallocation function is called to release the
    // storage.
    pimpl = (CGNImpl*)::operator new (sizeof(CGNImpl));
    try {
        new(pimpl) CGNImpl(kvargs);
        logger = &(pimpl->logger);
    }catch(const std::exception &e) {
        ::operator delete(pimpl);
        pimpl = nullptr;
        throw ; // rethrow with the previous dynamic type.
    }
}

// Make sure to call this function prior to ~CGN(), as the CGN API is an 
// exported global variable that will be automatically deleted when the 
// main-exe exits. Even though there are still many DLLs loaded, the static
// variables within them can still call the API and SymbolTable during DLL
// auto unload. 
// Since the destruction order is not guaranteed, it is necessary to call 
// release() in order to unload all DLLs before the program exits.
void CGN::release()
{
    if (pimpl){
        pimpl->~CGNImpl();
        ::operator delete(pimpl);
    }
    pimpl = nullptr;
}

// Return kvargs assigned from init().
const std::unordered_map<std::string, std::string> &CGN::get_kvargs() const
{
    return pimpl->cmd_kvargs;
}

const TLRuntime *CGN::get_debug_runtime() const
{
    return pimpl->tls_runtime;
}

std::string CGN::get_cgn_binary_mirror_path() const
{
    return pimpl->cgn_exe_shadow.string();
}

CGN::~CGN()
{
    release();
}

} // namespace cgnv1


CGN_EXPORT cgnv1::CGN api;