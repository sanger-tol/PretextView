
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


#ifndef GENOMEDATA_H
#define GENOMEDATA_H

#include "utilsPretextView.h"
#include "showWindowData.h"

struct
file_atlas_entry
{
    u32 base;  // start
    u32 nBytes;
};


struct contact_matrix
{
    GLuint textures;       // Store one or more OpenGL texture IDs (GLuint)
    GLuint pixelStartLookupBuffer;
    GLuint pixelRearrangmentLookupBuffer; // save the pixel rearrangement lookup buffer
    GLuint pixelStartLookupBufferTex;
    GLuint pixelRearrangmentLookupBufferTex;
    GLint pad;
    GLuint *vaos = nullptr;          // Pointer to VAO array
    GLuint *vbos = nullptr;          // A pointer to an array of VBOs
    GLuint shaderProgram;  // Shader ID
    GLint matLocation;
};


// one original contig may be split into multiple fragments
struct original_contig    
{
    u32 name[16];         // contig name
    u32 *contigMapPixels = nullptr; // global center coordinate of the fragments
    u32 nContigs;         // num of contigs, original contig is split into many fragments
    u32 pad;
};


struct contig
{
    u64 *metaDataFlags = nullptr;   // meta data flags for every contig
    u32 length;
    u32 originalContigId; // original contig id
    u32 startCoord;       // local coordinate on the original contig
    u32 scaffId;
    u32 pad;
};


// Contigs which have a pixel on the map
struct map_contigs
{
    u08 *contigInvertFlags = nullptr; // invert flag [num_pixels_1d / 8], 1 bit for one pixel
    contig *contigs_arr = nullptr;
    u32 numberOfContigs;
    u32 contigs_arr_capacity; // bytes allocated for contigs_arr (see UpdateContigsFromMapState loop guard)
};


struct meta_data
{
    u32 tags[64][16];

    meta_data()
    {
        memset(tags, 0, sizeof(tags));
    }
};


struct map_state
{
    u32 *contigIds = nullptr;         // [num_pixels_1d]
    u32 *originalContigIds = nullptr; // orignal contig id [num_pixels_1d]
    u32 *contigRelCoords = nullptr;   // local coordinate on the original contig [num_pixels_1d]
    u32 *scaffIds = nullptr;          // [num_pixels_1d]
    u64 *metaDataFlags = nullptr;     // [num_pixels_1d], 256kb for 32768 pixels

    void restore_cutted_contigs_all(const u32& num_pixel_1d)
    {
        for (u32 i = 0; i < num_pixel_1d; i++)
        {
            // These are now no-ops.  I don't understand what they originally did:
            // originalContigIds[i] = originalContigIds[i] % Max_Number_of_Contigs;
            originalContigIds[i] = originalContigIds[i];
        }
    }

    void restore_cutted_contigs(const s32 start_pixel, const s32 end_pixel)
    {   
        if (start_pixel < 0 || end_pixel < 0) return;
        for (u32 i = start_pixel; i <= end_pixel; i++)
        {
            // These are now no-ops.  I don't understand what they originally did:
            // originalContigIds[i] = originalContigIds[i] % Max_Number_of_Contigs;
            originalContigIds[i] = originalContigIds[i];
        }
    }

    map_state(int num_pixel_1d)
    {
        contigIds = new u32[num_pixel_1d];
        originalContigIds = new u32[num_pixel_1d];
        contigRelCoords = new u32[num_pixel_1d];
        scaffIds = new u32[num_pixel_1d];
        metaDataFlags = new u64[num_pixel_1d];
        memset(contigIds, 0, num_pixel_1d * sizeof(u32));
        memset(originalContigIds, 0, num_pixel_1d * sizeof(u32));
        memset(contigRelCoords, 0, num_pixel_1d * sizeof(u32));
        memset(scaffIds, 0, num_pixel_1d * sizeof(u32));
        memset(metaDataFlags, 0, num_pixel_1d * sizeof(u64));
    }
    ~map_state()
    {
        if (contigIds)
        {
            delete[] contigIds;
            contigIds = nullptr;
        }
        if (originalContigIds)
        {
            delete[] originalContigIds;
            originalContigIds = nullptr;
        }
        if (contigRelCoords)
        {
            delete[] contigRelCoords;
            contigRelCoords = nullptr;
        }
        if (scaffIds)
        {
            delete[] scaffIds;
            scaffIds = nullptr;
        }
        if (metaDataFlags)
        {
            delete[] metaDataFlags;
            metaDataFlags = nullptr;
        }
    }
};


/*
Used to store the calculated pixel_density value
*/
struct Extension_Graph_Data
{   
    bool is_prepared = false;
    f32* data=nullptr;
    Extension_Graph_Data(u32 num_pixel_1d) 
    {
        data = new f32[num_pixel_1d];
        memset(data, 0, num_pixel_1d * sizeof(f32));
    }

    ~Extension_Graph_Data()
    {
        if (data)
        {
            delete[] data;
            data = nullptr;
        }
    }
    
};


// extension structures
struct
graph  // NOTE: don't change the order of the members, or the SaveState and LoadState function will break
{
    u32 name[16];
    u32 *data = nullptr;    // save the data of the graph, 8 bytes
    GLuint vbo;
    GLuint vao;
    editable_plot_shader *shader = nullptr; // 8
    f32 scale;
    f32 base;
    f32 lineSize;
    u16 on;
    u16 nameOn;
    nk_colorf colour;
    u08 activecolor;
};


enum
extension_type
{
    extension_graph,
};


struct
extension_node
{
    extension_type type;
    u32 pad;
    void *extension = nullptr;
    extension_node *next = nullptr;
};


struct
extension_sentinel
{
    extension_node *head = nullptr;  // point the first node
    extension_node *tail = nullptr;  // point the last node

    u32 get_num_extensions()
    {
        u32 added_index = 0;
        TraverseLinkedList(this->head, extension_node)
        {
            added_index++;
        }
        return added_index;
    }

    bool is_empty()
    {
        return this->head == nullptr;
    }

    bool is_graph_name_exist(const std::string& name)
    {
        TraverseLinkedList(this->head, extension_node)
        {
            if (node->type == extension_graph)
            {
                if (strcmp((char*)((graph*)node->extension)->name, name.c_str()) == 0)
                {
                    return true;
                }
            }
        }
        return false;
    }

    /* 
        get the id of extension whose name including the tmp name
        return: -1 if not found
                id of the extension if found (return the first found id if multiple found)
    */
    s32 get_graph_id(const std::string name)
    {
        s32 id = 0;
        TraverseLinkedList(this->head, extension_node)
        {
            if (node->type == extension_graph)
            {
                if (std::string((char*)((graph*)node->extension)->name).find(name) != std::string::npos)
                {   
                    fmt::println("[extension_sentinel] Found the Extension name: {}, id: {}", (char*)((graph*)node->extension)->name, id);
                    return id;
                }
            }
            id++;
        }
        fmt::println("[extension_sentinel] Not found the Extension name: {}", name);
        return -1;
    }

    const u32* get_graph_data_ptr(const std::string name)
    {
        s32 id = get_graph_id(name);
        if (id < 0 ) return nullptr;
        extension_node* node = this->head;
        while ( id-- > 0 ) 
        {
            if (node && node->next) node = node->next;
            else 
            {
                fmt::println("[Pixel Cut::error]: The graph id is out of range");
                assert(0);
            }
        }
        return ((graph*)(node->extension))->data;
    }

};




#endif // GENOMEDATA_H