
#ifndef parse_agp_h
#define parse_agp_h

#include <fmt/core.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <cassert>
#include "genomeData.h"


struct Date
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    Date()
        : year(-1), month(-1), day(-1), hour(-1), minute(-1), second(-1) {}
    
    Date(int y, int m, int d, int h, int min, int s)
        : year(y), month(m), day(d), hour(h), minute(min), second(s) {}

    Date(int y, int m, int d)
        : year(y), month(m), day(d), hour(-1), minute(-1), second(-1) {}

    Date(const Date& other)
        : year(other.year), month(other.month), day(other.day),
          hour(other.hour), minute(other.minute), second(other.second) {}

    std::string to_string() const
    {   
        if (second>=0 && minute>=0 && hour>=0)
            return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", year, month, day, hour, minute, second);
        else if (year>=0 && month>=0 && day>=0)
            return fmt::format("{:04d}-{:02d}-{:02d}", year, month, day);
        else if (year>=0 && month>=0)
            return fmt::format("{:04d}-{:02d}", year, month);
        else if (year>=0)
            return fmt::format("{:04d}", year);
        else return "Unknown Date";
    }
};


struct Original_Contig_agp
{
    int64_t len;              // len in bp
    int num_frags;
    std::vector<int64_t> starts; // record the start position of each fragment

    Original_Contig_agp()
        : len(0), num_frags(0) {}
    Original_Contig_agp(int64_t l, int n) : len(l), num_frags(n) {}
        
};

struct Scaff_agp
{
    int64_t len;            // len in bp
    bool is_painted;
    Scaff_agp()
        : len(0), is_painted(false) {}
    Scaff_agp(bool p)
        : len(0), is_painted(p) {}
};


struct Frag
{
    const int orig_contig_id;   // start from 0
    const int scaff_id;         // start from 0
    const int64_t start;        // start position (bp) in the original contig
    const int64_t len;          // len in bp
    int local_index = -1;       // index within orignal contig
    const bool is_reverse;
    const bool is_painted ;
    const uint64_t meta_data_flag = 0;
    Frag(const int oci, const int si, int64_t s, int64_t l, bool r, bool p, const uint64_t& m)
        : orig_contig_id(oci), scaff_id(si), start(s), len(l), is_reverse(r), is_painted(p), meta_data_flag(m) {}
};


inline std::string agp_trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}


inline std::vector<std::string> agp_split_tabs(const std::string& line)
{
    std::vector<std::string> fields;
    std::string cur;
    for (char c : line)
    {
        if (c == '\t')
        {
            fields.push_back(cur);
            cur.clear();
        }
        else if (c != '\r')
        {
            cur.push_back(c);
        }
    }
    fields.push_back(cur);
    while (!fields.empty() && agp_trim(fields.back()).empty()) fields.pop_back();
    return fields;
}


inline bool agp_parse_i64(const std::string& s, int64_t& out)
{
    try
    {
        const std::string t = agp_trim(s);
        if (t.empty()) return false;
        size_t idx = 0;
        out = std::stoll(t, &idx, 10);
        return idx == t.size();
    }
    catch (...)
    {
        return false;
    }
}


inline bool agp_is_gap_component(const std::string& type)
{
    return type == "U" || type == "N";
}


inline bool agp_is_sequence_component(const std::string& type)
{
    return type == "W" || type == "D" || type == "F" || type == "A" || type == "P" || type == "O" || type == "G";
}


// Scaffold_12 / H1.scaffold_1 -> 0-based id from the trailing _number.
inline int get_scaff_id(const std::string& scaff_name)
{
    const auto pos = scaff_name.find_last_of('_');
    const std::string token = (pos != std::string::npos && pos + 1 < scaff_name.size())
        ? scaff_name.substr(pos + 1)
        : scaff_name;

    size_t i = 0;
    while (i < token.size() && std::isdigit((unsigned char)token[i])) ++i;
    if (i == 0)
    {
        fmt::print(stderr, "[Load AGP::warning]: scaffold name '{}' has no trailing number; using id 0.\n", scaff_name);
        return 0;
    }

    int64_t v = 0;
    if (!agp_parse_i64(token.substr(0, i), v) || v <= 0)
    {
        fmt::print(stderr, "[Load AGP::warning]: scaffold name '{}' has invalid trailing number; using id 0.\n", scaff_name);
        return 0;
    }
    if (v > (int64_t)std::numeric_limits<int>::max())
    {
        throw std::runtime_error(fmt::format("[Load AGP::error]: scaffold id in '{}' exceeds 32-bit index range.\n", scaff_name));
    }
    return (int)(v - 1);
}


