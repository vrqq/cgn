// FileDB format
// <1024 bytes file signature> + <block> + <block> + .... until file end
// block format (4 bytes aligned for each block):
// 1) string with mtime
//    <2 bits '00'> + <30 bits string_len BE> + <8 bytes mtime> + string
// 2) empty block (skip, no blkid acquired)
//    <2 bits '10'> + <30 bits whole_block_size BE> + "...any_content......"
// 3) GraphNode with self_name(key), file_list[] and inbound_edge_list[]
//    <2 bits '11'> + x:<30 bits num-of-relblocks BE> 
//      + init_state:<1 bits '1'> + <31 bytes self_name_offset> 
//      + x * <4 bytes inner file blkoff / inbound-edge blkoff>
//    init_state: '1' stale, '0' unknown
//    node_blkoff : title string offset start with (0b1<<31)
//    file_blkoff : title string offset start with (0b0<<31)
//
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "cgn_api.h" // static Tools
#include "graph.h"

namespace cgnv1 {

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
        //  mark stale and break
        if (std::filesystem::exists(fp) == false) {
            is_stale = true; break;
            // if (p->files[i]->mtime == 0)
            //     continue;
            // else {
            //     is_stale = true; break;
            // }
        }

        if (std::filesystem::is_regular_file(fp) == false)
            throw std::runtime_error{fp.string() + " is not regular file."};
    
        is_stale |= (stat_and_cache(fp) != p->files[i]->mem_mtime);
        p->max_mtime = std::max(p->max_mtime, p->files[i]->mem_mtime);
    }  //for(p->files[])

    int64_t max_mtime_from_rnode = 0;
    for (GraphEdgeID e = _iter_edge(p->rhead, false); e && !is_stale; e=_iter_edge(edges[e].rnext, false)) {
        if (edges[e].from->status == GraphNode::Unknown)
            test_status(edges[e].from);
        is_stale |= (edges[e].from->status == GraphNode::Stale);
        p->max_mtime = std::max(p->max_mtime, edges[e].from->max_mtime);
    }

    if (p->files.size()) //file[0] is output of current GraphNode
        is_stale |= (p->files[0]->mem_mtime < p->max_mtime);

    p->_status = is_stale? GraphNode::Stale : GraphNode::Latest;
} //Graph::test_status()

GraphNode *Graph::get_node(const std::string &name)
{
    if (auto fd = nodes.find(name); fd != nodes.end())
        return &(fd->second);
        
    GraphNode &node = nodes[name];
    node._title = get_graph_string(name);
    db_pending_write_node.insert(&node);

    return &node;
} //Graph::get_node()


void Graph::set_unknown_as_default_state(GraphNode *p) {
    if (!p->init_state_is_stale)
        return ;
    p->init_state_is_stale = false;
    db_pending_write_node.insert(p);
}
void Graph::set_stale_as_default_state(GraphNode *p) {
    if (p->init_state_is_stale)
        return ;
    p->init_state_is_stale = true;
    db_pending_write_node.insert(p);
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
    db_pending_write_node.insert(p);
} //Graph::remove_inbound_edges()

