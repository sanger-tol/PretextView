

#ifndef HIC_FIGURE_H
#define HIC_FIGURE_H


#include <iostream>
#include <random>
#include <sstream>
#include <vector>
#include <queue>
#include <string>
#include <filesystem>
#include <map>
#include <fstream>
#include <variant>
#include <type_traits>
#include "genomeData.h"
#include "showWindowData.h" 
#include "utilsPretextView.h"
#include "copy_texture.h"


#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
    #define STB_IMAGE_RESIZE_IMPLEMENTATION
    #include "stb_image_resize.h"
#endif // STB_IMAGE_RESIZE_IMPLEMENTATION

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include "stb_image_write.h"
#endif // STB_IMAGE_WRITE_IMPLEMENTATION



using Dict_json = std::map<std::string, std::variant<u64, f32, std::string>>;





std::string generate_random_string(u32 length) 
{
    const std::string chars = 
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"; 
    std::random_device rd;
    std::mt19937 generator(rd()); 
    std::uniform_int_distribution<> distrib(0, chars.size() - 1); 
    std::string result(length, 0);
    for (u32 i = 0; i < length; ++i) 
    {
        result[i] = chars[distrib(generator)]; 
    }
    return result;
}

struct ValueVisitor 
{
    std::stringstream& ss;

    void operator()(uint64_t val) 
    {
        ss << val;  // 
    }

    void operator()(float val) 
    {
        ss << val;  
    }

    void operator()(const std::string& val) 
    {
        ss << "\"" << val << "\"";  
    }
};


/*
Hi-C figure metadata
*/
struct Figure_Info
{
    u64 start_bp;
    u64 end_bp;
    u64 genome_bp;
    f32 bp_per_pixel_figure;
    f32 bp_per_pixel_hic;
    u32 figure_pixel_1d;
    u32 hic_pixel_1d;
    u32 num_pixels_1d_from_hic;
    f32 multiplier;
    std::string file_save_path;    // dir_path/random_name without extension
    Dict_json info_dic;
    std::string img_id;

    Figure_Info(
        u32 figure_pixel_1d_,
        u32 hic_pixel_1d_,
        u64 start_bp_, 
        u64 end_bp_,
        u64 genome_bp_,
        f32 bp_per_pixel_figure_, 
        f32 bp_per_pixel_hic_,
        f32 multiplier_,
        u32 num_pixels_1d_from_hic_, 
        std::string file_save_path_, 
        std::string img_id_)
        : 
        figure_pixel_1d(figure_pixel_1d_),
        hic_pixel_1d(hic_pixel_1d_),
        start_bp(start_bp_), 
        end_bp(end_bp_), 
        genome_bp(genome_bp_),
        bp_per_pixel_figure(bp_per_pixel_figure_), 
        bp_per_pixel_hic(bp_per_pixel_hic_),
        multiplier(multiplier_),
        num_pixels_1d_from_hic(num_pixels_1d_from_hic_), 
        file_save_path(file_save_path_), 
        img_id(img_id_)
    {   
        /*
        sample: 

        '/lustre/scratch125/ids/team117-assembly/sg35/hic/hali_lig/results/AutoHiC/autohic_results/0/png/500000/00aea05fb7824b5dafb68fa6e280816b.jpg': {
            'genome_id': 'png', 
            'resolution': 500000, 
            'chr_A': 'assembly', 
            'chr_A_start': 0, 
            'chr_A_end': 312298520, 
            'chr_B': 'assembly', 
            'chr_B_start': 0, 
            'chr_B_end': 312298520
        }
        
        */
        this->info_dic["figure_pixel_1d"] = figure_pixel_1d;
        this->info_dic["hic_pixel_1d"] = hic_pixel_1d;
        this->info_dic["start_bp"] = start_bp;
        this->info_dic["end_bp"] = end_bp;
        this->info_dic["genome_bp"] = genome_bp;
        this->info_dic["bp_per_pixel_figure"] = bp_per_pixel_figure;
        this->info_dic["bp_per_pixel_hic"] = bp_per_pixel_hic;
        this->info_dic["multiplier"] = multiplier;
        this->info_dic["img_id"] = img_id;

        this->info_dic["genome_id"] = "png";
        this->info_dic["resolution"] = bp_per_pixel_figure;
        this->info_dic["chr_A"] = "assembly";
        this->info_dic["chr_A_start"] = start_bp;
        this->info_dic["chr_A_end"] = end_bp;
        this->info_dic["chr_B"] = "assembly";
        this->info_dic["chr_B_start"] = start_bp;
        this->info_dic["chr_B_end"] = end_bp;
    }

