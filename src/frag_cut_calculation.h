
#ifndef FRAG_CUT_CALCULATION_H
#define FRAG_CUT_CALCULATION_H


#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <string>
#include <limits>
#include "genomeData.h"
#include "utilsPretextView.h"
#include "copy_texture.h"
#include "auto_curation_state.h"



void cal_prefix_sum(
    const std::vector<f32>& arr, std::vector<f32>& prefix_sum) 
{
    int n = arr.size();
    for (int i = 0; i < n; ++i) 
    {
        prefix_sum[i + 1] = prefix_sum[i] + arr[i];
    }
    return ;
}




class FragCutCal
{
public: 
    TexturesArray4AI *textures_array = nullptr;
    Frag4compress* frags = nullptr;
    std::string file_save_dir = std::filesystem::current_path().string() + "/hic_pixel_density_curve";
    std::vector<f32> hic_pixel_density_origin={};
    std::vector<f32> hic_pixel_density={};
    const SelectArea* select_area = nullptr;

    const u32 pixel_mean_window_size;
    f32 cut_threshold=0.05f;
    u32 smallest_frag_size_in_pixel = 8;

    FragCutCal(
        TexturesArray4AI* textures_array_, 
        const u32& num_pixels_1d, 
        const u32 pixel_mean_window_size_)

        : textures_array(textures_array_), 
        pixel_mean_window_size(pixel_mean_window_size_)
    {   
        if (textures_array == nullptr)
        {
            MY_CHECK("The textures_array is not initialized");
            assert(0);
        }

        #ifdef DEBUG // create folder to save the hic_pixel_density_curve
            if (std::filesystem::exists(this->file_save_dir))
            {
                std::filesystem::remove_all(this->file_save_dir);
            }
            std::filesystem::create_directory(this->file_save_dir);
        #endif
        
        this->frags = textures_array->get_frags(); // original (unsorted) contig layout

        // cal the hic_pixel_density
        this->cal_pixel_density(num_pixels_1d);
    }

    void cal_pixel_density(u32 num_pixels_1d)
    {   
        this->hic_pixel_density_origin.resize(num_pixels_1d, 0.f);
        for (u32 frag_id = 0; frag_id < this->frags->num; frag_id++)
        {
            this->get_single_fragment_hic_pixel_density(
                this->frags->startCoord[frag_id],
                this->frags->length[frag_id],
                this->hic_pixel_density_origin.data() + this->frags->startCoord[frag_id]);
        }

        #ifdef DEBUG // output the hic_pixel_density to text file
            std::string filename = fmt::format("{}/hic_pixel_density_origin.txt", this->file_save_dir);
            std::ofstream out(filename);
            if (!out)
            {
                fmt::print(stderr, "Cannot open the file {}\n", filename);
                assert(0);
            }
            for (u32 i = 0; i < this->hic_pixel_density_origin.size(); i++)
            {
                out << this->hic_pixel_density_origin[i] << std::endl;
            }
            out.close();
            fmt::print("[Pixel Cut]: output hic_pixel_density to {}\n", filename);
        #endif
    }

    void rearrange_hic_pixel_density(const u32* pixel_rearrange_index)
    {   
        this->hic_pixel_density.resize(this->hic_pixel_density_origin.size(), 0.f);
        for (u32 i = 0; i < this->hic_pixel_density.size(); i++)
        {   
            if (pixel_rearrange_index[i]<0 || pixel_rearrange_index[i] >= this->hic_pixel_density.size())
            {
                fmt::print("The pixel_rearrange_index ({}) is out of range[{}, {}]", pixel_rearrange_index[i], 0, this->hic_pixel_density.size()-1);
            }
            this->hic_pixel_density[i] = this->hic_pixel_density_origin[pixel_rearrange_index[i]];
        }
    }

    std::vector<int> get_cut_locs_pixel(
        const AutoCurationState& auto_curation_state,
        const u32* pixel_rearrange_index_, 
        const map_contigs* Contigs,
        const SelectArea* select_area=nullptr)
    {   
        // Cut parameters from auto-curation state
        this->smallest_frag_size_in_pixel = auto_curation_state.auto_cut_smallest_frag_size_in_pixel;
        this->cut_threshold = auto_curation_state.auto_cut_threshold;

        // rearrange index 
        this->rearrange_hic_pixel_density(pixel_rearrange_index_);

        // Run cuts within select_area
        this->frags->re_allocate_mem(Contigs, select_area, 1);
        std::vector<int> cut_locs_pixel;
        for (u32 i = 0; i < this->frags->num; i++)
        {
            this->get_single_fragment_cut_locs(i, cut_locs_pixel); 
        }
        return cut_locs_pixel;
    }