class AssemblyAGP
{
public:
    std::string sample_name;
    std::string agp_version;
    std::string description;
    Date date;
    double bp_per_pixel = 0.0;

    std::vector<Frag> frags;
    std::unordered_map<std::string, Original_Contig_agp> original_contigs;
    std::vector<Scaff_agp> scaffs;
    int64_t total_bp = 0;
    int skipped_line_count = 0;
    int skipped_unknown_contig_count = 0;


    int get_num_painted_scaff() const
    {
        int num = 0;
        for (const auto& scaff : scaffs)
        {
            if (scaff.is_painted) num++;
        }
        return num;
    }

    void add_frag(
        const original_contig* Original_Contigs, const int& num_original_contigs,
        const std::string& scaff_name, 
        const std::string& original_contig_name,
        const int64_t& start_local, const int64_t& end_local, 
        bool is_reverse,
        bool is_painted,
        const uint64_t& meta_tags )
    {   
        int original_contig_id = this->get_original_contig_id(Original_Contigs, num_original_contigs, original_contig_name);
        if (original_contig_id < 0)
        {
            ++this->skipped_unknown_contig_count;
            fmt::print(stderr, "[Load AGP::warning]: original contig name '{}' not found in the map; AGP fragment skipped.\n", original_contig_name);
            return;
        }
        int scaff_id = get_scaff_id(scaff_name); // start from 0
        
        if (this->original_contigs.find(original_contig_name) == this->original_contigs.end())
            this->original_contigs[original_contig_name] = Original_Contig_agp(end_local, 1);
        else {
            this->original_contigs[original_contig_name].num_frags++;
            this->original_contigs[original_contig_name].len = std::max(this->original_contigs[original_contig_name].len, end_local);
        }
        this->original_contigs[original_contig_name].starts.push_back(start_local);
        const int64_t frag_len = (end_local >= start_local)
            ? (end_local - start_local + 1)
            : (start_local - end_local + 1);
        this->frags.emplace_back(original_contig_id, scaff_id, start_local, frag_len, is_reverse, is_painted, meta_tags);

        if ((int)this->scaffs.size() <= scaff_id)
            this->scaffs.resize((size_t)scaff_id + 1);
        this->scaffs[scaff_id].is_painted = this->scaffs[scaff_id].is_painted || is_painted;
        this->scaffs[this->frags.back().scaff_id].len += this->frags.back().len;
        this->total_bp += this->frags.back().len;
    }

    bool parse_comment_line(const std::string& line)
    {
        if (line.compare(0, 13, "##agp-version") == 0)
        {
            this->agp_version = agp_trim(line.substr(13));
            return true;
        }
        if (line.compare(0, 14, "# DESCRIPTION:") == 0)
        {
            this->description = agp_trim(line.substr(14));
            return true;
        }
        const std::string res_prefix = "# HiC MAP RESOLUTION:";
        if (line.compare(0, res_prefix.size(), res_prefix) == 0)
        {
            std::string rest = agp_trim(line.substr(res_prefix.size()));
            const auto bp_pos = rest.find("bp/texel");
            if (bp_pos != std::string::npos) rest = agp_trim(rest.substr(0, bp_pos));
            try { this->bp_per_pixel = std::stod(rest); }
            catch (...) { this->bp_per_pixel = 0.0; }
            return true;
        }
        return line.empty() || line[0] == '#';
    }

