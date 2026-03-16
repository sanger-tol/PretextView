/*
Copyright (c) 2024 Shaoheng Guan, Wellcome Sanger Institute

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "frag_sort.h"


void dfs(
    u32 i, 
    std::vector<bool>& visited, 
    std::deque<s32>& order, 
    const f32& threshold, 
    const LikelihoodTable& likelihood_table,
    bool head_flag,
    bool inversed_flag)
{   
    s32 tmp_i;
    if (inversed_flag) tmp_i = - (s32)(i+1);
    else tmp_i = (s32)(i+1);
    if (head_flag) order.push_front(tmp_i);
    else order.push_back(tmp_i);
    visited[i] = true;

    // Check if all nodes are visited
    if (std::all_of(visited.begin(), visited.end(), [](bool v) { return v; }))  return;

    // find the best neighbor of the head and the end
    s32 max_id;
    f32 max_val = -1e9;
    head_flag = true;
    // from head
    u32 head_id = std::abs(order.front()) - 1;
    bool head_is_inversed = order.front() < 0;
    for (u32 j = 0; j < likelihood_table.get_num_frags(); j++)
    {   
        if ( j == head_id || visited[j] ) continue;

        if (!head_is_inversed) // check the start
        {
            if (likelihood_table(head_id, j, 0) > max_val) // link to the start
            {
                max_val = likelihood_table(head_id, j, 0);
                max_id = -(s32)j;
            }
            if (likelihood_table(head_id, j, 1) > max_val) // link to the end
            {
                max_val = likelihood_table(head_id, j, 1);
                max_id = (s32)j;
            }
        }
        else // check the end
        {
            if (likelihood_table(head_id, j, 2) > max_val) // link to the start
            {
                max_val = likelihood_table(head_id, j, 2);
                max_id = -(s32)j;
            }
            if (likelihood_table(head_id, j, 3) > max_val) // link to the end
            {
                max_val = likelihood_table(head_id, j, 3);
                max_id = (s32)j;
            }
        }
    }
    // from end
    u32 end_id = std::abs(order.back()) - 1;
    bool end_is_inversed = order.back() < 0;
    for (u32 j = 0; j < likelihood_table.get_num_frags(); j++)
    {
        if ( j == end_id || visited[j] ) continue;

        if (!end_is_inversed) // check the end
        {
            if (likelihood_table(end_id, j, 2) > max_val) // link to the start
            {
                max_val = likelihood_table(end_id, j, 2);
                max_id = (s32)j;
                head_flag = false;
            }
            if (likelihood_table(end_id, j, 3) > max_val) // link to the end
            {
                max_val = likelihood_table(end_id, j, 3);
                max_id = -(s32)j;
                head_flag = false;
            }
        }
        else // check the start
        {
            if (likelihood_table(end_id, j, 0) > max_val) // link to the start
            {
                max_val = likelihood_table(end_id, j, 0);
                max_id = (s32)j;
                head_flag = false;
            }
            if (likelihood_table(end_id, j, 1) > max_val) // link to the end
            {
                max_val = likelihood_table(end_id, j, 1);
                max_id = -(s32)j;
                head_flag = false;
            }
        }
    }

    // check the threshold
    if (max_val < threshold) return;
    
    // dfs
    inversed_flag = max_id < 0;
    dfs(std::abs(max_id), visited, order, threshold, likelihood_table, head_flag, inversed_flag);
    
    return ;
}


void FragSortTool::sort_according_likelihood_dfs(
    const LikelihoodTable& likelihood_table, 
    FragsOrder& frags_order, 
    const f32 threshold, 
    const Frag4compress* frags) const 
{

    // sort the fragments according to length

    // search for the order via dfs
    std::vector<bool> visited(likelihood_table.get_num_frags(), false);
    std::vector<std::deque<s32>> chromosomes;
    for (u32 i = 0; i < likelihood_table.get_num_frags(); i++)
    {
        if (visited[i]) continue;
        if (std::all_of(visited.begin(), visited.end(), [](bool v) { return v; }))  break;

        std::deque<s32> chromosome;
        dfs(i, visited, chromosome, threshold, likelihood_table, true, false);
        chromosomes.push_back(chromosome);
    }

    // sort the chromosomes according to the length
    if (frags)
    {
        std::sort(
            chromosomes.begin(), 
            chromosomes.end(), 
            [&frags](const std::deque<s32>& a, const std::deque<s32>& b){
                u32 len_a = 0, len_b = 0;
                for (auto i : a) { u32 idx = (u32)std::abs(i); len_a += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                for (auto i : b) { u32 idx = (u32)std::abs(i); len_b += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                return len_a > len_b;
            });
    }

    // set the order
    frags_order.set_order(chromosomes);

    return ;
}


bool is_head_or_end(const std::deque<s32>& chromosome, const u32& a)
{
    if (std::abs(chromosome.front())-1 == a || std::abs(chromosome.back())-1 == a) return true;
    else return false;
}


// decide whether fragment a can be attached with the link
bool is_attachable(bool is_front, bool is_inversed, bool is_head_linked)
{
    if ((is_front && is_inversed) || (!is_front && !is_inversed)) return is_head_linked? false:true; // end available
    else if ((is_front && !is_inversed) || (!is_front && is_inversed)) return is_head_linked? true:false; // start available
    else
    {
        std::cerr << "Error: is_attachable" << std::endl;
        assert(0);
    }
    return false;
}


void initilise_chromosomes(
    std::vector<std::deque<s32>>& chromosomes, 
    std::vector<std::deque<s32>>& chromosomes_excluded, 
    std::vector<s32>& chain_id, 
    const u32& n, // total number of fragments
    const LikelihoodTable& likelihood_table)
{
    for (u32 i=0; i < n; i++) 
    {
        if (likelihood_table.excluded_fragment_idx.count(i)!=0)  // excluded fragment
        {
            chromosomes_excluded.push_back({(s32)i+1});
        }
        else
        {
            chain_id[i] = chromosomes.size();
            chromosomes.push_back({(s32)i+1});
        }
    }
}

/*
Conditions:
1.The head of a source fragment cannot be connected.
2.The tail of a sink fragment cannot be connected.
3.A source and a sink cannot be connected to each other.
    return: 
            0: not attachable, 
            1: attachable
*/
bool check_source_sink_attachable(
    const Link& lnk, 
    u32 source_frag_id, 
    u32 sink_frag_id, 
    const u32& source_chain_id, 
    const u32& sink_chain_id,
    const u32& ca, 
    const u32& cb
)
{   
    // source and sink can not be linked to the same chain
    if ((source_chain_id == ca && sink_chain_id == cb) || 
        (sink_chain_id == ca && source_chain_id == cb)) return 0;

    source_frag_id--; sink_frag_id -- ; // In the link set, id starts from 0, but source_frag_id starts from 1.

    // source and sink can not be linked to each other
    if ((lnk.frag_a == source_frag_id && lnk.frag_b==sink_frag_id)||
        (lnk.frag_b == source_frag_id && lnk.frag_a==sink_frag_id))
        return 0;

    // source can not be linked to the head
    if ( source_frag_id >= 0 && 
        ((lnk.frag_a == source_frag_id && lnk.is_a_head_linked) || 
        (lnk.frag_b == source_frag_id && lnk.is_b_head_linked))
    ) 
        return 0;
    // sink can not be linked to the end
    if ( sink_frag_id >= 0 && 
        ((lnk.frag_a == sink_frag_id && !lnk.is_a_head_linked) || 
        (lnk.frag_b == sink_frag_id && !lnk.is_b_head_linked))
    ) 
        return 0;
    return 1;
}

