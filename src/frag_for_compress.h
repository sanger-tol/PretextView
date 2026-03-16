
#ifndef FRAG_FOR_COMPRESS_H
#define FRAG_FOR_COMPRESS_H

#include <iostream>
#include "utilsPretextView.h"
#include "genomeData.h"
#include "auto_curation_state.h"
#include <fmt/core.h>


struct Frag4compress {
    u32 num;                      // number of fragments
    u32* frag_id       = nullptr;
    u32* startCoord    = nullptr; // global start coordinate
    u32* length        = nullptr;
    bool* inversed     = nullptr;
    u32 total_length;
    u64* metaDataFlags = nullptr;

    Frag4compress(const map_contigs* Contigs)
        : total_length(0)
    {
        re_allocate_mem(Contigs);
    }

    Frag4compress()
        : total_length(0){}

    ~Frag4compress()
    {
        cleanup();
    }

    void re_allocate_mem( // re-initialize frags within the selected area
        const map_contigs* Contigs,
        const SelectArea* select_area=nullptr, 
        bool use_for_cut_flag=false,
        bool cluster_flag=false
    )
    {
        cleanup();
        u08 using_select_area = (select_area != nullptr && select_area->select_flag) ? 1:0;

        std::vector<s32> selected_frag_ids_tmp; 
        if (using_select_area )
        {   
            selected_frag_ids_tmp = select_area->selected_frag_ids;
            this->num = select_area->selected_frag_ids.size();
            if (this->num < 2 && !use_for_cut_flag)
            {
                fmt::print(
                    stderr, 
                    "The number_of_select_fragments_for_sorting({}) should not be less than 2, file:{}, line:{}\n", this->num, __FILE__, __LINE__);
                assert(0);
            }

            // add the source and sink fragments to the the selected fragments
            if (select_area->source_frag_id >=0 && !use_for_cut_flag && !cluster_flag) 
            {
                this->num ++;
                selected_frag_ids_tmp.insert(selected_frag_ids_tmp.begin(), select_area->source_frag_id);
            }
            if (select_area->sink_frag_id >=0 && !use_for_cut_flag  && !cluster_flag) 
            {
                this->num ++;
                selected_frag_ids_tmp.push_back(select_area->sink_frag_id);
            }
        }
        else // global sort or cut
        {   
            this->num = Contigs->numberOfContigs;
            selected_frag_ids_tmp.resize(this->num);
            std::iota(selected_frag_ids_tmp.begin(), selected_frag_ids_tmp.end(), 0);
        }

        this->frag_id = new u32[num];
        this->startCoord = new u32[num];
        this->length = new u32[num];
        this->inversed = new bool[num];
        this->metaDataFlags = new u64[num];
        this->total_length = 0;

        // Initialize the startCoord, length, and inversed
        frag_id[0] = selected_frag_ids_tmp[0];
        inversed[0] = false;
        startCoord[0] = 0;
        if (using_select_area)
        {
            s32 tmp = 0;
            while (tmp < frag_id[0])
            {   
                if (tmp >= Contigs->numberOfContigs)
                {
                    fmt::print(
                        stderr, 
                        "The frag_id[0]({}) should be less than the number_of_contigs({}), file:{}, line:{}\n", 
                        frag_id[0], Contigs->numberOfContigs, __FILE__, __LINE__);
                    assert(0);
                }
                startCoord[0] += Contigs->contigs_arr[tmp++].length;
            }
        }
        length[0] = Contigs->contigs_arr[frag_id[0]].length;
        metaDataFlags[0] = (Contigs->contigs_arr[frag_id[0]].metaDataFlags == nullptr)?
            0 : *(Contigs->contigs_arr[frag_id[0]].metaDataFlags);
        total_length = length[0];

        for (u32 i = 1; i < num; i++)
        {   
            frag_id[i] = selected_frag_ids_tmp[i];
            inversed[i] = false; // currently, this is not used
            startCoord[i] = startCoord[i-1] + length[i-1];
            length[i] = Contigs->contigs_arr[frag_id[i]].length;
            metaDataFlags[i] = (Contigs->contigs_arr[frag_id[i]].metaDataFlags == nullptr)?
                0 : *(Contigs->contigs_arr[frag_id[i]].metaDataFlags);
            total_length += length[i];
        }
    }

    void re_allocate_mem( // re-initialize frags within selected fragments
        const map_contigs* Contigs,
        const std::vector<s32>& selected_frag_ids_tmp
    )
    {
        cleanup();
        this->num = selected_frag_ids_tmp.size();
        if (this->num < 2)
        {
            fmt::print(
                stderr, 
                "[Frag4compress::re_allocate_mem::warning]: number_of_select_fragments_for_sorting({}) should not < 2, file:{}, line:{}\n", this->num, __FILE__, __LINE__);
        }

        this->frag_id = new u32[num];
        this->startCoord = new u32[num]();
        this->length = new u32[num];
        this->inversed = new bool[num];
        this->metaDataFlags = new u64[num];
        this->total_length = 0;

        // Initialize the startCoord, length, and inversed
        s32 global_frag_index = 0;
        for (s32 i = 0; i < num; i++) 
        {
            frag_id[i] = selected_frag_ids_tmp[i];
            inversed[i] = false;
            if ((u32)frag_id[i] >= Contigs->numberOfContigs)
            {
                fmt::print(stderr, "[Frag4compress::re_allocate_mem] frag_id[{}]={} >= numberOfContigs={}\n", i, frag_id[i], Contigs->numberOfContigs);
                assert(0);
            }
            length[i] = Contigs->contigs_arr[frag_id[i]].length;
            total_length += length[i];
            metaDataFlags[i] = (Contigs->contigs_arr[frag_id[i]].metaDataFlags == nullptr)?0:*(Contigs->contigs_arr[frag_id[i]].metaDataFlags);
            while (global_frag_index < frag_id[i])
            {   
                if (global_frag_index >= Contigs->numberOfContigs)
                {
                    fmt::print(
                        stderr, 
                        "The frag_id[0]({}) should be less than the number_of_contigs({}), file:{}, line:{}\n", 
                        frag_id[0], Contigs->numberOfContigs, __FILE__, __LINE__);
                    assert(0);
                }
                startCoord[i] += Contigs->contigs_arr[global_frag_index++].length;
            }
        }
    }
    

private:
    void cleanup()
    {   
        if (frag_id)
        {
            delete[] frag_id;
            frag_id = nullptr;
        }

        if (startCoord)
        {
            delete[] startCoord;
            startCoord = nullptr;
        }
        if (length)
        {
            delete[] length;
            length = nullptr;
        }
        if (inversed)
        {
            delete[] inversed;
            inversed = nullptr;
        }
        if (metaDataFlags)
        {
            delete[] metaDataFlags;
            metaDataFlags = nullptr;
        }
        total_length = 0;
        num = 0;
    }
};

#endif // FRAG4COMPRESS_H