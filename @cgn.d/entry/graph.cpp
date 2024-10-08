// FileDB format
// <1024 bytes file signature> + <64 bytes version> + <block> + .... until file end
// block format:
// 1) string with mtime
//    <2 bits '00'> + <30 bits string_len BE> + <8 bytes mtime> + string
// 2) empty block
//    <2 bits '10'> + <30 bits string_len BE> + "...any_content......"
// 3) GraphNode with self_name(key), file_list[] and inbound_edge_list[]
//    <2 bits '11'> + x:<30 bits num-of-relblocks BE> 
//    + init_state:<1 bits '1'> + <31 bytes self_name_id> 
//    + x * <4 bytes inner-file_blkid / inbound-edge-of-node_selfname_id>
//    init_state: '1':stale, '0':unknown
//    inbound-edge blk_nameid : start with (0b1<<31)
//    inner-file blkid : start with (0b0<<31)
#include <filesystem>
#include <sstream>
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
        std::filesystem::path fp{*(p->files[i]->strkey)};
        
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
    
    //update db_blocks[]
    db_blocks[node.db_selfname_id].as_nodename = &node;

    return &node;
} //Graph::get_node()


void Graph::set_unknown_as_default_state(GraphNode *p) {
    if (!p->init_status_to_stale)
        return ;
    p->init_status_to_stale = false;
    p->db_enforce_write = true;
    db_pending_write.insert(p);
}
void Graph::set_stale_as_default_state(GraphNode *p) {
    if (p->init_status_to_stale)
        return ;
    p->init_status_to_stale = true;
    p->db_enforce_write = true;
    db_pending_write.insert(p);
}

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
        ff->mtime = stat_and_cache(*ff->strkey);
        fseek(file_, ff->db_offset + sizeof(uint32_t), SEEK_SET);
        fwrite(&(ff->mtime), sizeof(ff->mtime), 1, file_);
        if (logger.verbose) {
            std::stringstream ss;
            ss<<"Graph.fwrite(): ["<<ff->db_id<<"] " + *ff->strkey
                <<"\n    * mtime "<<last_mtime <<" -> "<<ff->mtime<<"\n";
            logger.paragraph(ss.str());
        }
    }

    // check nodes from inbound edges. For example there has a edge U->V, 
    // U must already Latest before set_node_status_to_latest(V)
    for (GraphEdgeID eid = _iter_edge(p->rhead, false); eid; 
         eid = _iter_edge(edges[eid].rnext, false))
            assert(edges[eid].from->status == GraphNode::Latest);
    
    p->_status = GraphNode::Latest;
} //Graph::set_node_status_to_latest()

void Graph::set_node_status_to_unknown(GraphNode *p)
{
    if (p == nullptr) {
        for (auto &[name, node] : nodes)
            node._status = GraphNode::Unknown;
    }
    else {
        if (p->status == GraphNode::Unknown)
            return ;
        p->_status = GraphNode::Unknown;
        for (GraphEdgeID eid = _iter_edge(p->head, true); eid; 
             eid = _iter_edge(edges[eid].next, true))
                set_node_status_to_unknown(edges[eid].to);
    }
} //Graph::set_node_status_to_unknown()

void Graph::set_node_status_to_stale(GraphNode *p)
{
    set_node_status_to_unknown(p);
    p->_status = GraphNode::Stale;
}