/*
reverse the chain and return the new chain_id
*/
void reverse_chain(
    std::deque<s32>& cha)
{
    std::reverse(cha.begin(), cha.end());
    for (auto &val: cha) val = -val; // flip orientation
}

void reverse_chain(
    std::deque<s32>& cha,
    u32& i // frag_id in the chain
)
{
    std::reverse(cha.begin(), cha.end());
    for (auto &val: cha) val = -val; // flip orientation
    i = cha.size() - 1 - i;
}



/* 
union find operation
*/
void union_find_process(
    const std::vector<Link>& all_links, 
    std::vector<std::deque<s32>>& chromosomes, 
    SelectArea& select_area, 
    std::vector<s32> & chain_id, 
    const s32& source_frag_id, 
    const s32& sink_frag_id,
    s32& source_chain_id,
    s32& sink_chain_id
)
{
    // Process links in order of decreasing score
    for (const Link &lnk : all_links)
    {
        u32 a = lnk.frag_a;
        u32 b = lnk.frag_b;
        s32 ca = chain_id[a], cb = chain_id[b];

        if (ca < 0 || cb < 0) 
        {
            fprintf(stderr, "Error: ca(%d) or cb(%d) should >= 0, only chain_id of excluded fragment can be smaller than 0.\n", ca, cb);
            assert(0);
        }

        if (ca == cb) continue;


        // Decide how to connect them:
        auto &cha = chromosomes[ca];
        auto &chb = chromosomes[cb];

        // if a/b is the source, it can only be linked to the tail
        // if a/b is the sink, it can only be linked to the head
        // if a and b are source and sink or sink and source, they can not be linked
        // source and link can not be linked into one chromosome
        if (!check_source_sink_attachable(lnk, source_frag_id, sink_frag_id, source_chain_id, sink_chain_id, ca, cb)) continue;
        
        // find the index i/j of a/b on cha/chb
        u32 i = 0, j = 0;
        for (; i < cha.size(); i++)
            if (std::abs(cha[i]) == a+1)
                break;
        for (; j < chb.size(); j++)
            if (std::abs(chb[j]) == b+1)
                break;
        if (i == cha.size() || j == chb.size())
        {
            fprintf(stderr, "Error: i(%d should no more than %zu)/j(%d should no more than %zu) not found\n", i, cha.size(), j, chb.size());
            assert(0);
        }
        
        // Reverse a and/or b if necessary to make it always a->b
        if (lnk.is_a_head_linked != (cha[i] < 0))
        {
            reverse_chain(cha, i);
        }
        if (lnk.is_b_head_linked != (chb[j] > 0))
        {
            reverse_chain(chb, j);
        }
        
        // join cha and chb if a is the last and b is the first
        if (i == cha.size() - 1 && j == 0) 
        {
            std::copy(chb.begin(), chb.end(), std::back_inserter(cha));
            for (auto &val: chb)
                chain_id[std::abs(val)-1] = ca;
            if (sink_chain_id == cb) sink_chain_id = ca;
            if (source_chain_id == cb) source_chain_id = ca;
            chb.clear();
        }
    }

}


