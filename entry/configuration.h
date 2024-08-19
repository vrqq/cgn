// CGN public header, implement in cli
// * Rename a configuration would not make target recompile,
//   because the content unchanged.
// * The config name only get from CGN-CELL-SETUP.
// * A config without name assigned wouid still keep in output,
//   and the last build result would also kept.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <stdexcept>
#include "../api_export.h"

namespace cgn {

using ConfigurationID = std::string;
class Graph;
struct GraphNode;

class Configuration
{
    using type_data = std::unordered_map<std::string, std::string>;
public:
    struct VisitRec {
        bool all = false;
        std::unordered_set<std::string> item;
    };

    //typename V: std::string or const std::string
    template<typename V> struct Cell {
        const std::string key;

        const std::string &operator=(const std::string &v) {
            if (ptr->flag_lock)
                throw std::runtime_error{"Configuration locked."};
            if (v.empty()) {
                if (!pvalue)
                    return v;
                
                ptr->hash_hlp -= (int)((*pvalue)[0]<<(pvalue->size()%20));;
                pvalue = nullptr;
                ptr->data.erase(key);
                ptr->hashid.clear();
            }
            else {
                if (pvalue && *pvalue == v)
                    return *pvalue;

                if (pvalue) {
                    *pvalue = v;
                    ptr->hash_hlp -= (int)((*pvalue)[0]<<(pvalue->size()%20));
                }
                else
                    *(pvalue = &ptr->data[key]) = v;
                ptr->hash_hlp += (int)(v[0]<<(v.size()%20));
                ptr->hashid.clear();
            }
            return v;
        }

        operator const std::string&() const {
            return pvalue? *pvalue : empty_string;
        }
        bool operator==(const std::string &rhs) const {
            return pvalue? (*pvalue == rhs) : rhs.empty();
        }
        bool operator!=(const std::string &rhs) const {
            return !(*this == rhs);
        }

        template<typename RV> bool operator==(const Cell<RV> &rhs) const {
            if (pvalue && rhs.pvalue)
                return *pvalue == *rhs.pvalue;
            return pvalue == rhs.pvalue;
        }

        Cell(Configuration *ptr, const std::string &key, V *pvalue)
        : ptr(ptr), key(key), pvalue(pvalue) { }

    private:
        Configuration *ptr;
        V *pvalue;  //nullptr or pointer to data in hash-table.
    }; //end struct Cell
    using type_const_cell = Cell<const std::string>;
    using type_cell = Cell<std::string>;

    type_cell operator[](const std::string &key) {
        rec_one(key);
        return type_cell(this, key, detect_value(key));
    }
    const type_const_cell operator[](const std::string &key) const { 
        rec_one(key);
        return type_const_cell(nullptr, key, detect_value(key));
    }

    bool operator==(const Configuration &rhs) const {
        rec_all();
        if (hashid.size() && rhs.hashid.size())
            return hashid == rhs.hashid;
        return data == rhs.data;
    }

    void lock() { flag_lock = true; }
    ConfigurationID get_id() const { return hashid; }

    std::size_t size() const { return data.size(); }
    std::size_t count(const std::string &key) const { return data.count(key); }

    // only const iterator allowed: using Cell to modify data.
    // TEMP Solution: The function inherit from .data
    // OnRoad: iterator<cell / const_cell>
    using iterator = type_data::iterator;
    using const_iterator = type_data::const_iterator;
    const_iterator begin()  const { rec_all(); return data.cbegin(); }
    const_iterator end()    const { rec_all(); return data.cend(); }
    const_iterator cbegin() const { rec_all(); return data.cbegin(); }
    const_iterator cend()   const { rec_all(); return data.cend(); }

    Configuration() {}

    Configuration(const Configuration &rhs)
    : hashid(rhs.hashid), hash_hlp(rhs.hash_hlp), 
      data(rhs.data), visit_rec(rhs.visit_rec) {}