    bool parse_data_line(
        const std::string& line,
        int line_no,
        const original_contig* Original_Contigs,
        const int& num_original_contigs,
        meta_data* Meta_Data)
    {
        const std::vector<std::string> fields = agp_split_tabs(line);
        if (fields.size() < 5)
        {
            fmt::print(stderr, "[Load AGP::warning]: skipped unparsed AGP line {}: {}\n", line_no, line);
            ++this->skipped_line_count;
            return false;
        }

        const std::string& component_type = fields[4];
        if (agp_is_gap_component(component_type))
            return true;

        if (!agp_is_sequence_component(component_type))
        {
            fmt::print(stderr, "[Load AGP::warning]: skipped AGP line {} with component type '{}': {}\n", line_no, component_type, line);
            ++this->skipped_line_count;
            return false;
        }

        if (fields.size() < 9)
        {
            fmt::print(stderr, "[Load AGP::warning]: skipped short sequence AGP line {}: {}\n", line_no, line);
            ++this->skipped_line_count;
            return false;
        }

        int64_t start_local = 0, end_local = 0;
        if (!agp_parse_i64(fields[6], start_local) || !agp_parse_i64(fields[7], end_local))
        {
            fmt::print(stderr, "[Load AGP::warning]: skipped AGP line {} with non-integer coordinates: {}\n", line_no, line);
            ++this->skipped_line_count;
            return false;
        }

        const std::string& scaff_name = fields[0];
        const std::string& original_contig_name = fields[5];
        const bool is_reverse = fields[8] == "-";
        bool is_painted = false;
        std::string tags_str;
        for (size_t i = 9; i < fields.size(); ++i)
        {
            const std::string field = agp_trim(fields[i]);
            if (field.empty()) continue;
            if (!is_painted && field == "Painted")
            {
                is_painted = true;
                continue;
            }
            if (!tags_str.empty()) tags_str.push_back(' ');
            tags_str += field;
        }

        this->add_frag(
            Original_Contigs, num_original_contigs,
            scaff_name, original_contig_name,
            start_local, end_local,
            is_reverse, is_painted, parse_tags(tags_str, Meta_Data)
        );
        return true;
    }

    /* 
        restore the assembly from the 
            - UN-CORRECTED .agp file
    */
    AssemblyAGP(
        const std::string& agp_file,
        const original_contig* Original_Contigs,
        const int& num_original_contigs,
        meta_data* Meta_Data // used to restore the meta data tag
    )
    {
        std::ifstream file(agp_file);
        if (!file.is_open())
        {
            throw std::runtime_error(fmt::format("Failed to open AGP file: {}\n", agp_file));
        }

        sample_name = agp_file.substr(agp_file.find_last_of("/\\") + 1);
        sample_name = sample_name.substr(0, sample_name.find('.'));

        std::string line;
        int line_no = 0;
        while (std::getline(file, line))
        {   
            ++line_no;
            if (line.empty()) continue;
            if (line[0] == '#')
            {
                this->parse_comment_line(line);
                continue;
            }
            this->parse_data_line(line, line_no, Original_Contigs, num_original_contigs, Meta_Data);
        }
        file.close();
        this->sort_frags_local_index(Original_Contigs);
    }

    uint64_t parse_tags(const std::string tags_str, meta_data* Meta_Data)
    {   
        if (tags_str.empty() || Meta_Data == nullptr) return 0;
        uint64_t tags_u64 = 0;
        std::istringstream iss(tags_str);
        std::string tag;
        
        while (iss >> tag) 
        {
            for (int i = 0 ; i<64 ; i ++ )
            {   
                if (std::string((char*)Meta_Data->tags[i]).empty()) 
                {   
                    const size_t n = std::min(tag.size(), sizeof(Meta_Data->tags[i]) - 1);
                    memcpy(Meta_Data->tags[i], tag.data(), n);
                    ((char*)Meta_Data->tags[i])[n] = 0;
                    tags_u64 |= (1ULL << i);
                    break;
                }
                else if (tag == std::string((char*)(Meta_Data->tags[i])))
                {
                    tags_u64 |= (1ULL << i);
                    break;
                }
            }
        }
        return tags_u64;
    }

    /* validate the number of base pairs */
    int64_t cal_total_bp()
    {
        int64_t total = 0;
        for (const auto& frag : frags)
        {
            total += frag.len;
        }

        int64_t total_bp_scaff = 0;
        for (const auto& scaff : scaffs) 
            total_bp_scaff += scaff.len; 
        if (total != total_bp_scaff)
        {
            fmt::print(stderr, "Warning: total bp in frags ({}) != total bp in scaffs ({})\n", total, total_bp_scaff);
            assert(total == total_bp_scaff);
            return -1;
        }
        return total;
    }

