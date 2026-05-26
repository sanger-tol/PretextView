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

#include "Header.h"
#include "layer.h"

#include <stdio.h>
#include <string.h>

static const u08 Pretext_File_Magic[4] = {'p', 's', 't', 'm'};

struct pretext_layer_section
{
    file_atlas_entry *atlas;
    u32 n_atlas_entries;
    pretext_layer_info info;
};

struct pretext_layer_manager_internal
{
    u08 enabled;
    u32 n_layers;
    u32 n_loaded_sections;
    u32 active;
    u64 file_scan_cursor;
    pretext_layer_section sections[Max_Pretext_Layers];
};

global_variable
libdeflate_decompressor *
PretextLayer_Decompressor = 0;

global_variable
pretext_layer_manager_internal
Pretext_Layer_Manager = {};

global_function
u08
PretextLayer_HeaderBytesMatch(const u08 *a, const u08 *b, u32 n)
{
    ForLoop(n)
    {
        if (a[index] != b[index])
        {
            return(0);
        }
    }
    return(1);
}

void
PretextLayer_Reset(void)
{
    Pretext_Layer_Manager = {};
}

u08
PretextLayer_ParseHeaderExtension(
    u08 **header,
    u08 *header_end,
    pretext_layer_info *info)
{
    if (!header || !*header || !info)
    {
        return(0);
    }

    u08 *cursor = *header;
    if ((u64)(header_end - cursor) < (u64)Pretext_Layer_Header_Extension_Size)
    {
        return(0);
    }

    if (cursor[0] != Pretext_Layer_Magic_0 ||
        cursor[1] != Pretext_Layer_Magic_1 ||
        cursor[2] != Pretext_Layer_Magic_2 ||
        cursor[3] != Pretext_Layer_Magic_3)
    {
        return(0);
    }

    cursor += 4;
    info->version = *(cursor++);
    info->kind = *(cursor++);

    if (info->version != 1 || info->kind != (u08)pretext_layer_kind_mapq)
    {
        fprintf(stderr, "[PretextView layer] Unsupported layer metadata (version=%u kind=%u)\n",
                info->version, info->kind);
        return(0);
    }

    u16 val16;
    u08 *ptr = (u08 *)&val16;
    ForLoop(2) { *ptr++ = *cursor++; }
    info->index = val16;

    ptr = (u08 *)&val16;
    ForLoop(2) { *ptr++ = *cursor++; }
    info->count = val16;

    u32 val32;
    ptr = (u08 *)&val32;
    ForLoop(4) { *ptr++ = *cursor++; }
    info->mapq_threshold = val32;

    ptr = (u08 *)&val32;
    ForLoop(4) { *ptr++ = *cursor++; }
    info->mapq_min = val32;

    *header = cursor;
    return(1);
}

void
PretextLayer_ApplyLayerInfo(const pretext_layer_info *info)
{
    if (!info || !info->count)
    {
        return;
    }

    Pretext_Layer_Manager.enabled = 1;
    Pretext_Layer_Manager.n_layers = (u32)info->count;
    if (Pretext_Layer_Manager.n_layers > Max_Pretext_Layers)
    {
        Pretext_Layer_Manager.n_layers = Max_Pretext_Layers;
    }
    Pretext_Layer_Manager.active = (u32)info->index;
    if (Pretext_Layer_Manager.active >= Pretext_Layer_Manager.n_layers)
    {
        Pretext_Layer_Manager.active = 0;
    }
}

global_function
u08
PretextLayer_ReadU64(u08 **cursor, u64 *out)
{
    u64 val64 = 0;
    u08 *ptr = (u08 *)&val64;
    ForLoop(8)
    {
        *ptr++ = *(*cursor)++;
    }
    *out = val64;
    return(1);
}

global_function
u08
PretextLayer_ReadU32(u08 **cursor, u32 *out)
{
    u32 val32 = 0;
    u08 *ptr = (u08 *)&val32;
    ForLoop(4)
    {
        *ptr++ = *(*cursor)++;
    }
    *out = val32;
    return(1);
}

global_function
u08
PretextLayer_SkipContigBlock(u08 **cursor, u32 n_contigs)
{
    ForLoop(n_contigs)
    {
        *cursor += 68;
    }
    return(1);
}