    Configuration &operator=(const Configuration &rhs) {
        hashid = rhs.hashid, hash_hlp = rhs.hash_hlp, data = rhs.data;
        return *this;
    }

    Configuration(Configuration &&rhs)
    : hashid(std::move(rhs.hashid)), hash_hlp(rhs.hash_hlp), 
      data(std::move(rhs.data)), visit_rec(rhs.visit_rec) {}

    Configuration &operator=(Configuration &&rhs) {
        hashid = std::move(rhs.hashid), hash_hlp = rhs.hash_hlp, data = std::move(rhs.data);
        return *this;
    }

private: friend class ConfigurationManager;
    Configuration(
        const std::string &hashid, 
        const std::unordered_map<std::string, std::string> &data,
        VisitRec * const visit_rec = nullptr
    ): hashid(hashid), data(data), visit_rec(visit_rec) {}

    bool flag_lock = false;  // lock() called or not
    int  hash_hlp = 0;       // the helper of hash function
    std::string hashid;      // the name of current configuration
    std::unordered_map<std::string, std::string> data;
    CGN_EXPORT static std::string empty_string;

    // visit record
    VisitRec * const visit_rec = nullptr;
    void rec_all() const { if (visit_rec) visit_rec->all = true; }
    void rec_one(const std::string &in) const {
        if (visit_rec) visit_rec->item.insert(in);
    }
    
    std::string *detect_value(const std::string &key) {
        auto fd = data.find(key);
        if (fd != data.end())
            return &(fd->second);
        return nullptr;
    }
    const std::string *detect_value(const std::string &key) const {
        auto fd = data.find(key);
        if (fd != data.end())
            return &(fd->second);
        return nullptr;
    }
}; //class Configuration

class ConfigurationManager {
public:
    struct KVRestriction {
        // std::string default_value;
        std::vector<std::string> opt_values;
    };

    CGN_EXPORT void set_name(const std::string &name, const ConfigurationID &hash);

    // Get configuration by name
    // @return <Configuration*, adep> if found, otherwise <nullptr, nullptr>.
    CGN_EXPORT std::pair<const Configuration *, GraphNode *>
    get(const std::string name) const;

    CGN_EXPORT std::string get_name_id_mapper(const std::string &name) const;

    //Save configuration and write into .cfg file.
    // @return: the configuration unique_hash
    CGN_EXPORT ConfigurationID commit(Configuration cfg);

    // @param storage_dir: cgn-out/configurations
    CGN_EXPORT ConfigurationManager(const std::string &storage_dir, Graph *g);

private:
    using CfgData = std::unordered_map<std::string, std::string>;
    struct CDataRef {
        const CfgData *ptr;
        int       hash_hlp;
        bool operator==(const CDataRef &rhs) const {
            if (ptr == rhs.ptr)
                return true;
            return hash_hlp == rhs.hash_hlp && *ptr == *rhs.ptr;
        }
        bool operator!=(const CDataRef &rhs) const {
            return !(*this == rhs);
        }
        CDataRef(const CfgData *ptr, int hash_hlp) : ptr(ptr), hash_hlp(hash_hlp) {}
        CDataRef(const Configuration *cfg) : ptr(&cfg->data), hash_hlp(cfg->hash_hlp) {}
    };
    struct CHasher {
        size_t operator()(const CDataRef &ref) const {
            return ref.ptr->size() ^ (size_t)(~ref.hash_hlp);
        }
    };

    //cfg_indexs[CDataRef{&cfg_hashs[NAME].data}] = NAME
    std::unordered_map<std::string, Configuration*>    named_cfgs;
    std::unordered_map<ConfigurationID, Configuration> cfg_hashs;
    std::unordered_map<CDataRef, std::string, CHasher> cfg_indexs;
    
    std::unordered_map<std::string, KVRestriction> kv_restrictions;

    const std::string storage_dir;

    Graph *graph;

    CGN_EXPORT std::string get_hash(size_t want);

}; //class ConfigurationManager

} //namespace