    std::string dump_info() {
        std::stringstream ss;
        ss << "{";
        bool first = true;
        
        for (const auto& [key, value] : info_dic) {
            if (!first) ss << ", ";
            ss << "\"" << key << "\": ";
            
            ValueVisitor visitor{ss};
            std::visit(visitor, value);
            
            first = false;
        }
        ss << "}";
        return ss.str();
    }
};

/*
Export Hi-C contact maps as figures at the given
    genomic span, pixel size, and resolution.
*/
class Hic_Figure
{
public:
    u64 genome_bp;
    u32 hic_pixel_1d;
    u32 figure_pixel_1d=1116;
    TexturesArray4AI* textures_array;
    f32 bp_per_pixel_hic;
    std::string file_save_dir;
    std::string hic_figure_save_dir;
    std::string auto_cut_output_dir;
    std::string info_save_file_full_path;
    #ifdef DEBUG
        std::vector<f32> resolution_mutiplier = {4};
    #else
        std::vector<f32> resolution_mutiplier = {
            0.5, 1, 2, 4, 8, 16, 32};
    #endif // DEBUG
    Figure_Info* figure_info=nullptr;
    
public:
    Hic_Figure(
        u64 genome_bp_,
        std::string file_save_dir_, 
        TexturesArray4AI* textures_array_,
        std::string hic_figure_dir_name = "/hic_figures",
        std::string auto_cut_output_name = "/auto_cut_output",
        std::string info_file_name = "/info.json"
    ) 
        : 
        genome_bp(genome_bp_),
        hic_pixel_1d(textures_array_->get_num_pixels_1d())
    {   
        this->textures_array = textures_array_;
        this->bp_per_pixel_hic = (f32)this->genome_bp / (f32)this->hic_pixel_1d;
        // if the folder does not exist, create it
        this->file_save_dir = std::filesystem::current_path().string() + file_save_dir_;
        if (std::filesystem::exists(this->file_save_dir))
        {
            // remove this one 
            std::cout << "Remove folder: " 
                      << this->file_save_dir
                      << std::endl;
            std::filesystem::remove_all(this->file_save_dir);
        }
        std::cout << "Creat folder: " 
                  << (std::filesystem::current_path().string() + this->file_save_dir) 
                  << std::endl;
        std::filesystem::create_directories(this->file_save_dir);
        // clear save pics
        this->auto_cut_output_dir = this->file_save_dir + auto_cut_output_name;
        std::filesystem::create_directories(this->auto_cut_output_dir);
        this->hic_figure_save_dir = this->file_save_dir + hic_figure_dir_name;
        std::filesystem::create_directories(this->hic_figure_save_dir);
        // clear the saved info
        this->info_save_file_full_path = this->hic_figure_save_dir+info_file_name;
        std::ofstream out_file(this->info_save_file_full_path);
        if (!out_file.is_open())
        {   
            std::stringstream ss;
            ss << "Cannot open: " << this->info_save_file_full_path << std::endl;
            MY_CHECK(ss.str().c_str());
            assert(0);
        }
        out_file << "{" << std::endl << "}";
        out_file.close();
    }

    ~Hic_Figure(){
        if (this->figure_info)
        {
            delete this->figure_info;
            this->figure_info = nullptr;
        }
    }