global_function
u08
PretextLayer_ValidateSectionHeader(
    u08 *header,
    u32 n_bytes_header,
    u64 expected_genome_length,
    u32 expected_n_contigs,
    u08 expected_texture_res,
    u08 expected_n_text_res,
    u08 expected_mip_levels,
    pretext_layer_info *info_out)
{
    u08 *cursor = header;
    u08 *header_end = header + n_bytes_header;

    u64 genome_length = 0;
    PretextLayer_ReadU64(&cursor, &genome_length);

    u32 n_contigs = 0;
    PretextLayer_ReadU32(&cursor, &n_contigs);

    if (genome_length != expected_genome_length || n_contigs != expected_n_contigs)
    {
        fprintf(stderr, "[PretextView layer] Section header genome/contig count mismatch\n");
        return(0);
    }

    PretextLayer_SkipContigBlock(&cursor, n_contigs);

    if ((u64)(header_end - cursor) < 3)
    {
        return(0);
    }

    u08 texture_res = *(cursor++);
    u08 n_text_res = *(cursor++);
    u08 mip_levels = *(cursor++);

    if (texture_res != expected_texture_res ||
        n_text_res != expected_n_text_res ||
        mip_levels != expected_mip_levels)
    {
        fprintf(stderr, "[PretextView layer] Section header texture parameters mismatch\n");
        return(0);
    }

    pretext_layer_info tmp = {};
    if (PretextLayer_ParseHeaderExtension(&cursor, header_end, &tmp))
    {
        if (info_out)
        {
            *info_out = tmp;
        }
    }
    else if (info_out)
    {
        memset(info_out, 0, sizeof(*info_out));
    }

    return(1);
}

global_function
u32
PretextLayer_BuildAtlasForSection(
    FILE *file,
    u64 texture_data_start,
    u32 n_texture_entries,
    memory_arena *arena,
    file_atlas_entry *atlas)
{
    u32 curr_location = (u32)texture_data_start;

    ForLoop(n_texture_entries)
    {
        file_atlas_entry *entry = atlas + index;
        u32 n_bytes = 0;

        fseek(file, (long)curr_location, SEEK_SET);
        fread(&n_bytes, 1, 4, file);
        curr_location += 4;

        fseek(file, (long)n_bytes, SEEK_CUR);
        entry->base = curr_location;
        entry->nBytes = n_bytes;
        curr_location += n_bytes;
    }

    return(curr_location);
}

global_function
u08
PretextLayer_ParseLayerInfoFromSectionHeader(
    FILE *file,
    u64 section_offset,
    u32 n_bytes_header_comp,
    u32 n_bytes_header,
    memory_arena *arena,
    pretext_layer_info *info_out)
{
    if (!info_out || !PretextLayer_Decompressor || !n_bytes_header_comp || !n_bytes_header)
    {
        return(0);
    }

    u08 *compression_buffer = PushArrayP(arena, u08, n_bytes_header_comp);
    u08 *header = PushArrayP(arena, u08, n_bytes_header);

    fseek(file, (long)(section_offset + 12), SEEK_SET);
    if (fread(compression_buffer, 1, n_bytes_header_comp, file) != n_bytes_header_comp)
    {
        FreeLastPushP(arena);
        FreeLastPushP(arena);
        return(0);
    }

    if (libdeflate_deflate_decompress(
            PretextLayer_Decompressor,
            (const void *)compression_buffer,
            n_bytes_header_comp,
            (void *)header,
            n_bytes_header,
            NULL))
    {
        FreeLastPushP(arena);
        FreeLastPushP(arena);
        return(0);
    }
    FreeLastPushP(arena);

    u08 *cursor = header;
    u08 *header_end = header + n_bytes_header;

    u64 genome_length = 0;
    u32 n_contigs = 0;
    PretextLayer_ReadU64(&cursor, &genome_length);
    PretextLayer_ReadU32(&cursor, &n_contigs);
    PretextLayer_SkipContigBlock(&cursor, n_contigs);

    if ((u64)(header_end - cursor) < 3)
    {
        FreeLastPushP(arena);
        return(0);
    }

    cursor += 3; // textureRes, nTextRes, mipMapLevels

    u08 parsed = PretextLayer_ParseHeaderExtension(&cursor, header_end, info_out);

    FreeLastPushP(arena);
    return(parsed);
}

