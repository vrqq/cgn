#include "cgn_impl.h"
#include "configuration.h"
#include "../cgn.h"


namespace cgn {

std::string CGN::get_filepath(const std::string &file_label) const
{
    return pimpl->expand_filelabel_to_filepath(file_label);
}

const CGNScript *CGN::active_script(const std::string &label)
{
    return &pimpl->active_script(label);
}

void CGN::offline_script(const std::string &label)
{
    return pimpl->offline_script(label);
}

CGNTarget CGN::analyse_target(
    const std::string &label, const Configuration &cfg
) {
    auto rv = pimpl->analyse_target(label, cfg);
    if (rv.infos.empty())
        throw std::runtime_error{"analyse: " + label + " not found."};
    return rv;
}

void CGN::build(const std::string &label, const Configuration &cfg)
{
    pimpl->build_target(label, cfg);
} //CGN::build()

ConfigurationID CGN::commit_config(const Configuration &plat_cfg)
{
    return pimpl->cfg_mgr->commit(plat_cfg);
}

const Configuration *CGN::query_config(const std::string &name) const
{
    return pimpl->cfg_mgr->get(name);
}

void CGN::add_adep_edge(GraphNode *early, GraphNode *late)
{
    return pimpl->add_adep(early, late);
}

std::shared_ptr<void> CGN::bind_factory_part2(
    const std::string &label, CGNFactoryLoader loader
) {
    return pimpl->bind_target_factory(label, loader);
}

void CGN::init(const std::unordered_map<std::string, std::string> &kvargs)
{
    pimpl = new CGNImpl;
    pimpl->init(kvargs);
}

const std::unordered_map<std::string, std::string> &CGN::get_kvargs() const
{
    return pimpl->cmd_kvargs;
}

CGN::~CGN()
{
    if (pimpl)
        delete pimpl;
}

// bool CGN::clean_all()
// {
//     std::filesystem::path dir{pimpl->cmd_kvargs["cgn-out"]};
//     if (std::filesystem::exists(dir / ".cgn_out_root.stamp")){
//         std::filesystem::remove_all(dir);
//         return true;
//     }
//     return false;
// }

}; //namespace

// void cgn_setup(CGNInitSetup &x) {}