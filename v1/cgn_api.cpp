
#include "cgn_impl.h"
#include "cgn_api.h"

namespace cgnv1
{

std::string CGN::get_filepath(const std::string &file_label) const
{
    return pimpl->expand_filelabel_to_filepath(file_label);
}

// Clear all mtime cache, rescan all files to check which is changed and reload them.
void CGN::start_new_round()
{
    return pimpl->start_new_round();
}

// load CGNScript, auto rebuild if necessary.
std::pair<GraphNode*, std::string>
CGN::active_script(const std::string &label)
{
    return pimpl->active_script(label);
}

// unload CGNScript, it's safe to delete dll file after return.
void CGN::offline_script(const std::string &label)
{
    return pimpl->offline_script(label);
}

// Analyse specific target
CGNTarget CGN::analyse_target(
    const std::string &label, const Configuration &cfg
) {
    return pimpl->analyse_target(label, cfg);
}

// Build specific target
void CGN::build(const std::string &label, const Configuration &cfg)
{
    return pimpl->build_target(label, cfg);
}

// ConfigurationID commit_config(const Configuration &plat_cfg);

// Query named configuration assigned in cgn_setup.cgn.cc
std::pair<const Configuration *, GraphNode *>
CGN::query_config(const std::string &name) const
{
    return pimpl->cfg_mgr->get(name);
}

void CGN::add_adep_edge(GraphNode *early, GraphNode *late)
{
    return pimpl->add_adep(early, late);
}

std::shared_ptr<void> CGN::bind_target_builder(
    const std::string &factory_label, 
    std::function<void(CGNTargetOptIn*)> loader
) {
    return pimpl->bind_target_builder(factory_label, loader);
}

// Commit from CGNTargetOptIn.confirm();
CGNTargetOpt *CGN::confirm_target_opt(CGNTargetOptIn *in)
{
    return pimpl->confirm_target_opt(in);
}

// The init function must be called before others.
void CGN::init(const std::unordered_map<std::string, std::string> &kvargs)
{
    if (pimpl)
        throw std::runtime_error{"Inited"};
    pimpl = (CGNImpl*)::operator new (sizeof(CGNImpl));
    new(pimpl) CGNImpl(kvargs);
    logger = &(pimpl->logger);
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

CGN::~CGN()
{
    release();
}

} // namespace cgnv1


CGN_EXPORT cgnv1::CGN api;