global_function
u08
PretextLayer_LoadSectionAtlasAt(
    FILE *file,
    u64 file_size,
    u64 section_offset,
    memory_arena *arena,
    u32 n_texture_entries,
    u32 layer_slot,
    u32 *section_end_out)
{
    if (section_offset + 12 > file_size || layer_slot >= Max_Pretext_Layers || !section_end_out)
    {
        return(0);
    }

    u08 magic_test[4];
    fseek(file, (long)section_offset, SEEK_SET);
    if (fread(magic_test, 1, sizeof(magic_test), file) != sizeof(magic_test))
    {
        return(0);
    }

    if (!PretextLayer_HeaderBytesMatch(magic_test, Pretext_File_Magic, 4))
    {
        return(0);
    }

    u32 n_bytes_header_comp = 0;
    u32 n_bytes_header = 0;
    fread(&n_bytes_header_comp, 1, 4, file);
    fread(&n_bytes_header, 1, 4, file);

    if (!n_bytes_header_comp ||
        n_bytes_header_comp > (256u * 1024u * 1024u) ||
        section_offset + 12 + (u64)n_bytes_header_comp > file_size)
    {
        return(0);
    }

    pretext_layer_info info = {};
    info.index = (u16)layer_slot;
    PretextLayer_ParseLayerInfoFromSectionHeader(
        file,
        section_offset,
        n_bytes_header_comp,
        n_bytes_header,
        arena,
        &info);
    if (!info.count)
    {
        info.count = (u16)Pretext_Layer_Manager.n_layers;
    }
    if (!info.count)
    {
        info.count = (u16)(layer_slot + 1);
    }

    u64 texture_data_start = section_offset + 12 + (u64)n_bytes_header_comp;
    file_atlas_entry *atlas = PushArrayP(arena, file_atlas_entry, n_texture_entries);
    u32 section_end = PretextLayer_BuildAtlasForSection(
        file,
        texture_data_start,
        n_texture_entries,
        arena,
        atlas);

    if (section_end <= (u32)texture_data_start)
    {
        FreeLastPushP(arena);
        return(0);
    }

    pretext_layer_section *section = Pretext_Layer_Manager.sections + layer_slot;
    section->atlas = atlas;
    section->n_atlas_entries = n_texture_entries;
    section->info = info;

    *section_end_out = section_end;
    return(1);
}

global_function
u32
PretextLayer_ScanFileSectionOffsets(
    FILE *file,
    u64 file_size,
    u64 *offsets,
    u32 max_offsets)
{
    u32 found = 0;

    for (u64 pos = 0; pos + 16 < file_size && found < max_offsets; ++pos)
    {
        u08 magic_test[4];
        fseek(file, (long)pos, SEEK_SET);
        if (fread(magic_test, 1, sizeof(magic_test), file) != sizeof(magic_test))
        {
            break;
        }

        if (!PretextLayer_HeaderBytesMatch(magic_test, Pretext_File_Magic, 4))
        {
            continue;
        }

        u32 n_bytes_header_comp = 0;
        fread(&n_bytes_header_comp, 1, 4, file);
        if (n_bytes_header_comp < 64 ||
            n_bytes_header_comp > (256u * 1024u * 1024u) ||
            pos + 12 + (u64)n_bytes_header_comp > file_size)
        {
            continue;
        }

        offsets[found++] = pos;
        pos = pos + 11 + (u64)n_bytes_header_comp;
    }

    return(found);
}

void
PretextLayer_AdoptPrimaryAtlas(
    memory_arena *arena,
    file_atlas_entry *atlas,
    u32 n_texture_entries,
    const pretext_layer_info *info)
{
    (void)arena;

    pretext_layer_section *section = Pretext_Layer_Manager.sections + 0;
    section->atlas = atlas;
    section->n_atlas_entries = n_texture_entries;

    if (info)
    {
        section->info = *info;
        PretextLayer_ApplyLayerInfo(info);
    }
    else
    {
        memset(&section->info, 0, sizeof(section->info));
        section->info.index = 0;
        section->info.count = 1;
    }

    Pretext_Layer_Manager.n_loaded_sections = 1;
    if (!Pretext_Layer_Manager.n_layers)
    {
        Pretext_Layer_Manager.n_layers = 1;
    }
}

global_function
u32
PretextLayer_SectionEndFromAtlas(
    file_atlas_entry *atlas,
    u32 n_texture_entries)
{
    if (!atlas || !n_texture_entries)
    {
        return(0);
    }

    file_atlas_entry *last = atlas + (n_texture_entries - 1);
    return(last->base + last->nBytes);
}