void Graph::set_node_status_to_latest(GraphNode *p)
{
    if (p->status == GraphNode::Latest)
        return ;

    // stat and write file mtime into db
    for (auto &ff : p->_file_blocks) {
        ff->mem_mtime = stat_and_cache(*ff->strkey);
        db_pending_write_str.insert(ff);
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
} //Graph::set_node_status_to_stale()

void Graph::set_node_files(GraphNode *p, std::vector<std::string> in)
{
    //check modified or not
    bool is_same = (p->files.size() == in.size());
    if (is_same) {
        std::unordered_set<std::string> s1{in.begin(), in.end()};
        for (auto blk : p->files)
            if (s1.count(*blk->strkey) == 0) {
                is_same = false; break;
            }
    }
    if (is_same)
        return;

    //clear the previous record and write to db if modified
    p->_file_blocks.clear();
    for (auto &fpath : in)
        p->_file_blocks.push_back(get_graph_string(fpath));
    set_node_status_to_unknown(p);
    db_pending_write_node.insert(p);
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

Graph::Graph(Logger *logger) : logger(logger) {
    //EdgeID == 0 (nullptr)
    GraphEdge e;
    e.from = e.to = 0;
    e.next = e.rnext = 0;
    edges.push_back(e);
}
Graph::~Graph() {
    db_flush();
    if (file_)
        fclose(file_);
}

void Graph::db_load(const std::string &filename)
{
    constexpr const char version[DB_VERSION_FIELD_SIZE] = "CGN-demo";
    std::ifstream fin(filename, std::ios::binary);

    auto fn_create_new = [&](const std::string &errmsg = "") {
        if (fin)
            fin.close();

        if (errmsg.size())
            logger->paragraph("GraphDB load error: " + errmsg);
        logger->println("Create new GraphDB: " + filename);

        //db_load: block title at err_offset
        std::filesystem::remove(filename);

        nodes.clear();
        edges.clear();
        edge_recovery_pool.clear();

        db_strings.clear();
        db_recycle_pool.clear();
        db_pending_write_node.clear();
        db_pending_write_str.clear();

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
        return fn_create_new("version conflict");
    
    //after parse all db, convert node_blocks to GraphNode and save to this->nodes[]
    std::unordered_map<uint32_t, GraphString*> str_by_offset;
    std::vector<Graph::DBNodeBlock> node_blocks;

    for(std::size_t pos = DB_VERSION_FIELD_SIZE; pos < buf.size();) {
        uint32_t *title = (uint32_t*)(buf.data() + pos);
        if (pos + sizeof(uint32_t) > buf.size())  //error if dbfile too small
            return fn_create_new("unexpected EOF, blkoff=" + std::to_string(pos));

        uint32_t block_type = (*title &  (0b11UL<<30));
        uint32_t body_len   = (*title & ~(0b11UL<<30));

        if (block_type == DB_BIT_EMPTY) { // type empty field
            db_recycle_pool.insert({pos, (uint32_t)body_len});
            pos += body_len;
            assert(pos % sizeof(uint32_t) == 0); //align check
        }

        else if (block_type == DB_BIT_STR) { // type string
            std::size_t aligned_size = sizeof(uint32_t) + sizeof(int64_t) + (body_len+3)/4*4;
            if (pos + aligned_size > buf.size())
                return fn_create_new("unexpected EOF, (str)blkoff=" + std::to_string(pos));

            GraphString block;
            block.self_offset = pos;
            block.mem_mtime = block.db_mtime = *(int64_t*)(buf.data() + pos + sizeof(uint32_t));
            std::string body{
                buf.data() + pos + sizeof(uint32_t) + sizeof(int64_t),
                body_len
            };
            
            auto [iter, nx] = db_strings.insert({body, std::move(block)});
            iter->second.strkey = &(iter->first);
            str_by_offset[pos] = &(iter->second);

            pos += aligned_size;
        }
        
        else if (block_type == DB_BIT_NODE) { // type GraphNode
            std::size_t aligned_size = sizeof(uint32_t) + sizeof(int32_t) 
                                     + body_len*sizeof(uint32_t);
            if (pos + aligned_size > buf.size())  //error if dbfile too small
                return fn_create_new("unexpected EOF, (node)blkoff=" + std::to_string(pos));

            auto &blk = node_blocks.emplace_back();
            blk.self_offset = pos;
            blk.init_state_is_stale = false;
            
            uint32_t *p_name_off = (uint32_t*)(buf.data() + pos + sizeof(uint32_t));
            if (*p_name_off & (1<<31))
                blk.init_state_is_stale = true, *p_name_off -= (1ul<<31);
            blk.title_offset = *p_name_off;

            for (size_t i=0; i<body_len; i++)
                blk.rel_off.push_back(*(p_name_off + 1 + i));

            pos += aligned_size;
        }
        else
            return fn_create_new(
                    "Invalid block type " + std::to_string(block_type) 
                    + " off=" + std::to_string(pos));
    } //for all blocks in file_

    // iterate node_blocks[] and create GraphNode*
    // outbound_edges[U node_offset] = (TargetNode*)V  U->V
    std::map<uint32_t, GraphNode*> nodes_by_titleoff;
    std::multimap<uint32_t, GraphNode*> outbound_edges;
    for (DBNodeBlock &blk : node_blocks) {
        GraphString *node_title;
        if (auto fd = str_by_offset.find(blk.title_offset); fd != str_by_offset.end())
            node_title = fd->second;
        else
            return fn_create_new("node " + std::to_string(blk.self_offset) + " no title found.");

        // create GraphNode by DBNodeBlock
        GraphNode &n = nodes[*node_title->strkey];
        n._title = node_title;
        nodes_by_titleoff[blk.title_offset] = &n;

        // process outbound_edges[] cache, add_edge and remove record from cache.
        for (auto e = outbound_edges.find(blk.title_offset); e != outbound_edges.end(); ) {
            if (e->first == blk.title_offset) {
                add_edge(&n, e->second);
                e = outbound_edges.erase(e);
            }
            else
                break;
        }

        // parse blk.rels[]
        n.init_state_is_stale = blk.init_state_is_stale;
        for (auto marked_rel : blk.rel_off) {
            uint32_t real_off = marked_rel & ((1UL<<31) - 1);
            bool is_edge = (marked_rel & (1UL<<31)) != 0;
            if (!is_edge) {
                auto fd = str_by_offset.find(real_off);
                if (fd == str_by_offset.end())
                    return fn_create_new(
                        "node[off=" + std::to_string(blk.self_offset) + "] "
                        " cannot find file string by offset=" + std::to_string(real_off)
                    );
                n._file_blocks.push_back(fd->second);
            }
            else {
                // cache if the GraphNode* haven't created
                // otherwise add_edge()
                auto fd = nodes_by_titleoff.find(real_off);
                if (fd == nodes_by_titleoff.end())
                    outbound_edges.insert({real_off, &n});
                else
                    add_edge(fd->second, &n);
            }
        }

        if (n.init_state_is_stale)
            n._status = GraphNode::Stale;

        // move blk into GraphNode
        n.filedb = std::move(blk);
    }

    if (outbound_edges.size())
        return fn_create_new("some GraphNode record missing, there still have "
            + std::to_string(outbound_edges.size()) + " edges cached.");
    
    //debug
    {
        auto str_status = [](GraphNode::Status s) {
            if (s == GraphNode::Latest)
                return "Latest";
            else if (s == GraphNode::Stale)
                return "Stale";
            return "Unknown";
        };
        std::stringstream ss;
        ss<<"GraphDB load(): "<< nodes.size() <<" GraphNodes loaded."
           <<" (file size "<< buf.size()<<" bytes)\n";
        for (auto &[name, node] : nodes) {
            ss<<" Node["<<name<<"] off="<<node.filedb.self_offset
              <<" status="<< str_status(node.status) <<"\n";
            for (auto e = node.head; e; e=edges[e].next)
                ss<<"   |--> ["<< *edges[e].to->_title->strkey <<"]\n";
            for (auto it : node.files) {
                ss<<"   | mtime="<< it->mem_mtime<< " "<<*(it->strkey)<<"\n";
            }
        }
        ss<<"=================================================================\n";
        logger->verbose_paragraph(ss.str());
    }

    // close stream and open file by fopen()
    fin.close();
    file_ = fopen(filename.c_str(), "r+b");

} //Graph::db_load()

void Graph::db_flush()
{
    constexpr static const char fill0[4] = {0, 0, 0, 0};
    // ** STEP1 **
    // write GraphString
    for (GraphString *s : db_pending_write_str)
    {
        // create new if not found
        if (s->self_offset == 0) {
            file_seek_new_block(s->db_block_size());
            s->self_offset = ftell(file_);
            s->db_mtime    = s->mem_mtime;

            uint32_t title = s->strkey->size() + DB_BIT_STR;
            fwrite(&title,        sizeof(title), 1, file_);
            fwrite(&s->mem_mtime, sizeof(s->mem_mtime), 1, file_);
            fwrite(s->strkey->c_str(), s->strkey->size(), 1, file_);
            if (s->strkey->size() % 4 != 0)  //padding to int32_t (x4 bytes)
                fwrite(fill0, 4 - s->strkey->size()%4, 1, file_);
            logger->verbose_paragraph(
                "GraphDB new string '" + *s->strkey 
                + "' blk_off=" + std::to_string(s->self_offset)
            );
        }
        else if(s->db_mtime != s->mem_mtime) {
            fseek(file_, s->self_offset + sizeof(uint32_t), SEEK_SET);
            fwrite(&(s->mem_mtime), sizeof(s->mem_mtime), 1, file_);
            logger->verbose_paragraph(
                "GraphDB update mtime " + *(s->strkey) 
                + " " + std::to_string(s->db_mtime) 
                + " -> " + std::to_string(s->mem_mtime)
            );
            s->db_mtime = s->mem_mtime;
        }
    } //end_for(db_pending_write_str)
    db_pending_write_str.clear();

    // ** STEP 2 : write GraphNode **
    // for each node, calculate the rels[] body and compare with
    // n->filedb->rels[], write to file if not equal.
    for (GraphNode *n : db_pending_write_node)
    {
        std::vector<uint32_t> newdata;
        for (auto f : n->files)
            newdata.push_back(f->self_offset);
        for (GraphEdgeID eid = _iter_edge(n->rhead, false); eid; 
        eid = _iter_edge(edges[eid].rnext, false))
            newdata.push_back(edges[eid].from->_title->self_offset | (1UL<<31));

        // skip if same with last data
        // (the first file is output file[0])
        if (newdata.size())
            std::sort(++newdata.begin(), newdata.end());
        if (n->filedb.self_offset != 0 &&
            n->init_state_is_stale == n->filedb.init_state_is_stale &&
            newdata == n->filedb.rel_off)
                continue;
        
        //seek and write data into db
        // 1) modify the previous block (if existed)
        //    off + <u32 title> + <u32 selfname> + <rel_blocks>...
        // 2) write a new record
        if (n->filedb.self_offset && newdata.size() == n->filedb.rel_off.size()) {
            uint32_t field_titleoff_u4 = n->_title->self_offset;
            if (n->init_state_is_stale)
                field_titleoff_u4 |= (1<<31);
            fseek(file_, n->filedb.self_offset + sizeof(uint32_t), SEEK_SET);
            fwrite(&field_titleoff_u4, sizeof(uint32_t), 1, file_);
            fwrite(newdata.data(), sizeof(uint32_t), newdata.size(), file_);
        }
        else {
            if (n->filedb.self_offset)
                file_mark_recycle(n->filedb.self_offset, n->filedb.db_block_size());
            file_seek_new_block(n->filedb.db_block_size());

            uint32_t field_blktype_u4 = newdata.size() + DB_BIT_NODE;
            uint32_t field_titleoff_u4 = n->_title->self_offset;
            if (n->init_state_is_stale)
                field_titleoff_u4 |= (1<<31);
            fwrite(&field_blktype_u4,  sizeof(field_blktype_u4),  1, file_);
            fwrite(&field_titleoff_u4, sizeof(field_titleoff_u4), 1, file_);
            fwrite(newdata.data(), sizeof(uint32_t), newdata.size(), file_);
            n->filedb.self_offset = ftell(file_);
            n->filedb.title_offset = n->_title->self_offset;
        }
        n->filedb.init_state_is_stale = n->init_state_is_stale;
        n->filedb.rel_off = newdata;
        { //debug
            std::string logtxt = "GraphDB upsert Node[" + *n->_title->strkey + "] : ";
            for (auto it : n->files)
                logtxt += *it->strkey + ", ";
            logger->verbose_paragraph(logtxt);
        }
    } //end_for(db_pending_write_node)
    db_pending_write_node.clear();

    if (int flush_rv = fflush(file_); flush_rv != 0)
        throw std::runtime_error{"GraphDB fflush() return " + std::to_string(flush_rv)};

} //Graph::db_flush()

Graph::GraphString *Graph::get_graph_string(const std::string &str)
{
    auto [iter, nx] = db_strings.emplace(str, GraphString{});
    if (nx){
        iter->second.strkey = &(iter->first);
        db_pending_write_str.insert(&(iter->second));
    }
    return &(iter->second);
}

void Graph::file_seek_new_block(std::size_t want_size)
{
    // TODO: alloc block from this->db_recycle_pool
    fseek(file_, 0, SEEK_END);
}

void Graph::file_mark_recycle(std::size_t offset, uint32_t whole_block_size)
{
    { //debug
        std::stringstream ss;
        ss<<"GraphDB: recycle OFFSET "<< offset
          <<" -> "<<offset + whole_block_size<<std::endl;
        logger->verbose_paragraph(ss.str());
    }
    uint32_t block_type = whole_block_size | DB_BIT_EMPTY;
    fseek(file_, offset, SEEK_SET);
    fwrite(&block_type, sizeof(block_type), 1, file_);

    db_recycle_pool.insert({offset, whole_block_size});
}

} //namespace