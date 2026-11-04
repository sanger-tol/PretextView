#ifndef PRETEXT_VIEW_CONSTANTS_H
#define PRETEXT_VIEW_CONSTANTS_H

#include <limits>

#define PretextView_Version_Label "1.1.0-beta"
#define PretextView_Version "PretextViewAI Version " PretextView_Version_Label
#define PretextView_Title "PretextViewAI " PretextView_Version_Label " - Wellcome Sanger Institute"

#define UI_SHADER_LOC_POSITION 0
#define UI_SHADER_LOC_TEXCOORD 1
#define UI_SHADER_LOC_COLOR 2

#define text_box_font_size_default 20.0f
#define Grey_Background {0.569f, 0.549f, 0.451f, 1.0f}
#define Yellow_Text_Float {0.941176471f, 0.725490196f, 0.058823529f, 1.0f}
#define Red_Text_Float {0.941176471f, 0.039215686f, 0.019607843f, 1.0f}
#define Green_Float {0.3f, 0.6f, 0.0f, 0.2f}
#define Red_Float {0.6f, 0.3f, 0.0f, 0.2f}
#define Blue_Float {0.0f, 0.3f, 0.6f, 0.2f}
#define Red_Full {1.0f, 0.0f, 0.0f, 1.0f}
#define Blue_Full {0.0f, 0.0f, 1.0f, 1.0f}

#define MAX_PATH_LEN 512
#define ClipboardEditorBufferSize 32768
#define ClipboardEditorLastCopiedSize 4096
#define Edits_Stack_Size 32768
#define Waypoints_Stack_Size 128
#define Waypoints_Quadtree_Levels 5
#define Waypoint_Select_Distance 8.0f
#define Waypoint_Rect_Epsilon 0.001f
#define MaxTicksPerScaleBar 128
#define DefaultWaypointSize 18.0f
#define DefaultExtensionSize 20.0f
#define DefaultScaffSize 40.0f
#define DefaultMetaDataSize 20.0f
#define MAX_VERTEX_MEMORY KiloByte(512)
#define MAX_ELEMENT_MEMORY KiloByte(128)
#define DefaultGraphScale 0.2f
#define DefaultGraphBase 32.0f
#define DefaultGraphLineSize 1.0f
#define DefaultGraphColour {0.1f, 0.8f, 0.7f, 1.0f}
#define Colour_5p_telomere {0.533f, 0.800f, 0.933f, 1.0f}
#define Colour_3p_telomere {0.867f, 0.800f, 0.467f, 1.0f}
#define DefaultNameLabelTextSize 32.0f
#define DefaultScaleBarSize 20.0f
#define DefaultGridSize 0.00025f
#define DefaultContigIdSize (DefaultGridSize * 3.0f)
#define DefaultToolTipTextSize 20.0f
#define NK_Memory_Size MegaByte(32)
#define SaveState_Format_Version_Min 2
#define SaveState_Format_Version_Current 3
#define SaveState_Scaff_Entry_Bytes_V2 8
#define SaveState_Scaff_Entry_Bytes_V3 12

global_variable const char *bg_color[] = {
    "Classic",
    "Jasper",
    "Heather",
    "Carolina",
    "White",
    "Grey",
    "Black"};

global_variable const char *metaColors[] = {
    "Color set 1",
    "Color set 2",
    "Color set 3",
    "Color set 4"};

global_variable const char *outlineColors[] = {
    "No-Outline",
    "Black-Outline",
    "White-Outline"};

global_variable const char *colour_graph[] = {
    "Default",
    "Vermillion",
    "Blue",
    "Purple"};

global_variable const char *EXT_NAME_3P_TELOMERE = "3p_telomere";
global_variable const char *EXT_NAME_5P_TELOMERE = "5p_telomere";
global_variable const char *EXT_NAME_TELOMERE = "telomere";
global_variable const char *EXT_NAME_COVERAGE = "coverage";
global_variable const char *EXT_NAME_GAP = "gap";
global_variable const char *EXT_NAME_REPEAT_DENSITY = "repeat_density";

global_variable const char *Default_Tags[] = {
    "Haplotig",
    "Unloc",
    "X",
    "Y",
    "Z",
    "W",
    "HAP1",
    "HAP2",
    "Target",
    "Contaminant",
    "Singleton",
    "X1",
    "X2",
    "Y1",
    "Y2",
    "Z1",
    "Z2",
    "W1",
    "W2",
    "I",
    "II",
    "III",
    "IV",
    "V",
    "U",
    "B1",
    "B2",
    "B3",
    "FalseDuplicate",
    "Primary",
    "vertPaint",
    "horzPaint",
    "crossPaint",
};

global_variable const s32 Break_Edit_Delta = std::numeric_limits<s32>::min();

#endif