    void single_hic_figure_config(u32 start_pixel_figure, f32 bp_per_pixel_figure, f32 multiplier)
    {
        u32 start_bp = (u32) (start_pixel_figure * bp_per_pixel_figure);
        u32 end_bp = (u32) ((start_pixel_figure + this->figure_pixel_1d) * bp_per_pixel_figure);
        
        std::string img_id = generate_random_string(16);
        std::string file_save_path = this->hic_figure_save_dir + "/" + img_id;

        if (this->figure_info)
        {
            delete this->figure_info;
            this->figure_info = nullptr;
        }
        this->figure_info = new Figure_Info(
            this->figure_pixel_1d,
            this->hic_pixel_1d,
            start_bp, 
            end_bp, 
            this->genome_bp,
            bp_per_pixel_figure, 
            this->bp_per_pixel_hic,
            multiplier,
            this->hic_pixel_1d,
            file_save_path,
            img_id);
        
        this->dump_info_json(this->figure_info->info_dic);
    }


    void dump_info_json(std::map<std::string, std::variant<u64, f32, std::string>> info)
    {
        if (info.size() == 0) 
        {
            std::cerr << "The info is empty" << std::endl;
            assert(0);
        }
        std::fstream out_file = std::fstream(this->info_save_file_full_path, std::ios::in | std::ios::out);
        if (!out_file.is_open())
        {   
            std::stringstream ss;
            ss << "Cannot open: " << this->info_save_file_full_path << std::endl;
            MY_CHECK(ss.str().c_str());
            assert(0);
        }
        // move cursor to the second last line
        out_file.seekg(0, std::ios_base::end);
        u64 ptr = (u64) out_file.tellg() - 1 ;
        char ch;
        while (ptr > 0) 
        {   
            out_file.seekg(ptr);
            out_file.get(ch);
            if (ch == '}') 
            {   
                out_file.seekg(ptr-2);
                out_file.get(ch);
                if (ch == '}')
                {   
                    ptr -- ;
                    out_file.seekp(ptr);
                    out_file << "," << std::endl;
                    ptr += 2 ;
                }
                out_file.seekp(ptr);
                out_file << "\"" << this->hic_figure_save_dir << "/" ; 
                std::visit(
                    [&out_file](const auto& v) 
                    {
                        out_file << v;
                    }, 
                    info["img_id"]);
                out_file << ".png" << "\"" << ": " 
                         << this->figure_info->dump_info() << std::endl << "}";
                break;
            }
            ptr--;
        }
        if (ptr == 0)
        {
            char buff[512];
            snprintf(buff, sizeof(buff), "Cannot find ']' or '}' in the json file: %s\n", info_save_file_full_path.c_str());
            MY_CHECK(buff);
            assert(0);
        }
        out_file.close();
    }

    void copy_data_from_hic(
        u08* data_figure, 
        u32 start_pixel_hic, 
        u32 num_pixels_to_copy_from_hic)
    {   
        if (num_pixels_to_copy_from_hic > this->hic_pixel_1d)
        {
            char buff[512];
            snprintf(buff, sizeof(buff), "num_pixels_to_copy_from_hic(%d) > total_pixels_1d(%d)\n", num_pixels_to_copy_from_hic, this->hic_pixel_1d);
            MY_CHECK(buff);
            assert(0);
        }
        start_pixel_hic = my_Min(this->hic_pixel_1d - num_pixels_to_copy_from_hic, start_pixel_hic);
        for (u32 i = 0; i < num_pixels_to_copy_from_hic; i++)
        {
            for (u32 j = 0; j < num_pixels_to_copy_from_hic; j++)
            {
                data_figure[i*num_pixels_to_copy_from_hic + j] = textures_array->operator()(i + start_pixel_hic, j + start_pixel_hic);
            }
        }
    }

