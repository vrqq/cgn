// cgn.d internal
//
#pragma once
#include "configuration.h"

namespace cgnv1 {
class Graph;
struct GraphNode;


class ConfigurationManager {
public:
    struct KVRestriction {
        // std::string default_value;
        std::vector<std::string> opt_values;
    };

    void set_name(const std::string &name, const ConfigurationID &hash);

    // Get configuration by name
    // @return <Configuration*, adep> if found, otherwise <nullptr, nullptr>.
    std::pair<Configuration, GraphNode *>
    get(const std::string name) const;

    std::string get_name_id_mapper(const std::string &name) const;

    //Save configuration and write into .cfg file.
    // @return: the configuration unique_hash
    ConfigurationID commit(Configuration &cfg);

    // @param storage_dir: cgn-out/configurations
    ConfigurationManager(const std::string &storage_dir, Graph *g);

private:
    struct CDataRef {
        using CfgData = std::unordered_map<std::string, std::string>;
        const CfgData *rec_visited;
        int   hash_hlp;
        bool operator==(const CDataRef &rhs) const {
            if (rec_visited == rhs.rec_visited)
                return true;
            return hash_hlp == rhs.hash_hlp && *rec_visited == *rhs.rec_visited;
        }
        bool operator!=(const CDataRef &rhs) const {
            return !(*this == rhs);
        }
        CDataRef(const CfgData *ptr, int hash_hlp) : rec_visited(ptr), hash_hlp(hash_hlp) {}
        CDataRef(const Configuration *cfg) : rec_visited(&cfg->_data->visited), hash_hlp(cfg->_data->hash_hlp) {}
    };
    struct CHasher {
        size_t operator()(const CDataRef &ref) const {
            return ref.rec_visited->size() ^ (size_t)(~ref.hash_hlp);
        }
    };

    //cfg_indexs[CDataRef{&cfg_hashs[NAME].data}] = NAME
    std::unordered_map<std::string, Configuration*>    named_cfgs;
    std::unordered_map<ConfigurationID, Configuration> cfg_hashs;
    std::unordered_map<CDataRef, std::string, CHasher> cfg_indexs;
    
    std::unordered_map<std::string, KVRestriction> kv_restrictions;

    const std::string storage_dir;

    Graph *graph;

    std::string get_hash(size_t want);

}; //class ConfigurationManager


}