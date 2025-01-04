// cgn.d internal, depend on Logger
//
#pragma once
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include "logger.h"

namespace cgnv1
{

struct GraphNode;

// 同一条边 在 "head边表" 和 "rhead边表" 两地存储
// this->to == nullptr && this->from == nullptr 表示此边待删除 等待最后一个访问者
// e.g.当前待删除: 
//      先通过head正向遍历 set next=nullptr, 然后在未来通过 rhead 反向遍历边表时, 
//      发现已有 next==nullptr, 则可安全从内存中delete GraphEdge (The last visitor)
using GraphEdgeID = std::size_t;
struct GraphEdge {
    GraphNode *to, *from;
    GraphEdgeID next, rnext; //rnext is not previous edge!
};

// A build graph with edge from Early-Node to Late-Node
class Graph
{
public:

    //[block_case 1 string] the fileDB data mirror
    struct GraphString { // block id and offset in DB
        // variable constant after constructor
        const std::string* strkey;
        
        // variable constant after first assigned if existed in db, 
        // or after db_flush() if newly added
        uint32_t self_offset = 0;  //block offset in file

        // mtime for current path (only valid for filepath)
        // db_mtime and dbfile can updated by db_flush() if not equal below
        int64_t mem_mtime = 0;  // the data read/write by class member
        int64_t db_mtime = 0;   // the data in fileDB

        std::size_t db_block_size() {
            return sizeof(uint32_t) + sizeof(int64_t) + (strkey->size()+3)/4*4;
        }
    };

    //[block_case 3 GraphNode] the fileDB data mirror in memory, update when db_flush() 
    struct DBNodeBlock {
        // variable constant after construct if existed in db, or db_flush() if not
        uint32_t self_offset;   //current block offset in file
        uint32_t title_offset;  //node title string block offset in file, zero when unassigned.

        // variables can be updated by db_flush()
        bool init_state_is_stale;      
        std::vector<uint32_t> rel_off;  //related files[] or inbound_nodes[]

        std::size_t db_block_size() {
            return sizeof(uint32_t) + sizeof(int32_t) + rel_off.size()*sizeof(uint32_t);
        }
    };

    GraphNode *get_node(const std::string &name);

    // Set the initial state when db_load()
    // Default is 'unknown' if no assigned
    void set_unknown_as_default_state(GraphNode *p);
    void set_stale_as_default_state(GraphNode *p);

    // Add edge
    void add_edge(GraphNode *from, GraphNode *to);
    
    // Remove all inbound edges of p, usually used on 
    // re-analyse target and clear its deps.
    void remove_inbound_edges(GraphNode *p);

    // Judge p->status if Unknown.
    void test_status(GraphNode *p);

    // Propagate the status from node U to its connected node V via edge U->V.
    // - If U's status is 'Stale', set V's status to 'Stale'.
    // - If U's status is 'Unknown' or 'Latest', do not modify V's state.
    void forward_status(GraphNode *p);

    // update db[files[*]].mtime and set p->status to Latest
    void set_node_status_to_latest(GraphNode *p);

    // set p->status and dfs(p)->status to Unknown
    //@param p : nullptr to set all node to Unknown status
    //           otherwise for p and dfs(p)
    void set_node_status_to_unknown(GraphNode *p = nullptr);

    // set dfs(p)->status to Unknown and p->status to Stale.
    void set_node_status_to_stale(GraphNode *p);

    // set p->files = in and set p and dfs(p)->status = Unknown
    void set_node_files(GraphNode *p, std::vector<std::string> in);

    void clear_mtime_cache() { 
        win_mtime_folder_exist.clear(); win_mtime_cache.clear();
    }
    
    void clear_filelist_mtime_cache(std::vector<std::string> file_list);
    void clear_file0_mtime_cache(GraphNode *p);

    // flush db_pending_write_str[] and db_pending_write_node[] into db
    void db_flush();

    void db_load(const std::string &filename);

    Graph(Logger *logger);
    ~Graph();

private:
    Logger *logger;

    //GraphNode index by NodeTitle
    std::unordered_map<std::string, GraphNode> nodes;

    std::vector<GraphEdge> edges;
    std::vector<GraphEdgeID> edge_recovery_pool;
    