    /* break_points holds cut positions in global pixel coordinates */
    void find_break_points(
        const u32 start_pixel,
        const std::vector<f32>& arr,
        std::vector<int>& break_points)
    {   
        const auto& windows_size = this->smallest_frag_size_in_pixel;
        const auto& threshold = this->cut_threshold;

        int n = arr.size();
        if (n < 2 * windows_size + 1) 
        {
            return ; 
        }

        // detect the break points according to the value
        {
            std::vector<f32> prefix_sum(n + 1, 0.f);
            cal_prefix_sum(arr, prefix_sum);
            for (int i = windows_size; i < n - windows_size; ++i) 
            {
                f32 avgBefore = (prefix_sum[i] - prefix_sum[i - windows_size]);
                
                f32 avgAfter = (prefix_sum[i + windows_size + 1] - prefix_sum[i + 1]);
                
                f32 current_window = (prefix_sum[i + windows_size/2 + 1] - prefix_sum[i - windows_size/2 + 1]);
                
                if (
                    (avgBefore - current_window > avgBefore * threshold && avgAfter - current_window > avgAfter * threshold) ||
                    (current_window - avgBefore > avgBefore * threshold && current_window - avgAfter  > avgAfter * threshold)
                ) 
                {   
                    u08 min_flag = avgBefore > current_window;  
                    u32 min_i = i;
                    // find the minimum value in the window
                    for (u32 tmp_i = i;
                        tmp_i < n - windows_size && tmp_i < i + windows_size+1;
                        tmp_i++ )
                    {
                        f32 current_next = (prefix_sum[tmp_i + windows_size/2 + 2] - prefix_sum[tmp_i - windows_size/2 + 2]);
                        if (
                            (min_flag && current_window > current_next) ||
                            (!min_flag && current_window < current_next) )
                        {
                            current_window = current_next;
                            min_i = i;
                        }
                    }
                    break_points.push_back((int)(min_i + start_pixel));
                    i += windows_size ;
                }
            }
        }

        return ;
    }

    void get_single_fragment_cut_locs(
        const u32& frag_id, 
        std::vector<int>& cut_locs)
    {   
        // copy orginal to local
        std::vector<f32> hic_pixel_density_tmp(this->hic_pixel_density.begin() + this->frags->startCoord[frag_id], 
            this->hic_pixel_density.begin() + this->frags->startCoord[frag_id] + this->frags->length[frag_id]);

        u32 n = hic_pixel_density_tmp.size();

        // std::vector<f32> order1_derivative(n, 0.f);
        // std::vector<f32> order2_derivative(n, 0.f);
        // get_smoothed_1st_order_derivative(hic_pixel_density_tmp, order1_derivative);
        // get_smoothed_1st_order_derivative(order1_derivative, order2_derivative);
        
        #ifdef DEBUG_OUTPUT_PIXEL_CUT_FILE // output the hic_pixel_density to text file
            std::string filename = fmt::format("{}/current_id_{}.txt", this->file_save_dir, frag_id);
            std::ofstream out(filename);
            if (!out)
            {
                fmt::print(stderr, "Cannot open the file {}\n", filename);
                assert(0);
            }
            for (u32 i = 0; i < hic_pixel_density_tmp.size(); i++)
            {
                out << hic_pixel_density_tmp[i] << std::endl;
            }
            out.close();
            fmt::print("[Pixel Cut]: output hic_pixel_density to {}\n", filename);
        #endif // DEBUG_OUTPUT_PIXEL_CUT_FILE

        std::vector<int> break_points(0);
        this->find_break_points(
            this->frags->startCoord[frag_id],
            hic_pixel_density_tmp,
            break_points);
        if (break_points.size() > 0)
        {
            cut_locs.insert(cut_locs.end(), break_points.begin(), break_points.end());
        }
        
    }

    
    void get_single_fragment_hic_pixel_density(
        const u32& start_pixel,
        const u32& len_pixel,
        f32* hic_pixel_density
    )
    {   
        for (u32 i=0; i < len_pixel; i++)
        {
            hic_pixel_density[i] = this->pixel_mean(start_pixel, i, len_pixel);
        }
        if (len_pixel > 4)
        {
            hic_pixel_density[0] = hic_pixel_density[1] = hic_pixel_density[2];
            hic_pixel_density[len_pixel-1] = hic_pixel_density[len_pixel-2] = hic_pixel_density[len_pixel-3];
        }
        else 
        {
            for (u32 i = 0; i < len_pixel; i++)
            {
                hic_pixel_density[i] = 1.0f;
            }
        }
        return ;
    }

    // Mean of off-diagonal pairs with separation i < pixel_mean_window_size
    f32 pixel_mean(
        const u32& frag_start_pixel,
        const u32& relative_index, 
        const u32& len_pixel) 
    {   
        u32 num = 0, pixel_index=frag_start_pixel+relative_index;
        f32 sum = 0.f;

        for (u32 i = 1; i < this->pixel_mean_window_size ; i++) 
        {   
            if (i < relative_index && i + relative_index < len_pixel-1)
            {
                sum += ( (f32)((*(this->textures_array))(
                    pixel_index - i,
                    pixel_index + i)) ); // / this->norm_diag_mean[i]
                num++;
            }
            else break;
        }
        return num==0 ? 0.f : (f32)sum / (f32)num;
    }

    /* 
    Smoothed first derivative.
        arr:        input series
        derivative: output buffer
    */
    void get_smoothed_1st_order_derivative(
        const std::vector<f32>& arr, 
        std::vector<f32>& derivative, 
        u32 window_size=10)
    {   
        u32 dist = window_size / 2;
        for (u32 i = 0; i < arr.size(); i++)
        {
            derivative[i] = (arr[my_Min(i + dist, arr.size() - 1)] - arr[i > dist? i - dist:0]) / (f32)(dist * 2);
        }
    }


};





#endif // FRAG_CUT_CALCULATION_H