u08
PretextLayer_DiscoverAdditionalSections(
    FILE *file,
    u64 file_size,
    memory_arena *arena,
    u64 first_section_texture_start,
    u32 n_texture_entries,
    u64 expected_genome_length,
    u32 expected_n_contigs,
    u08 expected_texture_res,
    u08 expected_n_text_res,
    u08 expected_mip_levels)
{
    (void)first_section_texture_start;
    (void)expected_genome_length;
    (void)expected_n_contigs;
    (void)expected_texture_res;
    (void)expected_n_text_res;
    (void)expected_mip_levels;

    u32 layer_slot = 1;
    u32 header_layer_count = Pretext_Layer_Manager.n_layers;

    Pretext_Layer_Manager.file_scan_cursor = PretextLayer_SectionEndFromAtlas(
        Pretext_Layer_Manager.sections[0].atlas,
        n_texture_entries);

    u32 cursor = (u32)Pretext_Layer_Manager.file_scan_cursor;
    u32 max_layers_to_probe = (header_layer_count > 1) ? header_layer_count : Max_Pretext_Layers;

    while (layer_slot < max_layers_to_probe &&
           layer_slot < Max_Pretext_Layers &&
           (u64)(cursor + 4) <= file_size)
    {
        u32 section_end = 0;
        if (!PretextLayer_LoadSectionAtlasAt(
                file,
                file_size,
                (u64)cursor,
                arena,
                n_texture_entries,
                layer_slot,
                &section_end))
        {
            break;
        }

        cursor = section_end;
        ++layer_slot;
    }

    if (layer_slot <= 1)
    {
        u64 section_offsets[Max_Pretext_Layers];
        u32 n_offsets = PretextLayer_ScanFileSectionOffsets(
            file,
            file_size,
            section_offsets,
            Max_Pretext_Layers);

        layer_slot = 1;
        for (u32 scan_index = 1; scan_index < n_offsets && layer_slot < Max_Pretext_Layers; ++scan_index)
        {
            u32 section_end = 0;
            if (PretextLayer_LoadSectionAtlasAt(
                    file,
                    file_size,
                    section_offsets[scan_index],
                    arena,
                    n_texture_entries,
                    layer_slot,
                    &section_end))
            {
                cursor = section_end;
                ++layer_slot;
            }
        }
    }

    Pretext_Layer_Manager.n_loaded_sections = layer_slot;
    Pretext_Layer_Manager.n_layers = layer_slot;
    Pretext_Layer_Manager.enabled = (layer_slot > 1) ? 1 : 0;
    if (Pretext_Layer_Manager.active >= Pretext_Layer_Manager.n_layers)
    {
        Pretext_Layer_Manager.active = 0;
    }

    if (header_layer_count > 1 && layer_slot < header_layer_count)
    {
        fprintf(stderr,
                "[PretextView layer] Header listed %u layers but %u were loaded from file\n",
                header_layer_count,
                layer_slot);
    }
    else if (layer_slot > 1)
    {
        printf("[PretextView] Loaded %u MAPQ layers (press 0 to switch)\n", layer_slot);
    }

    Pretext_Layer_Manager.file_scan_cursor = (u64)cursor;
    return(layer_slot > 1);
}

u64
PretextLayer_GetFileScanCursor(void)
{
    return(Pretext_Layer_Manager.file_scan_cursor);
}

void
PretextLayer_SetDecompressor(libdeflate_decompressor *decompressor)
{
    PretextLayer_Decompressor = decompressor;
}

u08
PretextLayer_IsEnabled(void)
{
    return(Pretext_Layer_Manager.n_loaded_sections > 1);
}

u32
PretextLayer_GetActiveIndex(void)
{
    return(Pretext_Layer_Manager.active);
}

u32
PretextLayer_GetCount(void)
{
    return(Pretext_Layer_Manager.n_layers ? Pretext_Layer_Manager.n_layers : 1);
}

const pretext_layer_info *
PretextLayer_GetActiveInfo(void)
{
    if (Pretext_Layer_Manager.active < Max_Pretext_Layers)
    {
        return(&Pretext_Layer_Manager.sections[Pretext_Layer_Manager.active].info);
    }
    return(0);
}

void
PretextLayer_FormatActiveLabel(char *buffer, u32 buffer_size)
{
    if (!buffer || !buffer_size)
    {
        return;
    }

    buffer[0] = 0;
    if (!PretextLayer_IsEnabled())
    {
        return;
    }

    const pretext_layer_info *info = PretextLayer_GetActiveInfo();
    if (info && info->kind == (u08)pretext_layer_kind_mapq)
    {
        snprintf(
            buffer,
            buffer_size,
            "Layer %u/%u (MAPQ >= %u)",
            (u32)(Pretext_Layer_Manager.active + 1),
            PretextLayer_GetCount(),
            info->mapq_threshold);
    }
    else
    {
        snprintf(
            buffer,
            buffer_size,
            "Layer %u/%u",
            (u32)(Pretext_Layer_Manager.active + 1),
            PretextLayer_GetCount());
    }
}

void
PretextLayer_ApplyActiveAtlas(file_atlas_entry **file_atlas)
{
    if (!file_atlas)
    {
        return;
    }

    u32 active = Pretext_Layer_Manager.active;
    if (active < Max_Pretext_Layers && Pretext_Layer_Manager.sections[active].atlas)
    {
        *file_atlas = Pretext_Layer_Manager.sections[active].atlas;
    }
}

u08
PretextLayer_CycleActive(void)
{
    u32 n = Pretext_Layer_Manager.n_loaded_sections;
    if (n <= 1)
    {
        return(0);
    }

    for (u32 step = 0; step < n; ++step)
    {
        Pretext_Layer_Manager.active = (Pretext_Layer_Manager.active + 1) % n;
        if (Pretext_Layer_Manager.sections[Pretext_Layer_Manager.active].atlas)
        {
            return(1);
        }
    }

    return(0);
}
