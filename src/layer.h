/*
Copyright (c) 2025 Wellcome Sanger Institute
author: Yumi Sims, yy5@sanger.ac.uk, Wellcome Sanger Institute

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

#ifndef PRETEXT_LAYER_H
#define PRETEXT_LAYER_H

#include "genomeData.h"
#include "utilsPretextView.h"

#define Pretext_Layer_Magic_0 'l'
#define Pretext_Layer_Magic_1 'a'
#define Pretext_Layer_Magic_2 'y'
#define Pretext_Layer_Magic_3 'r'
#define Pretext_Layer_Header_Extension_Size 18
#define Max_Pretext_Layers 16

enum pretext_layer_kind
{
    pretext_layer_kind_mapq = 1
};

struct pretext_layer_info
{
    u16 index;
    u16 count;
    u32 mapq_threshold;
    u32 mapq_min;
    u08 kind;
    u08 version;
};

// Reset layer state (e.g. before loading a new map).
void PretextLayer_Reset(void);

// Register the shared libdeflate decompressor used during section discovery.
void PretextLayer_SetDecompressor(libdeflate_decompressor *decompressor);

// Parse optional 'layr' bytes immediately after mipMapLevels in the decompressed header.
// Advances *header past the extension when present. Returns 1 if a valid extension was read.
u08 PretextLayer_ParseHeaderExtension(
    u08 **header,
    u08 *header_end,
    pretext_layer_info *info);

// Apply parsed layer metadata to the global layer manager (primary header only).
void PretextLayer_ApplyLayerInfo(const pretext_layer_info *info);

// Scan the file for additional pstm sections after the first texture block.
// first_section_texture_start is the file offset of the first texture payload byte.
// n_texture_entries is (Number_of_Textures_1D + 1) * (Number_of_Textures_1D >> 1).
u08 PretextLayer_DiscoverAdditionalSections(
    FILE *file,
    u64 file_size,
    memory_arena *arena,
    u64 first_section_texture_start,
    u32 n_texture_entries,
    u64 expected_genome_length,
    u32 expected_n_contigs,
    u08 expected_texture_res,
    u08 expected_n_text_res,
    u08 expected_mip_levels);

u08 PretextLayer_IsEnabled(void);
u32 PretextLayer_GetActiveIndex(void);
u32 PretextLayer_GetCount(void);
const pretext_layer_info *PretextLayer_GetActiveInfo(void);

// Write a short human-readable label for the active layer into buffer.
void PretextLayer_FormatActiveLabel(char *buffer, u32 buffer_size);

// Point the global File_Atlas at the active layer's on-disk offsets.
void PretextLayer_ApplyActiveAtlas(file_atlas_entry **file_atlas);

// Cycle active layer (0 .. n-1). Returns 1 when the active layer changed.
u08 PretextLayer_CycleActive(void);

// Store the first section atlas built during LoadFile as layer 0.
void PretextLayer_AdoptPrimaryAtlas(
    memory_arena *arena,
    file_atlas_entry *atlas,
    u32 n_texture_entries,
    const pretext_layer_info *info);

// File offset after the last loaded layer section (for extension scanning).
u64 PretextLayer_GetFileScanCursor(void);

// Reload contact-matrix GPU textures for the active layer (now moved to PretextView.cpp for better organization --maybe).
u08 PretextLayer_ReloadActiveTextures(memory_arena *arena, const char *file_path);

#endif // PRETEXT_LAYER_H
