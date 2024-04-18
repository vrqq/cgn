#pragma once
#include <list>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>

namespace cgn
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
class Graph {
public:
    struct DBStringBlock { // block id and offset in DB
        const std::string* key_filepath;
        uint32_t    db_id;      //block id in file
        std::size_t db_offset;  //block offset in file

        int64_t     mtime = 0;  //mtime for current path (only valid for filepath)
        // GraphNode  *gnode = nullptr; //key_filepath as node title (only valid for GraphNode)
    };

    GraphNode *get_node(const std::string &name);

    void add_edge(GraphNode *from, GraphNode *to);
    
    void remove_inbound_edges(GraphNode *p);

    //Judge p->status if Unknown.
    void test_status(GraphNode *p);

    // void remove_node(GraphNode *p = nullptr);

    // update db[files[*]].mtime and set p->status to Latest
    void set_node_status_to_latest(GraphNode *p);

    // set p->status to Unknown
    //@param p : nullptr to set all node to Unknown status
    //           otherwise for p and dfs(p)
    void set_node_status_to_unknown(GraphNode *p = nullptr);

    // set p->files = in and set p and dfs(p)->status = Stale 
    void set_node_files(GraphNode *p, std::vector<std::string> in);

    void clear_mtime_cache() { win_mtime_cache.clear(); }

    // flush db modifications: node.files[] and node.inbound_edges[]
    void db_flush_node();

    void db_load(const std::string &filename);

    Graph();
    ~Graph();

private:
    std::unordered_map<std::string, GraphNode> nodes;
    std::vector<GraphEdge> edges;
    std::vector<GraphEdgeID> edge_recovery_pool;
    
    // call before each edge visited
    GraphEdgeID _iter_edge(GraphEdgeID &edge_id, bool loop_from_edge_start);

    // (windows only) get files mtime by folder.
    // * win_mtime_cache[folder] existed when all files in this folder cached 
    //   (no subfolder included)
    // * win_mtime_cache[filepath] = mtime
    std::unordered_map<std::string, int64_t> win_mtime_cache;

    int64_t stat_and_cache(const std::filesystem::path &filepath);

    // DB index saved in 3 places:
    //  nodes[].dbfile_xx, db_strings[].dbfile_xx and db_recovery
    FILE* file_ = nullptr;
    uint32_t db_nxt_blkid = 0;

    // all db blocks
    struct DBBlock {
        DBStringBlock *as_string = nullptr;
        GraphNode     *as_node   = nullptr;
    };
    std::vector<DBBlock> db_blocks;

    // string (as filepath) and mtime storage
    std::unordered_map<std::string, DBStringBlock> db_strings;

    //empty block (unused currently)
    //db_recovery[free block size] = offset in file
    std::multimap<std::size_t, std::size_t> db_recovery;

    std::unordered_set<GraphNode*> db_pending_write;

    constexpr static std::size_t DB_VERSION_FIELD_SIZE = 1024;
    constexpr static uint32_t DB_BIT_NODE  = (0b11UL << 30);
    constexpr static uint32_t DB_BIT_EMPTY = (0b10UL << 30);
    constexpr static uint32_t DB_BIT_STR   = (0b00UL << 30);

    DBStringBlock *db_fetch_string(std::string name);

    DBStringBlock *db_fetch_path(std::string path, int64_t mtime);

    void db_set_to_emptyblock(std::size_t offset, uint32_t whole_block_size);

    // void *db_update_node() {}

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
    const std::vector<Graph::DBStringBlock*> &files{_file_blocks};
    // const std::vector<std::string> &files{_files};

private: friend class Graph;
    Status _status = Unknown;
    // std::vector<std::string> _files;
    std::vector<Graph::DBStringBlock*> _file_blocks;
    int64_t max_mtime = 0;

    GraphEdgeID head = 0, rhead = 0;
    
    // block id and offset in DB
    uint32_t    db_block_id = 0;
    uint32_t    db_selfname_id = 0;
    std::size_t db_offset = 0;
    std::size_t db_block_size = 0;
}; //struct GraphNode

} // namespace cgn