    // call before each edge visited
    GraphEdgeID _iter_edge(GraphEdgeID &edge_id, bool loop_from_edge_start);

    // (windows) get files mtime by folder.
    // * win_mtime_cache[folder] existed when all files in this folder cached 
    //   (no subfolder included)
    // * win_mtime_cache[filepath] = mtime
    // (no windows) get file mtime directly then cache?
    // * keep cache until this->clear_mtime_cache() called?
    std::unordered_set<std::string> win_mtime_folder_exist;
    std::unordered_map<std::string, int64_t> win_mtime_cache;

    // void remove_mtime_cache(const std::filesystem::path &filepath);
    int64_t stat_and_cache(std::filesystem::path filepath);

    // DB index saved in 3 places:
    //  nodes[].dbfile_xx, db_strings[].dbfile_xx and db_recovery
    FILE* file_ = nullptr;

    // all db blocks
    // struct DBBlock {
    //     DBStringBlock *as_string   = nullptr;
    //     GraphNode     *as_nodename = nullptr;
    // };
    // std::vector<DBBlock> db_blocks;

    // string (as filepath) and mtime storage
    std::unordered_map<std::string, GraphString> db_strings;

    //empty block (unused currently)
    //db_recycle[free block size] = offset in file
    std::multimap<std::size_t, uint32_t> db_recycle_pool;

    // Write from anywhere, utilized by db_flush().
    std::unordered_set<GraphString*> db_pending_write_str;
    std::unordered_set<GraphNode*>   db_pending_write_node;

    constexpr static std::size_t DB_VERSION_FIELD_SIZE = 1024;
    constexpr static uint32_t DB_BIT_NODE  = (0b11UL << 30);
    constexpr static uint32_t DB_BIT_EMPTY = (0b10UL << 30);
    constexpr static uint32_t DB_BIT_STR   = (0b00UL << 30);

    GraphString *get_graph_string(const std::string &str);

    void file_seek_new_block(std::size_t want_size);

    void file_mark_recycle(std::size_t offset, uint32_t whole_block_size);
}; //class Graph

//
// Typical node example
// files[]: system specified path separator
// 
// * cgn script(s):
//      name  = "//cgn.d/library/lang_rust.cgn.bundle" (always backslash '/')
//      files = ["cgn.d/library/lang_rust.cgn.bundle/lang_rust.cgn.h"
//               "cgn.d/library/lang_rust.cgn.bundle/lang_rust.cgn.cc"
//               "cgn.d/library/lang_rust.cgn.bundle/build.ninja"]
// * target: (or folder)
//      name  = "cgn-out/demo_/hello_FFFF1234/" (system specified path separator)
//      files = ["cgn-out/demo_/hello_FFFF1234/build.ninja"]
// * configuration file: (or single file)
//      name  = "cgn-out/configurations/cfg_FFFF1234.cfg" (system specified path separator)
//      files = ["cgn-out/configurations/cfg_FFFF1234.cfg"]
struct GraphNode {
    enum Status {
        Latest,
        Stale,  
        Unknown
    };

    const Status &status{_status};

    // file paths relavent path to working root
    // system-specified path separator
    // files[0] is the output of current GraphNode, usually to check mtime with
    // rdfs(this).max_mtime
    const std::vector<Graph::GraphString*> &files{_file_blocks};
    // const std::vector<std::string> &files{_files};

private: friend class Graph;
    // Graph
    GraphEdgeID head = 0, rhead = 0;

    // variable in memory
    int64_t max_mtime = 0;  // for test_status() only.
    Status _status = Unknown;
    bool init_state_is_stale = false;
    // std::vector<std::string> _files;
    Graph::GraphString *_title = nullptr;
    std::vector<Graph::GraphString*> _file_blocks;

    // variable of db mirror
    Graph::DBNodeBlock filedb;
    // // block id and offset in DB
    // // uint32_t    db_block_id = 0;
    // uint32_t    db_selfname_id = 0;
    // std::size_t db_offset = 0;
    // std::size_t db_block_size = 0;

    // // db data (already sorted before last save)
    // bool db_enforce_write = true;
    // std::vector<uint32_t> lastdb_data;

}; //struct GraphNode

struct DBFile {};

} // namespace cgn
