#ifndef FRAGS_ORDER_H
#define FRAGS_ORDER_H

#include "utilsPretextView.h"


struct FragsOrder
{
private:
    u32 num_frags;                  // number of all the fragments
    std::vector<std::vector<s32>> order;        // [num_chromosomes, num_frags_in_chromosomes], + - represents the direction of the fragment, start from 1
    void cleanup()
    {   
        num_frags = 0;
        if (order.size() > 0)
        {
            for (u32 i = 0; i < order.size(); i++)
            {
                if (order[i].size() > 0) order[i].clear();
            }
            order.clear();
        }
    }

public:
    FragsOrder(u32 num_frags_)
    {
        cleanup();
        num_frags = num_frags_;
        order.push_back(std::vector<s32>(num_frags));
        for (s32 i = 0; i < num_frags; i++) order[0][i] = i + 1;
        return ;
    }

    FragsOrder(std::vector<int> order) // order start from 1, +/- represents the direction of the fragment
    {
        cleanup();
        num_frags = order.size();
        this->order.push_back(std::vector<s32>(num_frags));
        for (s32 i = 0; i < num_frags; i++) this->order[0][i] = order[i];
        return ;
    }

    FragsOrder( // merge to build new fragsOrder
        const std::vector<FragsOrder>& frags_order_list, 
        const std::vector<std::vector<s32>>& clusters)
    {   
        cleanup();
        for (u32 i = 0; i < frags_order_list.size(); i++) // iterate each cluster
        {   
            this->num_frags += clusters[i].size();
            auto tmp_order = frags_order_list[i].get_order_without_chromosomeInfor();
            for (auto& tmp:tmp_order)
                tmp = (clusters[i][std::abs(tmp)-1] + 1) * (tmp>0?1:-1); 
            this->order.push_back(std::move(tmp_order));
        }
    }

    ~FragsOrder()
    {   
        cleanup();
    }
    
    u32 get_num_frags() const {return num_frags;}

    void set_order(const std::vector<std::deque<s32>>& chromosomes)
    {   
        cleanup(); // make sure the memory is released
        for (auto chromosome : chromosomes) 
        {
            if (chromosome.size()==0) continue;
            num_frags += chromosome.size();
            order.emplace_back(chromosome.begin(), chromosome.end());
        }
    }

    std::vector<s32> get_order_without_chromosomeInfor() const
    {
        if (num_frags == 0)
        {
            std::cerr << "Error: the order is not set yet" << std::endl;
            assert(0);
        }
        std::vector<s32> output;
        for (auto& chromorsome:order) for (auto& frag: chromorsome) output.push_back(frag);
        return output;
    }

    void print_order() const
    {
        if (num_frags == 0)
        {
            std::cerr << "Error: the order is not set yet" << std::endl;
            assert(0);
        }
        std::cout << "[Pixel Sort::print_order] Sorting results \n";
        for (u32 i = 0; i < order.size(); i++)
        {
            std::cout << "\tChr [" << i+1 << "]: ";
            for (u32 j = 0; j < order[i].size(); j++)
            {
                std::cout << order[i][j] << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
        return ;
    }
};




#endif // FRAGS_ORDER_H