void Graph::set_node_files(GraphNode *p, std::vector<std::string> in)
{
    //check modified or not
    bool is_same = (p->files.size() == in.size());
    std::unordered_set<std::string> s1{in.begin(), in.end()};
    if (is_same) {
        for (auto blk : p->files)
            if (s1.count(*blk->strkey) == 0) {
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


void Graph::clear_file0_mtime_cache(GraphNode *p)
{
    assert(p->_file_blocks.size());
    const std::string *key = p->_file_blocks[0]->strkey;
    #ifdef _WIN32
        auto fp = std::filesystem::path{*key}.make_preferred();
        std::filesystem::path basedir = fp.has_parent_path()?fp.parent_path():".";
        win_mtime_cache.erase(*key);
        win_mtime_folder_exist.erase(basedir.string());
    #else
        win_mtime_cache.erase(*key);
    #endif
}


// void remove_mtime_cache(const std::filesystem::path &filepath)
int64_t Graph::stat_and_cache(std::filesystem::path fp)
{
#ifdef _WIN32
    // (windows only)
    // if file in current folder not stated 
    // (no subfolder and its file included)
    fp = fp.make_preferred();
    std::filesystem::path basedir = fp.has_parent_path()?fp.parent_path():".";
    if (win_mtime_folder_exist.count(basedir.string()) == 0) {
        auto tmap = Tools::win32_stat_folder(basedir.string());
        for (auto &[path, mtime] : tmap) {
            win_mtime_cache.insert_or_assign(
                fp.has_parent_path()? (basedir/path).string():path, mtime);
        }
        win_mtime_folder_exist.insert(basedir.string());
        // win_mtime_cache[basedir.string()] = -1; //-1 for dir 
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
    if (file_)
        fclose(file_);
}

void Graph::db_load(const std::string &filename)
{
    constexpr const char version[DB_VERSION_FIELD_SIZE] = "CGN-demo";
    std::ifstream fin(filename, std::ios::binary);

    auto fn_create_new = [&](std::size_t err_offset = 0) {
        if (fin)
            fin.close();

        //db_load: block title at err_offset
        std::filesystem::remove(filename);
        db_blocks.clear();
        db_strings.clear();
        db_pending_write.clear();

        if (logger.verbose)
            logger.paragraph("Create new GraphDB: " + filename);

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
            // placeholder, db_recycle[] is still being developed.
            db_blocks.emplace_back();
            pos += body_len;
        }

        if (block_type == DB_BIT_STR) { // type string
            std::size_t aligned_size = sizeof(uint32_t) + sizeof(int64_t) + (body_len+3)/4*4;
            if (pos + aligned_size > buf.size())
                return fn_create_new(pos);

            DBStringBlock block;
            block.db_offset = pos;
            block.db_id  = db_blocks.size();
            block.mtime  = *(int64_t*)(buf.data() + pos + sizeof(uint32_t));
            std::string body{
                buf.data() + pos + sizeof(uint32_t) + sizeof(int64_t),
                body_len
            };
            
            auto [iter, nx] = db_strings.insert({body, std::move(block)});
            iter->second.strkey = &(iter->first);

            db_blocks.emplace_back().as_string = &(iter->second);
            
            pos += aligned_size;
        }
        
        if (block_type == DB_BIT_NODE) { // type GraphNode
            // placeholder, the GraphNode in db don't have blk_id
            db_blocks.emplace_back();

            std::size_t aligned_size = sizeof(uint32_t) + sizeof(int32_t) 
                                     + body_len*sizeof(uint32_t);
            if (pos + aligned_size > buf.size())
                return fn_create_new(pos);

            uint32_t *name_id = (uint32_t*)(buf.data() + pos + sizeof(uint32_t));
            bool init_state_to_stale = false;
            if (*name_id & (1<<31))
                init_state_to_stale = true, *name_id -= (1ul<<31);
            GraphNode &n = nodes[*db_blocks[*name_id].as_string->strkey];
            n.db_offset      = pos;
            n.db_block_size  = aligned_size;
            n.db_enforce_write = false;  // if existed in db
            n.init_status_to_stale = init_state_to_stale;
            db_blocks[n.db_selfname_id = *name_id].as_nodename = &n;

            for (size_t i=0; i<body_len; i++) {
                uint32_t original = *(name_id + 1 + i);
                n.lastdb_data.push_back(original);
                uint32_t rel_id = original & ((1UL<<31) - 1);
                if ((original & (1UL<<31)) != 0) //as edge
                    edges_with_blkid.push_back({rel_id, *name_id});
                else //as file
                    n._file_blocks.push_back(db_blocks[rel_id].as_string);
            }
            if (init_state_to_stale)
                n._status = GraphNode::Stale;

            pos += aligned_size;
        }
    } //for all blocks in file_
    
    //add edges
    for (auto &[b1, b2]: edges_with_blkid) {
        assert(db_blocks[b1].as_nodename && db_blocks[b2].as_nodename);
        add_edge(db_blocks[b1].as_nodename, db_blocks[b2].as_nodename);
    }
    
    //debug
    if (logger.verbose) {
        std::stringstream ss;
        ss<<"Graph.db_load(): "<< db_blocks.size() <<" Blocks loaded\n";
        for (size_t i=0; i<db_blocks.size(); i++) {
            if (db_blocks[i].as_string)
                ss<<" "<<i<<"] mtime:"
                    <<db_blocks[i].as_string->mtime<<" "
                    <<*db_blocks[i].as_string->strkey
                    <<std::endl;
            if (db_blocks[i].as_nodename) {
                auto &n = *db_blocks[i].as_nodename;
                ss<<" "<<i<<"] Node: "
                    <<*db_blocks[i].as_string->strkey + "\n";
                for (auto fblk : n.files)
                    ss<<"     | FILE: "<<*fblk->strkey<<" ("
                          <<fblk->db_id<<")\n";
                for (GraphEdgeID eid = n.rhead; eid; eid = edges[eid].rnext)
                    ss<<"     | "<<edges[eid].from->db_selfname_id
                        <<" -> "<<edges[eid].to->db_selfname_id<<"\n";
            }
        }
        logger.paragraph(ss.str());
    }

    // close stream and open file by fopen()
    fin.close();
    file_ = fopen(filename.c_str(), "r+b");
}

void Graph::db_flush_node()
{
    // for each node, calculate the rel_blocks[] body and compare with
    // n->lastdb_data, write to file if not equal.
    // ** STEP1 **
    for (GraphNode *n : db_pending_write) {
        std::vector<uint32_t> newdata;
        for (auto f : n->files)
            newdata.push_back(f->db_id);
        for (GraphEdgeID eid = _iter_edge(n->rhead, false); eid; 
             eid = _iter_edge(edges[eid].rnext, false)) {
                uint32_t tmp = edges[eid].from->db_selfname_id;
                newdata.push_back(tmp | (1UL<<31));
            }
        
        //skip if same with last data
        // FIXED: if Node not existed in DB, the n->lastdb_data also keep empty
        //      so we use n->db_enforce_write to do extra check
        //      and when we set 'init_status_to_stale' flag, we also
        //      enforce write to db here.
        if (newdata.size())
            std::sort(++newdata.begin(), newdata.end());
        if (!n->db_enforce_write && newdata == n->lastdb_data)
            continue;

        //seek and write data into db
        // 1) modify the previous block (if existed)
        //    off + <u32 title> + <u32 selfname> + <rel_blocks>...
        // 2) write a new record
        if (n->db_offset && newdata.size() == n->lastdb_data.size())
            fseek(file_, n->db_offset + 2*sizeof(uint32_t), SEEK_SET);
        else {//TODO: use blk_recycle here
            if (n->db_offset) //erase existed one in db
                db_set_to_emptyblock(n->db_offset, n->db_block_size);

            fseek(file_, 0, SEEK_END);
            n->db_offset = ftell(file_);
            uint32_t title = newdata.size() + DB_BIT_NODE;
            fwrite(&title, sizeof(title), 1, file_);

            uint32_t name_4byte = n->db_selfname_id;
            if (n->init_status_to_stale)
                name_4byte |= (1<<31);
            fwrite(&name_4byte, sizeof(uint32_t), 1, file_);
        }
        fwrite(newdata.data(), sizeof(uint32_t), newdata.size(), file_);
        n->db_block_size = ftell(file_) - n->db_offset;
        n->lastdb_data = newdata;
        if (logger.verbose) {
            std::stringstream ss;
            auto *self_name = db_blocks[n->db_selfname_id].as_string->strkey;
            ss<<"Graph.fwrite(): off=" << n->db_offset 
              << " [blk" + std::to_string(n->db_selfname_id) + "] " + *self_name 
                 + (n->init_status_to_stale?" (DEFAULT_STALE)":"") + "\n";
            for (auto oitem : newdata) {
                uint32_t blk_id = (oitem & ~(1UL<<31));
                if (oitem & (1UL<<31))
                    ss<<"    * Edge Node[blk "<<blk_id<<"] -> this\n";
                else
                    ss<<"    * File["<<blk_id<<"] "
                        <<*db_blocks[blk_id].as_string->strkey<<"\n";
            }
            logger.paragraph(ss.str());
        }
    } //end for(db_pending_write)
    fflush(file_);
} //Graph::db_flush_node()

Graph::DBStringBlock *Graph::db_fetch_string(std::string name)
{
    static const char fill0[4] = {0, 0, 0, 0};
    auto [iter, nx] = db_strings.insert({name, {}});
    if (!nx)
        return &(iter->second);
    Graph::DBStringBlock &blk = iter->second;

    uint32_t title = name.size() + DB_BIT_STR;
    fseek(file_, 0, SEEK_END);
    blk.db_offset = ftell(file_);
    blk.db_id     = db_blocks.size();
    blk.strkey    = &(iter->first);
    fwrite(&title,     sizeof(title), 1, file_);
    fwrite(&blk.mtime, sizeof(blk.mtime), 1, file_);
    fwrite(name.c_str(), name.size(), 1, file_);
    if (name.size() % 4 != 0)  //padding to int32_t (x4 bytes)
        fwrite(fill0, 4 - name.size()%4, 1, file_);
    
    db_blocks.emplace_back().as_string = &blk;
    return &blk;
}

void Graph::db_set_to_emptyblock(std::size_t offset, uint32_t whole_block_size)
{
    if (logger.verbose) {
        std::stringstream ss;
        ss<<"Graph.fwrite(): recycle OFFSET "<< offset
          <<" -> "<<offset + whole_block_size<<std::endl;
        logger.paragraph(ss.str());
    }
    uint32_t title = whole_block_size | DB_BIT_EMPTY;
    fseek(file_, offset, SEEK_SET);
    fwrite(&title, sizeof(title), 1, file_);
}

} //namespace