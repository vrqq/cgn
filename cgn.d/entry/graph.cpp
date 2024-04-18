// FileDB format
// <1024 bytes file signature> + <64 bytes version> + <block> + .... until file end
// block format:
// 1) string with mtime
//    <2 bits '00'> + <30 bits string_len BE> + <8 bytes mtime> + string
// 2) empty block
//    <2 bits '10'> + <30 bits string_len BE> + "...any_content......"
// 3) GraphNode with self_name(key), file_list[] and inbound_edge_list[]
//    <2 bits '11'> + <30 bits num-of-blk BE> 
//    + <4 bytes self_name_id> + {<4 bytes inner-file_id> / <edge-from-node_id> + ...}
#include <filesystem>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "graph.h"
#include "../cgn.h"
#include "debug.h"

namespace cgn {

GraphEdgeID Graph::_iter_edge(GraphEdgeID &eid, bool loop_from_edge_start)
{
    while(eid && edges[eid].to == nullptr) {
        GraphEdgeID now = eid;
        if (loop_from_edge_start) {
            eid = edges[eid].next;
            edges[now].next = 0;
        }
        else {
            eid = edges[eid].rnext;
            edges[now].rnext = 0;
        }
        if (edges[now].rnext && edges[now].next) //delete edge safety
            edge_recovery_pool.push_back(now);
    }
    return eid;
}

void Graph::test_status(GraphNode *p)
{
    if (p->status != GraphNode::Unknown)
        return ;
    
    bool is_stale = false;
    p->max_mtime = 0;
    for (std::size_t i=0; i<p->files.size() && !is_stale; i++) {
        std::filesystem::path fp{*(p->files[i]->key_filepath)};
        
        //if file not existed.
        if (std::filesystem::exists(fp) == false) {
            if (p->files[i]->mtime == 0)
                continue;
            else {
                is_stale = true; break;
            }
        }

        if (std::filesystem::is_regular_file(fp) == false)
            throw std::runtime_error{fp.string() + " is not regular file."};
    
        is_stale |= (stat_and_cache(fp) != p->files[i]->mtime);
        p->max_mtime = std::max(p->max_mtime, p->files[i]->mtime);
    }  //for(p->files[])

    int64_t max_mtime_from_rnode = 0;
    for (GraphEdgeID e = _iter_edge(p->rhead, false); e && !is_stale; e=_iter_edge(edges[e].rnext, false)) {
        if (edges[e].from->status == GraphNode::Unknown)
            test_status(edges[e].from);
        is_stale |= (edges[e].from->status == GraphNode::Stale);
        p->max_mtime = std::max(p->max_mtime, edges[e].from->max_mtime);
    }

    if (p->files.size()) //file[0] is output of current GraphNode
        is_stale |= (p->files[0]->mtime < p->max_mtime);

    p->_status = is_stale? GraphNode::Stale : GraphNode::Latest;
} //Graph::test_status()

GraphNode *Graph::get_node(const std::string &name)
{
    if (auto fd = nodes.find(name); fd != nodes.end())
        return &(fd->second);
        
    GraphNode &node = nodes[name];
    node.db_selfname_id = db_fetch_string(name)->db_id;
    db_pending_write.insert(&node);

    return &node;
} //Graph::get_node()

void Graph::add_edge(GraphNode *from, GraphNode *to)
{
    size_t eid;
    if (edge_recovery_pool.size())
        eid = edge_recovery_pool.back(), edge_recovery_pool.pop_back();
    else
        eid = edges.size(), edges.emplace_back();
    GraphEdge &e = edges[eid];
    e.from = from;
    e.to   = to;
    e.next = from->head; from->head = eid;
    e.rnext = to->rhead; to->rhead  = eid;
} //Graph::add_edge()

void Graph::remove_inbound_edges(GraphNode *p)
{
    for (GraphEdgeID eid = p->rhead; eid; eid=edges[eid].rnext)
        edges[eid].from = edges[eid].to = nullptr;
    db_pending_write.insert(p);
} //Graph::remove_inbound_edges()

void Graph::set_node_status_to_latest(GraphNode *p)
{
    if (p->status == GraphNode::Latest)
        return ;

    // stat and write file mtime into db
    for (auto &ff : p->_file_blocks) {
        auto last_mtime = ff->mtime;
        ff->mtime = stat_and_cache(*ff->key_filepath);
        fseek(file_, ff->db_offset + sizeof(uint32_t), SEEK_SET);
        fwrite(&(ff->mtime), sizeof(ff->mtime), 1, file_);
        if (1) {
            logout<<"Graph.fwrite(): ["<<ff->db_id<<"] " + *ff->key_filepath
                <<"\n    * mtime "<<last_mtime <<" -> "<<ff->mtime<<"\n";
        }
    }

    // check nodes from inbound edges
    for (GraphEdgeID eid = _iter_edge(p->rhead, false); eid; 
         eid = _iter_edge(edges[eid].rnext, false))
            assert(edges[eid].from->status == GraphNode::Latest);
    
    p->_status = GraphNode::Latest;
} //Graph::set_node_status_to_latest()

void Graph::set_node_status_to_unknown(GraphNode *p)
{
    if (p->status == GraphNode::Unknown)
        return ;
    if (p == nullptr) {
        for (auto &[name, node] : nodes)
            node._status = GraphNode::Unknown;
    }
    else {
        for (GraphEdgeID eid = _iter_edge(p->head, true); eid; 
             eid = _iter_edge(edges[eid].next, true))
                set_node_status_to_unknown(edges[eid].to);
    }
} //Graph::set_node_status_to_unknown()

void Graph::set_node_files(GraphNode *p, std::vector<std::string> in)
{
    //check modified or not
    bool is_same = (p->files.size() == in.size());
    std::unordered_set<std::string> s1{in.begin(), in.end()};
    if (is_same) {
        for (auto blk : p->files)
            if (s1.count(*blk->key_filepath) == 0) {
                is_same = false; break;
            }
    }
    if (is_same)
        return;

    //write to db if modified
    for (auto &fp : in)
        p->_file_blocks.push_back(db_fetch_string(fp));
    p->_status = GraphNode::Unknown;
    db_pending_write.insert(p);
}


int64_t Graph::stat_and_cache(const std::filesystem::path &fp)
{
#ifdef _WIN32
    // (windows only)
    // if file in current folder not stated 
    // (no subfolder and its file included)
    std::filesystem::path basedir = fp.has_parent_path()?fp.parent_path():".";
    if (win_mtime_cache.count(basedir) == 0) {
        auto tmap = Tools::win32_stat_folder(basedir);
        win_mtime_cache.merge(tmap);
        assert(tmap.empty());
        win_mtime_cache[basedir] = -1; //-1 meanless
    }
    
    auto fd = win_mtime_cache.find(fp.string());
    // return 0 for file not existed
    return (fd == win_mtime_cache.end())? 0 : fd->second;
#else
    // (for other systems)
    // use stat() or stat64() directly without cache
    return Tools::stat(fp.string());
#endif
}


// FileDB operations
// -----------------

Graph::Graph() {
    //EdgeID == 0 (nullptr)
    GraphEdge e;
    e.from = e.to = 0;
    e.next = e.rnext = 0;
    edges.push_back(e);
}
Graph::~Graph() {
    db_flush_node();
}

void Graph::db_load(const std::string &filename)
{
    constexpr const char version[DB_VERSION_FIELD_SIZE] = "CGN-demo";
    std::ifstream fin(filename);

    auto fn_create_new = [&](std::size_t err_offset = 0) {
        if (fin)
            fin.close();

        //db_load: block title at err_offset
        std::filesystem::remove(filename);
        db_blocks.clear();
        db_strings.clear();
        db_pending_write.clear();
        db_nxt_blkid = 0;

        file_ = fopen(filename.c_str(), "wb+");
        fseek(file_, 0, SEEK_SET);
        fwrite(version, DB_VERSION_FIELD_SIZE, 1, file_);
    };

    if (!fin)
        return fn_create_new();
    std::stringstream ss;
    ss<<fin.rdbuf(); //read whole file
    fin.close();

    std::string buf = ss.str();
    if (buf.size() < DB_VERSION_FIELD_SIZE 
     || memcmp(buf.data(), version, 8) != 0)
        return fn_create_new(); //"version conflict"
    
    std::vector<std::pair<int32_t, int32_t>> edges_with_blkid;
    for(std::size_t pos = DB_VERSION_FIELD_SIZE; pos < buf.size();) {
        uint32_t *title = (uint32_t*)(buf.data() + pos);
        if (pos + sizeof(uint32_t) > buf.size())
            return fn_create_new(pos);

        uint32_t block_type = (*title &  (0b11UL<<30));
        uint32_t body_len   = (*title & ~(0b11UL<<30));

        if (block_type == DB_BIT_EMPTY) {// type empty field
            db_blocks.emplace_back();
            db_nxt_blkid++;
            pos += body_len;
        }

        if (block_type == DB_BIT_STR) { // type string
            std::size_t aligned_size = sizeof(uint32_t) + sizeof(int64_t) + (body_len+3)/4*4;
            if (pos + aligned_size > buf.size())
                return fn_create_new(pos);

            DBStringBlock block;
            block.db_offset = pos;
            block.db_id  = db_nxt_blkid++;
            block.mtime  = *(int64_t*)(buf.data() + pos + sizeof(uint32_t));
            std::string body{
                buf.data() + pos + sizeof(uint32_t) + sizeof(int64_t),
                body_len
            };
            
            auto [iter, nx] = db_strings.insert({body, std::move(block)});
            iter->second.key_filepath = &(iter->first);

            db_blocks.resize(db_nxt_blkid);
            db_blocks[iter->second.db_id].as_string = &(iter->second);
            
            pos += aligned_size;
        }
        
        if (block_type == DB_BIT_NODE) { // type GraphNode
            std::size_t aligned_size = sizeof(uint32_t) + sizeof(int32_t) 
                                     + body_len*sizeof(uint32_t);
            if (pos + aligned_size > buf.size())
                return fn_create_new(pos);

            uint32_t *name_id = (uint32_t*)(buf.data() + pos + sizeof(uint32_t));
            GraphNode &n = nodes[*db_blocks[*name_id].as_string->key_filepath];
            n.db_offset      = pos;
            n.db_block_id    = db_nxt_blkid++;
            n.db_block_size  = aligned_size;
            n.db_selfname_id = *name_id;

            for (size_t i=0; i<body_len; i++) {
                uint32_t *rel_blk_id = name_id + 1 + i;
                if (db_blocks[*rel_blk_id].as_string)
                    n._file_blocks.push_back(db_blocks[*rel_blk_id].as_string);
                else
                    edges_with_blkid.push_back({*rel_blk_id, n.db_block_id});
            }

            db_blocks.resize(db_nxt_blkid+1);
            db_blocks[n.db_block_id].as_node = &n;

            pos += aligned_size;
        }
    } //for all blocks in file_
    
    //add edges
    for (auto &[b1, b2]: edges_with_blkid) {
        assert(db_blocks[b1].as_node && db_blocks[b2].as_node);
        add_edge(db_blocks[b1].as_node, db_blocks[b2].as_node);
    }
    
    //debug
    if (1) {
        logout<<"db_load(): "<< db_blocks.size() <<" Blocks loaded\n";
        for (size_t i=0; i<db_blocks.size(); i++) {
            if (db_blocks[i].as_string)
                logout<<" "<<i<<"] mtime:"
                    <<db_blocks[i].as_string->mtime<<" "
                    <<*db_blocks[i].as_string->key_filepath
                    <<std::endl;
            if (db_blocks[i].as_node) {
                auto &n = *db_blocks[i].as_node;
                logout<<" "<<i<<"] Node: "
                    <<*db_blocks[n.db_selfname_id].as_string->key_filepath + "\n";
                for (auto fblk : n.files)
                    logout<<"    | FILE: "<<*fblk->key_filepath<<" ("
                          <<fblk->db_id<<")\n";
                for (GraphEdgeID eid = n.rhead; eid; eid = edges[eid].rnext)
                    logout<<"    | "<<edges[eid].from->db_block_id
                        <<" -> "<<edges[eid].to->db_block_id<<"\n";
            }
        }
        logout<<"------ ------ ------ ------ ------ ------"<<std::endl;
    }

    // close stream and open file by fopen()
    fin.close();
    file_ = fopen(filename.c_str(), "r+b");
}

void Graph::db_flush_node()
{
    // two-step write back
    // 1) calculate block number (edge number) and write block
    // 2) write inbound edge table into block
    std::vector<std::pair<GraphNode*, uint32_t>> tmp;
    tmp.reserve(db_pending_write.size());

    // ** STEP1 **
    for (GraphNode *n : db_pending_write) {
        //remove existed one in db
        if (n->db_offset) {
            db_set_to_emptyblock(n->db_offset, n->db_block_size);
            n->db_offset = 0;
        }

        //calculate block length
        uint32_t node_cnt = n->files.size();
        for (GraphEdgeID eid = _iter_edge(n->rhead, false); eid; 
             eid = _iter_edge(edges[eid].rnext, false)) 
                node_cnt++;

        //write a new record (placeholder)
        std::vector<uint32_t> placeholder(node_cnt, 0);
        uint32_t title = (uint32_t)node_cnt + DB_BIT_NODE;
        fseek(file_, 0, SEEK_END);
        n->db_block_id = db_nxt_blkid++;
        n->db_offset   = ftell(file_);
        fwrite(&title, sizeof(title), 1, file_);
        fwrite(&(n->db_selfname_id), sizeof(uint32_t), 1, file_);
        fwrite(placeholder.data(), sizeof(uint32_t), placeholder.size(), file_);
        n->db_block_size = ftell(file_) - n->db_offset;

        //update db_blocks[]
        db_blocks.resize(db_nxt_blkid);
        db_blocks[n->db_block_id].as_node = n;

        //for step2
        tmp.push_back({n, node_cnt});
    }


    // ** STEP2 **
    // All Nodes in db have valid block_id currently.
    for (auto [n, len] : tmp) {
        std::vector<uint32_t> data; data.reserve(len);
        for (auto f : n->files)
            data.push_back(f->db_id);
        for (GraphEdgeID eid = n->rhead; eid; eid = edges[eid].rnext)
            data.push_back(edges[eid].from->db_block_id);
        
        //write to db
        // [title] + [self_name_id] + [node_id] + ...
        fseek(file_, n->db_offset + 2*sizeof(uint32_t), SEEK_SET);
        fwrite(data.data(), sizeof(uint32_t), data.size(), file_);

        if (1) {
            auto *self_name = db_blocks[n->db_selfname_id].as_string->key_filepath;
            logout<<"Graph.fwrite(): new_blk_id=" << n->db_block_id 
                  << " " + *self_name + "\n";
            for (auto blk_id : data) {
                if (db_blocks[blk_id].as_string)
                    logout<<"    * STR["<<blk_id<<"] "
                        <<*db_blocks[blk_id].as_string->key_filepath<<"\n";
                else if (db_blocks[blk_id].as_node)
                    logout<<"    * Edge Node[blk "<<blk_id<<"] -> this\n";
                else
                    logout<<"    * ERROR REL_BLK_ID "<<blk_id<<"\n";
            }
        }
    }
} //Graph::db_flush_node()

Graph::DBStringBlock *Graph::db_fetch_string(std::string name)
{
    static const char fill0[4] = {0, 0, 0, 0};
    if (auto fd = db_strings.find(name); fd != db_strings.end())
        return &(fd->second);
    auto iter = db_strings.insert({name, {}}).first;
    Graph::DBStringBlock &blk = iter->second;

    uint32_t title = name.size() + DB_BIT_STR;
    fseek(file_, 0, SEEK_END);
    blk.db_offset = ftell(file_);
    blk.db_id     = db_nxt_blkid++;
    blk.key_filepath = &(iter->first);
    fwrite(&title,     sizeof(title), 1, file_);
    fwrite(&blk.mtime, sizeof(blk.mtime), 1, file_);
    fwrite(name.c_str(), name.size(), 1, file_);
    if (name.size() % 4 != 0)  //padding to int32_t (x4 bytes)
        fwrite(fill0, 4 - name.size()%4, 1, file_);
    
    db_blocks.resize(db_nxt_blkid);
    db_blocks[blk.db_id].as_string = &blk;
    return &blk;
}

void Graph::db_set_to_emptyblock(std::size_t offset, uint32_t whole_block_size)
{
    if (1) {
        logout<<"Graph.fwrite(): recycle OFFSET "<< offset
              <<" -> "<<offset + whole_block_size<<std::endl;
    }
    uint32_t title = whole_block_size | DB_BIT_EMPTY;
    fseek(file_, offset, SEEK_SET);
    fwrite(&title, sizeof(title), 1, file_);
}

} //namespace