void fix_source_sink_frag(
    std::vector<std::deque<s32>>& chromosomes, 
    SelectArea& select_area, 
    const s32& source_frag_id,
    const s32 sink_frag_id
)
{   

    if (select_area.selected_frag_ids.empty() || (sink_frag_id < 0 && source_frag_id < 0 ) ) return ;

    for (u32 i = 0; i < chromosomes.size() ; i ++ )
    {   
        std::deque<s32>& tmp = chromosomes[i];
        if (tmp.empty()) continue;
        
        if (std::abs(tmp.back()) == source_frag_id && tmp.size() > 1 ) 
        {
            if (tmp.back() > 0) 
            {
                fmt::print(
                    stderr,
                    "Error: source_frag({})'s head should not be linked, chromosome[{}].back() = {} \n", 
                    source_frag_id, 
                    i, 
                    tmp.back());
                assert(0);
            } 
            reverse_chain(tmp);
        }
        if (std::abs(tmp.front()) == source_frag_id && i != 0)
        {
            std::swap(tmp, chromosomes.front());
            continue;
        }

        if (std::abs(tmp.front()) == sink_frag_id && tmp.size() > 1) 
        {
            if (tmp.front() > 0 )
            {
                fmt::print(
                    stderr,
                    "Error: the sink_frag_id({})'s end should not be linked, chromosome[{}].front() = {} \n", 
                    sink_frag_id, 
                    i, 
                    tmp.front());
                assert(0);
            }
            reverse_chain(tmp);
        }
        if (std::abs(tmp.back()) == sink_frag_id && i != chromosomes.size()-1)
        {   
            std::swap(tmp, chromosomes.back());
            continue;
        }
    }
}