    /*
        restore the map_state from the assembly
            - UN-CORRECTED .agp file
        return: 0 if success, -1 if failed
    */
    int restore_map_state_from_agp(
        const int& num_pix_1d,
        map_state* Map_State,
        const original_contig* Original_Contigs,
        const int& num_original_contigs
    )
    {   
        (void)Original_Contigs;
        (void)num_original_contigs;
        if (!(this->bp_per_pixel > 0.0))
        {
            fmt::print(stderr, "[Load AGP::error]: bp/texel is missing or invalid.\n");
            return -1;
        }
        int pixel_index;
        double start_pixel_global = 0, len_pixel = 0, local_start_pixel;
        for ( size_t i  = 0 ; i < this->frags.size() ;  i++ )
        {   
            const Frag& frag = frags[i];
            local_start_pixel = (double) frag.start / this->bp_per_pixel;
            len_pixel = (double) frag.len / this->bp_per_pixel;
            for (int j = 0; j < std::round(len_pixel); j ++ )
            {   
                pixel_index = (int)std::round(start_pixel_global) + j;
                if ( pixel_index >= num_pix_1d )
                {
                    fmt::print(stderr, "Warning: pixel index ({:.2f}) out of range [0, {}).\n", pixel_index, num_pix_1d);
                    assert(0);
                    return -1;
                }
                Map_State->contigIds[pixel_index] = (u32)i;
                Map_State->originalContigIds[pixel_index] = frag.orig_contig_id;
                Map_State->contigRelCoords[pixel_index] = frag.is_reverse ? 
                    (u32)(std::round(local_start_pixel + len_pixel) - j - 1): 
                    (u32)(std::round(local_start_pixel) + j);
                Map_State->scaffIds[pixel_index] = frag.scaff_id;
                pixel_index++;
            }
            start_pixel_global += len_pixel;
        }
        #ifdef DEBUG
            int n = 2000;
            fmt::print("[Load AGP::restore_map_state_from_agp]: Relative coordinates of the first {} pixels:\n", n);
            for (int i = 0; i < n ; i++)
            {
                fmt::print("{}:{} ", Map_State->originalContigIds[i], Map_State->contigRelCoords[i]);
                if (i % 40 == 39) fmt::print("\n");
            }
            fmt::print("\n");

        #endif // DEBUG


        return 0;
    }

    int get_original_contig_id(
        const original_contig* Original_Contigs, const int& num_original_contigs, const std::string& name)
    {
        if (Original_Contigs == nullptr) return -1;
        for (int i = 0; i < num_original_contigs; i++)
        {
            if (strcmp((char*)(Original_Contigs+i)->name, name.c_str()) == 0)
            {
                return i;
            }
        }
        return -1;
    }

    std::string __str__() const
    {
        std::string 
        str =  fmt::format("Sample name:   {}\n", sample_name);
        str += fmt::format("AGP Version:   {}\n", agp_version);
        str += fmt::format("Description:   {}\n", description);
        str += fmt::format("Date:          {}\n", date.to_string());
        str += fmt::format("BP per pixel:  {}\n", bp_per_pixel);
        str += fmt::format("Total BP:      {}\n", total_bp);
        return str;
    }

    void sort_frags_local_index(const original_contig* Original_Contigs)
    {   
        for (auto& it:this->original_contigs) std::sort(it.second.starts.begin(), it.second.starts.end());
        for (size_t i = 0 ; i < frags.size(); i++)
        {
            std::string original_contig_name = std::string((char*)Original_Contigs[frags[i].orig_contig_id].name);
            int tmp_index = 0;
            while (tmp_index < (int)this->original_contigs[original_contig_name].starts.size() && 
                this->frags[i].start != this->original_contigs[original_contig_name].starts[tmp_index]) tmp_index++;
            if (tmp_index >= (int)this->original_contigs[original_contig_name].starts.size())
            {
                throw std::runtime_error(fmt::format("Error: original contig name {} not found in Original_Contigs. file: {}, line: {}\n", original_contig_name, __FILE__, __LINE__));
            }
            this->frags[i].local_index = tmp_index;
        }
    }
};

#endif // parse_agp_h