    void resize_and_save_figure(u08* data_figure, u32 num_pixels_copy_from_hic, f32 gamma = 0.5)
    {
        int num_pixels = this->figure_pixel_1d * this->figure_pixel_1d;
        u08* data_out = new u08[num_pixels ];
        stbir_resize_uint8(
            (const u08*)data_figure,      // input buffer
            num_pixels_copy_from_hic,     // input width
            num_pixels_copy_from_hic,     // input height
            0,                            // input stride
            (u08*)data_out,               // output buffer
            this->figure_pixel_1d,        // output width
            this->figure_pixel_1d,        // output height
            0,                            // output stride
            1                             // number of channels
        );

        // 95 percentile
        u08 percentile_95 = percentile_cal(data_out, num_pixels, 0.95);
        f32* data_out_f32 = new f32[num_pixels];
        for (int i = 0; i < num_pixels; ++i) 
        {
            data_out_f32[i] = (f32)my_Min(data_out[i], percentile_95) / (f32)percentile_95;
        }

        // Allocate RGB buffer (3 channels)
        u32 num_channels = 3;
        u08* rgb_data = new u08[num_pixels * num_channels];

        // Map normalized signal to white background with blue channel from (1 - val)
        for (int i = 0; i < num_pixels; ++i) 
        {   
            // rgb_data[i*3]   = (u08) (data_out_f32[i] < gamma? 255 : data_out_f32[i] * 255.f);         // R
            rgb_data[i*num_channels]   = 255;         // R
            rgb_data[i*num_channels+2] = rgb_data[i*num_channels+1] = (u08) ((1.f - data_out_f32[i]) * 255.f); // G & B
            // rgb_data[i*num_channels+3] = 255;          // A
        }
        delete[] data_out_f32;

        // Build output PNG path
        char buff[512];
        snprintf(
            buff, sizeof(buff), 
            "%s/%s.png",  
            this->hic_figure_save_dir.c_str(),  
            this->figure_info->img_id.c_str()
        );

        stbi_write_png(
            buff, 
            this->figure_pixel_1d, 
            this->figure_pixel_1d, 
            num_channels,  
            rgb_data,           
            this->figure_pixel_1d * num_channels         
        );
        delete[] rgb_data;
        delete[] data_out;
    }

    void generate_hic_figures()
    {   
        u32 figure_num = 0;
        for (f32 multiplier : this->resolution_mutiplier)
        {   
            u32 start_pixel_figure = 0;
            f32 bp_per_pixel_figure = (f32)this->bp_per_pixel_hic * (f32)multiplier;
            f32 total_pixel_figure = (f32)this->genome_bp / bp_per_pixel_figure;
            u32 num_pixels_copy_from_hic = (u32)((f32)this->figure_pixel_1d * multiplier);
            if (this->figure_pixel_1d > total_pixel_figure) continue;
            u08* data_figure = new u08[num_pixels_copy_from_hic * num_pixels_copy_from_hic];
            while (start_pixel_figure < (total_pixel_figure - (this->figure_pixel_1d>>1)))
            {   
                if (start_pixel_figure + this->figure_pixel_1d > total_pixel_figure)
                {
                    start_pixel_figure = (u32)total_pixel_figure - this->figure_pixel_1d;
                }
                this->single_hic_figure_config(
                    start_pixel_figure, 
                    bp_per_pixel_figure, 
                    multiplier);
                
                std::cout   << "[" << ++figure_num << "] "
                            << "Multiplier: " << multiplier 
                            << ", start_pixel_figure: " << start_pixel_figure << "/" << total_pixel_figure 
                            << ", start_hic: " << start_pixel_figure * multiplier << "/" << this->hic_pixel_1d
                            << ", start_bp: " << this->figure_info->start_bp << "/" << this->genome_bp << std::endl;

                this->copy_data_from_hic(
                    data_figure,
                    (u32)((f32)start_pixel_figure * multiplier), 
                    num_pixels_copy_from_hic);
                this->resize_and_save_figure(data_figure, num_pixels_copy_from_hic);
                if (start_pixel_figure+this->figure_pixel_1d >= (u32)total_pixel_figure)
                    break;
                start_pixel_figure += (this->figure_pixel_1d>>1);

                #ifdef DEBUG    
                    if (figure_num >= 100) 
                    {
                        std::cout << "Debug: figure_num = " << figure_num <<  ", break" << std::endl;
                        delete[] data_figure;
                        return;
                    }
                #endif // DEBUG
            }
            delete[] data_figure;
        }
    }
};



#endif // HIC_FIGURE_H