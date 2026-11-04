#ifndef YAHS_SORT_H
#define YAHS_SORT_H

// likelihood table
#include "likelihood_table.h"
// frags_order
#include "frags_order.h"
#include <memory>


// Graph-related structures
struct GraphArc 
{
    uint32_t v;      // upper 31 bits: node id; LSB: orientation
    uint32_t w;      // upper 31 bits: node id; LSB: orientation
    double wt;       // edge weight
    int64_t link_id; // arc id
    bool del=false;        // marked for deletion
    bool comp;       // complementary arc

    GraphArc(uint32_t v, uint32_t w, double wt, int64_t link_id, bool comp)
        : v(v), w(w), wt(wt), link_id(link_id), comp(comp) {}
};

class Graph 
{
public:
    Graph() = default;
    ~Graph() = default;
    
    // Basic graph operations
    // Add an arc
    int64_t add_arc(uint32_t v, uint32_t w, double wt, int64_t link_id, bool comp = false) ;
    // Preallocate arc storage
    void reserve_arcs(size_t size) {
        arcs.reserve(size);
    }
    // Outgoing arc count for node v
    size_t get_arc_count(uint32_t v) const {
        if (v >= idx.size()) return 0;
        return idx[v] & 0xFFFFFFFF; // lower 32 bits
    }
    // Pointer to outgoing arcs for node v
    GraphArc* get_arcs(uint32_t v) {
        if (v >= idx.size()) return nullptr;
        uint64_t offset = idx[v] >> 32;
        return &arcs[offset];
    }
    // Const pointer to outgoing arcs for node v
    const GraphArc* get_arcs(uint32_t v) const {
        if (v >= idx.size()) return nullptr;
        uint64_t offset = idx[v] >> 32;
        return &arcs[offset];
    }
    // Total number of arcs
    size_t get_arc_size() const { return arcs.size(); }

    // Graph maintenance
    // Sort arcs by head vertex
    void arc_sort();
    // Build per-vertex arc index
    void arc_index();
    // Remove arcs marked deleted
    void clean(bool shear);
    
    // Graph trimming passes
    // Weight-based simple filter
    int trim_simple_filter(double min_wt, double min_diff_h, double min_diff_l, int min_len);
    // Trim tip arcs
    int trim_tips();
    // Trim blunt ends
    int trim_blunts();
    // Trim repeat motifs
    int trim_repeats();
    // Trim transitive edges
    int trim_transitive_edges();
    // Trim bubble structures
    int trim_pop_bubbles();
    // Trim undirected pops
    int trim_pop_undirected();
    // Trim weak edges
    int trim_weak_edges();
    // Trim self-loops
    int trim_self_loops();
    // Trim ambiguous edges
    int trim_ambiguous_edges();

private:
    std::vector<GraphArc> arcs;
    std::vector<uint64_t> idx;  // per-vertex arc range in arcs (offset | count)
    
    // Helpers
    bool is_mwt(uint32_t v, uint32_t w) const; // true if max-weight arc from v goes to w
    bool exist_strong_edge(uint32_t v, double wt) const; // true if v has an arc with weight > wt
};




class YaHS
{
    
private:
    double min_norm_;
    double la_;
    
public:
    YaHS() = default;
    
    // Build graph from link scores
    Graph* build_graph_from_links(const LikelihoodTable& likelihood_table, double min_norm, double la);
    
    // Run YaHS scaffolding / ordering
    std::vector<std::vector<uint32_t>> yahs_sorting(const LikelihoodTable& likelihood_table, double min_norm = 0.1, double la = 0.1);
};


#endif // YAHS_H 