FragSortTool::FragSortTool() {}

FragSortTool::~FragSortTool() {}



void FragSortTool::sort_according_likelihood_unionFind(
    const LikelihoodTable& likelihood_table, 
    FragsOrder& frags_order, 
    SelectArea& select_area,
    const f32 threshold, 
    const Frag4compress* frags, 
    const bool sort_according_len_flag
) const
{   
    // this is used to fix the source and sink during local sort
    s32 source_frag_id = -1, source_chain_id = -1, 
        sink_frag_id = -1,   sink_chain_id = -1;
    if (select_area.source_frag_id >=0)
    {
        source_frag_id = 1;
        source_chain_id = 0;
    }
    if (select_area.sink_frag_id >=0)
    {
        sink_frag_id = select_area.get_to_sort_frags_num();
        sink_chain_id = sink_frag_id - 1;
    }

    u32 n = likelihood_table.get_num_frags();
    std::vector<Link> all_links;

    // 计算的分位数
    f32 threshold_val = percentile_cal(
        likelihood_table.data, likelihood_table.size, this->threshold_ratio);
    f32 threshold_using = std::min(threshold_val, threshold);
    fmt::print(stderr, "[Pixel Sort] threshold at {}: {}, fixed_threshold: {}, using: {}\n", this->threshold_ratio, threshold_val, threshold, threshold_using);

    std::vector<std::pair<bool, bool>> link_kinds = {
        {true,  true}, {true,  false}, 
        {false, true}, {false, false}};
    for (u32 i = 0; i < n; i++) 
    {
        // Skip excluded fragments
        if (likelihood_table.excluded_fragment_idx.count(i) != 0) continue;
        
        for (u32 j = i+1; j < n; j++)
        {
            // Skip excluded fragments
            if (likelihood_table.excluded_fragment_idx.count(j) != 0) continue;
            
            for (u32 k = 0; k < 4; k++)
            {   
                f32 val = likelihood_table(i, j, k);
                if (val >= threshold_using) {
                    all_links.push_back(
                        Link(val, i, j, link_kinds[k].first, link_kinds[k].second));
                }
            }
        }
    }

    // Sort all links by descending score
    std::sort(all_links.begin(), all_links.end(), [](const Link& a, const Link& b) {
        return a.score > b.score;
    });
    // initialize the chromosomes and union-find
    // every fragment is initilised as a not-inverted chain
    std::vector<std::deque<s32>> chromosomes;
    std::vector<s32> chain_id(n, -1);
    std::vector<std::deque<s32>> chromosomes_excluded;
    initilise_chromosomes(chromosomes, chromosomes_excluded, chain_id, n, likelihood_table);

    // run union find sort
    union_find_process(
        all_links, 
        chromosomes, 
        select_area,
        chain_id,
        source_frag_id, 
        sink_frag_id, 
        source_chain_id, 
        sink_chain_id
    );

    // sort the chromosomes according to the length
    if (frags && sort_according_len_flag)
    {
        std::sort(chromosomes.begin(), chromosomes.end(), 
            [&frags](const std::deque<s32>& a, const std::deque<s32>& b) {
                u32 len_a = 0, len_b = 0;
                for (auto &val: a) { u32 idx = (u32)std::abs(val); len_a += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                for (auto &val: b) { u32 idx = (u32)std::abs(val); len_b += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                return len_a > len_b;
            });
        
        std::sort(chromosomes_excluded.begin(), chromosomes_excluded.end(), 
            [&frags](const std::deque<s32>& a, const std::deque<s32>& b) {
                u32 len_a = 0, len_b = 0;
                for (auto &val: a) { u32 idx = (u32)std::abs(val); len_a += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                for (auto &val: b) { u32 idx = (u32)std::abs(val); len_b += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                return len_a > len_b;
            });
    }

    // fix source and sink to the head and end
    fix_source_sink_frag(
        chromosomes, 
        select_area, 
        source_frag_id, 
        sink_frag_id
    );

    // merge two vectors
    chromosomes.insert(chromosomes.end(), chromosomes_excluded.begin(), chromosomes_excluded.end());

    frags_order.set_order(chromosomes);
}


f32 link_score(
    const std::vector<Link>& all_links,
    const std::map<u64,u32>& map_links,
    s32 source, s32 sink)
{
    if (source == 0 || sink == 0)
        return 0;
    u64 k = (u64)(std::abs(source)<<1|(source<0?1:0))<<32|(std::abs(sink)<<1|(sink<0?1:0));
    auto it = map_links.find(k);
    if (it != map_links.end())
    {
        return all_links[it->second].score;
    }
    return -std::numeric_limits<f32>::infinity();
}


std::deque<s32> fuse_chromosomes(
    const std::vector<Link>& all_links,
    const std::map<u64, u32>& map_links,
    std::deque<s32>& cha,
    std::deque<s32>& chb,
    s32 source, 
    s32 sink,
    const f32 threshold
)
{
    u32 n = cha.size(), m = chb.size();
    cha.push_front(source);
    chb.push_front(source);

    std::vector<std::vector<f32>> dp_a(n+1, std::vector<f32>(m+1, -std::numeric_limits<f32>::infinity())); // highest score end with cha[i]
    std::vector<std::vector<f32>> dp_b(n+1, std::vector<f32>(m+1, -std::numeric_limits<f32>::infinity())); // highest score end with chb[j]

    dp_a[0][0] = dp_b[0][0] = 0;
    
    for (u32 j = 1; j <= m; j++)
    {
        dp_b[0][j] = dp_b[0][j-1] + link_score(all_links, map_links, chb[j-1], chb[j]);
    }

    std::vector<std::vector<u64>> bt(2, std::vector<u64>(((u64)n * m + 7) / 8 , 0)); // track switch 
    
    f32 link1, link2;
    u32 cnt = 0;
    for (u32 i = 1; i <= n; i++)
    {   
        dp_a[i][0] = dp_a[i-1][0] + link_score(all_links, map_links, cha[i-1], cha[i]);
        for (u32 j = 1; j <= m; j++)
        {   
            cnt = (i-1) * m + (j-1);

            // process dp_a[i][j]
            link1 = dp_a[i-1][j] + link_score(all_links, map_links, cha[i-1], cha[i]);
            link2 = dp_b[i-1][j] + link_score(all_links, map_links, chb[j],   cha[i]);
            dp_a[i][j] = std::max(link1, link2);
            bt[0][cnt/8] |= (u64)(link1 < link2 ? 1 : 0) << (cnt % 8); // true: switched from chb, false: from cha
            // process dp_b[i][j]
            link1 = dp_a[i][j-1] + link_score(all_links, map_links, cha[i],   chb[j]);
            link2 = dp_b[i][j-1] + link_score(all_links, map_links, chb[j-1], chb[j]);
            dp_b[i][j] = std::max(link1, link2);
            bt[1][cnt/8] |= (u64)(link1 < link2 ? 0 : 1) << (cnt % 8); // true: switched from cha, false: from chb
        }
    }

    f32 link, maxLink;
    link = 0;
    for (int i = 1; i < cha.size(); ++i)
        link += link_score(all_links, map_links, cha[i-1], cha[i]);
    for (int j = 2; j < chb.size(); ++j)
        link += link_score(all_links, map_links, chb[j-1], chb[j]);
    link += link_score(all_links, map_links, chb.back(), sink);
        
    link1 = dp_a[n][m] + link_score(all_links, map_links, cha[n], sink);
    link2 = dp_b[n][m] + link_score(all_links, map_links, chb[m], sink);
    maxLink = std::max(link1, link2);

    // link is not strong enough
    if (maxLink < link + threshold)
        return std::deque<s32>();

    // pop source
    cha.pop_front(); // n
    chb.pop_front(); // m

    // do backtracking
    std::deque<s32> fused(0);
    std::array<std::reference_wrapper<std::deque<s32>>, 2> 
        lists = {std::ref(cha), std::ref(chb)};
    std::array<s32, 2> sizes = {(s32)n - 1, (s32)m - 1};
    uint8_t k = link1>link2? 0 : 1;
    while(sizes[0] >= 0 && sizes[1] >= 0) // push cha and chb to fused according to backtracking matrix
    {
        cnt = sizes[0]*m + sizes[1];
        fused.push_front(lists[k].get()[sizes[k]--]);
        k ^= !!(bt[k][cnt/8]&((uint8_t)1<<(cnt%8))); // check if current step is swicthed from the other chain
    }
    while(sizes[0] >= 0)
        fused.push_front(lists[0].get()[sizes[0]--]);
    while(sizes[1] >= 0)
        fused.push_front(lists[1].get()[sizes[1]--]);
    
    return fused;
}


void FragSortTool::sort_according_likelihood_unionFind_doFuse(
    const LikelihoodTable& likelihood_table, 
    FragsOrder& frags_order, 
    SelectArea& select_area,
    const f32 threshold, 
    const Frag4compress* frags,
    const bool doStageOne,
    const bool doStageTwo, 
    bool sort_according_len_flag) const 
{   

    // this is used to fix the source and sink during local sort
    s32 source_frag_id = -1,  // 第一个片段的id
        sink_frag_id = -1,    // 最后一个片段的id
        source_chain_id = -1, // 第一个片段所在的chain的id
        sink_chain_id = -1;   // 最后一个片段所在的chain的id 
    if (select_area.source_frag_id >=0)
    {
        source_frag_id = 1;
        source_chain_id = 0;
    }
    if (select_area.sink_frag_id >=0)
    {
        sink_frag_id = select_area.get_to_sort_frags_num();
        sink_chain_id = sink_frag_id - 1;
    }

    // 计算的分位数
    f32 threshold_val = percentile_cal(likelihood_table.data, likelihood_table.size, this->threshold_ratio);
    if (threshold_val < 0)
    {   
        fmt::print(stderr, "[Pixel Sort::error]: threshold at {}: {}, fixed_threshold: {}, using: {}, table.size={} \n", this->threshold_ratio, threshold_val, threshold, threshold_val, likelihood_table.size);
        assert(0);
        threshold_val = 0;
    }
    f32 threshold_using = std::min(threshold_val, threshold);
    fmt::print(stderr, "[Pixel Sort]: link score threshold at {}: {}, fixed_threshold: {}, using: {}\n", this->threshold_ratio, threshold_val, threshold, threshold_using);

    // 建立所有的links 
    u32 n = likelihood_table.get_num_frags();
    std::vector<Link> all_links;
    std::map<u64, u32> map_links;
    
    std::vector<std::pair<bool, bool>> link_kinds = {
        {true,  true}, {true,  false}, 
        {false, true}, {false, false}};
    for (u32 i = 0; i < n; i++) 
    {
        // Skip excluded fragments
        if (likelihood_table.excluded_fragment_idx.count(i) != 0) continue;
        
        for (u32 j = i+1; j < n; j++)
        {
            // Skip excluded fragments
            if (likelihood_table.excluded_fragment_idx.count(j) != 0) continue;
            
            for (u32 k = 0; k < 4; k++)
            {   
                f32 val = likelihood_table(i, j, k);
                if (val >= threshold_using) {
                    all_links.push_back(
                        Link(val, i, j, link_kinds[k].first, link_kinds[k].second));
                }
            }
        }
    }

    // Sort all links by descending score
    std::sort(all_links.begin(), all_links.end(), [](const Link& a, const Link& b) {
        return a.score > b.score;
    });
    if (doStageTwo)
    {
        // Map sort links to frag pairs  (frga<<1|oria)<<32|(frgb<<1|orib)
        for (u32 i = 0; i < all_links.size(); i++)
        {
            auto &lnk = all_links[i];
            map_links[(u64)((lnk.frag_a+1)<<1|(lnk.is_a_head_linked?1:0))<<32|((lnk.frag_b+1)<<1|(lnk.is_b_head_linked?0:1))] = i;
            map_links[(u64)((lnk.frag_b+1)<<1|(lnk.is_b_head_linked?1:0))<<32|((lnk.frag_a+1)<<1|(lnk.is_a_head_linked?0:1))] = i; 
        }
    }
    // initialize the chromosomes and union-find
    // every fragment is initilised as a not-inverted chain
    std::vector<std::deque<s32>> chromosomes;
    std::vector<s32> chain_id(n, -1);
    std::vector<std::deque<s32>> chromosomes_excluded;
    initilise_chromosomes(chromosomes, chromosomes_excluded, chain_id, n, likelihood_table);

    // Process links in order of decreasing score
    if (doStageOne)
    {
        union_find_process(
            all_links, 
            chromosomes,
            select_area,
            chain_id, 
            source_frag_id,   
            sink_frag_id,     
            source_chain_id,  
            sink_chain_id     
        );
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    if (doStageTwo)
    {
        // Stage II make extra join by fusing chromosomes
        for (auto &lnk : all_links)
        {
            u32 a = lnk.frag_a;
            u32 b = lnk.frag_b;
            u32 ca = chain_id[a], cb = chain_id[b];

            if (ca == cb) continue;
            if (!check_source_sink_attachable(lnk, source_frag_id, sink_frag_id, source_chain_id, sink_chain_id, ca, cb)) continue;

            // Decide how to connect them:
            auto &cha = chromosomes[ca];
            auto &chb = chromosomes[cb];
            
            // Find the index i/j of a/b on cha/b
            u32 i = 0, j = 0;
            for (; i < cha.size(); i++)
                if (std::abs(cha[i]) == a+1)
                    break;
            for (; j < chb.size(); j++)
                if (std::abs(chb[j]) == b+1)
                    break;
            
            // Reverse a and/or b if necessary to make it always a->b
            if (lnk.is_a_head_linked != (cha[i] < 0))
            {
                reverse_chain(cha, i);
            }
            if (lnk.is_b_head_linked != (chb[j] > 0))
            {
                reverse_chain(chb, j);
            }
            
            // should not fuse the source or sink inside of the chain 
            if (std::abs(cha.back()) ==  source_frag_id || std::abs(cha.back()) ==  sink_frag_id || 
                std::abs(chb.front()) == source_frag_id || std::abs(chb.front()) == sink_frag_id 
            ) continue;

            // Now try to fuse cha and chb at position i and j
            // the fragments (i...end] on cha
            // and the fragments [beg...j) on chb
            // the sequence order in each chromosome keep unchanged
            // are fused to maximum the overall score (measured by link strength)
            std::deque<s32> atail(0); 
            if (cha.size() > i+1) 
                std::copy(cha.begin()+i+1, cha.end(), std::back_inserter(atail));
            std::deque<s32> bhead(chb.begin(), chb.begin()+j+1);

            // todo （3, Apr. 2025） Investigate why the data still gets corrupted during the deep fusion process.
            std::deque<s32> fused = fuse_chromosomes(
                all_links, 
                map_links, 
                atail,                       // cha 
                bhead,                       // chb
                cha[i],                      // source frag id
                chb.size()>(j+1)?chb[j+1]:0, // sink frag id
                threshold_using);
            if (!fused.empty()) 
            {
                if (cha.size() > i+1) 
                    cha.erase(cha.begin()+i+1, cha.end());                          // remove tail of cha
                std::copy(fused.begin(), fused.end(), std::back_inserter(cha));     // append the fused to cha
                if (chb.size() > j+1)  
                    std::copy(chb.begin()+j+1, chb.end(), std::back_inserter(cha)); // append the rest of chb to cha
                for (auto &val: chb)                                                // update chain_id of fragments in chb 
                    chain_id[std::abs(val)-1] = ca;
                if (sink_chain_id == cb) sink_chain_id = ca;
                if (source_chain_id == cb) source_chain_id = ca;
                chb.clear();
            }
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<f64> elapsed_seconds = end_time-start_time;
    fmt::print(
        stdout, 
        "[Pixel Sort]: fusing consumed time {:.2f}s\n", 
        elapsed_seconds.count()
    );

    // sort the chromosomes according to the length
    if (frags && sort_according_len_flag)
    {
        std::sort(chromosomes.begin(), chromosomes.end(), 
            [&frags](const std::deque<s32>& a, const std::deque<s32>& b) {
                u32 len_a = 0, len_b = 0;
                for (auto &val: a) { u32 idx = (u32)std::abs(val); len_a += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                for (auto &val: b) { u32 idx = (u32)std::abs(val); len_b += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                return len_a > len_b;
            });
        
        std::sort(chromosomes_excluded.begin(), chromosomes_excluded.end(), 
            [&frags](const std::deque<s32>& a, const std::deque<s32>& b) {
                u32 len_a = 0, len_b = 0;
                for (auto &val: a) { u32 idx = (u32)std::abs(val); len_a += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                for (auto &val: b) { u32 idx = (u32)std::abs(val); len_b += (idx >= 1 && idx <= frags->num) ? frags->length[idx-1] : 0; }
                return len_a > len_b;
            });
    }

    // fix source and sink to the head and end
    fix_source_sink_frag(
        chromosomes, 
        select_area, 
        source_frag_id, 
        sink_frag_id
    );

    // merge two vectors
    chromosomes.insert(chromosomes.end(), chromosomes_excluded.begin(), chromosomes_excluded.end());

    frags_order.set_order(chromosomes);
}

void FragSortTool::sort_according_yahs(
    const LikelihoodTable& likelihood_table,
    FragsOrder& frags_order,
    SelectArea& select_area,
    const f32 threshold,
    const Frag4compress* frags) const
{
    // Create YaHS instance
    YaHS yahs;
    
    // Run YaHS sorting with the given parameters
    auto chromosomes = yahs.yahs_sorting(likelihood_table, threshold, 0.1);
    
    // Convert chromosomes to frags_order format
    std::vector<std::deque<s32>> formatted_chromosomes;
    for (const auto& chromosome : chromosomes) {
        std::deque<s32> formatted_chromosome;
        for (uint32_t frag_id : chromosome) {
            formatted_chromosome.push_back(frag_id + 1); // Convert to 1-based indexing
        }
        formatted_chromosomes.push_back(std::move(formatted_chromosome));
    }
    
    // Set the order in frags_order
    frags_order.set_order(formatted_chromosomes);
    
}