// CGN public header, implement in cli
// * Rename a configuration would not make target recompile,
//   because the content unchanged.
// * The config name only get from CGN-CELL-SETUP.
// * A config without name assigned wouid still keep in output,
//   and the last build result would also kept.
#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include "api_export.h"

namespace cgnv1 {

using ConfigurationID = std::string;
class Graph;
struct GraphNode;

// After locked, user cannot modify any value and visit someone which not visited before.
//
class Configuration
{
    using type_data = std::unordered_map<std::string, std::string>;
    struct DataBlock {
        type_data visited, remain;
        ConfigurationID hashid;
        int hash_hlp = 0;
        bool locked = false;
    };

    static int hash_one(const std::string &value) {
        return ((int)value[0]) << (value.size()%20);
    }

public:

    struct Cell {
        const std::string &operator=(const std::string &val)
        {
            // user cannot modify value after locked.
            if (_data->locked)
                throw std::runtime_error{"Configuration locked."};
            
            //find in both set
            auto fd1 = _data->visited.find(key);
            auto fd2 = _data->remain.find(key);
            type_data *active_map = nullptr;
            type_data::iterator iter;
            if (fd1 != _data->visited.end())
                active_map = &_data->visited, iter = fd1;
            else if (fd2 != _data->remain.end())
                active_map = &_data->remain, iter = fd2;
            
            if (val.empty()) { //case: clear value
                if (!active_map)  // if not exist before
                    return val;
                
                _data->hash_hlp -= hash_one(iter->second);
                active_map->erase(iter);
                _data->hashid.clear();
            }
            else { //case: assign value
                if (active_map && iter->second == val)
                    return val;
                
                if (active_map) {
                    _data->hash_hlp -= hash_one(iter->second);
                    iter->second = val;
                }else
                    _data->visited[key] = val;
                _data->hash_hlp += hash_one(val);
                _data->hashid.clear();
            }
            return val;
        }

        // move from remain[] to visited[] when visit, whatever const Cell or not;
        operator const std::string&() const {
            auto fd1 = _data->visited.find(key);
            if (fd1 != _data->visited.end())
                return fd1->second;

            auto fd2 = _data->remain.find(key);
            if (fd2 != _data->remain.end()) {
                if (_data->locked) //cannot visit the straight one after lock.
                    throw std::runtime_error{"Configuration locked."};
                std::pair<type_data::iterator, bool> insert_result = _data->visited.insert({fd2->first, fd2->second});
                _data->remain.erase(fd2);
                return insert_result.first->second;
            }
            return empty_string;
        }

        bool operator==(const std::string &rhs) const {
            std::string dumm1;
            std::string dumm2 = dumm1;
            const std::string &self_str = *this;
            return self_str == rhs;
        }
        bool operator!=(const std::string &rhs) const {
            return !(*this == rhs);
        }
        bool operator==(const Cell &rhs) const {
            return (const std::string&)(*this) == (const std::string&)rhs;
        }

        Cell(DataBlock *_data, const std::string &key)
        : _data(_data), key(key) {}
    
    private:
        DataBlock *_data;
        std::string key;
    }; //struct Cell

    Configuration()  { 
        _data = new DataBlock;
        // std::cout<<"\n[CREATED "<<(void*)_data<<std::endl;
    }
    ~Configuration() { 
        if (_data) {delete _data;
        // std::cout<<"\n[DELETE "<<(void*)_data<<std::endl;
        }
    }

    Configuration(const Configuration &rhs) {
        _data = new DataBlock();
        _data->remain = rhs._data->remain;
        _data->remain.insert(rhs._data->visited.begin(), rhs._data->visited.end());
        _data->hashid = rhs._data->hashid;
        _data->hash_hlp = rhs._data->hash_hlp;
        _data->locked = false;
    }
    Configuration &operator=(const Configuration &rhs) {
        if (_data == nullptr)
            _data = new DataBlock();
        _data->visited.clear();
        _data->remain = rhs._data->remain;
        _data->remain.insert(rhs._data->visited.begin(), rhs._data->visited.end());
        _data->hashid = rhs._data->hashid;
        _data->hash_hlp = rhs._data->hash_hlp;
        _data->locked = false;
        return *this;
    }
    
    Configuration(Configuration &&rhs) : _data(rhs._data) { rhs._data = nullptr; }
    Configuration &operator=(Configuration &&rhs) {
        if (_data)
            delete _data;
        _data = rhs._data; rhs._data = nullptr;
        return *this;
    }

    Cell get(const std::string &key) {
        return Cell(this->_data, key);
    }
    Cell operator[](const std::string &key) {
        return Cell(this->_data, key);
    }
    const Cell get(const std::string &key) const {
        return Cell(this->_data, key);
    }
    const Cell operator[](const std::string &key) const {
        return Cell(this->_data, key);
    }

    void visit_keys(const Configuration &rhs) const {
        for (auto iter = rhs.begin(); iter != rhs.end(); iter++)
            (std::string)this->get(iter->first);
    }
    void visit_keys(const std::vector<std::string> &keys) const {
        for (auto iter = keys.begin(); iter != keys.end(); iter++)
            (std::string)this->get(*iter);
    }
    
    void visit_all_keys() const {
        this->_data->visited.insert(
            _data->remain.begin(),
            _data->remain.end()
        );
        _data->remain.clear();
    }

    std::size_t size() const { return _data->visited.size() + _data->remain.size(); }
    std::size_t count(const std::string &key) {
        if (_data->locked)
            return _data->visited.count(key);
        auto fd = _data->remain.find(key);
        if (fd != _data->remain.end()) {
            _data->visited.insert({*fd}), _data->remain.erase(fd);
            return 1;
        }
        return 0;
    }

    using iterator = type_data::iterator;
    using const_iterator = type_data::const_iterator;
    const_iterator begin()  const { visit_all(); return _data->visited.cbegin(); }
    const_iterator end()    const { visit_all(); return _data->visited.cend(); }
    const_iterator cbegin() const { visit_all(); return _data->visited.cbegin(); }
    const_iterator cend()   const { visit_all(); return _data->visited.cend(); }
    const type_data &data() const { visit_all(); return _data->visited; }

    ConfigurationID get_id() const { return _data->hashid; }

    bool is_locked() const { return _data->locked; }
    void trim_lock() {
        for (auto &remain_entry : _data->remain)
            _data->hash_hlp -= hash_one(remain_entry.second);
        if (_data->remain.size()) {
            _data->hashid.clear();
            _data->remain.clear();
        }
        _data->locked = true;
    }

private: friend class ConfigurationManager;
    CGN_EXPORT static std::string empty_string;

    DataBlock *_data = nullptr;
    // bool locked = false;

    void visit_all() const {
        if (_data->remain.empty())
            return ;
        if (_data->locked)
            throw std::runtime_error{"Configuration locked."};
        _data->visited.insert(_data->remain.begin(), _data->remain.end());
        _data->remain.clear();
    }
}; //class Configuration

} //namespace