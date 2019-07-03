/*
Copyright (c) 2019 Ed Harry, Wellcome Sanger Institute

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

#define PretextView_Version "PretextView Version 0.0.4-dev"

#include "Header.h"

#ifdef DEBUG
#include <errno.h>
#endif

#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#include <glad/glad.h>
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#pragma GCC diagnostic ignored "-Wdocumentation-unknown-command"
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#include <GLFW/glfw3.h>
#pragma clang diagnostic pop

#include "TextureLoadQueue.cpp"
#include "ColorMapData.cpp"

#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wcomma"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"
#define GLFONTSTASH_IMPLEMENTATION
#include "gl3corefontstash.h"
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wextra-semi-stmt"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-variable-declarations"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#pragma GCC diagnostic ignored "-Wextra-semi-stmt"
#pragma GCC diagnostic ignored "-Wcomma"
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#ifndef DEBUG
#define STBIW_ASSERT(x)
#endif
#include "stb_image_write.h"
#pragma clang diagnostic pop

#if 1
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#ifndef DEBUG
#define STBI_ASSERT(x)
#endif
#include "stb_image.h"
#pragma clang diagnostic pop
#endif

#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wcovered-switch-default"
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#pragma GCC diagnostic ignored "-Wcomma"
#pragma GCC diagnostic ignored "-Wextra-semi-stmt"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#define NK_PRIVATE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_STANDARD_IO
#define NK_ZERO_COMMAND_MEMORY
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_INCLUDE_STANDARD_VARARGS
#ifndef DEBUG
#define NK_ASSERT(x)
#endif
#include "nuklear.h"
#pragma clang diagnostic pop

#include "Resources.cpp"

global_variable
const
GLchar *
VertexSource_Texture = R"glsl(
    #version 330
    in vec2 position;
    in vec3 texcoord;
    out vec3 Texcoord;
    uniform mat4 matrix;
    void main()
    {
        Texcoord = texcoord;
        gl_Position = matrix * vec4(position, 0.0, 1.0);
    }
)glsl";

global_variable
const
GLchar *
FragmentSource_Texture = R"glsl(
    #version 330
    in vec3 Texcoord;
    out vec4 outColor;
    uniform sampler2DArray tex;
    uniform usamplerBuffer pixstartlookup;
    uniform usamplerBuffer pixrearrangelookup;
    uniform uint pixpertex;
    uniform uint ntex1dm1;
    uniform float oopixpertex;
    uniform samplerBuffer colormap;
    uniform vec3 controlpoints;
    float bezier(float t)
    {
        float tsq = t * t;
        float omt = 1.0 - t;
        float omtsq = omt * omt;
        return((omtsq * controlpoints.x) + (2.0 * t * omt * controlpoints.y) + (tsq * controlpoints.z));
    }
    float linearTextureID(vec2 coords)
    {
        float min;
        float max;
        
        if (coords.x > coords.y)
        {
            min = coords.y;
            max = coords.x;
        }
        else
        {
            min = coords.x;
            max = coords.y;
        }

        int i = int(min);
        return((min * ntex1dm1) -
        ((i & 1) == 1 ? (((i-1)>>1) * min) : 
         ((i>>1)*(min-1))) + max);
    }
    // https://community.khronos.org/t/mipmap-level-calculation-using-dfdx-dfdy/67480
    float mip_map_level(in vec2 texture_coordinate)
    {
        // The OpenGL Graphics System: A Specification 4.2
        //  - chapter 3.9.11, equation 3.21


        vec2  dx_vtc        = dFdx(texture_coordinate);
        vec2  dy_vtc        = dFdy(texture_coordinate);
        float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));

        return 0.5 * log2(delta_max_sqr);
    }
    vec3 pixLookup(vec3 inCoords)
    {
        vec2 pix = floor(inCoords.xy * pixpertex);
        
        vec2 over = inCoords.xy - (pix * oopixpertex);
        vec2 pixStart = texelFetch(pixstartlookup, int(inCoords.z)).xy;
        
        pix += pixStart;
        
        pix = vec2(texelFetch(pixrearrangelookup, int(pix.x)).x, texelFetch(pixrearrangelookup, int(pix.y)).x);
        
        if (pix.y > pix.x)
        {
            pix = vec2(pix.y, pix.x);
            over = vec2(over.y, over.x);
        }
        
        vec2 tileCoord = pix * oopixpertex;
        vec2 tileCoordFloor = floor(tileCoord);
        
        float z = linearTextureID(tileCoordFloor);
        
        pix = tileCoord - tileCoordFloor;
        pix = clamp(pix + over, vec2(0, 0), vec2(1, 1));
        
        return(vec3(pix, z));
    }
    // https://www.codeproject.com/Articles/236394/Bi-Cubic-and-Bi-Linear-Interpolation-with-GLSL/
    float CatMullRom( float x )
    {
        const float B = 0.0;
        const float C = 0.5;
        float f = x;
        if( f < 0.0 )
        {
            f = -f;
        }
        if( f < 1.0 )
        {
            return ( ( 12 - 9 * B - 6 * C ) * ( f * f * f ) +
                ( -18 + 12 * B + 6 *C ) * ( f * f ) +
                ( 6 - 2 * B ) ) / 6.0;
        }
        else if( f >= 1.0 && f < 2.0 )
        {
            return ( ( -B - 6 * C ) * ( f * f * f )
                + ( 6 * B + 30 * C ) * ( f *f ) +
                ( - ( 12 * B ) - 48 * C  ) * f +
                8 * B + 24 * C)/ 6.0;
        }
        else
        {
            return 0.0;
        }
    } 
    // https://www.codeproject.com/Articles/236394/Bi-Cubic-and-Bi-Linear-Interpolation-with-GLSL/
    float BiCubic( vec3 inCoord, vec2 texSize, float lod )
    {
        //float texelSizeX = 1.0 / fWidth; //size of one texel 
        //float texelSizeY = 1.0 / fHeight; //size of one texel
        vec2 texelSize = 1.0 / texSize;
        float nSum = 0.0;
        float nDenom = 0.0;
        //float a = fract( TexCoord.x * fWidth ); // get the decimal part
        //float b = fract( TexCoord.y * fHeight ); // get the decimal part
        vec2 a = fract(inCoord.xy * texSize);
        for( int m = -1; m <=2; m++ )
        {
            for( int n =-1; n<= 2; n++)
            {
                float vecData = textureLod(tex, 
                                   pixLookup(inCoord + vec3(texelSize * vec2(float( m ), float( n )), 0)), lod).r;
                float f  = CatMullRom( float( m ) - a.x );
                float f1 = CatMullRom( -( float( n ) - a.y ) );
                nSum = nSum + ( vecData * f1 * f  );
                nDenom = nDenom + ( f1 * f );
            }
        }
        return nSum / nDenom;
    }
    // https://www.codeproject.com/Articles/236394/Bi-Cubic-and-Bi-Linear-Interpolation-with-GLSL/
    float BiLinear( vec3 inCoord, vec2 texSize, float lod )
    {
        vec2 texelSize = 1.0 / texSize;

        vec3 coord = pixLookup(inCoord);
        
        float p0q0 = textureLod(tex, coord, lod).r;
        float p1q0 = textureLod(tex, pixLookup(inCoord + vec3(texelSize.x, 0, 0)), lod).r;

        float p0q1 = textureLod(tex, pixLookup(inCoord + vec3(0, texelSize.y, 0)), lod).r;
        float p1q1 = textureLod(tex, pixLookup(inCoord + vec3(texelSize, 0)), lod).r;

        float a = fract( coord.x * texSize.x ); // Get Interpolation factor for X direction.
                        // Fraction near to valid data.

        float pInterp_q0 = mix( p0q0, p1q0, a ); // Interpolates top row in X direction.
        float pInterp_q1 = mix( p0q1, p1q1, a ); // Interpolates bottom row in X direction.

        float b = fract( coord.y * texSize.y );// Get Interpolation factor for Y direction.
        return mix( pInterp_q0, pInterp_q1, b ); // Interpolate in Y direction.
    }
    void main()
    {
        float mml = mip_map_level(Texcoord.xy * textureSize(tex, 0).xy);
        
        float floormml = floor(mml);

        float f1 = BiLinear(Texcoord, textureSize(tex, int(floormml)).xy, floormml);
        float f2 = BiLinear(Texcoord, textureSize(tex, int(floormml) + 1).xy, floormml + 1);

        float value = bezier(mix(f1, f2, fract(mml)));
        int idx = int(round(value * 0xFF));
        outColor = vec4(texelFetch(colormap, idx).rgb, 1.0);
    }
)glsl";

global_variable
const
GLchar *
VertexSource_Flat = R"glsl(
    #version 330
    in vec2 position;
    uniform mat4 matrix;
    void main()
    {
        gl_Position = matrix * vec4(position, 0.0, 1.0);
    }
)glsl";

global_variable
const
GLchar *
FragmentSource_Flat = R"glsl(
    #version 330
    out vec4 outColor;
    uniform vec4 color;
    void main()
    {
        outColor = color;
    }
)glsl";

#define UI_SHADER_LOC_POSITION 0
#define UI_SHADER_LOC_TEXCOORD 1
#define UI_SHADER_LOC_COLOR 2

global_variable
const
GLchar *
VertexSource_UI = R"glsl(
    #version 330
    layout (location = 0) in vec2 position;
    layout (location = 1) in vec2 texcoord;
    out vec2 Texcoord;
    layout (location = 2) in vec4 color;
    out vec4 Color;
    uniform mat4 matrix;
    void main()
    {
        Texcoord = texcoord;
        Color = color;
        gl_Position = matrix * vec4(position, 0.0, 1.0);
    }
)glsl";

global_variable
const
GLchar *
FragmentSource_UI = R"glsl(
    #version 330
    in vec2 Texcoord;
    in vec4 Color;
    out vec4 outColor;
    uniform sampler2D tex;
    void main()
    {
        outColor = texture(tex, Texcoord) * Color;
    }
)glsl";

struct
tex_vertex
{
    f32 x, y, pad;
    f32 s, t, u;
};

struct
vertex
{
    f32 x, y;
};

global_variable
memory_arena
Working_Set;

global_variable
thread_pool *
Thread_Pool;

global_variable
u32
Redisplay = 0;

global_variable
s32
Window_Width, Window_Height, FrameBuffer_Width, FrameBuffer_Height;

global_variable
struct nk_vec2
Screen_Scale;

enum
theme
{
    THEME_BLACK,
    THEME_WHITE,
    THEME_RED,
    THEME_BLUE,
    THEME_DARK,
    
    THEME_COUNT
};

global_variable
theme
Current_Theme;

global_variable
u08 *
Theme_Name[THEME_COUNT];

global_function
void
SetTheme(struct nk_context *ctx, enum theme theme);

global_variable
nk_context *
NK_Context;

global_function
void
ChangeSize(s32 width, s32 height)
{
    glViewport(0, 0, width, height);
}

global_function
void
UpdateScreenScale()
{
    Screen_Scale.x = (f32)FrameBuffer_Width / (f32)Window_Width;
    Screen_Scale.y = (f32)FrameBuffer_Height / (f32)Window_Height;
    SetTheme(NK_Context, Current_Theme);
}

global_function
void
GLFWChangeFrameBufferSize(GLFWwindow *win, s32 width, s32 height)
{
    (void)win;
    ChangeSize(width, height);
    Redisplay = 1;
    FrameBuffer_Height = height;
    FrameBuffer_Width = width;
    UpdateScreenScale();
}

global_function
void
GLFWChangeWindowSize(GLFWwindow *win, s32 width, s32 height)
{
    (void)win;
    Window_Width = width;
    Window_Height = height;
    UpdateScreenScale();
}

struct
contact_matrix
{
    GLuint textures;
    GLuint pixelStartLookupBuffer;
    GLuint pixelRearrangmentLookupBuffer;
    GLuint pixelStartLookupBufferTex;
    GLuint pixelRearrangmentLookupBufferTex;
    GLint pad;
    GLuint *vaos;
    GLuint *vbos;
    GLuint shaderProgram;
    GLint matLocation;
};

struct
flat_shader
{
    GLuint shaderProgram;
    GLint matLocation;
    GLint colorLocation;
    u32 pad;
};

struct
ui_shader
{
    GLuint shaderProgram;
    GLint matLocation;
};

struct
quad_data
{
    GLuint *vaos;
    GLuint *vbos;
    u32 nBuffers;
    u32 pad;
};

struct
color_maps
{
    GLuint *maps;
    u32 currMap;
    u32 nMaps;
    GLint cpLocation;
    GLfloat controlPoints[3];
    struct nk_image *mapPreviews;
};

struct 
nk_glfw_vertex
{
    f32 position[2];
    f32 uv[2];
    nk_byte col[4];
};

struct
device
{
    u08 *lastContextMemory;
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    GLuint vbo, vao, ebo;
    GLuint prog;
    GLuint vert_shdr;
    GLuint frag_shdr;
    GLint attrib_pos;
    GLint attrib_uv;
    GLint attrib_col;
    GLint uniform_tex;
    GLint uniform_proj;
    GLuint font_tex;
};

global_variable
device *
NK_Device;

global_variable
nk_font_atlas *
NK_Atlas;

global_variable
nk_font *
NK_Font;

global_variable
nk_font *
NK_Font_Browser;

global_variable
color_maps *
Color_Maps;

#ifdef DEBUG
global_variable
quad_data *
Texture_Tile_Grid;

global_variable
quad_data *
QuadTree_Data;
#endif

global_variable
quad_data *
Grid_Data;

global_variable
quad_data *
Select_Box_Data;

global_variable
quad_data *
Edit_Mode_Data;

global_variable
quad_data *
Label_Box_Data;

global_variable
quad_data *
Scale_Bar_Data;

global_variable
quad_data *
Tool_Tip_Data;

global_variable
quad_data *
Waypoint_Data;

global_variable
ui_shader *
UI_Shader;

global_variable
flat_shader *
Flat_Shader;

global_variable
contact_matrix *
Contact_Matrix;

global_variable
threadSig
Texture_Ptr = 0;

global_variable
volatile texture_buffer *
Current_Loaded_Texture = 0;

global_variable
u32
Texture_Resolution;

global_variable
u32
Number_of_MipMaps;

global_variable
u32
Number_of_Textures_1D;

global_variable
u32
Bytes_Per_Texture;

global_variable
texture_buffer_queue *
Texture_Buffer_Queue;

struct
pointi
{
    s32 x;
    s32 y;
};

struct
pointui
{
    u32 x, y;
};

struct
pointd
{
    f64 x;
    f64 y;
};

struct
point2f
{
    f32 x;
    f32 y;
};

struct
quad
{
    point2f corner[2];
};

struct
point3f
{
    f32 x;
    f32 y;
    f32 z;
};

global_variable
pointd
Mouse_Move;

struct
tool_tip
{
    pointui pixels;
    point2f worldCoords;
};

global_variable
tool_tip
Tool_Tip_Move;

struct
edit_pixels
{
    pointui pixels;
    point2f worldCoords;
    u32 editing;
    u32 pad;
};

global_variable
edit_pixels
Edit_Pixels;

global_variable
pointd
Mouse_Select;

global_variable
quad
Select_Box;

global_variable
point3f
Camera_Position;

global_variable
FONScontext *
FontStash_Context;

struct
ui_colour_element_bg
{
    u32 on;
    nk_colorf fg;
    nk_colorf bg;
};

#define Grey_Background {0.569f, 0.549f, 0.451f, 1.0f}
//#define Yellow_Text glfonsRGBA(240, 185, 15, 255)
#define Yellow_Text_Float {0.941176471f, 0.725490196f, 0.058823529f, 1.0f}
//#define Red_Text glfonsRGBA(240, 10, 5, 255)
#define Red_Text_Float {0.941176471f, 0.039215686f, 0.019607843f, 1.0f}

#define Green_Float {0.3f, 0.6f, 0.0f, 0.2f}
#define Red_Float {0.6f, 0.3f, 0.0f, 0.2f}
#define Blue_Float {0.0f, 0.3f, 0.6f, 0.2f}

#define Red_Full {1.0f, 0.0f, 0.0f, 1.0f}
#define Blue_Full {0.0f, 0.0f, 1.0f, 1.0f}

global_variable
ui_colour_element_bg *
Contig_Name_Labels;

global_variable
ui_colour_element_bg *
Scale_Bars;

global_variable
ui_colour_element_bg *
Tool_Tip;

struct
ui_colour_element
{
    u32 on;
    nk_colorf bg;
};

global_variable
ui_colour_element *
Grid;

#ifdef DEBUG
global_variable
ui_colour_element *
Tiles;

global_variable
ui_colour_element *
QuadTrees;
#endif

struct
edit_mode_colours
{
    nk_colorf preSelect;
    nk_colorf select;
    nk_colorf invSelect;
    nk_colorf fg;
    nk_colorf bg;
};

global_variable
edit_mode_colours *
Edit_Mode_Colours;

struct
waypoint_mode_colours
{
    nk_colorf base;
    nk_colorf selected;
    nk_colorf text;
    nk_colorf bg;
};

global_variable
waypoint_mode_colours *
Waypoint_Mode_Colours;

global_variable
u32
UI_On = 0;

enum
global_mode
{
    mode_normal = 0,
    mode_edit = 1,
    mode_waypoint = 2
};

global_variable
global_mode
Global_Mode = mode_normal;

#define Edit_Mode (Global_Mode == mode_edit)
#define Normal_Mode (Global_Mode == mode_normal)
#define Waypoint_Mode (Global_Mode == mode_waypoint)

global_variable
s32
Font_Normal = FONS_INVALID;

global_variable
s32
Font_Bold = FONS_INVALID;

global_function
void
ClampCamera()
{
    Camera_Position.z = Min(Max(Camera_Position.z, 1.0f), ((f32)(Pow2((Number_of_MipMaps + 1)))));
    Camera_Position.x = Min(Max(Camera_Position.x, -0.5f), 0.5f);
    Camera_Position.y = Min(Max(Camera_Position.y, -0.5f), 0.5f);
}

global_function
void
ZoomCamera(f32 dir)
{
    f32 scrollFactor = 1.1f;
    scrollFactor += dir > 0.0f ? dir : -dir;
    Camera_Position.z *= (dir > 0.0f ? scrollFactor : (1.0f / scrollFactor));
    ClampCamera();
}

global_variable
u32
Loading = 0;

enum
rearrange_map_flags
{
    rearrangeFlag_internalContigLock = 1,
    rearrangeFlag_allowNewContigCreation = 2,
    rearrangeFlag_wasWholeContig = 4,
    rearrangeFlag_firstMove = 8
};

global_function
s32
RearrangeMap(u32 pixelFrom, u32 pixelTo, s32 delta, u32 *flags);

global_function
void
InvertMap(u32 pixelFrom, u32 pixelTo);

global_variable
u16 *
Pixel_Contig_Lookup;

global_variable
u32
Global_Edit_Invert_Flag = 0;

global_variable
u32
Number_of_Contigs_to_Display;

#define Max_Number_of_Contigs_to_Display 2048

global_variable
u32 *
Contig_Display_Order;

struct
contig
{
    f32 fractionalLength;
    u32 index;
    u32 name[16];
    contig *prev;
    contig *next;
};

struct
contig_sentinel
{
    contig *tail;
    u32 nFree;
    u32 pad;
};

global_variable
contig_sentinel *
Contig_Sentinel;

global_variable
contig *
Contigs;

enum
map_edit_flags
{
    editFlag_inverted = 1,
    editFlag_wasWholeContig = 2
};

struct
map_edit
{
    pointui finalPixels;
    s32 delta;
    u16 oldContigId;
    u16 flags;
    u32 oldContigStartPixel;
    u32 newContigStartPixel;
    u32 oldName[16];
    u32 newName[16];
};

struct
waypoint;

struct
waypoint_quadtree_node
{
    waypoint *wayp;
    waypoint_quadtree_node *next;
    waypoint_quadtree_node *prev;
};

struct
waypoint
{
    point2f coords;
    f32 z;
    u32 index;
    waypoint *prev;
    waypoint *next;
    waypoint_quadtree_node *node;
};

#define Edits_Stack_Size 128
#define Waypoints_Stack_Size 128

struct
map_editor
{
    map_edit *edits;
    u32 nEdits;
    u32 editStackPtr;
};

global_variable
map_editor *
Map_Editor;

#define Waypoints_Quadtree_Levels 5

struct
waypoint_quadtree_level
{
#ifdef DEBUG
    u32 show;
#else
    u32 pad;
#endif
    f32 size;
    point2f lowerBound;
    waypoint_quadtree_level *children[4];
    waypoint_quadtree_node headNode;
};

struct
waypoint_editor
{
    waypoint freeWaypoints;
    waypoint activeWaypoints;
    waypoint_quadtree_level *quadtree;
    waypoint_quadtree_node freeNodes;
    u32 nWaypointsActive;
    u32 pad;
};

global_variable
waypoint_editor *
Waypoint_Editor;

#define Waypoint_Select_Distance 8.0f
global_variable
waypoint *
Selected_Waypoint = 0;

global_function
waypoint_quadtree_level *
PushQuadTree(memory_arena *arena, u32 level = 0, point2f lowerBound = {-0.5f, -0.5f}, f32 size = 1.0f)
{
    waypoint_quadtree_level *quadtreeLevel = 0;

    if (level < Waypoints_Quadtree_Levels)
    {
        quadtreeLevel = PushStructP(arena, waypoint_quadtree_level);
        quadtreeLevel->size = size;
        quadtreeLevel->lowerBound = lowerBound;
        quadtreeLevel->headNode = {};

#ifdef DEBUG
        quadtreeLevel->show = 0;
#endif

        f32 halfSize = size * 0.5f;
        quadtreeLevel->children[0] = PushQuadTree(arena, level + 1, {lowerBound.x, lowerBound.y}, halfSize);
        quadtreeLevel->children[1] = PushQuadTree(arena, level + 1, {lowerBound.x, lowerBound.y + halfSize}, halfSize);
        quadtreeLevel->children[2] = PushQuadTree(arena, level + 1, {lowerBound.x + halfSize, lowerBound.y}, halfSize);
        quadtreeLevel->children[3] = PushQuadTree(arena, level + 1, {lowerBound.x + halfSize, lowerBound.y + halfSize}, halfSize);
    }

    return(quadtreeLevel);
}

global_function
void
GetWaypointsWithinRectange(point2f lowerBound, point2f size, waypoint ***bufferPtr);

global_function
void
UpdateWayPoint(waypoint *wayp, point2f coords);

global_function
void
MoveWayPoints(map_edit *edit, u32 undo = 0);

global_function
void
AddMapEdit(s32 delta, pointui finalPixels, u16 oldId, u32 originalContigFirstPixel, u16 flags, u32 *oldName)
{
    ++Map_Editor->nEdits;
    
    map_edit *edit = Map_Editor->edits + Map_Editor->editStackPtr++;

    if (Map_Editor->editStackPtr == Edits_Stack_Size)
    {
        Map_Editor->editStackPtr = 0;
    }

    u32 newContigFirstPixel = Min(finalPixels.x, finalPixels.y);

    while (newContigFirstPixel > 0 && Pixel_Contig_Lookup[newContigFirstPixel] == Pixel_Contig_Lookup[newContigFirstPixel - 1])
    {
        --newContigFirstPixel;
    }

    edit->finalPixels = finalPixels;
    edit->flags = flags;
    edit->oldContigId = oldId;
    edit->delta = delta;
    edit->oldContigStartPixel = originalContigFirstPixel;
    edit->newContigStartPixel = newContigFirstPixel;

    ForLoop(16)
    {
        edit->oldName[index] = oldName[index];
    }

    ForLoop(16)
    {
        edit->newName[index] = (Contigs + Pixel_Contig_Lookup[newContigFirstPixel])->name[index];
    }

    MoveWayPoints(edit);
}

global_function
void
UndoMapEdit()
{
    if (Map_Editor->nEdits && !Edit_Pixels.editing)
    {
        --Map_Editor->nEdits;

        if (!Map_Editor->editStackPtr)
        {
            Map_Editor->editStackPtr = Edits_Stack_Size + 1;
        }

        map_edit *edit = Map_Editor->edits + (--Map_Editor->editStackPtr);

        if (edit->flags & editFlag_inverted)
        {
            InvertMap(edit->finalPixels.x, edit->finalPixels.y);
        }

        u32 start = Min(edit->finalPixels.x, edit->finalPixels.y);
        u32 end = Max(edit->finalPixels.x, edit->finalPixels.y);

        u32 flags = 0;
        RearrangeMap(start, end, -edit->delta, &flags);

        u32 range = end - start + 1;
        ForLoop(range)
        {
            Pixel_Contig_Lookup[index + (u32)((s32)start - edit->delta)] = edit->oldContigId;
        }

        flags |= rearrangeFlag_internalContigLock;
        if (edit->flags & editFlag_wasWholeContig)
        {
            flags |= rearrangeFlag_wasWholeContig;
        }

        RearrangeMap((u32)((s32)start - edit->delta), (u32)((s32)end - edit->delta), 0, &flags);

        if (flags & rearrangeFlag_wasWholeContig)
        {
            ForLoop(16)
            {
                (Contigs + edit->oldContigId)->name[index] = edit->oldName[index];
            }
        }

        MoveWayPoints(edit, 1);
    }
}

global_function
void
MoveWayPoints(map_edit *edit, u32 undo)
{
    pointui tmp;
    s32 delta;
    pointui finalPixels;
    if (undo)
    {
        tmp = edit->finalPixels;
        delta = -edit->delta;
        finalPixels = {(u32)((s32)tmp.x + delta), (u32)((s32)tmp.y + delta)};
    }
    else
    {
        tmp = {(u32)((s32)edit->finalPixels.x - edit->delta), (u32)((s32)edit->finalPixels.y - edit->delta)};
        delta = edit->delta;
        finalPixels = edit->finalPixels;
    }

    pointui startPixels = {Min(tmp.x, tmp.y), Max(tmp.x, tmp.y)};
    u32 lengthMove = startPixels.y - startPixels.x + 1;
    pointui editRange;

    if (delta > 0)
    {
        editRange.x = startPixels.x;
        editRange.y = Max(finalPixels.x, finalPixels.y);
    }
    else
    {
        editRange.x = Min(finalPixels.x, finalPixels.y);
        editRange.y = startPixels.y;
    }

    f32 ooNPixels = 1.0f / (f32)(Number_of_Textures_1D * Texture_Resolution);
    point2f editRangeModel = {((f32)editRange.x * ooNPixels) - 0.5f, ((f32)editRange.y * ooNPixels) - 0.5f};
    f32 dRange = editRangeModel.y - editRangeModel.x;

    waypoint **searchBuffer = PushArray(Working_Set, waypoint*, Waypoints_Stack_Size);
    waypoint **bufferEnd = searchBuffer;

    GetWaypointsWithinRectange({editRangeModel.x, -0.5f}, {dRange, 1.0f}, &bufferEnd);
    GetWaypointsWithinRectange({-0.5f, editRangeModel.x}, {editRangeModel.x, dRange}, &bufferEnd);
    GetWaypointsWithinRectange({editRangeModel.y, editRangeModel.x}, {0.5f - editRangeModel.y, dRange}, &bufferEnd);

    u32 nWayp = (u32)((Max((size_t)bufferEnd, (size_t)searchBuffer) - Min((size_t)bufferEnd, (size_t)searchBuffer)) / sizeof(searchBuffer));
    waypoint **seen = PushArray(Working_Set, waypoint*, nWayp);
    u32 seenIdx = 0;

    for (   waypoint **waypPtr = searchBuffer;
            waypPtr != bufferEnd;
            ++waypPtr )
    {
        waypoint *wayp = *waypPtr;
        u32 newWayP = 1;

        ForLoop(seenIdx)
        {
            if (wayp == seen[index])
            {
                newWayP = 0;
                break;
            }
        }

        if (newWayP)
        {
            point2f normCoords = {Min(wayp->coords.x, wayp->coords.y), Max(wayp->coords.x, wayp->coords.y)};

            u32 inXRange = normCoords.x >= editRangeModel.x && normCoords.x < editRangeModel.y;
            u32 inYRange = normCoords.y >= editRangeModel.x && normCoords.y < editRangeModel.y;
            u32 inRange = inXRange || inYRange;

            if (inRange)
            {
                u32 upperTri = wayp->coords.x > wayp->coords.y;
                
                pointui pix = {(u32)((0.5f + wayp->coords.x) / ooNPixels), (u32)((0.5f + wayp->coords.y) / ooNPixels)};

                if (pix.x >= startPixels.x && pix.x < startPixels.y)
                {
                    pix.x = (u32)((s32)pix.x + delta);

                    if (edit->flags & editFlag_inverted)
                    {
                        pix.x = Max(finalPixels.x, finalPixels.y) - pix.x + Min(finalPixels.x, finalPixels.y);
                    }
                }
                else if (pix.x >= editRange.x && pix.x < editRange.y)
                {
                    pix.x = delta > 0 ? pix.x - lengthMove : pix.x + lengthMove;
                }

                if (pix.y >= startPixels.x && pix.y < startPixels.y)
                {
                    pix.y = (u32)((s32)pix.y + delta);

                    if (edit->flags & editFlag_inverted)
                    {
                        pix.y = Max(finalPixels.x, finalPixels.y) - pix.y + Min(finalPixels.x, finalPixels.y);
                    }
                }
                else if (pix.y >= editRange.x && pix.y < editRange.y)
                {
                    pix.y = delta > 0 ? pix.y - lengthMove : pix.y + lengthMove;
                }

                point2f newCoords = {((f32)pix.x * ooNPixels) - 0.5f, ((f32)pix.y * ooNPixels) - 0.5f};

                u32 upperTriNew = newCoords.x > newCoords.y;
                if (upperTriNew != upperTri)
                {
                    newCoords = {newCoords.y, newCoords.x};
                }

                UpdateWayPoint(wayp, newCoords);
            }

            seen[seenIdx++] = wayp;    
        }
    }

    FreeLastPush(Working_Set); // searchBuffer
    FreeLastPush(Working_Set); // seen
}

global_function
void
GetWaypointsWithinRectange(waypoint_quadtree_level *level, point2f lowerBound, point2f size, waypoint ***bufferPtr)
{
    if (level->children[0])
    {
        u08 toSearch[4] = {1, 1, 1, 1};
#define epsilon 0.001f
        ForLoop(4)
        {
            // bottom left
            waypoint_quadtree_level *child = level->children[index];
            point2f insertSize = {lowerBound.x - child->lowerBound.x, lowerBound.y - child->lowerBound.y};
            if (insertSize.x >= 0 && insertSize.x < child->size && insertSize.y >= 0 && insertSize.y < child->size)
            {
                point2f recSize = {Min(size.x, child->size - insertSize.x - epsilon), Min(size.y, child->size - insertSize.y - epsilon)};
                GetWaypointsWithinRectange(child, lowerBound, recSize, bufferPtr);
                toSearch[index] = 0;
                break;
            }
        }
        ForLoop(4)
        {
            if (toSearch[index])
            {
                // bottom right
                waypoint_quadtree_level *child = level->children[index];
                point2f insertSize = {lowerBound.x + size.x - child->lowerBound.x, lowerBound.y - child->lowerBound.y};
                if (insertSize.x >= 0 && insertSize.x < child->size && insertSize.y >= 0 && insertSize.y < child->size)
                {
                    point2f recSize = {Min(size.x, insertSize.x), Min(size.y, child->size - insertSize.y - epsilon)};
                    GetWaypointsWithinRectange(child, {child->lowerBound.x + insertSize.x - recSize.x, lowerBound.y}, recSize, bufferPtr);
                    toSearch[index] = 0;
                    break;
                }
            }
        }
        ForLoop(4)
        {
            if (toSearch[index])
            {
                // top left
                waypoint_quadtree_level *child = level->children[index];
                point2f insertSize = {lowerBound.x - child->lowerBound.x, lowerBound.y + size.y - child->lowerBound.y};
                if (insertSize.x >= 0 && insertSize.x < child->size && insertSize.y >= 0 && insertSize.y < child->size)
                {
                    point2f recSize = {Min(size.x, child->size - insertSize.x - epsilon), Min(size.y, insertSize.y)};
                    GetWaypointsWithinRectange(child, {lowerBound.x, child->lowerBound.y + insertSize.y - recSize.y}, recSize, bufferPtr);
                    toSearch[index] = 0;
                    break;
                }
            }
        }
        ForLoop(4)
        {
            if (toSearch[index])
            {
                // top right
                waypoint_quadtree_level *child = level->children[index];
                point2f insertSize = {lowerBound.x + size.x - child->lowerBound.x, lowerBound.y + size.y - child->lowerBound.y};
                if (insertSize.x >= 0 && insertSize.x < child->size && insertSize.y >= 0 && insertSize.y < child->size)
                {
                    point2f recSize = {Min(size.x, insertSize.x), Min(size.y, insertSize.y)};
                    GetWaypointsWithinRectange(child, {child->lowerBound.x + insertSize.x - recSize.x, child->lowerBound.y + insertSize.y - recSize.y}, recSize, bufferPtr);
#ifdef DEBUG
                    toSearch[index] = 0;
#endif
                    break;
                }
            }
        }
   
#ifdef DEBUG
        ForLoop(4)
        {
            level->children[index]->show = !toSearch[index];
        }
#endif
    
    }
    else
    {
        TraverseLinkedList(level->headNode.next, waypoint_quadtree_node)
        {
            **bufferPtr = node->wayp;
            ++(*bufferPtr);
        }
    }
}

#ifdef DEBUG
global_function
void
TurnOffDrawingForQuadTreeLevel(waypoint_quadtree_level *level = 0)
{
    if (!level) level = Waypoint_Editor->quadtree;
    level->show = 0;
    ForLoop(4)
    {
        if (level->children[index]) TurnOffDrawingForQuadTreeLevel(level->children[index]);
    }
}
#endif

global_function
void
GetWaypointsWithinSquare(point2f lowerBound, f32 size, waypoint ***bufferPtr)
{
#ifdef DEBUG
    TurnOffDrawingForQuadTreeLevel();
#endif
    GetWaypointsWithinRectange(Waypoint_Editor->quadtree, lowerBound, {size, size}, bufferPtr);
}

global_function
void
GetWaypointsWithinRectange(point2f lowerBound, point2f size, waypoint ***bufferPtr)
{
#ifdef DEBUG
    TurnOffDrawingForQuadTreeLevel();
#endif
    GetWaypointsWithinRectange(Waypoint_Editor->quadtree, lowerBound, size, bufferPtr);
}

global_function
waypoint_quadtree_level *
GetWaypointQuadTreeLevel(point2f coords)
{
    coords = {Max(Min(coords.x, 0.499f), -0.5f), Max(Min(coords.y, 0.499f), -0.5f)};
    
    waypoint_quadtree_level *level = Waypoint_Editor->quadtree;

    while (level->children[0])
    {
#ifdef DEBUG
        u32 found = 0;
#endif
        ForLoop(4)
        {
            point2f insertSize = {coords.x - level->children[index]->lowerBound.x, coords.y - level->children[index]->lowerBound.y};
            f32 size = level->children[index]->size;
            if (insertSize.x >= 0 && insertSize.x < size && insertSize.y >= 0 && insertSize.y < size)
            {
                level = level->children[index];
                
#ifdef DEBUG
                found = 1;
#endif
                
                break;
            }
        }

#ifdef DEBUG
        if (!found)
        {
            Assert(found);
        }
#endif

    }

    return(level);
}

global_function
void
AddWayPoint(point2f coords)
{
    u32 nFree = Waypoints_Stack_Size - Waypoint_Editor->nWaypointsActive;

    if (nFree)
    {
        waypoint *wayp = Waypoint_Editor->freeWaypoints.next;
        Waypoint_Editor->freeWaypoints.next = wayp->next;
        if (Waypoint_Editor->freeWaypoints.next) Waypoint_Editor->freeWaypoints.next->prev = &Waypoint_Editor->freeWaypoints;

        wayp->next = 0;
        wayp->prev = 0;
        switch (Waypoint_Editor->nWaypointsActive)
        {
            case 0:
                Waypoint_Editor->activeWaypoints.next = wayp;
                wayp->index = 0;
                break;

            default:
                wayp->next = Waypoint_Editor->activeWaypoints.next;
                Waypoint_Editor->activeWaypoints.next->prev = wayp;
                Waypoint_Editor->activeWaypoints.next = wayp;
                wayp->prev = &Waypoint_Editor->activeWaypoints;
                wayp->index = wayp->next->index + 1;
        }

        wayp->coords = coords;
        wayp->z = Camera_Position.z;
        ++Waypoint_Editor->nWaypointsActive;

        waypoint_quadtree_level *level = GetWaypointQuadTreeLevel(wayp->coords);
        
        waypoint_quadtree_node *node = Waypoint_Editor->freeNodes.next;
        if (node->next) node->next->prev = &Waypoint_Editor->freeNodes;
        Waypoint_Editor->freeNodes.next = node->next;

        if (level->headNode.next) level->headNode.next->prev = node;
        node->next = level->headNode.next;
        node->prev = &level->headNode;
        level->headNode.next = node;

        wayp->node = node;
        node->wayp = wayp;
    }
}

global_function
void
RemoveWayPoint(waypoint *wayp)
{
    switch (Waypoint_Editor->nWaypointsActive)
    {
        case 0:
            wayp = 0;
            break;

        case 1:
            Waypoint_Editor->activeWaypoints.next = 0;
            Waypoint_Editor->nWaypointsActive = 0;
            break;

        default:
            if (wayp->next) wayp->next->prev = wayp->prev;
            if (wayp->prev) wayp->prev->next = wayp->next;
            --Waypoint_Editor->nWaypointsActive;
    }

    if (wayp)
    {
        u32 nFree = Waypoints_Stack_Size - Waypoint_Editor->nWaypointsActive - 1;
        
        wayp->next = wayp->prev = 0;
        switch (nFree)
        {
            case 0:
                Waypoint_Editor->freeWaypoints.next = wayp;
                break;

            default:
                wayp->next = Waypoint_Editor->freeWaypoints.next;
                Waypoint_Editor->freeWaypoints.next->prev = wayp;
                Waypoint_Editor->freeWaypoints.next = wayp;
                wayp->prev = &Waypoint_Editor->freeWaypoints;
        }

        waypoint_quadtree_node *node = wayp->node;

        if (node->next) node->next->prev = node->prev;
        if (node->prev) node->prev->next = node->next;
        node->prev = 0;

        if (Waypoint_Editor->freeNodes.next) Waypoint_Editor->freeNodes.next->prev = node;
        node->next = Waypoint_Editor->freeNodes.next;
        Waypoint_Editor->freeNodes.next = node;
        node->prev = &Waypoint_Editor->freeNodes;
    }
}

global_function
void
UpdateWayPoint(waypoint *wayp, point2f coords)
{
    waypoint_quadtree_node *node = wayp->node;

    if (node->next) node->next->prev = node->prev;
    if (node->prev) node->prev->next = node->next;
    node->prev = 0;

    wayp->coords = coords;

    waypoint_quadtree_level *level = GetWaypointQuadTreeLevel(wayp->coords);

    if (level->headNode.next) level->headNode.next->prev = node;
    node->next = level->headNode.next;
    node->prev = &level->headNode;
    level->headNode.next = node;

    wayp->node = node;
    node->wayp = wayp;
}

global_function
void
FreeContig(u32 index)
{
    contig *cont = Contigs + index;
    cont->prev = 0;

    switch (Contig_Sentinel->nFree)
    {
        case 0:
            cont->next = 0;
            Contig_Sentinel->tail = cont;
            break;

        default:
            cont->next = Contig_Sentinel->tail;
            Contig_Sentinel->tail->prev = cont;
            Contig_Sentinel->tail = cont;
    }
    
    ++Contig_Sentinel->nFree;

    Number_of_Contigs_to_Display = Number_of_Contigs_to_Display ? Number_of_Contigs_to_Display - 1 : 0;
}

global_function
contig *
GetFreeContig()
{
    contig *cont = Contig_Sentinel->tail;

    switch (Contig_Sentinel->nFree)
    {
        case 0:
            break;

        case 1:
            Contig_Sentinel->tail = 0;
            Contig_Sentinel->nFree = 0;
            break;

        default:
            Contig_Sentinel->tail = cont->next;
            --Contig_Sentinel->nFree;
    }

    ++Number_of_Contigs_to_Display;
    Number_of_Contigs_to_Display = Min(Number_of_Contigs_to_Display, Max_Number_of_Contigs_to_Display);

    return(cont);
}

global_function
void
MouseMove(GLFWwindow* window, f64 x, f64 y)
{
    if (Loading)
    {
        return;
    }

    (void)window;

    u32 redisplay = 0;

    if (UI_On)
    {
    }
    else
    {
        if (Edit_Mode)
        {
            static u32 firstMove = 1;
            static u32 wasWholeContig = 0;
            static s32 netDelta = 0;
            static u32 originalContigId = 0;
            static u32 oldNameCache[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            static u32 firstPixelOfOriginalContig = 0;

            s32 w, h;
            glfwGetWindowSize(window, &w, &h);
            f32 height = (f32)h;
            f32 width = (f32)w;

            f32 factor1 = 1.0f / (2.0f * Camera_Position.z);
            f32 factor2 = 2.0f / height;
            f32 factor3 = width * 0.5f;

            f32 wx = (factor1 * factor2 * ((f32)x - factor3)) + Camera_Position.x;
            f32 wy = (-factor1 * (1.0f - (factor2 * (f32)y))) - Camera_Position.y;

            wx = Max(-0.5f, Min(0.5f, wx));
            wy = Max(-0.5f, Min(0.5f, wy));

            u32 nPixels = Number_of_Textures_1D * Texture_Resolution;

            u32 pixel1 = (u32)((f64)nPixels * (0.5 + (f64)wx));
            u32 pixel2 = (u32)((f64)nPixels * (0.5 + (f64)wy));

            u16 contig = Pixel_Contig_Lookup[pixel1];
            u32 internalMove = 1;

            if (Pixel_Contig_Lookup[pixel2] != contig)
            {
                internalMove = 0;

                u32 testPixel = pixel1;
                u32 testContig = contig;
                while (testContig == contig)
                {
                    testContig = pixel1 < pixel2 ? Pixel_Contig_Lookup[++testPixel] : Pixel_Contig_Lookup[--testPixel];
                    if (testPixel == 0 || testPixel >= (nPixels - 1)) break;
                }
                pixel2 = pixel1 < pixel2 ? testPixel - 1 : testPixel + 1;
            }
            
            if (Edit_Pixels.editing)
            {
                u32 midPixel = (pixel1 + pixel2) >> 1;
                u32 oldMidPixel = (Edit_Pixels.pixels.x + Edit_Pixels.pixels.y) >> 1;
                s32 diff = (s32)midPixel - (s32)oldMidPixel;

                u32 forward = diff > 0;
                
                s32 newX = (s32)Edit_Pixels.pixels.x + diff;
                newX = Max(0, Min((s32)nPixels - 1, newX));
                s32 newY = (s32)Edit_Pixels.pixels.y + diff;
                newY = Max(0, Min((s32)nPixels - 1, newY));

                s32 diffx = newX - (s32)Edit_Pixels.pixels.x;
                s32 diffy = newY - (s32)Edit_Pixels.pixels.y;

                if (forward)
                {
                    diff = Min(diffx, diffy);
                }
                else
                {
                    diff = Max(diffx, diffy);
                }
                
                newX = (s32)Edit_Pixels.pixels.x + diff;
                newY = (s32)Edit_Pixels.pixels.y + diff;
                
                u32 allowNewContigCreation = 0;
                if (internalMove)
                {
                    u32 newP1 = Min(pixel1, pixel2);
                    u32 newP2 = Max(pixel1, pixel2);
                    
                    u32 oldP1 = Min(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y);
                    u32 oldP2 = Max(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y);
                    
                    u32 dp1 = (newP1 < oldP1) ? (oldP1 - newP1) : (newP1 - oldP1);
                    u32 dp2 = (newP2 < oldP2) ? (oldP2 - newP2) : (newP2 - oldP2);

                    u32 dp = Min(dp1, dp2);
                    u32 range = Max(4, (Edit_Pixels.pixels.x > Edit_Pixels.pixels.y ? Edit_Pixels.pixels.x - Edit_Pixels.pixels.y : Edit_Pixels.pixels.y - Edit_Pixels.pixels.x) >> 2);

                    allowNewContigCreation = dp < range;
                }
                
                u32 flags = 0;
                if (internalMove)
                {
                    flags |= rearrangeFlag_internalContigLock;
                }
                if (allowNewContigCreation)
                {
                    flags |= rearrangeFlag_allowNewContigCreation;
                }
                if (firstMove)
                {
                    flags |= rearrangeFlag_firstMove;
                }
                if (wasWholeContig)
                {
                    flags |= rearrangeFlag_wasWholeContig;
                }

                if (firstMove)
                {
                    originalContigId = Pixel_Contig_Lookup[Edit_Pixels.pixels.x];
                    
                    u32 testPixel = Min(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y);
                    
                    while (testPixel > 0 && Pixel_Contig_Lookup[testPixel] == Pixel_Contig_Lookup[testPixel - 1])
                    {
                        --testPixel;
                    }

                    firstPixelOfOriginalContig = testPixel;
                }

                diff = RearrangeMap(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y, diff, &flags);
                netDelta += diff;

                if (firstMove)
                {
                    firstMove = 0;
                    wasWholeContig = flags & rearrangeFlag_wasWholeContig;

                    ForLoop(16)
                    {
                        oldNameCache[index] = (Contigs + originalContigId)->name[index];
                    }
                }

                newX = (s32)Edit_Pixels.pixels.x + diff;
                newY = (s32)Edit_Pixels.pixels.y + diff;
                
                Edit_Pixels.pixels.x = (u32)newX;
                Edit_Pixels.pixels.y = (u32)newY;

                Edit_Pixels.worldCoords.x = (f32)(((f64)((2 * Edit_Pixels.pixels.x) + 1)) / ((f64)(2 * nPixels))) - 0.5f;
                Edit_Pixels.worldCoords.y = (f32)(((f64)((2 * Edit_Pixels.pixels.y) + 1)) / ((f64)(2 * nPixels))) - 0.5f;
            }
            else
            {
                if (!firstMove)
                {
                    u16 flags = Global_Edit_Invert_Flag ? editFlag_inverted : 0;
                    if (wasWholeContig)
                    {
                        flags |= editFlag_wasWholeContig;
                    }

                    AddMapEdit(netDelta, Edit_Pixels.pixels, (u16)originalContigId, firstPixelOfOriginalContig, flags, oldNameCache);
                }

                wx = (f32)(((f64)((2 * pixel1) + 1)) / ((f64)(2 * nPixels))) - 0.5f;
                wy = (f32)(((f64)((2 * pixel2) + 1)) / ((f64)(2 * nPixels))) - 0.5f;

                Edit_Pixels.pixels.x = pixel1;
                Edit_Pixels.pixels.y = pixel2;
                Edit_Pixels.worldCoords.x = wx;
                Edit_Pixels.worldCoords.y = wy;

                firstMove = 1;
                wasWholeContig = 0;
                netDelta = 0;
                originalContigId = 0;
                firstPixelOfOriginalContig = 0;
                
                Global_Edit_Invert_Flag = 0;
            }

            redisplay = 1;
        }
        else if (Tool_Tip->on || Waypoint_Mode)
        {
            s32 w, h;
            glfwGetWindowSize(window, &w, &h);
            f32 height = (f32)h;
            f32 width = (f32)w;

            f32 factor1 = 1.0f / (2.0f * Camera_Position.z);
            f32 factor2 = 2.0f / height;
            f32 factor3 = width * 0.5f;

            f32 wx = (factor1 * factor2 * ((f32)x - factor3)) + Camera_Position.x;
            f32 wy = (-factor1 * (1.0f - (factor2 * (f32)y))) - Camera_Position.y;

            wx = Max(-0.4999f, Min(0.4999f, wx));
            wy = Max(-0.4999f, Min(0.4999f, wy));

            u32 nPixels = Number_of_Textures_1D * Texture_Resolution;

            u32 pixel1 = (u32)((f64)nPixels * (0.5 + (f64)wx));
            u32 pixel2 = (u32)((f64)nPixels * (0.5 + (f64)wy));

            Tool_Tip_Move.pixels.x = pixel1;
            Tool_Tip_Move.pixels.y = pixel2;
            Tool_Tip_Move.worldCoords.x = wx;
            Tool_Tip_Move.worldCoords.y = wy;

            if (Waypoint_Mode)
            {
                Selected_Waypoint = 0;
                f32 selectDis = Waypoint_Select_Distance / (height * Camera_Position.z);
                f32 closestDistanceSq = selectDis * selectDis;

                waypoint **searchBuffer = PushArray(Working_Set, waypoint*, Waypoints_Stack_Size);
                waypoint **bufferEnd = searchBuffer;
                GetWaypointsWithinSquare({Tool_Tip_Move.worldCoords.x - selectDis, Tool_Tip_Move.worldCoords.y - selectDis}, 
                        2.0f * selectDis, &bufferEnd);
                for (   waypoint **waypPtr = searchBuffer;
                        waypPtr != bufferEnd;
                        ++waypPtr )
                {
                    waypoint *wayp = *waypPtr;

                    f32 dx = Tool_Tip_Move.worldCoords.x - wayp->coords.x;
                    f32 dy = Tool_Tip_Move.worldCoords.y - wayp->coords.y;
                    f32 disSq = (dx * dx) + (dy * dy);

                    if (disSq < closestDistanceSq)
                    {
                        closestDistanceSq = disSq;
                        Selected_Waypoint = wayp;
                    }
                }

                FreeLastPush(Working_Set); // searchBuffer
            }

            redisplay = 1;
        }

        if (Mouse_Move.x >= 0.0)
        {
            s32 w, h;
            glfwGetWindowSize(window, &w, &h);
            f32 height = (f32)h;

            f32 factor = 1.0f / (height * Camera_Position.z);
            f32 dx = (f32)(Mouse_Move.x - x) * factor;
            f32 dy = (f32)(y - Mouse_Move.y) * factor;

            Camera_Position.x += dx;
            Camera_Position.y += dy;
            ClampCamera();

            Mouse_Move.x = x;
            Mouse_Move.y = y;

            redisplay = 1;
        }

        if (Mouse_Select.x >= 0.0)
        {
            s32 w, h;
            glfwGetWindowSize(window, &w, &h);
            f32 height = (f32)h;
            f32 width = (f32)w;

            f32 factor1 = 1.0f / (2.0f * Camera_Position.z);
            f32 factor2 = 2.0f / height;
            f32 factor3 = width * 0.5f;

            f32 wx0 = (factor1 * factor2 * ((f32)Mouse_Select.x - factor3)) + Camera_Position.x;
            f32 wx1 = (factor1 * factor2 * ((f32)x - factor3)) + Camera_Position.x;

            f32 wy0 = (factor1 * (1.0f - (factor2 * (f32)Mouse_Select.y))) + Camera_Position.y;
            f32 wy1 = (factor1 * (1.0f - (factor2 * (f32)y))) + Camera_Position.y;

            Select_Box.corner[0].x = wx0;
            Select_Box.corner[0].y = wy0;
            Select_Box.corner[1].x = wx1;
            Select_Box.corner[1].y = wy1;

            redisplay = 1;
        }

    }

    if (redisplay)
    {
        Redisplay = 1;
    }
}

global_function
void
Mouse(GLFWwindow* window, s32 button, s32 action, s32 mods)
{
    if (Loading)
    {
        return;
    }
    
    (void)mods;

    f64 x, y;
    glfwGetCursorPos(window, &x, &y);

    if (UI_On)
    {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
        {
            UI_On = !UI_On;
            Mouse_Move.x = Mouse_Move.y = Mouse_Select.x = Mouse_Select.y = -1;
            ++NK_Device->lastContextMemory[0];
        }
    }
    else
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && Edit_Mode && action == GLFW_PRESS)
        {
            Edit_Pixels.editing = !Edit_Pixels.editing;
            MouseMove(window, x, y);
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && Edit_Mode && action == GLFW_PRESS && !Edit_Pixels.editing)
        {
            Edit_Pixels.editing = 1;

            Edit_Pixels.pixels.y = Edit_Pixels.pixels.x;

            while(  Edit_Pixels.pixels.x < ((Texture_Resolution * Number_of_Textures_1D) - 1) &&
                    Pixel_Contig_Lookup[Edit_Pixels.pixels.x] == Pixel_Contig_Lookup[1 + Edit_Pixels.pixels.x])
            {
                ++Edit_Pixels.pixels.x;
            }

            while(  Edit_Pixels.pixels.y > 0 &&
                    Pixel_Contig_Lookup[Edit_Pixels.pixels.y] == Pixel_Contig_Lookup[Edit_Pixels.pixels.y - 1])
            {
                --Edit_Pixels.pixels.y;
            }

            u32 nPixels = Texture_Resolution * Number_of_Textures_1D;

            f32 wx = (f32)(((f64)((2 * Edit_Pixels.pixels.x) + 1)) / ((f64)(2 * nPixels))) - 0.5f;
            f32 wy = (f32)(((f64)((2 * Edit_Pixels.pixels.y) + 1)) / ((f64)(2 * nPixels))) - 0.5f;

            Edit_Pixels.worldCoords.x = wx;
            Edit_Pixels.worldCoords.y = wy;

            Redisplay = 1;
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && Edit_Mode && Edit_Pixels.editing && action == GLFW_PRESS)
        {
            InvertMap(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y);
            Global_Edit_Invert_Flag = !Global_Edit_Invert_Flag;
            
            Redisplay = 1;
        }
        else if (button == GLFW_MOUSE_BUTTON_LEFT && Waypoint_Mode && action == GLFW_PRESS)
        {
            AddWayPoint(Tool_Tip_Move.worldCoords);
            MouseMove(window, x, y);
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && Waypoint_Mode && action == GLFW_PRESS)
        {
            if (Selected_Waypoint)
            {
                RemoveWayPoint(Selected_Waypoint);
                MouseMove(window, x, y);
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_LEFT && Normal_Mode)
        {
            if (action == GLFW_PRESS)
            {
                Mouse_Select.x = x;
                Mouse_Select.y = y;
            }
            else
            {
                Mouse_Select.x = Mouse_Select.y = -1.0;

                f32 x0 = Min(Select_Box.corner[0].x, Select_Box.corner[1].x);
                f32 x1 = Max(Select_Box.corner[0].x, Select_Box.corner[1].x);
                f32 y0 = Min(Select_Box.corner[0].y, Select_Box.corner[1].y);
                f32 y1 = Max(Select_Box.corner[0].y, Select_Box.corner[1].y);

                f32 rx = x1 - x0;
                f32 ry = y1 - y0;
                f32 range = Max(rx, ry);

                Camera_Position.x = 0.5f * (x0 + x1);
                Camera_Position.y = 0.5f * (y0 + y1);
                Camera_Position.z = 1.0f / range;
                ClampCamera();

                Redisplay = 1;
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            if (action == GLFW_PRESS)
            {
                Mouse_Move.x = x;
                Mouse_Move.y = y;
            }
            else
            {
                Mouse_Move.x = Mouse_Move.y = -1.0;
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
        {
            UI_On = !UI_On;
            Mouse_Move.x = Mouse_Move.y = Mouse_Select.x = Mouse_Select.y = -1;
            Redisplay = 1;
            ++NK_Device->lastContextMemory[0];
        }
    }
}

global_variable
struct nk_vec2
NK_Scroll;

global_function
void
Scroll(GLFWwindow* window, f64 x, f64 y)
{
    if (Loading)
    {
        return;
    }
    
    if (UI_On)
    {
        NK_Scroll.y = (f32)y;
        NK_Scroll.x = (f32)x;
    }
    else
    {
        if (y != 0.0)
        {
            ZoomCamera(Max(Min((f32)y, 0.01f), -0.01f));
            Redisplay = 1;
        }

        if (Edit_Mode || Tool_Tip->on || Waypoint_Mode)
        {
            f64 mousex, mousey;
            glfwGetCursorPos(window, &mousex, &mousey);
            MouseMove(window, mousex, mousey);
        }
    }
}

global_variable
u64
Total_Genome_Length;

global_function
void
Setup();

global_function
u32
SubDivideScaleBar(f32 left, f32 right, f32 middle, f32 bpPerPixel)
{
    u32 result = 0;

    if (left < right)
    {
        f32 length = right - left;
        f32 half = length * 0.5f;
        char buff[16];
        stbsp_snprintf(buff, 16, "%$.2f", (f64)(half * bpPerPixel));
        f32 width = fonsTextBounds(FontStash_Context, middle, 0.0, buff, 0, NULL);
        f32 halfWidth = 0.5f * width;

        if ((middle + halfWidth) < right && (middle - halfWidth) > left)
        {
            u32 leftResult = SubDivideScaleBar(left, middle - halfWidth, (left + middle) * 0.5f, bpPerPixel);
            u32 rightResult = SubDivideScaleBar(middle + halfWidth, right, (right + middle) * 0.5f, bpPerPixel);
            result = 1 + Min(leftResult, rightResult);   
        }
    }

    return(result);
}

global_variable
u32
File_Loaded = 0;

global_variable
nk_color
Theme_Colour;

#ifdef DEBUG
global_function
void
DrawQuadTreeLevel (u32 *ptr, waypoint_quadtree_level *level, vertex *vert, f32 lineWidth, u32 n = 0)
{
    f32 colour[5][4] = {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}};
    f32 *col = (f32 *)colour[n%5];

    glUniform4fv(Flat_Shader->colorLocation, 1, col);
    
    if (level->show)
    {
        vert[0].x = level->lowerBound.x;
        vert[0].y = -level->lowerBound.y;
        vert[1].x = level->lowerBound.x;
        vert[1].y = -level->lowerBound.y - level->size;
        vert[2].x = level->lowerBound.x + lineWidth;
        vert[2].y = -level->lowerBound.y - level->size;
        vert[3].x = level->lowerBound.x + lineWidth;
        vert[3].y = -level->lowerBound.y;

        glBindBuffer(GL_ARRAY_BUFFER, QuadTree_Data->vbos[*ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(QuadTree_Data->vaos[(*ptr)++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        vert[0].x = level->lowerBound.x;
        vert[0].y = -level->lowerBound.y - level->size;
        vert[1].x = level->lowerBound.x + level->size;
        vert[1].y = -level->lowerBound.y - level->size;
        vert[2].x = level->lowerBound.x + level->size;
        vert[2].y = -level->lowerBound.y - level->size + lineWidth;
        vert[3].x = level->lowerBound.x;
        vert[3].y = -level->lowerBound.y - level->size + lineWidth;

        glBindBuffer(GL_ARRAY_BUFFER, QuadTree_Data->vbos[*ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(QuadTree_Data->vaos[(*ptr)++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        vert[0].x = level->lowerBound.x + level->size - lineWidth;
        vert[0].y = -level->lowerBound.y;
        vert[1].x = level->lowerBound.x + level->size - lineWidth;
        vert[1].y = -level->lowerBound.y - level->size;
        vert[2].x = level->lowerBound.x + level->size;
        vert[2].y = -level->lowerBound.y - level->size;
        vert[3].x = level->lowerBound.x + level->size;
        vert[3].y = -level->lowerBound.y;

        glBindBuffer(GL_ARRAY_BUFFER, QuadTree_Data->vbos[*ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(QuadTree_Data->vaos[(*ptr)++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        vert[0].x = level->lowerBound.x;
        vert[0].y = -level->lowerBound.y;
        vert[1].x = level->lowerBound.x;
        vert[1].y = -level->lowerBound.y - lineWidth;
        vert[2].x = level->lowerBound.x + level->size;
        vert[2].y = -level->lowerBound.y - lineWidth;
        vert[3].x = level->lowerBound.x + level->size;
        vert[3].y = -level->lowerBound.y;

        glBindBuffer(GL_ARRAY_BUFFER, QuadTree_Data->vbos[*ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(QuadTree_Data->vaos[(*ptr)++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
    }

    if (level->children[0])
    {
        ForLoop(4)
        {
            DrawQuadTreeLevel(ptr, level->children[index], vert, lineWidth, n+1);
        }
    }
}
#endif

global_function
u32
FourFloatColorToU32(nk_colorf colour)
{
    return(glfonsRGBA((u08)(colour.r * 255.0f), (u08)(colour.g * 255.0f),
                (u08)(colour.b * 255.0f), (u08)(colour.a * 255.0f)));
}

global_function
void
Render()
{
    // Projection Matrix
    f32 width;
    f32 height;
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.2f, 0.6f, 0.4f, 1.0f);

        s32 viewport[4];
        glGetIntegerv (GL_VIEWPORT, viewport);
        width = (f32)viewport[2];
        height = (f32)viewport[3];

        f32 mat[16];
        memset(mat, 0, sizeof(mat));
        mat[0] = 2.0f * Camera_Position.z * height / width;
        mat[5] = 2.0f * Camera_Position.z;
        mat[10] = -2.0f;
        mat[12] = -2.0f * height * Camera_Position.x * Camera_Position.z / width;
        mat[13] = -2.0f * Camera_Position.y * Camera_Position.z;
        mat[15] = 1.0f;

        glUseProgram(Contact_Matrix->shaderProgram);
        glUniformMatrix4fv(Contact_Matrix->matLocation, 1, GL_FALSE, mat);
        glUseProgram(Flat_Shader->shaderProgram);
        glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, mat);
    }

    // Textures
    if (File_Loaded)
    {
        glUseProgram(Contact_Matrix->shaderProgram);
        glBindTexture(GL_TEXTURE_2D_ARRAY, Contact_Matrix->textures);
        u32 ptr = 0;
        ForLoop(Number_of_Textures_1D)
        {
            ForLoop2(Number_of_Textures_1D)
            {
                glBindVertexArray(Contact_Matrix->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
            }
        }
    }

#ifdef DEBUG
    if (File_Loaded && Tiles->on)
    {
        glUseProgram(Flat_Shader->shaderProgram);
        glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&Tiles->bg);

        u32 ptr = 0;
        vertex vert[4];

        f32 lineWidth = 0.001f / sqrtf(Camera_Position.z);
        f32 position = 0.0f;
        f32 spacing = 1.0f / (f32)Number_of_Textures_1D;

        vert[0].x = -0.5f;
        vert[0].y = -0.5f;
        vert[1].x = lineWidth - 0.5f;
        vert[1].y = -0.5f;
        vert[2].x = lineWidth - 0.5f;
        vert[2].y = 0.5f;
        vert[3].x = -0.5f;
        vert[3].y = 0.5f;

        glBindBuffer(GL_ARRAY_BUFFER, Texture_Tile_Grid->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Texture_Tile_Grid->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        f32 x = -0.5f;
        ForLoop(Number_of_Textures_1D - 1)
        {
            position += spacing;
            f32 px = x + lineWidth;
            x = position - (0.5f * (lineWidth + 1.0f));

            if (x > px)
            {
                vert[0].x = x;
                vert[0].y = -0.5f;
                vert[1].x = x + lineWidth;
                vert[1].y = -0.5f;
                vert[2].x = x + lineWidth;
                vert[2].y = 0.5f;
                vert[3].x = x;
                vert[3].y = 0.5f;

                glBindBuffer(GL_ARRAY_BUFFER, Texture_Tile_Grid->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Texture_Tile_Grid->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
            }
        }

        vert[0].x = 0.5f - lineWidth;
        vert[0].y = -0.5f;
        vert[1].x = 0.5f;
        vert[1].y = -0.5f;
        vert[2].x = 0.5f;
        vert[2].y = 0.5f;
        vert[3].x = 0.5f - lineWidth;
        vert[3].y = 0.5f;

        glBindBuffer(GL_ARRAY_BUFFER, Texture_Tile_Grid->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Texture_Tile_Grid->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        position = 0.0f;

        vert[0].x = -0.5f;
        vert[0].y = 0.5f - lineWidth;
        vert[1].x = 0.5f;
        vert[1].y = 0.5f - lineWidth;
        vert[2].x = 0.5f;
        vert[2].y = 0.5f;
        vert[3].x = -0.5f;
        vert[3].y = 0.5f;

        glBindBuffer(GL_ARRAY_BUFFER, Texture_Tile_Grid->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Texture_Tile_Grid->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        f32 y = 0.5f;
        ForLoop(Number_of_Textures_1D - 1)
        {
            position += spacing;
            f32 py = y - lineWidth;
            y = 1.0f - position + (0.5f * (lineWidth - 1.0f));

            if (y < py)
            {
                vert[0].x = -0.5f;
                vert[0].y = y - lineWidth;
                vert[1].x = 0.5f;
                vert[1].y = y - lineWidth;
                vert[2].x = 0.5f;
                vert[2].y = y;
                vert[3].x = -0.5f;
                vert[3].y = y;

                glBindBuffer(GL_ARRAY_BUFFER, Texture_Tile_Grid->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Texture_Tile_Grid->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
            }
        }

        vert[0].x = -0.5f;
        vert[0].y = -0.5f;
        vert[1].x = 0.5f;
        vert[1].y = -0.5f;
        vert[2].x = 0.5f;
        vert[2].y = lineWidth - 0.5f;
        vert[3].x = -0.5f;
        vert[3].y = lineWidth - 0.5f;

        glBindBuffer(GL_ARRAY_BUFFER, Texture_Tile_Grid->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Texture_Tile_Grid->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
    }

    // Quad Trees
    if (Waypoint_Mode)
    {
        glUseProgram(Flat_Shader->shaderProgram);
        glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&QuadTrees->bg);

        u32 ptr = 0;
        vertex vert[4];

        f32 lineWidth = 0.001f / sqrtf(Camera_Position.z);

        DrawQuadTreeLevel(&ptr, Waypoint_Editor->quadtree, vert, lineWidth);
    }
#endif
    
    // Grid
    if (File_Loaded && Grid->on)
    {
        glUseProgram(Flat_Shader->shaderProgram);
        glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&Grid->bg);

        u32 ptr = 0;
        vertex vert[4];

        f32 lineWidth = 0.001f / sqrtf(Camera_Position.z);
        f32 position = 0.0f;

        vert[0].x = -0.5f;
        vert[0].y = -0.5f;
        vert[1].x = lineWidth - 0.5f;
        vert[1].y = -0.5f;
        vert[2].x = lineWidth - 0.5f;
        vert[2].y = 0.5f;
        vert[3].x = -0.5f;
        vert[3].y = 0.5f;

        glBindBuffer(GL_ARRAY_BUFFER, Grid_Data->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Grid_Data->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        f32 x = -0.5f;
        ForLoop(Number_of_Contigs_to_Display - 1)
        {
            contig *cont = Contigs + Contig_Display_Order[index];

            position += cont->fractionalLength;
            f32 px = x + lineWidth;
            x = position - (0.5f * (lineWidth + 1.0f));

            if (x > px)
            {
                vert[0].x = x;
                vert[0].y = -0.5f;
                vert[1].x = x + lineWidth;
                vert[1].y = -0.5f;
                vert[2].x = x + lineWidth;
                vert[2].y = 0.5f;
                vert[3].x = x;
                vert[3].y = 0.5f;

                glBindBuffer(GL_ARRAY_BUFFER, Grid_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Grid_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
            }
        }

        vert[0].x = 0.5f - lineWidth;
        vert[0].y = -0.5f;
        vert[1].x = 0.5f;
        vert[1].y = -0.5f;
        vert[2].x = 0.5f;
        vert[2].y = 0.5f;
        vert[3].x = 0.5f - lineWidth;
        vert[3].y = 0.5f;

        glBindBuffer(GL_ARRAY_BUFFER, Grid_Data->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Grid_Data->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        position = 0.0f;

        vert[0].x = -0.5f;
        vert[0].y = 0.5f - lineWidth;
        vert[1].x = 0.5f;
        vert[1].y = 0.5f - lineWidth;
        vert[2].x = 0.5f;
        vert[2].y = 0.5f;
        vert[3].x = -0.5f;
        vert[3].y = 0.5f;

        glBindBuffer(GL_ARRAY_BUFFER, Grid_Data->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Grid_Data->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        f32 y = 0.5f;
        ForLoop(Number_of_Contigs_to_Display - 1)
        {
            contig *cont = Contigs + Contig_Display_Order[index];
            
            position += cont->fractionalLength;
            f32 py = y - lineWidth;
            y = 1.0f - position + (0.5f * (lineWidth - 1.0f));

            if (y < py)
            {
                vert[0].x = -0.5f;
                vert[0].y = y - lineWidth;
                vert[1].x = 0.5f;
                vert[1].y = y - lineWidth;
                vert[2].x = 0.5f;
                vert[2].y = y;
                vert[3].x = -0.5f;
                vert[3].y = y;

                glBindBuffer(GL_ARRAY_BUFFER, Grid_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Grid_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
            }
        }

        vert[0].x = -0.5f;
        vert[0].y = -0.5f;
        vert[1].x = 0.5f;
        vert[1].y = -0.5f;
        vert[2].x = 0.5f;
        vert[2].y = lineWidth - 0.5f;
        vert[3].x = -0.5f;
        vert[3].y = lineWidth - 0.5f;

        glBindBuffer(GL_ARRAY_BUFFER, Grid_Data->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Grid_Data->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
    }

    // Select_Box
    if (File_Loaded && Mouse_Select.x >= 0.0)
    {
        glUseProgram(Flat_Shader->shaderProgram);
        const GLfloat color[] = {0.078f, 0.392f, 0.784f, 0.5f};
        glUniform4fv(Flat_Shader->colorLocation, 1, color);

        u32 ptr = 0;
        vertex vert[4];

        f32 x0 = Min(Select_Box.corner[0].x, Select_Box.corner[1].x);
        f32 x1 = Max(Select_Box.corner[0].x, Select_Box.corner[1].x);
        f32 y0 = Min(Select_Box.corner[0].y, Select_Box.corner[1].y);
        f32 y1 = Max(Select_Box.corner[0].y, Select_Box.corner[1].y);

        f32 lineWidth = 0.005f / Camera_Position.z;

        vert[0].x = x0;
        vert[0].y = y0;
        vert[1].x = x1;
        vert[1].y = y0;
        vert[2].x = x1;
        vert[2].y = y0 + lineWidth;
        vert[3].x = x0;
        vert[3].y = y0 + lineWidth;

        glBindBuffer(GL_ARRAY_BUFFER, Select_Box_Data->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Select_Box_Data->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        vert[0].x = x1 - lineWidth;
        vert[0].y = y0;
        vert[1].x = x1;
        vert[1].y = y0;
        vert[2].x = x1;
        vert[2].y = y1;
        vert[3].x = x1 - lineWidth;
        vert[3].y = y1;

        glBindBuffer(GL_ARRAY_BUFFER, Select_Box_Data->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Select_Box_Data->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        vert[0].x = x0;
        vert[0].y = y1 - lineWidth;
        vert[1].x = x1;
        vert[1].y = y1 - lineWidth;
        vert[2].x = x1;
        vert[2].y = y1;
        vert[3].x = x0;
        vert[3].y = y1;

        glBindBuffer(GL_ARRAY_BUFFER, Select_Box_Data->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Select_Box_Data->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

        vert[0].x = x0;
        vert[0].y = y0;
        vert[1].x = x0 + lineWidth;
        vert[1].y = y0;
        vert[2].x = x0 + lineWidth;
        vert[2].y = y1;
        vert[3].x = x0;
        vert[3].y = y1;

        glBindBuffer(GL_ARRAY_BUFFER, Select_Box_Data->vbos[ptr]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
        glBindVertexArray(Select_Box_Data->vaos[ptr++]);
        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
    }

    // Text / UI Rendering
    if (Contig_Name_Labels->on || Scale_Bars->on || Tool_Tip->on || UI_On || Loading || Edit_Mode || Waypoint_Mode)
    {
        f32 textNormalMat[16];
        f32 textRotMat[16];

        f32 w2 = width * 0.5f;
        f32 h2 = height * 0.5f;
        f32 hz = height * Camera_Position.z;

        auto ModelXToScreen = [hz, w2](f32 xin)->f32 { return(((xin - Camera_Position.x) * hz) + w2); };
        auto ModelYToScreen = [hz, h2](f32 yin)->f32 { return(h2 - ((yin - Camera_Position.y) * hz)); };

        // Text Projection Matrix
        {
            memset(textNormalMat, 0, sizeof(textNormalMat));
            textNormalMat[0] = 2.0f / width;
            textNormalMat[5] = -2.0f / height;
            textNormalMat[10] = -1.0f;
            textNormalMat[12] = -1.0f;
            textNormalMat[13] = 1.0f;
            textNormalMat[15] = 1.0f;

            memset(textRotMat, 0, sizeof(textRotMat));
            textRotMat[4] = 2.0f / width;
            textRotMat[1] = 2.0f / height;
            textRotMat[10] = -1.0f;
            textRotMat[12] = -1.0f;
            textRotMat[13] = 1.0f;
            textRotMat[15] = 1.0f;
        }

        // Tool Tip
        if (File_Loaded && Tool_Tip->on && !Edit_Mode && !UI_On)
        {
            glUseProgram(Flat_Shader->shaderProgram);
            glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&Tool_Tip->bg);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textNormalMat);

            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textNormalMat);

            vertex vert[4];

            glViewport(0, 0, (s32)width, (s32)height);

            f32 lh = 0.0f;   

            fonsClearState(FontStash_Context);
            fonsSetSize(FontStash_Context, 20.0f * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Normal);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, FourFloatColorToU32(Tool_Tip->fg));
           
            f32 textBoxHeight = lh;
            textBoxHeight *= 3.0f;
            textBoxHeight += 2.0f;

            contig *cont1 = Contigs + Pixel_Contig_Lookup[Tool_Tip_Move.pixels.x];
            contig *cont2 = Contigs + Pixel_Contig_Lookup[Tool_Tip_Move.pixels.y];

            u32 start1 = Tool_Tip_Move.pixels.x;
            while (start1 > 0 && Pixel_Contig_Lookup[start1] == Pixel_Contig_Lookup[start1 - 1])
            {
                --start1;
            }

            u32 start2 = Tool_Tip_Move.pixels.y;
            while (start2 > 0 && Pixel_Contig_Lookup[start2] == Pixel_Contig_Lookup[start2 - 1])
            {
                --start2;
            }

            f64 bpPerPixel = (f64)Total_Genome_Length / (f64)(Texture_Resolution * Number_of_Textures_1D);

            char line1[64];
            char *line2 = (char *)"vs";
            char line3[64];

            stbsp_snprintf(line1, 64, "%s %$.2fbp", cont1->name, (f64)(Tool_Tip_Move.pixels.x - start1) * bpPerPixel);
            stbsp_snprintf(line3, 64, "%s %$.2fbp", cont2->name, (f64)(Tool_Tip_Move.pixels.y - start2) * bpPerPixel);

            f32 textWidth_1 = fonsTextBounds(FontStash_Context, 0, 0, line1, 0, NULL);
            f32 textWidth_2 = fonsTextBounds(FontStash_Context, 0, 0, line2, 0, NULL);
            f32 textWidth_3 = fonsTextBounds(FontStash_Context, 0, 0, line3, 0, NULL);
            f32 textWidth = Max(textWidth_1, textWidth_2);
            textWidth = Max(textWidth, textWidth_3);

            f32 spacing = 12.0f;

            glUseProgram(Flat_Shader->shaderProgram);

            vert[0].x = ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing;
            vert[0].y = ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing;
            vert[1].x = ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing;
            vert[1].y = ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing + textBoxHeight;
            vert[2].x = ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing + textWidth;
            vert[2].y = ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing + textBoxHeight;
            vert[3].x = ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing + textWidth;
            vert[3].y = ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing;

            glBindBuffer(GL_ARRAY_BUFFER, Label_Box_Data->vbos[0]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Label_Box_Data->vaos[0]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            glUseProgram(UI_Shader->shaderProgram);
            fonsDrawText(FontStash_Context, ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing, 
                    ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing, line1, 0);
            fonsDrawText(FontStash_Context, ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing, 
                    ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing + lh + 1.0f, line2, 0);
            fonsDrawText(FontStash_Context, ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing, 
                    ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing + (2.0f * lh) + 2.0f, line3, 0);
        }
        
        // Contig Labels
        if (File_Loaded && Contig_Name_Labels->on)
        {
            glUseProgram(Flat_Shader->shaderProgram);
            glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&Contig_Name_Labels->bg);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textNormalMat);

            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textNormalMat);

            u32 ptr = 0;
            vertex vert[4];

            glViewport(0, 0, (s32)width, (s32)height);

            f32 lh = 0.0f;   

            fonsClearState(FontStash_Context);
            fonsSetSize(FontStash_Context, 32.0f * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_CENTER | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Bold);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, FourFloatColorToU32(Contig_Name_Labels->fg));

            f32 leftPixel = ModelXToScreen(-0.5f);
            f32 totalLength = 0.0f;
            f32 wy0 = ModelYToScreen(0.5f);

            ForLoop(Number_of_Contigs_to_Display)
            {
                contig *cont = Contigs + Contig_Display_Order[index];
                
                totalLength += cont->fractionalLength;

                f32 rightPixel = ModelXToScreen(totalLength - 0.5f);

                if (rightPixel > 0.0f && leftPixel < width)
                {
                    const char *name = (const char *)cont->name;
                    f32 x = (rightPixel + leftPixel) * 0.5f;
                    f32 y = Max(wy0, 0.0f) + 10.0f;
                    f32 textWidth = fonsTextBounds(FontStash_Context, x, y, name, 0, NULL);

                    if (textWidth < (rightPixel - leftPixel))
                    {
                        f32 w2t = 0.5f * textWidth;

                        glUseProgram(Flat_Shader->shaderProgram);

                        vert[0].x = x - w2t;
                        vert[0].y = y + lh;
                        vert[1].x = x + w2t;
                        vert[1].y = y + lh;
                        vert[2].x = x + w2t;
                        vert[2].y = y;
                        vert[3].x = x - w2t;
                        vert[3].y = y;

                        glBindBuffer(GL_ARRAY_BUFFER, Label_Box_Data->vbos[ptr]);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                        glBindVertexArray(Label_Box_Data->vaos[ptr++]);
                        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                        glUseProgram(UI_Shader->shaderProgram);
                        fonsDrawText(FontStash_Context, x, y, name, 0);
                    }
                }

                leftPixel = rightPixel;
            }

            glUseProgram(Flat_Shader->shaderProgram);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textRotMat);

            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textRotMat);

            f32 topPixel = ModelYToScreen(0.5f);
            f32 wx0 = ModelXToScreen(-0.5f);
            totalLength = 0.0f;

            ForLoop(Number_of_Contigs_to_Display)
            {
                contig *cont = Contigs + Contig_Display_Order[index];
                
                totalLength += cont->fractionalLength;
                f32 bottomPixel = ModelYToScreen(0.5f - totalLength);

                if (topPixel < height && bottomPixel > 0.0f)
                {
                    const char *name = (const char *)cont->name;
                    f32 y = (topPixel + bottomPixel) * 0.5f;
                    f32 x = Max(wx0, 0.0f) + 10.0f;
                    f32 textWidth = fonsTextBounds(FontStash_Context, x, y, name, 0, NULL);

                    if (textWidth < (bottomPixel - topPixel))
                    {
                        f32 tmp = x;
                        x = -y;
                        y = tmp;

                        f32 w2t = 0.5f * textWidth;

                        glUseProgram(Flat_Shader->shaderProgram);

                        vert[0].x = x - w2t;
                        vert[0].y = y + lh;
                        vert[1].x = x + w2t;
                        vert[1].y = y + lh;
                        vert[2].x = x + w2t;
                        vert[2].y = y;
                        vert[3].x = x - w2t;
                        vert[3].y = y;

                        glBindBuffer(GL_ARRAY_BUFFER, Label_Box_Data->vbos[ptr]);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                        glBindVertexArray(Label_Box_Data->vaos[ptr++]);
                        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                        glUseProgram(UI_Shader->shaderProgram);
                        fonsDrawText(FontStash_Context, x, y, name, 0);
                    }
                }

                topPixel = bottomPixel;
            }

            ChangeSize((s32)width, (s32)height);
        }

        // Scale bars
#define MaxTicksPerScaleBar 128
        if (File_Loaded && Scale_Bars->on)
        {
            glUseProgram(Flat_Shader->shaderProgram);
            glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&Scale_Bars->bg);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textNormalMat);
            
            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textNormalMat);
            
            u32 ptr = 0;
            vertex vert[4];

            //u32 *labelsPerContig = PushArray(Working_Set, u32, Number_of_Contigs_to_Display);

            glViewport(0, 0, (s32)width, (s32)height);

            f32 lh = 0.0f;   

            fonsClearState(FontStash_Context);
            fonsSetSize(FontStash_Context, 20.0f * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_CENTER | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Normal);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, FourFloatColorToU32(Scale_Bars->fg));

            f32 leftPixel = ModelXToScreen(-0.5f);
            f32 rightPixel = ModelXToScreen(0.5f);
            f32 wy0 = ModelYToScreen(0.5);
            f32 offset = 45.0f * Screen_Scale.x;
            f32 y = Max(wy0, 0.0f) + offset;
            f32 totalLength = 0.0f;

            f32 bpPerPixel = (f32)((f64)Total_Genome_Length / (f64)(rightPixel - leftPixel));

            GLfloat *bg = (GLfloat *)&Scale_Bars->bg;
            /*static f32 scale = 1.0f / 255.0f;
            GLfloat fg[4];
            fg[0] = scale * (Scale_Bars->fg & 0xff);
            fg[1] = scale * ((Scale_Bars->fg >> 8) & 0xff);
            fg[2] = scale * ((Scale_Bars->fg >> 16) & 0xff);
            fg[3] = scale * ((Scale_Bars->fg >> 24) & 0xff);*/
            
            f32 scaleBarWidth = 4.0f * Screen_Scale.x;
            f32 tickLength = 3.0f * Screen_Scale.x;

            ForLoop(Number_of_Contigs_to_Display)
            {
                contig *cont = Contigs + Contig_Display_Order[index];
                
                totalLength += cont->fractionalLength;
                rightPixel = ModelXToScreen(totalLength - 0.5f);

                f32 pixelLength = rightPixel - leftPixel;

                u32 labelLevels = SubDivideScaleBar(leftPixel, rightPixel, (leftPixel + rightPixel) * 0.5f, bpPerPixel);
                u32 labels = 0;
                ForLoop2(labelLevels)
                {
                    labels += (labels + 1);
                }
                labels = Min(labels, MaxTicksPerScaleBar);

                //labelsPerContig[index] = labels;

                if (rightPixel > 0.0f && leftPixel < width)
                {
                    if (labels)
                    {
                        glUseProgram(Flat_Shader->shaderProgram);
                        glUniform4fv(Flat_Shader->colorLocation, 1, bg);

                        vert[0].x = leftPixel + 1.0f;
                        vert[0].y = y + scaleBarWidth + tickLength + 1.0f + lh;
                        vert[1].x = rightPixel - 1.0f;
                        vert[1].y = y + scaleBarWidth + tickLength + 1.0f + lh;
                        vert[2].x = rightPixel - 1.0f;
                        vert[2].y = y;
                        vert[3].x = leftPixel + 1.0f;
                        vert[3].y = y;

                        glBindBuffer(GL_ARRAY_BUFFER, Scale_Bar_Data->vbos[ptr]);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                        glBindVertexArray(Scale_Bar_Data->vaos[ptr++]);
                        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                        glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Scale_Bars->fg);

                        vert[0].x = leftPixel + 1.0f;
                        vert[0].y = y + scaleBarWidth;
                        vert[1].x = rightPixel - 1.0f;
                        vert[1].y = y + scaleBarWidth;
                        vert[2].x = rightPixel - 1.0f;
                        vert[2].y = y;
                        vert[3].x = leftPixel + 1.0f;
                        vert[3].y = y;

                        glBindBuffer(GL_ARRAY_BUFFER, Scale_Bar_Data->vbos[ptr]);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                        glBindVertexArray(Scale_Bar_Data->vaos[ptr++]);
                        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                        f32 fraction = 1.0f / (f32)(labels + 1);
                        f32 distance = 0.0f;
                        ForLoop2(labels)
                        {
                            distance += fraction;
                            f32 x = (pixelLength * distance) + (f32)leftPixel;

                            glUseProgram(Flat_Shader->shaderProgram);

                            vert[0].x = x - (0.5f * scaleBarWidth);
                            vert[0].y = y + scaleBarWidth + tickLength;
                            vert[1].x = x + (0.5f * scaleBarWidth);
                            vert[1].y = y + scaleBarWidth + tickLength;
                            vert[2].x = x + (0.5f * scaleBarWidth);
                            vert[2].y = y + scaleBarWidth;
                            vert[3].x = x - (0.5f * scaleBarWidth);
                            vert[3].y = y + scaleBarWidth;

                            glBindBuffer(GL_ARRAY_BUFFER, Scale_Bar_Data->vbos[ptr]);
                            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                            glBindVertexArray(Scale_Bar_Data->vaos[ptr++]);
                            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                            char buff[16];
                            stbsp_snprintf(buff, 16, "%$.2f", (f64)(pixelLength * distance * bpPerPixel));
                            glUseProgram(UI_Shader->shaderProgram);
                            fonsDrawText(FontStash_Context, x, y + scaleBarWidth + tickLength + 1.0f, buff, 0);
                        }
                    }
                }

                leftPixel = rightPixel;
            }

#if 0
            glUseProgram(Flat_Shader->shaderProgram);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textRotMat);

            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textRotMat);

            f32 topPixel = ModelYToScreen(0.5f);
            f32 wx0 = ModelXToScreen(-0.5f);
            y = Max(wx0, 0.0f) + offset;
            totalLength = 0.0f;

            ForLoop(Number_of_Contigs_to_Display)
            {
                contig *cont = Contigs + Contig_Display_Order[index];
                
                totalLength += cont->fractionalLength;
                f32 bottomPixel = ModelYToScreen(0.5f - totalLength);

                if (topPixel < height && bottomPixel > 0.0f)
                {
                    f32 pixelLength = bottomPixel - topPixel;

                    u32 labels = labelsPerContig[index];

                    if (labels)
                    {   
                        f32 topPixel_x = -topPixel;
                        f32 bottomPixel_x = -bottomPixel;

                        glUseProgram(Flat_Shader->shaderProgram);
                        glUniform4fv(Flat_Shader->colorLocation, 1, bg);

                        vert[0].x = bottomPixel_x + 1.0f;
                        vert[0].y = y + scaleBarWidth + tickLength + 1.0f + lh;
                        vert[1].x = topPixel_x - 1.0f;
                        vert[1].y = y + scaleBarWidth + tickLength + 1.0f + lh;
                        vert[2].x = topPixel_x - 1.0f;
                        vert[2].y = y;
                        vert[3].x = bottomPixel_x + 1.0f;
                        vert[3].y = y;

                        glBindBuffer(GL_ARRAY_BUFFER, Scale_Bar_Data->vbos[ptr]);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                        glBindVertexArray(Scale_Bar_Data->vaos[ptr++]);
                        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                        glUniform4fv(Flat_Shader->colorLocation, 1, fg);

                        vert[0].x = bottomPixel_x + 1.0f;
                        vert[0].y = y + scaleBarWidth;
                        vert[1].x = topPixel_x - 1.0f;
                        vert[1].y = y + scaleBarWidth;
                        vert[2].x = topPixel_x - 1.0f;
                        vert[2].y = y;
                        vert[3].x = bottomPixel_x + 1.0f;
                        vert[3].y = y;

                        glBindBuffer(GL_ARRAY_BUFFER, Scale_Bar_Data->vbos[ptr]);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                        glBindVertexArray(Scale_Bar_Data->vaos[ptr++]);
                        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                        f32 fraction = 1.0f / (f32)(labels + 1);
                        f32 distance = 0.0f;
                        ForLoop2(labels)
                        {
                            distance += fraction;
                            f32 x = topPixel_x - (pixelLength * distance);

                            glUseProgram(Flat_Shader->shaderProgram);

                            vert[0].x = x - (0.5f * scaleBarWidth);
                            vert[0].y = y + scaleBarWidth + tickLength;
                            vert[1].x = x + (0.5f * scaleBarWidth);
                            vert[1].y = y + scaleBarWidth + tickLength;
                            vert[2].x = x + (0.5f * scaleBarWidth);
                            vert[2].y = y + scaleBarWidth;
                            vert[3].x = x - (0.5f * scaleBarWidth);
                            vert[3].y = y + scaleBarWidth;

                            glBindBuffer(GL_ARRAY_BUFFER, Scale_Bar_Data->vbos[ptr]);
                            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                            glBindVertexArray(Scale_Bar_Data->vaos[ptr++]);
                            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                            char buff[16];
                            stbsp_snprintf(buff, 16, "%$.2f", (f64)(pixelLength * distance * bpPerPixel));
                            glUseProgram(UI_Shader->shaderProgram);
                            fonsDrawText(FontStash_Context, x, y + scaleBarWidth + tickLength + 1.0f, buff, 0);
                        }
                    }
                }

                topPixel = bottomPixel;
            }
#endif
            ChangeSize((s32)width, (s32)height);
            //FreeLastPush(Working_Set);
        }

        // Edit Mode
        if (File_Loaded && Edit_Mode && !UI_On)
        {
            u32 ptr = 0;
            vertex vert[4];

            glUseProgram(Flat_Shader->shaderProgram);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textNormalMat);
            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textNormalMat);

            glUseProgram(Flat_Shader->shaderProgram);
            glViewport(0, 0, (s32)width, (s32)height);

            f32 color[4];
            f32 *source = (f32 *)(Edit_Pixels.editing ? (Global_Edit_Invert_Flag ? &Edit_Mode_Colours->invSelect : 
                        &Edit_Mode_Colours->select) : &Edit_Mode_Colours->preSelect);
            ForLoop(4)
            {
                color[index] = source[index];
            }
            f32 alpha = color[3];
            color[3] = 1.0f;

            glUniform4fv(Flat_Shader->colorLocation, 1, color);

            f32 lineWidth = 0.005f / Camera_Position.z;

            vert[0].x = ModelXToScreen(Edit_Pixels.worldCoords.x - lineWidth);
            vert[0].y = ModelYToScreen(lineWidth - Edit_Pixels.worldCoords.x);
            vert[1].x = ModelXToScreen(Edit_Pixels.worldCoords.x - lineWidth);
            vert[1].y = ModelYToScreen(-lineWidth - Edit_Pixels.worldCoords.x);
            vert[2].x = ModelXToScreen(Edit_Pixels.worldCoords.x + lineWidth);
            vert[2].y = ModelYToScreen(-lineWidth - Edit_Pixels.worldCoords.x);
            vert[3].x = ModelXToScreen(Edit_Pixels.worldCoords.x + lineWidth);
            vert[3].y = ModelYToScreen(lineWidth - Edit_Pixels.worldCoords.x);

            glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            vert[0].x = ModelXToScreen(Edit_Pixels.worldCoords.y - lineWidth);
            vert[0].y = ModelYToScreen(lineWidth - Edit_Pixels.worldCoords.y);
            vert[1].x = ModelXToScreen(Edit_Pixels.worldCoords.y - lineWidth);
            vert[1].y = ModelYToScreen(-lineWidth - Edit_Pixels.worldCoords.y);
            vert[2].x = ModelXToScreen(Edit_Pixels.worldCoords.y + lineWidth);
            vert[2].y = ModelYToScreen(-lineWidth - Edit_Pixels.worldCoords.y);
            vert[3].x = ModelXToScreen(Edit_Pixels.worldCoords.y + lineWidth);
            vert[3].y = ModelYToScreen(lineWidth - Edit_Pixels.worldCoords.y);

            glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            f32 min = Min(Edit_Pixels.worldCoords.x, Edit_Pixels.worldCoords.y);
            f32 max = Max(Edit_Pixels.worldCoords.x, Edit_Pixels.worldCoords.y);

            if (Global_Edit_Invert_Flag)
            {
                f32 spacing = 0.002f / Camera_Position.z;
                f32 arrowWidth = 0.01f / Camera_Position.z;

                vert[0].x = ModelXToScreen(min + spacing);
                vert[0].y = ModelYToScreen(arrowWidth + spacing - min);
                vert[1].x = ModelXToScreen(min + spacing + arrowWidth);
                vert[1].y = ModelYToScreen(spacing - min);
                vert[2].x = ModelXToScreen(min + spacing + arrowWidth);
                vert[2].y = ModelYToScreen(arrowWidth + spacing - min);
                vert[3].x = ModelXToScreen(min + spacing + arrowWidth);
                vert[3].y = ModelYToScreen((2.0f * arrowWidth) + spacing - min);

                glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                vert[0].x = ModelXToScreen(min + spacing + arrowWidth);
                vert[0].y = ModelYToScreen((1.25f * arrowWidth) + spacing - min);
                vert[1].x = ModelXToScreen(min + spacing + arrowWidth);
                vert[1].y = ModelYToScreen((0.75f * arrowWidth) + spacing - min);
                vert[2].x = ModelXToScreen(max - spacing);
                vert[2].y = ModelYToScreen((0.75f * arrowWidth) + spacing - min);
                vert[3].x = ModelXToScreen(max - spacing);
                vert[3].y = ModelYToScreen((1.25f * arrowWidth) + spacing - min);

                glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
            }

            color[3] = alpha;
            glUniform4fv(Flat_Shader->colorLocation, 1, color);

            vert[0].x = ModelXToScreen(min);
            vert[0].y = ModelYToScreen(0.5f);
            vert[1].x = ModelXToScreen(min);
            vert[1].y = ModelYToScreen(-min);
            vert[2].x = ModelXToScreen(max);
            vert[2].y = ModelYToScreen(-min);
            vert[3].x = ModelXToScreen(max);
            vert[3].y = ModelYToScreen(0.5f);

            glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            vert[0].x = ModelXToScreen(-0.5f);
            vert[0].y = ModelYToScreen(-min);
            vert[1].x = ModelXToScreen(-0.5f);
            vert[1].y = ModelYToScreen(-max);
            vert[2].x = ModelXToScreen(min);
            vert[2].y = ModelYToScreen(-max);
            vert[3].x = ModelXToScreen(min);
            vert[3].y = ModelYToScreen(-min);

            glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            vert[0].x = ModelXToScreen(min);
            vert[0].y = ModelYToScreen(-max);
            vert[1].x = ModelXToScreen(min);
            vert[1].y = ModelYToScreen(-0.5f);
            vert[2].x = ModelXToScreen(max);
            vert[2].y = ModelYToScreen(-0.5f);
            vert[3].x = ModelXToScreen(max);
            vert[3].y = ModelYToScreen(-max);

            glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            vert[0].x = ModelXToScreen(max);
            vert[0].y = ModelYToScreen(-min);
            vert[1].x = ModelXToScreen(max);
            vert[1].y = ModelYToScreen(-max);
            vert[2].x = ModelXToScreen(0.5f);
            vert[2].y = ModelYToScreen(-max);
            vert[3].x = ModelXToScreen(0.5f);
            vert[3].y = ModelYToScreen(-min);

            glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            vert[0].x = ModelXToScreen(min);
            vert[0].y = ModelYToScreen(-min);
            vert[1].x = ModelXToScreen(min);
            vert[1].y = ModelYToScreen(-max);
            vert[2].x = ModelXToScreen(max);
            vert[2].y = ModelYToScreen(-max);
            vert[3].x = ModelXToScreen(max);
            vert[3].y = ModelYToScreen(-min);

            glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            {
                f32 lh = 0.0f;   
                
                fonsClearState(FontStash_Context);
                fonsSetSize(FontStash_Context, 18.0f * Screen_Scale.x);
                fonsSetAlign(FontStash_Context, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
                fonsSetFont(FontStash_Context, Font_Normal);
                fonsVertMetrics(FontStash_Context, 0, 0, &lh);
                fonsSetColor(FontStash_Context, FourFloatColorToU32(Edit_Mode_Colours->fg));
                
                f32 textBoxHeight = lh;
                textBoxHeight *= Edit_Pixels.editing ? 3.0f : 1.0f;
                textBoxHeight += Edit_Pixels.editing ? 3.0f : 0.0f;

                static char line1[64];
                static u32 line1Done = 0;
                char line2[64];

                char *midLineNoInv = (char *)"moved to";
                char *midLineInv = (char *)"inverted and moved to";
                char *midLine = Global_Edit_Invert_Flag ? midLineInv : midLineNoInv;

                u32 pix1 = Min(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y);
                u32 pix2 = Max(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y);

                contig *cont = Contigs + Pixel_Contig_Lookup[pix1];

                u32 nPixels = Number_of_Textures_1D * Texture_Resolution;
                f64 bpPerPixel = (f64)Total_Genome_Length / (f64)nPixels;

                u32 pixelStartContig = pix1;
                while (pixelStartContig > 0 && Pixel_Contig_Lookup[pixelStartContig] == Pixel_Contig_Lookup[pixelStartContig - 1])
                {
                    --pixelStartContig;
                }

                f64 bpStart = bpPerPixel * (f64)(pix1 - pixelStartContig);
                
                if (Edit_Pixels.editing)
                {
                    stbsp_snprintf(line2, 64, "%s - %$.2fbp", cont->name, bpStart);
                }
                else if (line1Done)
                {
                    line1Done = 0;
                }
                
                if (!line1Done)
                {
                    if ((pix1 == pixelStartContig) && ((pix2 == (nPixels - 1)) || (Pixel_Contig_Lookup[pix2] != Pixel_Contig_Lookup[pix2 + 1])))
                    {
                        stbsp_snprintf(line1, 64, "%s - full", cont->name);
                    }
                    else
                    {
                        f64 bpEnd = bpPerPixel * (f64)(pix2 - pixelStartContig);
                        stbsp_snprintf(line1, 64, "%s - %$.2fbp to %$.2fbp", cont->name, bpStart, bpEnd);
                    }
                    if (Edit_Pixels.editing)
                    {
                        line1Done = 1;
                    }
                }

                f32 textWidth_1 = fonsTextBounds(FontStash_Context, 0, 0, line1, 0, NULL);
                f32 textWidth_2 = fonsTextBounds(FontStash_Context, 0, 0, line2, 0, NULL);
                f32 textWidth_3 = fonsTextBounds(FontStash_Context, 0, 0, midLine, 0, NULL);
                f32 textWidth = Max(textWidth_1, textWidth_2);
                textWidth = Max(textWidth, textWidth_3);

                f32 spacing = 3.0f;

                glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Edit_Mode_Colours->bg);

                vert[0].x = ModelXToScreen(min) - spacing - textWidth;
                vert[0].y = ModelYToScreen(-max) + spacing;
                vert[1].x = ModelXToScreen(min) - spacing - textWidth;
                vert[1].y = ModelYToScreen(-max) + spacing + textBoxHeight;
                vert[2].x = ModelXToScreen(min) - spacing;
                vert[2].y = ModelYToScreen(-max) + spacing + textBoxHeight;
                vert[3].x = ModelXToScreen(min) - spacing;
                vert[3].y = ModelYToScreen(-max) + spacing;

                glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                glUseProgram(UI_Shader->shaderProgram);
                fonsDrawText(FontStash_Context, ModelXToScreen(min) - spacing - textWidth, ModelYToScreen(-max) + spacing, line1, 0);

                if (Edit_Pixels.editing)
                {
                    fonsDrawText(FontStash_Context, ModelXToScreen(min) - spacing - textWidth, ModelYToScreen(-max) + spacing + lh + 1.0f, midLine, 0);
                    fonsDrawText(FontStash_Context, ModelXToScreen(min) - spacing - textWidth, ModelYToScreen(-max) + spacing + (2.0f * (lh + 1.0f)), line2, 0);
                }

                {
                    fonsSetFont(FontStash_Context, Font_Bold);
                    fonsSetSize(FontStash_Context, 24.0f * Screen_Scale.x);
                    fonsVertMetrics(FontStash_Context, 0, 0, &lh);

                    textBoxHeight = lh;
                    textBoxHeight *= 5.0f;
                    textBoxHeight += 4.0f;
                    spacing = 10.0f;

                    char *helpText1 = (char *)"Edit Mode";
                    char *helpText2 = (char *)"E: exit, Q: undo";
                    char *helpText3 = (char *)"Left Click: pickup, place";
                    char *helpText4 = (char *)"Middle Click: pickup whole contig";
                    char *helpText5 = (char *)"Middle Click (while editing): invert sequence";

                    textWidth = fonsTextBounds(FontStash_Context, 0, 0, helpText5, 0, NULL);

                    glUseProgram(Flat_Shader->shaderProgram);

                    vert[0].x = width - spacing - textWidth;
                    vert[0].y = height - spacing - textBoxHeight;
                    vert[1].x = width - spacing - textWidth;
                    vert[1].y = height - spacing;
                    vert[2].x = width - spacing;
                    vert[2].y = height - spacing;
                    vert[3].x = width - spacing;
                    vert[3].y = height - spacing - textBoxHeight;

                    glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                    glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
                    glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                    glUseProgram(UI_Shader->shaderProgram);
                    fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight, helpText1, 0);
                    f32 textY = 1.0f + lh;
                    fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight + textY, helpText2, 0);
                    textY += (1.0f + lh);
                    fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight + textY, helpText3, 0);
                    textY += (1.0f + lh);
                    fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight + textY, helpText4, 0);
                    textY += (1.0f + lh);
                    fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight + textY, helpText5, 0);
                }
            }
        }

        // Waypoint Mode
        if (File_Loaded && Waypoint_Mode && !UI_On)
        {
            u32 ptr = 0;
            vertex vert[4];

            glUseProgram(Flat_Shader->shaderProgram);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textNormalMat);
            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textNormalMat);

            glViewport(0, 0, (s32)width, (s32)height);

            glUseProgram(Flat_Shader->shaderProgram);
            glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Waypoint_Mode_Colours->base);

            f32 lineWidth = 2.5f;
            f32 lineHeight = 10.0f;

            f32 lh = 0.0f;   

            u32 baseColour = FourFloatColorToU32(Waypoint_Mode_Colours->base);
            u32 selectColour = FourFloatColorToU32(Waypoint_Mode_Colours->selected);
            
            fonsClearState(FontStash_Context);
            fonsSetSize(FontStash_Context, 22.0f * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Bold);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, baseColour);

            char buff[4];

            TraverseLinkedList(Waypoint_Editor->activeWaypoints.next, waypoint)
            {
                point2f screen = {ModelXToScreen(node->coords.x), ModelYToScreen(-node->coords.y)};
                
                glUseProgram(Flat_Shader->shaderProgram);
                if (node == Selected_Waypoint)
                {
                    glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Waypoint_Mode_Colours->selected);
                }

                vert[0].x = screen.x - lineWidth;
                vert[0].y = screen.y - lineHeight;
                vert[1].x = screen.x - lineWidth;
                vert[1].y = screen.y + lineHeight;
                vert[2].x = screen.x + lineWidth;
                vert[2].y = screen.y + lineHeight;
                vert[3].x = screen.x + lineWidth;
                vert[3].y = screen.y - lineHeight;

                glBindBuffer(GL_ARRAY_BUFFER, Waypoint_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Waypoint_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                vert[0].x = screen.x - lineHeight;
                vert[0].y = screen.y - lineWidth;
                vert[1].x = screen.x - lineHeight;
                vert[1].y = screen.y + lineWidth;
                vert[2].x = screen.x - lineWidth;
                vert[2].y = screen.y + lineWidth;
                vert[3].x = screen.x - lineWidth;
                vert[3].y = screen.y - lineWidth;

                glBindBuffer(GL_ARRAY_BUFFER, Waypoint_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Waypoint_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                vert[0].x = screen.x + lineWidth;
                vert[0].y = screen.y - lineWidth;
                vert[1].x = screen.x + lineWidth;
                vert[1].y = screen.y + lineWidth;
                vert[2].x = screen.x + lineHeight;
                vert[2].y = screen.y + lineWidth;
                vert[3].x = screen.x + lineHeight;
                vert[3].y = screen.y - lineWidth;

                glBindBuffer(GL_ARRAY_BUFFER, Waypoint_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Waypoint_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
               
                if (node == Selected_Waypoint)
                {
                    glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Waypoint_Mode_Colours->base);
                }

                glUseProgram(UI_Shader->shaderProgram);
                if (node == Selected_Waypoint)
                {
                    fonsSetColor(FontStash_Context, selectColour);
                }
                
                stbsp_snprintf(buff, sizeof(buff), "%d", node->index + 1);
                fonsDrawText(FontStash_Context, screen.x + lineWidth + lineWidth, screen.y - lineWidth - lh, buff, 0);

                if (node == Selected_Waypoint)
                {
                    fonsSetColor(FontStash_Context, baseColour);
                }
            }

            fonsSetSize(FontStash_Context, 24.0f * Screen_Scale.x);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, FourFloatColorToU32(Waypoint_Mode_Colours->text));

            f32 textBoxHeight = lh;
            textBoxHeight *= 4.0f;
            textBoxHeight += 3.0f;
            f32 spacing = 10.0f;

            char *helpText1 = (char *)"Waypoint Mode";
            char *helpText2 = (char *)"W: exit";
            char *helpText3 = (char *)"Left Click: place";
            char *helpText4 = (char *)"Middle Click: delete";

            f32 textWidth = fonsTextBounds(FontStash_Context, 0, 0, helpText4, 0, NULL);

            glUseProgram(Flat_Shader->shaderProgram);
            glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Waypoint_Mode_Colours->bg);

            vert[0].x = width - spacing - textWidth;
            vert[0].y = height - spacing - textBoxHeight;
            vert[1].x = width - spacing - textWidth;
            vert[1].y = height - spacing;
            vert[2].x = width - spacing;
            vert[2].y = height - spacing;
            vert[3].x = width - spacing;
            vert[3].y = height - spacing - textBoxHeight;

            glBindBuffer(GL_ARRAY_BUFFER, Waypoint_Data->vbos[ptr]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Waypoint_Data->vaos[ptr++]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            glUseProgram(UI_Shader->shaderProgram);
            fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight, helpText1, 0);
            f32 textY = 1.0f + lh;
            fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight + textY, helpText2, 0);
            textY += (1.0f + lh);
            fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight + textY, helpText3, 0);
            textY += (1.0f + lh);
            fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight + textY, helpText4, 0);
        }

        // NK
        if (UI_On)
        {
            glDisable(GL_CULL_FACE);
            glEnable(GL_SCISSOR_TEST);

            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textNormalMat);
            glViewport(0, 0, (s32)width, (s32)height);

            {
                const struct nk_draw_command *cmd;
                void *vertices, *elements;
                nk_draw_index *offset = 0;

                glBindVertexArray(NK_Device->vao);
                glBindBuffer(GL_ARRAY_BUFFER, NK_Device->vbo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, NK_Device->ebo);

#define MAX_VERTEX_MEMORY KiloByte(512)
#define MAX_ELEMENT_MEMORY KiloByte(128)
                glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_MEMORY, NULL, GL_STREAM_DRAW);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_MEMORY, NULL, GL_STREAM_DRAW);

                vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
                elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

                {
                    nk_convert_config config;
                    static const nk_draw_vertex_layout_element vertex_layout[] = {
                        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(nk_glfw_vertex, position)},
                        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(nk_glfw_vertex, uv)},
                        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(nk_glfw_vertex, col)},
                        {NK_VERTEX_LAYOUT_END}
                    };

                    NK_MEMSET(&config, 0, sizeof(config));
                    config.vertex_layout = vertex_layout;
                    config.vertex_size = sizeof(nk_glfw_vertex);
                    config.vertex_alignment = NK_ALIGNOF(nk_glfw_vertex);
                    config.null = NK_Device->null;
                    config.circle_segment_count = 22;
                    config.curve_segment_count = 22;
                    config.arc_segment_count = 22;
                    config.global_alpha = 1.0f;
                    config.shape_AA = NK_ANTI_ALIASING_ON;
                    config.line_AA = NK_ANTI_ALIASING_ON;

                    {
                        nk_buffer vbuf, ebuf;
                        nk_buffer_init_fixed(&vbuf, vertices, MAX_VERTEX_MEMORY);
                        nk_buffer_init_fixed(&ebuf, elements, MAX_ELEMENT_MEMORY);
                        nk_convert(NK_Context, &NK_Device->cmds, &vbuf, &ebuf, &config);
                    }
                }
                glUnmapBuffer(GL_ARRAY_BUFFER);
                glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

                nk_draw_foreach(cmd, NK_Context, &NK_Device->cmds)
                {
                    if (!cmd->elem_count) continue;
                    glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
                    glScissor(
                            (GLint)(cmd->clip_rect.x),
                            (GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h))),
                            (GLint)(cmd->clip_rect.w),
                            (GLint)(cmd->clip_rect.h));
                    glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
                    offset += cmd->elem_count;
                }
            }

            ChangeSize((s32)width, (s32)height);
            glDisable(GL_SCISSOR_TEST);
            glEnable(GL_CULL_FACE);

            nk_clear(NK_Context);
        }

        if (Loading)
        {
            u32 colour = glfonsRGBA(Theme_Colour.r, Theme_Colour.g, Theme_Colour.b, Theme_Colour.a);

            fonsClearState(FontStash_Context);
            fonsSetSize(FontStash_Context, 64.0f * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);
            fonsSetFont(FontStash_Context, Font_Bold);
            fonsSetColor(FontStash_Context, colour);

            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textNormalMat);
            glViewport(0, 0, (s32)width, (s32)height);
            fonsDrawText(FontStash_Context, width * 0.5f, height * 0.5f, "Loading...", 0);

            ChangeSize((s32)width, (s32)height);
        }
    }
}

struct
file_atlas_entry
{
    u32 base;
    u32 nBytes;
};

global_variable
file_atlas_entry *
File_Atlas;

global_function
void
LoadTexture(void *in)
{
    GLuint *textureHandle = (GLuint *)in;

    u16 id[2];
    id[0] = (u16)(*textureHandle >> 8);
    id[1] = (u16)(*textureHandle & 255);
    texture_buffer *buffer = TakeTextureBufferFromQueue_Wait(Texture_Buffer_Queue);
    buffer->x = id[0];
    buffer->y = id[1];

    u32 linearIndex =   (buffer->x * (Number_of_Textures_1D - 1)) -
        ((buffer->x & 1) ? (((buffer->x-1)>>1) * buffer->x) : 
         ((buffer->x>>1)*(buffer->x-1))) + buffer->y;

    file_atlas_entry *entry = File_Atlas + linearIndex;
    u32 nBytes = entry->nBytes;
    fseek(buffer->file, entry->base, SEEK_SET);

    fread(buffer->compressionBuffer, 1, nBytes, buffer->file);

    if (libdeflate_deflate_decompress(buffer->decompressor, (const void *)buffer->compressionBuffer, nBytes, (void *)buffer->texture, Bytes_Per_Texture, NULL))
    {
        fprintf(stderr, "Could not decompress texture from disk\n");
    }

    u32 x;
    u32 y;
    do
    {
        FenceIn(linearIndex = (u32)Texture_Ptr);

        u32 n = Number_of_Textures_1D;
        x = 0;

        while (linearIndex >= n)
        {
            linearIndex -= n--;
            ++x;
        }
        y = x + linearIndex;

    } while (buffer->x != (u16)x || buffer->y != (u16)y);

    FenceIn(Current_Loaded_Texture = buffer);
}

global_function
void
PrintShaderInfoLog(GLuint shader)
{
    s32 infoLogLen = 0;
    s32 charsWritten = 0;
    GLchar *infoLog;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);

    if (infoLogLen > 0)
    {
        infoLog = PushArray(Working_Set, GLchar, (u32)infoLogLen);
        glGetShaderInfoLog(shader, infoLogLen, &charsWritten, infoLog);
        fprintf(stderr, "%s\n", infoLog);
        FreeLastPush(Working_Set);
    }
}

global_function
GLuint
CreateShader(const GLchar *fragmentShaderSource, const GLchar *vertexShaderSource)
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);

    GLint compiled;

    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compiled);

    if (compiled == GL_FALSE)
    {
        PrintShaderInfoLog(vertexShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        exit(1);
    }

    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compiled);

    if (compiled == GL_FALSE)
    {
        PrintShaderInfoLog(fragmentShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        exit(1);
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint isLinked;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &isLinked);
    if(isLinked == GL_FALSE)
    {
        GLint maxLength;
        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &maxLength);
        if(maxLength > 0)
        {
            char *pLinkInfoLog = PushArray(Working_Set, char, (u32)maxLength);
            glGetProgramInfoLog(shaderProgram, maxLength, &maxLength, pLinkInfoLog);
            fprintf(stderr, "Failed to link shader: %s\n", pLinkInfoLog);
            FreeLastPush(Working_Set);
        }

        glDetachShader(shaderProgram, vertexShader);
        glDetachShader(shaderProgram, fragmentShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(shaderProgram);
        exit(1);
    }

    return(shaderProgram);
}

global_function
void
AdjustColorMap(s32 dir)
{
    f32 unit = 0.05f;
    f32 delta = unit * (dir > 0 ? 1.0f : -1.0f);

    Color_Maps->controlPoints[1] = Max(Min(Color_Maps->controlPoints[1] + delta, Color_Maps->controlPoints[2]), Color_Maps->controlPoints[0]);

    glUseProgram(Contact_Matrix->shaderProgram);
    glUniform3fv( Color_Maps->cpLocation, 1, Color_Maps->controlPoints);
}

global_function
void
NextColorMap(s32 dir)
{
    glActiveTexture(GL_TEXTURE1);
    
    Color_Maps->currMap = dir > 0 ? (Color_Maps->currMap == (Color_Maps->nMaps - 1) ? 0 : Color_Maps->currMap + 1) : (Color_Maps->currMap == 0 ? Color_Maps->nMaps - 1 : Color_Maps->currMap - 1);
    
    glBindTexture(GL_TEXTURE_BUFFER, Color_Maps->maps[Color_Maps->currMap]);
    
    glActiveTexture(GL_TEXTURE0);
}

global_variable
GLuint
Quad_EBO;

enum
load_file_result
{
    ok,
    fileErr,
    decompErr,
};

global_variable
libdeflate_decompressor *
Decompressor;

global_variable
libdeflate_compressor *
Compressor;

global_variable
u08 Magic[] = {'p', 's', 't', 'm'};

global_function
FILE *
TestFile(const char *fileName)
{
    // Test File
    FILE *file;
    {
        file = fopen(fileName, "rb");
        if (!file)
        {
#ifdef DEBUG
            fprintf(stderr, "Error: %d\n", errno);
            exit(errno);
#endif
        }
        else
        {
            u08 magicTest[sizeof(Magic)];

            u32 bytesRead = (u32)fread(magicTest, 1, sizeof(magicTest), file);
            if (bytesRead == sizeof(magicTest))
            {
                ForLoop(sizeof(Magic))
                {
                    if (Magic[index] != magicTest[index])
                    {
                        fclose(file);
                        file = 0;
                        break;
                    }
                }
            }
            else
            {
                fclose(file);
                file = 0;
            }
        }
    }
    return(file);
}

global_function
void
SaveState(u64 headerHash);

global_function
void
LoadState(u64 headerHash);

global_function
load_file_result
LoadFile(const char *filePath, memory_arena *arena, char **fileName, u64 *headerHash)
{
    // Test File
    FILE *file = TestFile(filePath);
    if (!file)
    {
        return(fileErr);
    }
    
    FenceIn(File_Loaded = 0);

    static u32 reload = 0;
    
    if (!reload)
    {
        reload = 1;
    }
    else
    {
        glDeleteTextures(1, &Contact_Matrix->textures);

        glDeleteVertexArrays((GLsizei)(Number_of_Textures_1D * Number_of_Textures_1D), Contact_Matrix->vaos);
        glDeleteBuffers((GLsizei)(Number_of_Textures_1D * Number_of_Textures_1D), Contact_Matrix->vbos);

        glDeleteBuffers(1, &Contact_Matrix->pixelStartLookupBuffer);
        glDeleteTextures(1, &Contact_Matrix->pixelStartLookupBufferTex);

        glDeleteBuffers(1, &Contact_Matrix->pixelRearrangmentLookupBuffer);
        glDeleteTextures(1, &Contact_Matrix->pixelRearrangmentLookupBufferTex);

        glDeleteVertexArrays((GLsizei)Grid_Data->nBuffers, Grid_Data->vaos);
        glDeleteBuffers((GLsizei)Grid_Data->nBuffers, Grid_Data->vbos);

        glDeleteVertexArrays((GLsizei)Label_Box_Data->nBuffers, Label_Box_Data->vaos);
        glDeleteBuffers((GLsizei)Label_Box_Data->nBuffers, Label_Box_Data->vbos);

        glDeleteVertexArrays((GLsizei)Scale_Bar_Data->nBuffers, Scale_Bar_Data->vaos);
        glDeleteBuffers((GLsizei)Scale_Bar_Data->nBuffers, Scale_Bar_Data->vbos);

        Current_Loaded_Texture = 0;
        Texture_Ptr = 0;
        
        Mouse_Move.x = -1.0;
        Mouse_Move.y = -1.0;

        Mouse_Select.x = -1.0;
        Mouse_Select.y = -1.0;

        Camera_Position.x = 0.0f;
        Camera_Position.y = 0.0f;
        Camera_Position.z = 1.0f;
       
        Edit_Pixels.editing = 0;
        Global_Mode = mode_normal;

        ResetMemoryArenaP(arena);
    }

    // File Contents
    {
        u32 nBytesHeaderComp;
        u32 nBytesHeader;
        fread(&nBytesHeaderComp, 1, 4, file);
        fread(&nBytesHeader, 1, 4, file);

        u08 *header = PushArrayP(arena, u08, nBytesHeader);
        u08 *compressionBuffer = PushArrayP(arena, u08, nBytesHeaderComp);

        fread(compressionBuffer, 1, nBytesHeaderComp, file);
        *headerHash = FastHash64(compressionBuffer, nBytesHeaderComp, 0xbafd06832de619c2);
        if (libdeflate_deflate_decompress(Decompressor, (const void *)compressionBuffer, nBytesHeaderComp, (void *)header, nBytesHeader, NULL))
        {
            return(decompErr);
        }
        FreeLastPushP(arena); // comp buffer

        u64 val64;
        u08 *ptr = (u08 *)&val64;
        ForLoop(8)
        {
            *ptr++ = *header++;
        }
        Total_Genome_Length = val64;

        u32 val32;
        ptr = (u08 *)&val32;
        ForLoop(4)
        {
            *ptr++ = *header++;
        }
        u32 numberOfContigs = val32;

        Contigs = PushArrayP(arena, contig, Max_Number_of_Contigs_to_Display);
        Contig_Sentinel = PushStructP(arena, contig_sentinel);
        Contig_Sentinel->nFree = 0;
        Contig_Sentinel->tail = 0;

        f32 *contigFractionalLengths = PushArrayP(arena, f32, numberOfContigs);
        Contig_Display_Order = PushArrayP(arena, u32, Max_Number_of_Contigs_to_Display);

        ForLoop(numberOfContigs)
        {
            f32 frac;
            u32 name[16];

            ptr = (u08 *)&frac;
            ForLoop2(4)
            {
                *ptr++ = *header++;
            }
            contigFractionalLengths[index] = frac;

            if (index < Max_Number_of_Contigs_to_Display)
            {
                Contigs[index].fractionalLength = frac;
                Contigs[index].index = index;
                Contig_Display_Order[index] = index;
            }

            ptr = (u08 *)name;
            ForLoop2(64)
            {
                *ptr++ = *header++;
            }

            if (index < Max_Number_of_Contigs_to_Display)
            {
                ForLoop2(16)
                {
                    Contigs[index].name[index2] = name[index2];
                }
            }
        }

        if (numberOfContigs < Max_Number_of_Contigs_to_Display)
        {
            ForLoop(Max_Number_of_Contigs_to_Display - numberOfContigs)
            {
                Contigs[index + numberOfContigs].index = index + numberOfContigs;
                Contig_Display_Order[index + numberOfContigs] = index + numberOfContigs;
            }
            ForLoop(Max_Number_of_Contigs_to_Display - numberOfContigs)
            {
                FreeContig(Max_Number_of_Contigs_to_Display - index - 1);
            }
        }

        Number_of_Contigs_to_Display = Min(numberOfContigs, Max_Number_of_Contigs_to_Display);
        
        u08 textureRes = *header++;
        u08 nTextRes = *header++;
        u08 mipMapLevels = *header;

        Texture_Resolution = Pow2(textureRes);
        Number_of_Textures_1D = Pow2(nTextRes);
        Number_of_MipMaps = mipMapLevels;

        u32 nBytesPerText = 0;
        ForLoop(Number_of_MipMaps)
        {
            nBytesPerText += Pow2((2 * textureRes--));
        }
        nBytesPerText >>= 1;
        Bytes_Per_Texture = nBytesPerText;

        File_Atlas = PushArrayP(arena, file_atlas_entry, (Number_of_Textures_1D + 1) * (Number_of_Textures_1D >> 1));

        u32 currLocation = sizeof(Magic) + 8 + nBytesHeaderComp;
        ForLoop((Number_of_Textures_1D + 1) * (Number_of_Textures_1D >> 1))
        {
            file_atlas_entry *entry = File_Atlas + index;
            u32 nBytes;
            fread(&nBytes, 1, 4, file);
            fseek(file, nBytes, SEEK_CUR);
            currLocation += 4;

            entry->base = currLocation;
            entry->nBytes = nBytes;

            currLocation += nBytes;
        }

        fclose(file);

        u32 nPixels = Texture_Resolution * Number_of_Textures_1D;
        Pixel_Contig_Lookup = PushArrayP(arena, u16, nPixels);

        f32 totalLength = 0.0f;
        u32 lastPixel = 0;
        ForLoop(numberOfContigs)
        {
            totalLength += contigFractionalLengths[index];
            u32 pixel = (u32)((f64)nPixels * (f64)totalLength);
            
            while (lastPixel < pixel)
            {
                Pixel_Contig_Lookup[lastPixel++] = (u16)index;
            }
        }

        while (lastPixel < nPixels)
        {
            Pixel_Contig_Lookup[lastPixel++] = (u16)(numberOfContigs - 1);
        }
    }

    // Load Textures
    {
        InitialiseTextureBufferQueue(arena, Texture_Buffer_Queue, Bytes_Per_Texture, filePath);

        u32 nTextures = (Number_of_Textures_1D + 1) * (Number_of_Textures_1D >> 1);
        u32 *packedTextureIndexes = PushArrayP(arena, u32, nTextures);
        u32 ptr = 0;
        ForLoop(Number_of_Textures_1D)
        {
            ForLoop2(Number_of_Textures_1D - index)
            {
                packedTextureIndexes[ptr] = (index << 8) | (index + index2);
                ThreadPoolAddTask(Thread_Pool, LoadTexture, (void *)(packedTextureIndexes + ptr++));
            }
        }

        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &Contact_Matrix->textures);
        glBindTexture(GL_TEXTURE_2D_ARRAY, Contact_Matrix->textures);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, (GLint)Number_of_MipMaps - 1);

        u32 resolution = Texture_Resolution;
        ForLoop(Number_of_MipMaps)
        {
            glCompressedTexImage3D  (GL_TEXTURE_2D_ARRAY, (GLint)index, GL_COMPRESSED_RED_RGTC1, (GLsizei)resolution, (GLsizei)resolution, 
                                    (GLsizei)nTextures, 0, (GLsizei)((resolution >> 1) * resolution * nTextures), 0);
            resolution >>= 1;
        }
        ptr = 0;

        printf("Loading textures...\n");
        ForLoop(Number_of_Textures_1D)
        {
            ForLoop2(Number_of_Textures_1D - index)
            {
                volatile texture_buffer *loadedTexture = 0;
                do
                {
                    __atomic_load(&Current_Loaded_Texture, &loadedTexture, 0);
                } while (!loadedTexture);
                u08 *texture = loadedTexture->texture;

                resolution = Texture_Resolution;
                for (   GLint level = 0;
                        level < (GLint)Number_of_MipMaps;
                        ++level )
                {
                    GLsizei nBytes = (GLsizei)(resolution * (resolution >> 1));
                    
                    glCompressedTexSubImage3D(  GL_TEXTURE_2D_ARRAY, level, 0, 0, (GLint)ptr, (GLsizei)resolution, (GLsizei)resolution, 1,
                                                GL_COMPRESSED_RED_RGTC1, nBytes, texture);

                    resolution >>= 1;
                    texture += nBytes;
                }
                ++ptr;

                printf("\r%3d/%3d (%1.2f%%) textures loaded from disk...", Texture_Ptr + 1, nTextures, 100.0 * (f64)((f32)(Texture_Ptr + 1) / (f32)((Number_of_Textures_1D >> 1) * (Number_of_Textures_1D + 1))));
                fflush(stdout);

                AddTextureBufferToQueue(Texture_Buffer_Queue, (texture_buffer *)loadedTexture);
                FenceIn(Current_Loaded_Texture = 0);
                __atomic_fetch_add(&Texture_Ptr, 1, 0);
            }
        }

        printf("\n");
        CloseTextureBufferQueueFiles(Texture_Buffer_Queue);
        FreeLastPushP(arena); // packedTextureIndexes
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }

    // Contact Matrix Vertex Data
    {
        glUseProgram(Contact_Matrix->shaderProgram);

        Contact_Matrix->vaos = PushArrayP(arena, GLuint, Number_of_Textures_1D * Number_of_Textures_1D);
        Contact_Matrix->vbos = PushArrayP(arena, GLuint, Number_of_Textures_1D * Number_of_Textures_1D);

        GLuint posAttrib = (GLuint)glGetAttribLocation(Contact_Matrix->shaderProgram, "position");
        GLuint texAttrib = (GLuint)glGetAttribLocation(Contact_Matrix->shaderProgram, "texcoord");

        f32 x = 0.0f;
        f32 y = 1.0f;
        f32 quadSize = 1.0f / (f32)Number_of_Textures_1D;

        u32 ptr = 0;
        ForLoop(Number_of_Textures_1D)
        {
            ForLoop2(Number_of_Textures_1D)
            {
                tex_vertex textureVertices[4];

                glGenVertexArrays(1, Contact_Matrix->vaos + ptr);
                glBindVertexArray(Contact_Matrix->vaos[ptr]);

                f32 allCornerCoords[2][2] = {{0.0f, 1.0f}, {1.0f, 0.0f}};
                f32 *cornerCoords = allCornerCoords[index2 >= index ? 0 : 1];
                
                u32 min = Min(index, index2);
                u32 max = Max(index, index2);
                
                f32 u = (f32)((min * (Number_of_Textures_1D - 1)) -
                    ((min & 1) ? (((min-1)>>1) * min) : 
                     ((min>>1)*(min-1))) + max);
                
                textureVertices[0].x = x - 0.5f;
                textureVertices[0].y = y - quadSize - 0.5f;
                textureVertices[0].u = u;
                textureVertices[0].s = cornerCoords[0];
                textureVertices[0].t = cornerCoords[1];

                textureVertices[1].x = x - 0.5f + quadSize;
                textureVertices[1].y = y - quadSize - 0.5f;
                textureVertices[1].u = u;
                textureVertices[1].s = 1.0f;
                textureVertices[1].t = 1.0f;

                textureVertices[2].x = x - 0.5f + quadSize;
                textureVertices[2].y = y - 0.5f;
                textureVertices[2].u = u;
                textureVertices[2].s = cornerCoords[1];
                textureVertices[2].t = cornerCoords[0];

                textureVertices[3].x = x - 0.5f;
                textureVertices[3].y = y - 0.5f;
                textureVertices[3].u = u;
                textureVertices[3].s = 0.0f;
                textureVertices[3].t = 0.0f;

                glGenBuffers(1, Contact_Matrix->vbos + ptr);
                glBindBuffer(GL_ARRAY_BUFFER, Contact_Matrix->vbos[ptr]);
                glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(tex_vertex), textureVertices, GL_STATIC_DRAW);

                glEnableVertexAttribArray(posAttrib);
                glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(tex_vertex), 0);
                glEnableVertexAttribArray(texAttrib);
                glVertexAttribPointer(texAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(tex_vertex), (void *)(3 * sizeof(GLfloat)));

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Quad_EBO);

                x += quadSize;
                ++ptr;
            }

            y -= quadSize;
            x = 0.0f;
        }
    }

    // Texture Pixel Lookups
    {
        GLuint pixStart, pixStartTex, pixRearrage, pixRearrageTex;
       
        u32 nTex = (Number_of_Textures_1D + 1) * (Number_of_Textures_1D >> 1);
        u32 nPix1D = Number_of_Textures_1D * Texture_Resolution;

        glActiveTexture(GL_TEXTURE2);

        u16 *pixStartLookup = PushArrayP(arena, u16, 2 * nTex);
        u32 ptr = 0;
        ForLoop(Number_of_Textures_1D)
        {
            ForLoop2(Number_of_Textures_1D - index)
            {
                pixStartLookup[ptr++] = (u16)((index + index2) * Texture_Resolution);
                pixStartLookup[ptr++] = (u16)(index * Texture_Resolution);
            }
        }

        glGenBuffers(1, &pixStart);
        glBindBuffer(GL_TEXTURE_BUFFER, pixStart);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(u16) * 2 * nTex, pixStartLookup, GL_STATIC_DRAW);

        glGenTextures(1, &pixStartTex);
        glBindTexture(GL_TEXTURE_BUFFER, pixStartTex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RG16UI, pixStart);

        Contact_Matrix->pixelStartLookupBuffer = pixStart;
        Contact_Matrix->pixelStartLookupBufferTex = pixStartTex;

        FreeLastPushP(arena); // pixStartLookup

        glActiveTexture(GL_TEXTURE3);

        u16 *pixRearrageLookup = PushArrayP(arena, u16, nPix1D);
        ForLoop(nPix1D)
        {
            pixRearrageLookup[index] = (u16)index;
        }

        glGenBuffers(1, &pixRearrage);
        glBindBuffer(GL_TEXTURE_BUFFER, pixRearrage);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(u16) * nPix1D, pixRearrageLookup, GL_DYNAMIC_DRAW);

        glGenTextures(1, &pixRearrageTex);
        glBindTexture(GL_TEXTURE_BUFFER, pixRearrageTex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R16UI, pixRearrage);

        Contact_Matrix->pixelRearrangmentLookupBuffer = pixRearrage;
        Contact_Matrix->pixelRearrangmentLookupBufferTex = pixRearrageTex;

        FreeLastPushP(arena); // pixRearrageLookup

        glUniform1ui(glGetUniformLocation(Contact_Matrix->shaderProgram, "pixpertex"), Texture_Resolution);
        glUniform1f(glGetUniformLocation(Contact_Matrix->shaderProgram, "oopixpertex"), 1.0f / (f32)Texture_Resolution);
        glUniform1ui(glGetUniformLocation(Contact_Matrix->shaderProgram, "ntex1dm1"), Number_of_Textures_1D - 1);

        glActiveTexture(GL_TEXTURE0);
    }

    GLuint posAttribFlatShader = (GLuint)glGetAttribLocation(Flat_Shader->shaderProgram, "position");
    u32 pad = 0;
    auto PushGenericBuffer = [posAttribFlatShader, pad, arena] (quad_data **quadData, u32 numberOfBuffers)
    {
        (void)pad;

        *quadData = PushStructP(arena, quad_data);

        (*quadData)->vaos = PushArrayP(arena, GLuint, numberOfBuffers);
        (*quadData)->vbos = PushArrayP(arena, GLuint, numberOfBuffers);
        (*quadData)->nBuffers = numberOfBuffers;

        glUseProgram(Flat_Shader->shaderProgram);

        ForLoop(numberOfBuffers)
        {
            glGenVertexArrays(1, (*quadData)->vaos + index);
            glBindVertexArray((*quadData)->vaos[index]);

            glGenBuffers(1, (*quadData)->vbos + index);
            glBindBuffer(GL_ARRAY_BUFFER, (*quadData)->vbos[index]);
            glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vertex), NULL, GL_DYNAMIC_DRAW);

            glEnableVertexAttribArray(posAttribFlatShader);
            glVertexAttribPointer(posAttribFlatShader, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Quad_EBO);
        }
    };

    // Grid Data
    {
        PushGenericBuffer(&Grid_Data, 2 * (Number_of_Contigs_to_Display + 1));
    }

    // Label Box Data
    {
        PushGenericBuffer(&Label_Box_Data, 2 * Number_of_Contigs_to_Display);
    }

    //Scale Bar Data
    {
        PushGenericBuffer(&Scale_Bar_Data, Min(Number_of_Contigs_to_Display, 4) * (2 + MaxTicksPerScaleBar));
    }

#ifdef DEBUG
    {
        PushGenericBuffer(&Texture_Tile_Grid, 2 * (Number_of_Textures_1D + 1));
        PushGenericBuffer(&QuadTree_Data, 1 << (2 * (Waypoints_Quadtree_Levels + 1)));
    }
#endif
    
    // Map Editor
    {
        Map_Editor = PushStructP(arena, map_editor);
        Map_Editor->nEdits = 0;
        Map_Editor->editStackPtr = 0;
        Map_Editor->edits = PushArrayP(arena, map_edit, Edits_Stack_Size);
    }

    // Waypoint Editor
    {
        Waypoint_Editor = PushStructP(arena, waypoint_editor);
        Waypoint_Editor->nWaypointsActive = 0;
        Waypoint_Editor->freeWaypoints = {};
        Waypoint_Editor->activeWaypoints = {};
        Waypoint_Editor->freeWaypoints.next = PushArrayP(arena, waypoint, Waypoints_Stack_Size);

        Waypoint_Editor->quadtree = PushQuadTree(arena);
        Waypoint_Editor->freeNodes = {};
        Waypoint_Editor->freeNodes.next = PushArrayP(arena, waypoint_quadtree_node, Waypoints_Stack_Size);

        Waypoint_Editor->freeWaypoints.next->prev = &Waypoint_Editor->freeWaypoints;
        Waypoint_Editor->freeNodes.next->prev = &Waypoint_Editor->freeNodes;
        ForLoop(Waypoints_Stack_Size - 1)
        {
            waypoint *wayp = Waypoint_Editor->freeWaypoints.next + index;
            wayp->next = (wayp + 1);
            (wayp + 1)->prev = wayp;
            (wayp + 1)->next = 0;

            waypoint_quadtree_node *node = Waypoint_Editor->freeNodes.next + index;
            node->next = (node + 1);
            (node + 1)->prev = node;
            (node + 1)->next = 0;
        }
    }

    FenceIn(File_Loaded = 1);

    char *tmp = (char *)filePath;
#ifdef _WIN32
    char sep = '\\';
#else
    char sep = '/';
#endif
    
    while (*++tmp) {}
    while (*--tmp != sep) {}

    *fileName = tmp + 1;

    LoadState(*headerHash);
    return(ok);
}

global_variable
memory_arena *
Loading_Arena;

global_function
void
SetTheme(struct nk_context *ctx, enum theme theme)
{
    struct nk_color table[NK_COLOR_COUNT];
    u32 themeSet = 1;

    switch (theme)
    {
        case THEME_WHITE:
            {
                table[NK_COLOR_TEXT] = nk_rgba(70, 70, 70, 255);
                table[NK_COLOR_WINDOW] = nk_rgba(175, 175, 175, 255);
                table[NK_COLOR_HEADER] = nk_rgba(175, 175, 175, 255);
                table[NK_COLOR_BORDER] = nk_rgba(0, 0, 0, 255);
                table[NK_COLOR_BUTTON] = nk_rgba(185, 185, 185, 255);
                table[NK_COLOR_BUTTON_HOVER] = nk_rgba(170, 170, 170, 255);
                table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(160, 160, 160, 255);
                table[NK_COLOR_TOGGLE] = nk_rgba(150, 150, 150, 255);
                table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(120, 120, 120, 255);
                table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(175, 175, 175, 255);
                table[NK_COLOR_SELECT] = nk_rgba(190, 190, 190, 255);
                table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(175, 175, 175, 255);
                table[NK_COLOR_SLIDER] = nk_rgba(190, 190, 190, 255);
                table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(80, 80, 80, 255);
                table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(70, 70, 70, 255);
                table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(60, 60, 60, 255);
                table[NK_COLOR_PROPERTY] = nk_rgba(175, 175, 175, 255);
                table[NK_COLOR_EDIT] = nk_rgba(150, 150, 150, 255);
                table[NK_COLOR_EDIT_CURSOR] = nk_rgba(0, 0, 0, 255);
                table[NK_COLOR_COMBO] = nk_rgba(175, 175, 175, 255);
                table[NK_COLOR_CHART] = nk_rgba(160, 160, 160, 255);
                table[NK_COLOR_CHART_COLOR] = nk_rgba(45, 45, 45, 255);
                table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
                table[NK_COLOR_SCROLLBAR] = nk_rgba(180, 180, 180, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(140, 140, 140, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(150, 150, 150, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(160, 160, 160, 255);
                table[NK_COLOR_TAB_HEADER] = nk_rgba(180, 180, 180, 255);
            }
            break;

        case THEME_RED:
            {
                table[NK_COLOR_TEXT] = nk_rgba(190, 190, 190, 255);
                table[NK_COLOR_WINDOW] = nk_rgba(30, 33, 40, 215);
                table[NK_COLOR_HEADER] = nk_rgba(181, 45, 69, 220);
                table[NK_COLOR_BORDER] = nk_rgba(51, 55, 67, 255);
                table[NK_COLOR_BUTTON] = nk_rgba(181, 45, 69, 255);
                table[NK_COLOR_BUTTON_HOVER] = nk_rgba(190, 50, 70, 255);
                table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(195, 55, 75, 255);
                table[NK_COLOR_TOGGLE] = nk_rgba(51, 55, 67, 255);
                table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 60, 60, 255);
                table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(181, 45, 69, 255);
                table[NK_COLOR_SELECT] = nk_rgba(51, 55, 67, 255);
                table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(181, 45, 69, 255);
                table[NK_COLOR_SLIDER] = nk_rgba(51, 55, 67, 255);
                table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(181, 45, 69, 255);
                table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(186, 50, 74, 255);
                table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(191, 55, 79, 255);
                table[NK_COLOR_PROPERTY] = nk_rgba(51, 55, 67, 255);
                table[NK_COLOR_EDIT] = nk_rgba(51, 55, 67, 225);
                table[NK_COLOR_EDIT_CURSOR] = nk_rgba(190, 190, 190, 255);
                table[NK_COLOR_COMBO] = nk_rgba(51, 55, 67, 255);
                table[NK_COLOR_CHART] = nk_rgba(51, 55, 67, 255);
                table[NK_COLOR_CHART_COLOR] = nk_rgba(170, 40, 60, 255);
                table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
                table[NK_COLOR_SCROLLBAR] = nk_rgba(30, 33, 40, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
                table[NK_COLOR_TAB_HEADER] = nk_rgba(181, 45, 69, 220);
            }
            break;

        case THEME_BLUE:
            {
                table[NK_COLOR_TEXT] = nk_rgba(20, 20, 20, 255);
                table[NK_COLOR_WINDOW] = nk_rgba(202, 212, 214, 215);
                table[NK_COLOR_HEADER] = nk_rgba(137, 182, 224, 220);
                table[NK_COLOR_BORDER] = nk_rgba(140, 159, 173, 255);
                table[NK_COLOR_BUTTON] = nk_rgba(137, 182, 224, 255);
                table[NK_COLOR_BUTTON_HOVER] = nk_rgba(142, 187, 229, 255);
                table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(147, 192, 234, 255);
                table[NK_COLOR_TOGGLE] = nk_rgba(177, 210, 210, 255);
                table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(182, 215, 215, 255);
                table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(137, 182, 224, 255);
                table[NK_COLOR_SELECT] = nk_rgba(177, 210, 210, 255);
                table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(137, 182, 224, 255);
                table[NK_COLOR_SLIDER] = nk_rgba(177, 210, 210, 255);
                table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(137, 182, 224, 245);
                table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(142, 188, 229, 255);
                table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(147, 193, 234, 255);
                table[NK_COLOR_PROPERTY] = nk_rgba(210, 210, 210, 255);
                table[NK_COLOR_EDIT] = nk_rgba(210, 210, 210, 225);
                table[NK_COLOR_EDIT_CURSOR] = nk_rgba(20, 20, 20, 255);
                table[NK_COLOR_COMBO] = nk_rgba(210, 210, 210, 255);
                table[NK_COLOR_CHART] = nk_rgba(210, 210, 210, 255);
                table[NK_COLOR_CHART_COLOR] = nk_rgba(137, 182, 224, 255);
                table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba( 255, 0, 0, 255);
                table[NK_COLOR_SCROLLBAR] = nk_rgba(190, 200, 200, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(64, 84, 95, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(70, 90, 100, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
                table[NK_COLOR_TAB_HEADER] = nk_rgba(156, 193, 220, 255);
            }
            break;

        case THEME_DARK:
            {
                table[NK_COLOR_TEXT] = nk_rgba(210, 210, 210, 255);
                table[NK_COLOR_WINDOW] = nk_rgba(57, 67, 71, 215);
                table[NK_COLOR_HEADER] = nk_rgba(51, 51, 56, 220);
                table[NK_COLOR_BORDER] = nk_rgba(46, 46, 46, 255);
                table[NK_COLOR_BUTTON] = nk_rgba(48, 83, 111, 255);
                table[NK_COLOR_BUTTON_HOVER] = nk_rgba(58, 93, 121, 255);
                table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(63, 98, 126, 255);
                table[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
                table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 255);
                table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
                table[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 255);
                table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 255);
                table[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
                table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
                table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
                table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
                table[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
                table[NK_COLOR_EDIT] = nk_rgba(50, 58, 61, 225);
                table[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 255);
                table[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
                table[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 255);
                table[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 255);
                table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
                table[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
                table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
                table[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
            }
            break;

        case THEME_BLACK:
        case THEME_COUNT:
            themeSet = 0;
    }

    if (themeSet) nk_style_from_table(ctx, table, Screen_Scale.x, Screen_Scale.y);
    else nk_style_default(ctx, Screen_Scale.x, Screen_Scale.y);

    Theme_Colour = table[NK_COLOR_BUTTON_ACTIVE];
    
    NK_Context->style.slider.show_buttons = nk_true;

    Current_Theme = theme;
}

global_function
void
Setup()
{
    Decompressor = libdeflate_alloc_decompressor();
    if (!Decompressor)
    {
        fprintf(stderr, "Could not allocate decompressor\n");
        exit(1);
    }

    Compressor = libdeflate_alloc_compressor(12);
    if (!Compressor)
    {
        fprintf(stderr, "Could not allocate compressor\n");
        exit(1);
    }

    Texture_Buffer_Queue = PushStruct(Working_Set, texture_buffer_queue);
    
    // Contig Name Label UI
    {
        Contig_Name_Labels = PushStruct(Working_Set, ui_colour_element_bg);
        Contig_Name_Labels->on = 0;
        Contig_Name_Labels->fg = Yellow_Text_Float;
        Contig_Name_Labels->bg = Grey_Background;
    }
    
    // Scale Bar UI
    {
        Scale_Bars = PushStruct(Working_Set, ui_colour_element_bg);
        Scale_Bars->on = 0;
        Scale_Bars->fg = Red_Text_Float;
        Scale_Bars->bg = Grey_Background;
    }
   
    // Grid UI
    {
        Grid = PushStruct(Working_Set, ui_colour_element);
        Grid->on = 1;
        Grid->bg = Grey_Background;
    }

    // Tool Tip UI
    {
        Tool_Tip = PushStruct(Working_Set, ui_colour_element_bg);
        Tool_Tip->on = 1;
        Tool_Tip->fg = Yellow_Text_Float;
        Tool_Tip->bg = Grey_Background;
    }

    // Edit Mode Colours
    {
        Edit_Mode_Colours = PushStruct(Working_Set, edit_mode_colours);
        Edit_Mode_Colours->preSelect = Green_Float;
        Edit_Mode_Colours->select = Blue_Float;
        Edit_Mode_Colours->invSelect = Red_Float;
        Edit_Mode_Colours->fg = Yellow_Text_Float;
        Edit_Mode_Colours->bg = Grey_Background;
    }

    // Waypoint Mode Colours
    {
        Waypoint_Mode_Colours = PushStruct(Working_Set, waypoint_mode_colours);
        Waypoint_Mode_Colours->base = Red_Full;
        Waypoint_Mode_Colours->selected = Blue_Full;
        Waypoint_Mode_Colours->text = Yellow_Text_Float;
        Waypoint_Mode_Colours->bg = Grey_Background;
    }

#ifdef DEBUG
    {
        Tiles = PushStruct(Working_Set, ui_colour_element);
        Tiles->on = 1;
        Tiles->bg = {0.0f, 1.0f, 1.0f, 1.0f};

        QuadTrees = PushStruct(Working_Set, ui_colour_element);
        QuadTrees->on = 1;
        QuadTrees->bg = {0.0f, 1.0f, 0.0f, 1.0f};
    }
#endif

    // Contact Matrix Shader
    {
        Contact_Matrix = PushStruct(Working_Set, contact_matrix);
        Contact_Matrix->shaderProgram = CreateShader(FragmentSource_Texture, VertexSource_Texture);

        glUseProgram(Contact_Matrix->shaderProgram);
        glBindFragDataLocation(Contact_Matrix->shaderProgram, 0, "outColor");

        Contact_Matrix->matLocation = glGetUniformLocation(Contact_Matrix->shaderProgram, "matrix");
        glUniform1i(glGetUniformLocation(Contact_Matrix->shaderProgram, "tex"), 0);
        glUniform1i(glGetUniformLocation(Contact_Matrix->shaderProgram, "colormap"), 1);
        glUniform1i(glGetUniformLocation(Contact_Matrix->shaderProgram, "pixstartlookup"), 2);
        glUniform1i(glGetUniformLocation(Contact_Matrix->shaderProgram, "pixrearrangelookup"), 3);

        glActiveTexture(GL_TEXTURE1);

        Color_Maps = PushStruct(Working_Set, color_maps);
        u32 nMaps = Number_of_Color_Maps;
        Color_Maps->maps = PushArray(Working_Set, GLuint, nMaps);
        Color_Maps->mapPreviews = PushArray(Working_Set, struct nk_image, nMaps);
        Color_Maps->nMaps = nMaps;
        Color_Maps->currMap = 1;

        ForLoop(nMaps)
        {
            u32 mapPreviewImage[256];

            GLuint tbo, tboTex, texPreview;

            glGenBuffers(1, &tbo);
            glBindBuffer(GL_TEXTURE_BUFFER, tbo);
            glBufferData(GL_TEXTURE_BUFFER, sizeof(Color_Map_Data[index]), Color_Map_Data[index], GL_STATIC_DRAW);

            glGenTextures(1, &tboTex);
            glBindTexture(GL_TEXTURE_BUFFER, tboTex);
            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, tbo);

            Color_Maps->maps[index] = tboTex;

            glActiveTexture(GL_TEXTURE0);

            ForLoop2(256) //TODO SIMD
            {
                mapPreviewImage[index2] =   ((u32)(Color_Map_Data[index][3 * index2] * 255.0f)) |
                    (((u32)(Color_Map_Data[index][(3 * index2) + 1] * 255.0f)) << 8) |
                    (((u32)(Color_Map_Data[index][(3 * index2) + 2] * 255.0f)) << 16) |
                    0xff000000;
            }
            glGenTextures(1, &texPreview);
            glBindTexture(GL_TEXTURE_2D, texPreview);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, mapPreviewImage);
            glGenerateMipmap(GL_TEXTURE_2D);

            Color_Maps->mapPreviews[index] = nk_image_id((s32)texPreview);

            glActiveTexture(GL_TEXTURE1);
        }
        NextColorMap(-1);

        glActiveTexture(GL_TEXTURE0);

        Color_Maps->cpLocation = glGetUniformLocation(Contact_Matrix->shaderProgram, "controlpoints");
        Color_Maps->controlPoints[0] = 0.0f;
        Color_Maps->controlPoints[1] = 0.5f;
        Color_Maps->controlPoints[2] = 1.0f;
        glUniform3fv( Color_Maps->cpLocation, 1, Color_Maps->controlPoints);
    }

    // Flat Color Shader
    {
        Flat_Shader = PushStruct(Working_Set, flat_shader);
        Flat_Shader->shaderProgram = CreateShader(FragmentSource_Flat, VertexSource_Flat);

        glUseProgram(Flat_Shader->shaderProgram);
        glBindFragDataLocation(Flat_Shader->shaderProgram, 0, "outColor");

        Flat_Shader->matLocation = glGetUniformLocation(Flat_Shader->shaderProgram, "matrix");
        Flat_Shader->colorLocation = glGetUniformLocation(Flat_Shader->shaderProgram, "color");
    }
    

    // Fonts
    {
        UI_Shader = PushStruct(Working_Set, ui_shader);
        UI_Shader->shaderProgram = CreateShader(FragmentSource_UI, VertexSource_UI);
        glUseProgram(UI_Shader->shaderProgram);
        glBindFragDataLocation(UI_Shader->shaderProgram, 0, "outColor");
        glUniform1i(glGetUniformLocation(UI_Shader->shaderProgram, "tex"), 0);
        UI_Shader->matLocation = glGetUniformLocation(UI_Shader->shaderProgram, "matrix");

        FontStash_Context = glfonsCreate(512, 512, FONS_ZERO_TOPLEFT);
        Font_Normal = fonsAddFontMem(FontStash_Context, "Sans Regular", FontNormal, (s32)FontNormal_Size, 0);

        if (Font_Normal == FONS_INVALID)
        {
            fprintf(stderr, "Could not add font 'DroidSerif-Regular.ttf'\n");
            exit(1);
        }
        Font_Bold = fonsAddFontMem(FontStash_Context, "Sans Bold", FontBold, (s32)FontBold_Size, 0);
        if (Font_Bold == FONS_INVALID)
        {
            fprintf(stderr, "Could not add font 'DroidSerif-Bold.ttf'\n");
            exit(1);
        }
    }

    // Quad EBO
    {
        GLushort pIndexQuad[6];
        pIndexQuad[0] = 0;
        pIndexQuad[1] = 1;
        pIndexQuad[2] = 2;
        pIndexQuad[3] = 2;
        pIndexQuad[4] = 3;
        pIndexQuad[5] = 0;

        glGenBuffers(1, &Quad_EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Quad_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLushort), pIndexQuad, GL_STATIC_DRAW);
    }

    GLuint posAttribFlatShader = (GLuint)glGetAttribLocation(Flat_Shader->shaderProgram, "position");
    auto PushGenericBuffer = [posAttribFlatShader] (quad_data **quadData, u32 numberOfBuffers)
    {
        *quadData = PushStruct(Working_Set, quad_data);

        (*quadData)->vaos = PushArray(Working_Set, GLuint, numberOfBuffers);
        (*quadData)->vbos = PushArray(Working_Set, GLuint, numberOfBuffers);

        glUseProgram(Flat_Shader->shaderProgram);

        ForLoop(numberOfBuffers)
        {
            glGenVertexArrays(1, (*quadData)->vaos + index);
            glBindVertexArray((*quadData)->vaos[index]);

            glGenBuffers(1, (*quadData)->vbos + index);
            glBindBuffer(GL_ARRAY_BUFFER, (*quadData)->vbos[index]);
            glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vertex), NULL, GL_DYNAMIC_DRAW);

            glEnableVertexAttribArray(posAttribFlatShader);
            glVertexAttribPointer(posAttribFlatShader, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Quad_EBO);
        }
    };

    // Select Box Data
    {
        PushGenericBuffer(&Select_Box_Data, 4);
    }

    // Edit Mode Data
    {
        PushGenericBuffer(&Edit_Mode_Data, 11);
    }

    // Tool Tip Data
    {
        PushGenericBuffer(&Tool_Tip_Data, 1);
    }

    // Waypoint Data
    {
        PushGenericBuffer(&Waypoint_Data, (3 * Waypoints_Stack_Size) + 1);
    }

    // Nuklear Setup
    {
#define NK_Memory_Size MegaByte(32)
        NK_Device = PushStruct(Working_Set, device);
        u08 *nkCmdMemory = PushArray(Working_Set, u08, NK_Memory_Size);
        nk_buffer_init_fixed(&NK_Device->cmds, (void *)nkCmdMemory, NK_Memory_Size);
        NK_Device->lastContextMemory = PushArray(Working_Set, u08, NK_Memory_Size);
        memset(NK_Device->lastContextMemory, 0, NK_Memory_Size);
        NK_Device->prog = UI_Shader->shaderProgram;
        NK_Device->uniform_proj = UI_Shader->matLocation;
        NK_Device->attrib_pos = UI_SHADER_LOC_POSITION;
        NK_Device->attrib_uv = UI_SHADER_LOC_TEXCOORD;
        NK_Device->attrib_col = UI_SHADER_LOC_COLOR;

        GLsizei vs = sizeof(nk_glfw_vertex);
        size_t vp = offsetof(nk_glfw_vertex, position);
        size_t vt = offsetof(nk_glfw_vertex, uv);
        size_t vc = offsetof(nk_glfw_vertex, col);

        glGenBuffers(1, &NK_Device->vbo);
        glGenBuffers(1, &NK_Device->ebo);
        glGenVertexArrays(1, &NK_Device->vao);

        glBindVertexArray(NK_Device->vao);
        glBindBuffer(GL_ARRAY_BUFFER, NK_Device->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, NK_Device->ebo);

        glEnableVertexAttribArray((GLuint)NK_Device->attrib_pos);
        glEnableVertexAttribArray((GLuint)NK_Device->attrib_uv);
        glEnableVertexAttribArray((GLuint)NK_Device->attrib_col);

        glVertexAttribPointer((GLuint)NK_Device->attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void*)vp);
        glVertexAttribPointer((GLuint)NK_Device->attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void*)vt);
        glVertexAttribPointer((GLuint)NK_Device->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void*)vc);

        NK_Atlas = PushStruct(Working_Set, nk_font_atlas);
        nk_font_atlas_init_default(NK_Atlas);
        nk_font_atlas_begin(NK_Atlas);
        struct nk_font_config cfg = nk_font_config(14);
        cfg.oversample_h = 3;
        cfg.oversample_v = 3;
        NK_Font = nk_font_atlas_add_from_memory(NK_Atlas, FontBold, (nk_size)FontBold_Size, 22 * Screen_Scale.y, &cfg);
        NK_Font_Browser = nk_font_atlas_add_from_memory(NK_Atlas, FontBold, (nk_size)FontBold_Size, 14 * Screen_Scale.y, &cfg);

        s32 w,h;
        const void *image = nk_font_atlas_bake(NK_Atlas, &w, &h, NK_FONT_ATLAS_RGBA32);

        glGenTextures(1, &NK_Device->font_tex);
        glBindTexture(GL_TEXTURE_2D, NK_Device->font_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, image);

        nk_font_atlas_end(NK_Atlas, nk_handle_id((s32)NK_Device->font_tex), &NK_Device->null);
        u08 *nkContextMemory = PushArray(Working_Set, u08, NK_Memory_Size);
        nk_init_fixed(NK_Context, (void *)nkContextMemory, NK_Memory_Size, &NK_Font->handle);

        SetTheme(NK_Context, THEME_DARK);

        Theme_Name[THEME_RED] = (u08 *)"Red";
        Theme_Name[THEME_BLUE] = (u08 *)"Blue";
        Theme_Name[THEME_WHITE] = (u08 *)"White";
        Theme_Name[THEME_BLACK] = (u08 *)"Black";
        Theme_Name[THEME_DARK] = (u08 *)"Dark";
    }

    Loading_Arena = PushSubArena(Working_Set, MegaByte(128));
}

global_function
void
TakeScreenShot()
{
    s32 viewport[4];
    glGetIntegerv (GL_VIEWPORT, viewport);

    u08 *imageBuffer = PushArray(Working_Set, u08, (u32)(3 * viewport[2] * viewport[3]));
    glReadPixels (  0, 0, viewport[2], viewport[3],
            GL_RGB, GL_UNSIGNED_BYTE, imageBuffer);

    stbi_flip_vertically_on_write(1);
    stbi_write_png("PretextView_ScreenShot.png", viewport[2], viewport[3], 3, imageBuffer, 3 * viewport[2]); //TODO change png compression to use libdeflate zlib impl

    FreeLastPush(Working_Set);
}

global_function
void
InvertMap(u32 pixelFrom, u32 pixelTo)
{
    if (pixelFrom > pixelTo)
    {
        u32 tmp = pixelFrom;
        pixelFrom = pixelTo;
        pixelTo = tmp;
    }

    u32 nPixels = Number_of_Textures_1D * Texture_Resolution;

    Assert(pixelFrom < nPixels);
    Assert(pixelTo < nPixels);
    
    u32 copySize = (pixelTo - pixelFrom + 1) >> 1;
    
    u16 *tmpBuffer = PushArray(Working_Set, u16, copySize);

    glBindBuffer(GL_TEXTURE_BUFFER, Contact_Matrix->pixelRearrangmentLookupBuffer);
    u16 *buffer = (u16 *)glMapBufferRange(GL_TEXTURE_BUFFER, 0, nPixels * sizeof(u16), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    if (buffer)
    {
        ForLoop(copySize)
        {
            tmpBuffer[index] = buffer[pixelFrom + index];
        }

        ForLoop(copySize)
        {
            buffer[pixelFrom + index] = buffer[pixelTo - index];
        }

        ForLoop(copySize)
        {
            buffer[pixelTo - index] = tmpBuffer[index];
        }
    }
    else
    {
        fprintf(stderr, "Could not map pixel rearrange buffer\n");
    }

    glUnmapBuffer(GL_TEXTURE_BUFFER);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
    FreeLastPush(Working_Set); // tmpBuffer
}

global_function
u08 *
PushStringIntoIntArray(u32 *intArray, u32 arrayLength, u08 *string, u08 stringTerminator = '\0')
{
    u08 *stringToInt = (u08 *)intArray;
    u32 stringLength = 0;

    while(*string != stringTerminator)
    {
        *(stringToInt++) = *(string++);
        ++stringLength;
    }

    while (stringLength & 3)
    {
        *(stringToInt++) = 0;
        ++stringLength;
    }

    for (   u32 index = (stringLength >> 2);
            index < arrayLength;
            ++index )
    {
        intArray[index] = 0;
    }

    return(string);
}

global_function
s32
RearrangeMap(u32 pixelFrom, u32 pixelTo, s32 delta, u32 *flags)
{
    u32 internalContigLock = *flags & rearrangeFlag_internalContigLock;
    u32 allowNewContigCreation = *flags & rearrangeFlag_allowNewContigCreation;
    u32 firstMove = *flags & rearrangeFlag_firstMove;
    
    if (pixelFrom > pixelTo)
    {
        u32 tmp = pixelFrom;
        pixelFrom = pixelTo;
        pixelTo = tmp;
    }

    u32 nPixels = Number_of_Textures_1D * Texture_Resolution;

    Assert((delta > 0 ? (u32)delta : (u32)(-delta)) < nPixels);

    Assert(pixelFrom < nPixels);
    Assert(pixelTo < nPixels);

    u32 nPixelsInRange = pixelTo - pixelFrom + 1;

    pixelFrom += nPixels;
    pixelTo += nPixels;

    auto GetRealBufferLocation = [nPixels] (u32 index)
    {
        u32 result;
        
        if (index >= nPixels && index < (2 * nPixels))
        {
            result = index - nPixels;
        }
        else if (index >= (2 * nPixels))
        {
            result = index - (2 * nPixels);
        }
        else
        {
            result = index;
        }

        Assert(result >= 0 && result < nPixels);

        return(result);
    };
    
    u32 forward = delta > 0;
   
    if (internalContigLock)
    {
        u16 myContigId = Pixel_Contig_Lookup[GetRealBufferLocation(pixelFrom)];

        if (forward)
        {
            u32 loop = 1;
            while (loop)
            {
                loop = 0;

                u16 contigAheadAfter = Pixel_Contig_Lookup[GetRealBufferLocation(pixelTo + (u32)delta + 1)];
                u16 contigBehindAfter = Pixel_Contig_Lookup[GetRealBufferLocation(pixelTo + (u32)delta)];

                if (contigAheadAfter == contigBehindAfter && contigAheadAfter != myContigId)
                {
                    --delta;
                    loop = 1;
                }
            }
        }
        else
        {
            u32 loop = 1;
            while (loop)
            {
                loop = 0;

                u16 contigBehindAfter = Pixel_Contig_Lookup[GetRealBufferLocation((u32)((s32)pixelFrom + delta) - 1)];
                u16 contigAheadAfter = Pixel_Contig_Lookup[GetRealBufferLocation((u32)((s32)pixelFrom + delta))];

                if (contigAheadAfter == contigBehindAfter && contigAheadAfter != myContigId)
                {
                    ++delta;
                    loop = 1;
                }
            }

        }
    }
  
    forward = delta > 0;
    allowNewContigCreation = allowNewContigCreation && internalContigLock && delta == 0;

    u32 startCopyFromRange;
    u32 startCopyToRange;

    if (forward)
    {
        startCopyFromRange = pixelTo + 1;
        startCopyToRange = pixelFrom;
    }
    else
    {
        startCopyFromRange = (u32)((s32)pixelFrom + delta);
        startCopyToRange = (u32)((s32)pixelTo + delta) + 1;
    }
    
    u32 copySize = delta > 0 ? (u32)delta : (u32)-delta;
    
    u16 *tmpBuffer = PushArray(Working_Set, u16, copySize);
    u16 *tmpBuffer2 = PushArray(Working_Set, u16, copySize);

    glBindBuffer(GL_TEXTURE_BUFFER, Contact_Matrix->pixelRearrangmentLookupBuffer);
    u16 *buffer = (u16 *)glMapBufferRange(GL_TEXTURE_BUFFER, 0, nPixels * sizeof(u16), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    if (buffer)
    {
        u16 contigAheadBefore = 0;
        u16 contigBehindBefore = 0;
        u16 contigAheadAfter = 0;
        u16 contigBehindAfter = 0;
        
        auto GetNewContigId = [internalContigLock, forward, allowNewContigCreation, firstMove, flags] (u16 oldContigId, u16 aheadBefore, u16 behindBefore, u16 aheadAfter, u16 behindAfter)
        {
            u16 result;
            u32 wholeContigBefore = aheadBefore != behindBefore && oldContigId != aheadBefore && oldContigId != behindBefore;

            if (firstMove && wholeContigBefore)
            {
                *flags |= rearrangeFlag_wasWholeContig;
            }

            if (internalContigLock && !allowNewContigCreation)
            {
                result = oldContigId;
            }
            else
            {
                u32 internalToExistingContigAfter = aheadAfter == behindAfter;

                if (wholeContigBefore)
                {
                    FreeContig((u32)oldContigId);
                }

                if (!firstMove)
                {
                    wholeContigBefore = *flags & rearrangeFlag_wasWholeContig;
                }

                contig *cont;
                if (!internalToExistingContigAfter && (cont = GetFreeContig()))
                {
                    result = (u16)cont->index;
                   
                    if (result != oldContigId && !wholeContigBefore)
                    {
                        u08 buff[64];
                        stbsp_snprintf((char *)buff, 64, "New Contig %d", result);
                        PushStringIntoIntArray(cont->name, 16, buff);
                    }
                }
                else
                {
                    result = forward ? aheadAfter : behindAfter;
                }
            }
            
            return(result);
        };

        contigAheadBefore = Pixel_Contig_Lookup[GetRealBufferLocation(pixelTo + 1)];
        contigBehindBefore = Pixel_Contig_Lookup[GetRealBufferLocation(pixelFrom - 1)];
        
        ForLoop(copySize)
        {
            tmpBuffer[index] = buffer[GetRealBufferLocation(index + startCopyFromRange)];
            tmpBuffer2[index] = Pixel_Contig_Lookup[GetRealBufferLocation(index + startCopyFromRange)];
        }

        if (forward)
        {
            contigAheadAfter = Pixel_Contig_Lookup[GetRealBufferLocation(pixelTo + (u32)delta + 1)];
            contigBehindAfter = Pixel_Contig_Lookup[GetRealBufferLocation(pixelTo + (u32)delta)];

            u16 contigId = GetNewContigId(Pixel_Contig_Lookup[GetRealBufferLocation(pixelFrom)], contigAheadBefore, contigBehindBefore, contigAheadAfter, contigBehindAfter);

            ForLoop(nPixelsInRange)
            {
                buffer[GetRealBufferLocation(pixelTo + (u32)delta - index)] = buffer[GetRealBufferLocation(pixelTo - index)];
                Pixel_Contig_Lookup[GetRealBufferLocation(pixelTo + (u32)delta - index)] = contigId;
            }
        }
        else
        {
            if (delta)
            {
                contigBehindAfter = Pixel_Contig_Lookup[GetRealBufferLocation((u32)((s32)pixelFrom + delta) - 1)];
                contigAheadAfter = Pixel_Contig_Lookup[GetRealBufferLocation((u32)((s32)pixelFrom + delta))];
            }
            else
            {
                contigBehindAfter = contigBehindBefore;
                contigAheadAfter = contigAheadBefore;
            }

            u16 contigId = GetNewContigId(Pixel_Contig_Lookup[GetRealBufferLocation(pixelFrom)], contigAheadBefore, contigBehindBefore, contigAheadAfter, contigBehindAfter);

            ForLoop(nPixelsInRange)
            {
                buffer[GetRealBufferLocation((u32)((s32)pixelFrom + delta) + index)] = buffer[GetRealBufferLocation(pixelFrom + index)];
                Pixel_Contig_Lookup[GetRealBufferLocation((u32)((s32)pixelFrom + delta) + index)] = contigId;
            }
        }

        ForLoop(copySize)
        {
            buffer[GetRealBufferLocation(index + startCopyToRange)] = tmpBuffer[index];
            Pixel_Contig_Lookup[GetRealBufferLocation(index + startCopyToRange)] = tmpBuffer2[index];
        }

        {
            u32 contigPtr = 0;
            u32 lastPixel = 0;
            u16 lastContig = Pixel_Contig_Lookup[lastPixel];
            
            ForLoop(nPixels - 1)
            {
                u32 pixel = index + 1;
                u16 contigId = Pixel_Contig_Lookup[pixel];

                if (contigId != lastContig)
                {
                    Contig_Display_Order[contigPtr++] = lastContig;
                   
                    contig *cont = Contigs + lastContig;
                    cont->fractionalLength = (f32)(pixel - lastPixel) / (f32)nPixels;
                    
                    lastPixel = pixel;
                    lastContig = contigId;

                    if (contigPtr >= Number_of_Contigs_to_Display)
                    {
                        break;
                    }
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "Could not map pixel rearrange buffer\n");
    }

    glUnmapBuffer(GL_TEXTURE_BUFFER);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
    FreeLastPush(Working_Set); // tmpBuffer
    FreeLastPush(Working_Set); // tmpBuffer2

    return(delta);
}

global_function
u32
ToggleEditMode(GLFWwindow* window)
{
    u32 result = 1;

    if (Edit_Mode && !Edit_Pixels.editing)
    {
        Global_Mode = mode_normal;
        Mouse_Select.x = Mouse_Select.y = -1;
        if (Tool_Tip->on)
        {
            f64 mousex, mousey;
            glfwGetCursorPos(window, &mousex, &mousey);
            MouseMove(window, mousex, mousey);
        }
    }
    else if (Normal_Mode)
    {
        Global_Mode = mode_edit;
    }
    else
    {
        result = 0;
    }

    return(result);
}

global_function
u32
ToggleWaypointMode(GLFWwindow* window)
{
    u32 result = 1;

    if (Waypoint_Mode)
    {
        Global_Mode = mode_normal;
        Mouse_Select.x = Mouse_Select.y = -1;
        if (Tool_Tip->on)
        {
            f64 mousex, mousey;
            glfwGetCursorPos(window, &mousex, &mousey);
            MouseMove(window, mousex, mousey);
        }
    }
    else if (Normal_Mode)
    {
        Global_Mode = mode_waypoint;
        f64 mousex, mousey;
        glfwGetCursorPos(window, &mousex, &mousey);
        MouseMove(window, mousex, mousey);
    }
    else
    {
        result = 0;
    }

    return(result);
}

global_function
void
ToggleToolTip(GLFWwindow* window)
{
    Tool_Tip->on = !Tool_Tip->on;
    if (Tool_Tip->on)
    {
        f64 mousex, mousey;
        glfwGetCursorPos(window, &mousex, &mousey);
        MouseMove(window, mousex, mousey);
    }
}

global_variable
s32
Windowed_Xpos, Windowed_Ypos, Windowed_Width, Windowed_Height;

global_function
void
KeyBoard(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods)
{
    (void)scancode;

    if (!Loading && action != GLFW_RELEASE)
    {
        if (UI_On)
        {
            if (key == GLFW_KEY_ENTER && mods == GLFW_MOD_ALT)
            {
                if (glfwGetWindowMonitor(window))
                {
                    glfwSetWindowMonitor(window, NULL,
                            Windowed_Xpos, Windowed_Ypos,
                            Windowed_Width, Windowed_Height, 0);
                }
                else
                {
                    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                    if (monitor)
                    {
                        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                        glfwGetWindowPos(window, &Windowed_Xpos, &Windowed_Ypos);
                        glfwGetWindowSize(window, &Windowed_Width, &Windowed_Height);
                        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                    }
                }
            }
            else if (key == GLFW_KEY_ESCAPE && !mods) 
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            else if (key == GLFW_KEY_U)
            {
                UI_On = !UI_On;
                ++NK_Device->lastContextMemory[0];
            }
        }
        else
        {
            u32 keyPressed = 1;

            switch (key)
            {
                case GLFW_KEY_E:
                    keyPressed = ToggleEditMode(window);
                    break;

                case GLFW_KEY_W:
                    keyPressed = ToggleWaypointMode(window);
                    break;

                case GLFW_KEY_Q:
                    if (Edit_Mode)
                    {
                        UndoMapEdit();
                    }
                    else
                    {
                        keyPressed = 0;
                    }
                    break;

                case GLFW_KEY_T:
                    ToggleToolTip(window);
                    break;

                case GLFW_KEY_N:
                    Contig_Name_Labels->on = !Contig_Name_Labels->on;
                    break;

                case GLFW_KEY_B:
                    Scale_Bars->on = !Scale_Bars->on;
                    break;

                case GLFW_KEY_G:
                    Grid->on = !Grid->on;
                    break;

                case GLFW_KEY_S:
                    TakeScreenShot();
                    keyPressed = 0;
                    break;

                case GLFW_KEY_U:
                    UI_On = !UI_On;
                    ++NK_Device->lastContextMemory[0];
                    Mouse_Move.x = Mouse_Move.y = Mouse_Select.x = Mouse_Select.y = -1;
                    break;

                case GLFW_KEY_R:
                    if (mods == GLFW_MOD_CONTROL)
                    {
                        Loading = 1;
                    }
                    else
                    {
                        keyPressed = 0;
                    }
                    break;

                case GLFW_KEY_LEFT:
                    AdjustColorMap(-1);
                    break;

                case GLFW_KEY_RIGHT:
                    AdjustColorMap(1);
                    break;

                case GLFW_KEY_UP:
                    NextColorMap(1);
                    break;

                case GLFW_KEY_DOWN:
                    NextColorMap(-1);
                    break;

                case GLFW_KEY_ESCAPE:
                    if (!mods)
                    {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }
                    break;

                case GLFW_KEY_ENTER:
                    if (mods == GLFW_MOD_ALT)
                    {
                        if (glfwGetWindowMonitor(window))
                        {
                            glfwSetWindowMonitor(window, NULL,
                                    Windowed_Xpos, Windowed_Ypos,
                                    Windowed_Width, Windowed_Height, 0);
                        }
                        else
                        {
                            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                            if (monitor)
                            {
                                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                                glfwGetWindowPos(window, &Windowed_Xpos, &Windowed_Ypos);
                                glfwGetWindowSize(window, &Windowed_Width, &Windowed_Height);
                                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                            }
                        }
                    }
                    else
                    {
                        keyPressed = 0;
                    }
                    break;

                default:
                    keyPressed = 0;
            }

            if (keyPressed)
            {
                Redisplay = 1;
            }
        }
    }
}

global_function
void
ErrorCallback(s32 error, const char *desc)
{
    (void)error;
    fprintf(stderr, "Error: %s\n", desc);
}

// File Browser
// from nuklear file browser example
/* ===============================================================
 *
 *                          GUI
 *
 * ===============================================================*/
struct
icons
{
    struct nk_image home;
    struct nk_image computer;
    struct nk_image directory;

    struct nk_image default_file;
    struct nk_image img_file;
};

enum
file_groups
{
    FILE_GROUP_DEFAULT,
    FILE_GROUP_PRETEXT,
    FILE_GROUP_MAX
};

enum
file_types
{
    FILE_DEFAULT,
    FILE_PRETEXT,
    FILE_PSTM,
    FILE_MAX
};

struct
file_group
{
    enum file_groups group;
    u32 pad;
    const char *name;
    struct nk_image *icon;
};

struct
file
{
    enum file_types type;
    enum file_groups group;
    const char *suffix;
};

struct
media
{
    int font;
    int icon_sheet;
    struct icons icons;
    struct file_group group[FILE_GROUP_MAX];
    struct file files[FILE_MAX];
};

#define MAX_PATH_LEN 512
struct
file_browser
{
    /* path */
    char file[MAX_PATH_LEN];
    char home[MAX_PATH_LEN];
    char directory[MAX_PATH_LEN];

    /* directory content */
    char **files;
    char **directories;
    size_t file_count;
    size_t dir_count;
    struct media *media;
};

#if defined __unix__ || defined __APPLE__
#include <dirent.h>
#include <unistd.h>
#endif

#ifndef _WIN32
#include <pwd.h>
#endif

global_function
char*
StrDuplicate(const char *src)
{
    char *ret;
    size_t len = strlen(src);
    if (!len) return 0;
    ret = (char*)malloc(len+1);
    if (!ret) return 0;
    memcpy(ret, src, len);
    ret[len] = '\0';
    return ret;
}

global_function
void
DirFreeList(char **list, size_t size)
{
    size_t i;
    for (i = 0; i < size; ++i)
        free(list[i]);
    free(list);
}

global_function
char**
DirList(const char *dir, u32 return_subdirs, size_t *count)
{
    size_t n = 0;
    char buffer[MAX_PATH_LEN];
    char **results = NULL;
#ifndef _WIN32
    const DIR *none = NULL;
    DIR *z;
#else
    WIN32_FIND_DATA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    char dirBuff[MAX_PATH_LEN];
#endif
    size_t capacity = 32;
    size_t size;

    Assert(dir);
    Assert(count);
    strncpy(buffer, dir, MAX_PATH_LEN);
    n = strlen(buffer);

#ifndef _WIN32
    if (n > 0 && (buffer[n-1] != '/'))
        buffer[n++] = '/';
#else
    if (n > 0 && (buffer[n-1] != '\\'))
        buffer[n++] = '\\';
#endif

    size = 0;

#ifndef _WIN32
    z = opendir(dir);
#else
    strncpy(dirBuff, buffer, MAX_PATH_LEN);
    dirBuff[n] = '*';
    hFind = FindFirstFile(dirBuff, &ffd);
#endif
    
#ifndef _WIN32
    if (z != none)
#else
    if (hFind != INVALID_HANDLE_VALUE)
#endif
    {
#ifndef _WIN32
        u32 nonempty = 1;
        struct dirent *data = readdir(z);
        nonempty = (data != NULL);
        if (!nonempty) return NULL;
#endif
        do
        {
#ifndef _WIN32
            DIR *y;
#endif
            char *p;
            u32 is_subdir;
#ifndef _WIN32
            if (data->d_name[0] == '.')
#else
            if (ffd.cFileName[0] == '.') 
#endif
                continue;

#ifndef _WIN32
            strncpy(buffer + n, data->d_name, MAX_PATH_LEN-n);
            y = opendir(buffer);
            is_subdir = (y != NULL);
            if (y != NULL) closedir(y);
#else
            strncpy(buffer + n, ffd.cFileName, MAX_PATH_LEN-n);
            is_subdir = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#endif

            if ((return_subdirs && is_subdir) || (!is_subdir && !return_subdirs))
            {
                if (!size)
                {
                    results = (char**)calloc(sizeof(char*), capacity);
                } 
                else if (size >= capacity)
                {
                    void *old = results;
                    capacity = capacity * 2;
                    results = (char**)realloc(results, capacity * sizeof(char*));
                    Assert(results);
                    if (!results) free(old);
                }
#ifndef _WIN32
                p = StrDuplicate(data->d_name);
#else
                p = StrDuplicate(ffd.cFileName);
#endif  
                results[size++] = p;
            }
#ifndef _WIN32    
        } while ((data = readdir(z)) != NULL);
#else
        } while (FindNextFile(hFind, &ffd) != 0);
#endif
    }

#ifndef _WIN32
    if (z) closedir(z);
#else
    FindClose(hFind);
#endif
    *count = size;
    return results;
}

global_function
struct file_group
FILE_GROUP(enum file_groups group, const char *name, struct nk_image *icon)
{
    struct file_group fg;
    fg.group = group;
    fg.name = name;
    fg.icon = icon;
    return fg;
}

global_function
struct file
FILE_DEF(enum file_types type, const char *suffix, enum file_groups group)
{
    struct file fd;
    fd.type = type;
    fd.suffix = suffix;
    fd.group = group;
    return fd;
}

global_function
struct nk_image*
MediaIconForFile(struct media *media, const char *file)
{
    u32 i = 0;
    const char *s = file;
    char suffix[16];
    u32 found = 0;
    memset(suffix, 0, sizeof(suffix));

    /* extract suffix .xxx from file */
    while (*s++ != '\0')
    {
        if (found && i < (sizeof(suffix)-1))
            suffix[i++] = *s;

        if (*s == '.')
        {
            if (found)
            {
                found = 0;
                break;
            }
            found = 1;
        }
    }

    /* check for all file definition of all groups for fitting suffix*/
    for (   i = 0;
            i < FILE_MAX && found;
            ++i)
    {
        struct file *d = &media->files[i];
        {
            const char *f = d->suffix;
            s = suffix;
            while (f && *f && *s && *s == *f)
            {
                s++; f++;
            }

            /* found correct file definition so */
            if (f && *s == '\0' && *f == '\0')
                return media->group[d->group].icon;
        }
    }
    return &media->icons.default_file;
}

global_function
void
MediaInit(struct media *media)
{
    /* file groups */
    struct icons *icons = &media->icons;
    media->group[FILE_GROUP_DEFAULT] = FILE_GROUP(FILE_GROUP_DEFAULT,"default",&icons->default_file);
    media->group[FILE_GROUP_PRETEXT] = FILE_GROUP(FILE_GROUP_PRETEXT, "pretext", &icons->img_file);

    /* files */
    media->files[FILE_DEFAULT] = FILE_DEF(FILE_DEFAULT, NULL, FILE_GROUP_DEFAULT);
    media->files[FILE_PRETEXT] = FILE_DEF(FILE_PRETEXT, "pretext", FILE_GROUP_PRETEXT);
    media->files[FILE_PSTM] = FILE_DEF(FILE_PSTM, "pstm", FILE_GROUP_PRETEXT);
}

global_function
void
FileBrowserReloadDirectoryContent(struct file_browser *browser, const char *path)
{
    strncpy(browser->directory, path, MAX_PATH_LEN);
    DirFreeList(browser->files, browser->file_count);
    DirFreeList(browser->directories, browser->dir_count);
    browser->files = DirList(path, 0, &browser->file_count);
    browser->directories = DirList(path, 1, &browser->dir_count);
}

global_function
void
FileBrowserInit(struct file_browser *browser, struct media *media)
{
    memset(browser, 0, sizeof(*browser));
    browser->media = media;
    {
        /* load files and sub-directory list */
        const char *home = getenv("HOME");
#ifdef _WIN32
        if (!home) home = getenv("USERPROFILE");
#else
        if (!home) home = getpwuid(getuid())->pw_dir;
#endif
        {
            size_t l;
            strncpy(browser->home, home, MAX_PATH_LEN);
            l = strlen(browser->home);
#ifdef _WIN32
      char *sep = (char *)"\\";
#else
      char *sep = (char *)"/";
#endif
            strcpy(browser->home + l, sep);
            strcpy(browser->directory, browser->home);
        }
        
        browser->files = DirList(browser->directory, 0, &browser->file_count);
        browser->directories = DirList(browser->directory, 1, &browser->dir_count);
    }
}

global_function
u32
FileBrowserRun(struct file_browser *browser, struct nk_context *ctx, u32 show)
{
#ifndef _WIN32
    char pathSep = '/';
#else
    char pathSep = '\\';
#endif
   
    struct nk_window *window = nk_window_find(ctx, "File Browser");
    u32 doesExist = window != 0;

    if (!show && !doesExist)
    {
        return(0);
    }

    if (show && doesExist && (window->flags & NK_WINDOW_HIDDEN))
    {
        window->flags &= ~(nk_flags)NK_WINDOW_HIDDEN;
    }

    u32 ret = 0;
    struct media *media = browser->media;
    struct nk_rect total_space;

    if (nk_begin(ctx, "File Browser", nk_rect(Screen_Scale.x * 50, Screen_Scale.y * 50, Screen_Scale.x * 800, Screen_Scale.y * 600),
                NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE|NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE))
    {
        static f32 ratio[] = {0.25f, NK_UNDEFINED};
        f32 spacing_x = ctx->style.window.spacing.x;
        nk_style_set_font(ctx, &NK_Font_Browser->handle);

        /* output path directory selector in the menubar */
        ctx->style.window.spacing.x = 0;
        nk_menubar_begin(ctx);
        {
            char *d = browser->directory;
            char *begin = d + 1;
            nk_layout_row_dynamic(ctx, (s32)(Screen_Scale.y * 25.0f), 6);
            while (*d++)
            {
                if (*d == pathSep)
                {
                    *d = '\0';
                    if (nk_button_label(ctx, begin))
                    {
                        *d++ = pathSep; *d = '\0';
                        FileBrowserReloadDirectoryContent(browser, browser->directory);
                        break;
                    }
                    *d = pathSep;
                    begin = d + 1;
                }
            }
        }
        nk_menubar_end(ctx);
        ctx->style.window.spacing.x = spacing_x;

        /* window layout */
        total_space = nk_window_get_content_region(ctx);
        nk_layout_row(ctx, NK_DYNAMIC, total_space.h, 2, ratio);
        nk_group_begin(ctx, "Special", NK_WINDOW_NO_SCROLLBAR);
        {
            struct nk_image home = media->icons.home;
            struct nk_image computer = media->icons.computer;

            nk_layout_row_dynamic(ctx, (s32)(Screen_Scale.y * 40.0f), 1);
            if (nk_button_image_label(ctx, home, "home", NK_TEXT_CENTERED))
                FileBrowserReloadDirectoryContent(browser, browser->home);
            if (nk_button_image_label(ctx,computer,"computer",NK_TEXT_CENTERED))
#ifndef _WIN32
                FileBrowserReloadDirectoryContent(browser, "/");
#else
                FileBrowserReloadDirectoryContent(browser, "C:\\");
#endif
            nk_group_end(ctx);
        }

        /* output directory content window */
        nk_group_begin(ctx, "Content", 0);
        {
            s32 index = -1;
            size_t i = 0, j = 0, k = 0;
            size_t rows = 0, cols = 0;
            size_t count = browser->dir_count + browser->file_count;

            cols = 4;
            rows = count / cols;
            for (   i = 0;
                    i <= rows;
                    i += 1)
            {
                {
                    size_t n = j + cols;
                    nk_layout_row_dynamic(ctx, (s32)(Screen_Scale.y * 135.0f), (s32)cols);
                    for (   ; 
                            j < count && j < n;
                            ++j)
                    {
                        /* draw one row of icons */
                        if (j < browser->dir_count)
                        {
                            /* draw and execute directory buttons */
                            if (nk_button_image(ctx,media->icons.directory))
                                index = (s32)j;
                        } 
                        else 
                        {
                            /* draw and execute files buttons */
                            struct nk_image *icon;
                            size_t fileIndex = ((size_t)j - browser->dir_count);
                            icon = MediaIconForFile(media,browser->files[fileIndex]);
                            if (nk_button_image(ctx, *icon))
                            {
                                strncpy(browser->file, browser->directory, MAX_PATH_LEN);
                                n = strlen(browser->file);
                                strncpy(browser->file + n, browser->files[fileIndex], MAX_PATH_LEN - n);
                                ret = 1;
                            }
                        }
                    }
                }
                {
                    size_t n = k + cols;
                    nk_layout_row_dynamic(ctx, (s32)(Screen_Scale.y * 20.0f), (s32)cols);
                    for (   ;
                            k < count && k < n;
                            k++)
                    {
                        /* draw one row of labels */
                        if (k < browser->dir_count)
                        {
                            nk_label(ctx, browser->directories[k], NK_TEXT_CENTERED);
                        } 
                        else 
                        {
                            size_t t = k-browser->dir_count;
                            nk_label(ctx,browser->files[t],NK_TEXT_CENTERED);
                        }
                    }
                }
            }

            if (index != -1)
            {
                size_t n = strlen(browser->directory);
                strncpy(browser->directory + n, browser->directories[index], MAX_PATH_LEN - n);
                n = strlen(browser->directory);
                if (n < MAX_PATH_LEN - 1)
                {
                    browser->directory[n] = pathSep;
                    browser->directory[n+1] = '\0';
                }
                FileBrowserReloadDirectoryContent(browser, browser->directory);
            }
            nk_group_end(ctx);
        }

        nk_style_set_font(ctx, &NK_Font->handle);
    }
    nk_end(ctx);
    return ret;
}

global_function
void
AboutWindowRun(struct nk_context *ctx, u32 show)
{
    struct nk_window *window = nk_window_find(ctx, "About");
    u32 doesExist = window != 0;

    if (!show && !doesExist)
    {
        return;
    }

    if (show && doesExist && (window->flags & NK_WINDOW_HIDDEN))
    {
        window->flags &= ~(nk_flags)NK_WINDOW_HIDDEN;
    }

    static u32 showThirdParty = 0;

    if (nk_begin_titled(ctx, "About", PretextView_Version, nk_rect(Screen_Scale.x * 50, Screen_Scale.y * 50, Screen_Scale.x * 870, Screen_Scale.y * 610),
                NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE|NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE))
    {
        nk_menubar_begin(ctx);
        {
            nk_layout_row_dynamic(ctx, (s32)(Screen_Scale.y * 35.0f), 2);
            if (nk_button_label(ctx, "Licence"))
            {
                showThirdParty = 0;
            }
            if (nk_button_label(ctx, "Third Party Software"))
            {
                showThirdParty = 1;
            }
        }
        nk_menubar_end(ctx);

        struct nk_rect total_space = nk_window_get_content_region(ctx);
        f32 one = NK_UNDEFINED;
        nk_layout_row(ctx, NK_DYNAMIC, total_space.h, 1, &one);

        nk_group_begin(ctx, "About_Content", 0);
        {
            if (showThirdParty)
            {
                u08 text[] = R"text(PretextView was made possible thanks to the following third party libraries and
resources, click each entry to view its licence.)text";

                nk_layout_row_static(ctx, Screen_Scale.y * 60, (s32)(Screen_Scale.x * 820), 1);
                s32 len = sizeof(text);
                nk_edit_string(ctx, NK_EDIT_READ_ONLY | NK_EDIT_NO_CURSOR | NK_EDIT_SELECTABLE | NK_EDIT_MULTILINE, (char *)text, &len, len, 0);

                ForLoop(Number_of_ThirdParties)
                {
                    u32 nameIndex = 2 * index;
                    u32 licenceIndex = nameIndex + 1;
                    s32 *sizes = ThirdParty_Licence_Sizes[index];

                    if (nk_tree_push_id(NK_Context, NK_TREE_TAB, (const char *)ThirdParty[nameIndex], NK_MINIMIZED, (s32)index))
                    {
                        nk_layout_row_static(ctx, Screen_Scale.y * sizes[0], (s32)(Screen_Scale.x * sizes[1]), 1);
                        len = (s32)StringLength(ThirdParty[licenceIndex]);
                        nk_edit_string(ctx, NK_EDIT_READ_ONLY | NK_EDIT_NO_CURSOR | NK_EDIT_SELECTABLE | NK_EDIT_MULTILINE, (char *)ThirdParty[licenceIndex], &len, len, 0);
                        nk_tree_pop(NK_Context);
                    }
                }
            }
            else
            {
                nk_layout_row_static(ctx, Screen_Scale.y * 500, (s32)(Screen_Scale.x * 820), 1);
                s32 len = sizeof(Licence);
                nk_edit_string(ctx, NK_EDIT_READ_ONLY | NK_EDIT_NO_CURSOR | NK_EDIT_SELECTABLE | NK_EDIT_MULTILINE, (char *)Licence, &len, len, 0);
            }
            
            nk_group_end(ctx);
        }
    }

    nk_end(ctx);
}

global_function
struct nk_image
IconLoad(u08 *buffer, u32 bufferSize)
{
    s32 x,y,n;
    GLuint tex;
    u08 *data = stbi_load_from_memory((const u08*)buffer, (s32)bufferSize, &x, &y, &n, 0);
    if (!data)
    {
        fprintf(stderr, "Failed to load image\n");
        exit(1);
    }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return nk_image_id((s32)tex);
}
//

#ifdef DEBUG
global_function
void
GLAPIENTRY
MessageCallback( GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam )
{
    (void)source;
    (void)id;
    (void)length;
    (void)userParam;
    fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
    
    u32 BreakHere = 0;
    (void)BreakHere;
}
#endif

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

global_variable
u08 *
SaveState_Path = 0;

global_variable
u08 *
SaveState_Name = 0;

global_function
void
SetSaveStatePaths()
{
    if (!SaveState_Path)
    {
        char buff_[64];
        char *buff = (char *)buff_;
#ifndef _WIN32
        char sep = '/';
        const char *path = getenv("XDG_CONFIG_HOME");
        char *folder;
        char *folder2;
        if (path)
        {
            folder = (char *)"PretextMap";
            folder2 = 0;
        }
        else
        {
            path = getenv("HOME");
            if (!path) path = getpwuid(getuid())->pw_dir;
            folder = (char *)".config";
            folder2 = (char *)"PretextMap";
        }

        if (path)
        {
            while ((*buff++ = *path++)) {}
            *(buff - 1) = sep;
            while ((*buff++ = *folder++)) {}

            mkdir((char *)buff_, 0700);
            struct stat st = {};

            u32 goodPath = 0;

            if (!stat((char *)buff_, &st))
            {
                if (folder2)
                {
                    *(buff - 1) = sep;
                    while ((*buff++ = *folder2++)) {}

                    mkdir((char *)buff_, 0700);
                    if (!stat((char *)buff_, &st))
                    {
                        goodPath = 1;
                    }
                }
                else
                {
                    goodPath = 1;
                }
            } 
            
            if (goodPath)
            {
                u32 n = StringLength((u08 *)buff_);
                SaveState_Path = PushArray(Working_Set, u08, n + 18);
                CopyNullTerminatedString((u08 *)buff_, SaveState_Path);
                SaveState_Path[n] = (u08)sep;
                SaveState_Name = SaveState_Path + n + 1;
                SaveState_Path[n + 17] = 0;
            }
        }
#else
        // TODO
        wchar_t *path = 0;
        char sep = '\\';
        char *folder;
        HRESULT hres = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, &path);
        if (hres == S_OK)
        {

        }
        else
        {
            const char *home = getenv("HOME");
            if (!home) home = getenv("USERPROFILE");
        }
#endif
    }
}

global_variable
u08 SaveState_Magic[4] = {'p', 't', 's', 's'};

global_function
void
SaveState(u64 headerHash)
{
    if (!SaveState_Path)
    {
        SetSaveStatePaths();
    }
    
    if (SaveState_Path)
    {
        ForLoop(16)
        {
            u08 x = (u08)((headerHash >> (4 * index)) & 0xF);
            u08 c = 'a' + x;
            *(SaveState_Name + index) = c;
        }

        u32 nEdits = Min(Edits_Stack_Size, Map_Editor->nEdits);
        u32 nWayp = Waypoint_Editor->nWaypointsActive;

        u32 nFileBytes = 284 + (13 * nWayp) + (6 * nEdits) + (((2 * nEdits) + 7) >> 3);

        u08 *fileContents = PushArrayP(Loading_Arena, u08, nFileBytes);
        u08 *fileWriter = fileContents;

        // settings
        {
            u08 settings = (u08)Current_Theme;
            settings |= Contig_Name_Labels->on ? (1 << 4) : 0;
            settings |= Scale_Bars->on ? (1 << 5) : 0;
            settings |= Grid->on ? (1 << 6) : 0;
            settings |= Tool_Tip->on ? (1 << 7) : 0;

            *fileWriter++ = settings;
        }
        
        // colours
        {
            ForLoop(64)
            {
                *fileWriter++ = ((u08 *)Waypoint_Mode_Colours)[index];
            }

            ForLoop(80)
            {
                *fileWriter++ = ((u08 *)Edit_Mode_Colours)[index];
            }

            ForLoop(32)
            {
                *fileWriter++ = ((u08 *)Contig_Name_Labels)[index + 4];
            }

            ForLoop(32)
            {
                *fileWriter++ = ((u08 *)Scale_Bars)[index + 4];
            }

            ForLoop(16)
            {
                *fileWriter++ = ((u08 *)Grid)[index + 4];
            }

            ForLoop(32)
            {
                *fileWriter++ = ((u08 *)Tool_Tip)[index + 4];
            }
        }

        // colour map
        {
            u08 colourMap = (u08)Color_Maps->currMap;

            *fileWriter++ = colourMap;
        }

        // gamma
        {
            ForLoop(12)
            {
                *fileWriter++ = ((u08 *)Color_Maps->controlPoints)[index];
            }
        }

        // camera
        {
            ForLoop(12)
            {
                *fileWriter++ = ((u08 *)&Camera_Position)[index];
            }
        }

        // edits
        {
            *fileWriter++ = (u08)nEdits;
            
            u32 editStackPtr = Map_Editor->editStackPtr == nEdits ? 0 : Map_Editor->editStackPtr;
            u32 nContigFlags = ((2 * nEdits) + 7) >> 3;
            u08 *contigFlags = fileWriter + (6 * nEdits);
            memset(contigFlags, 0, nContigFlags);

            ForLoop(nEdits)
            {
                if (editStackPtr == nEdits)
                {
                    editStackPtr = 0;
                }
                
                map_edit *edit = Map_Editor->edits + editStackPtr;
                u16 x = (u16)((s32)edit->finalPixels.x - edit->delta);
                u16 y = (u16)((s32)edit->finalPixels.y - edit->delta);
                s16 d = (s16)edit->delta;

                *fileWriter++ = ((u08 *)&x)[0];
                *fileWriter++ = ((u08 *)&x)[1];
                *fileWriter++ = ((u08 *)&y)[0];
                *fileWriter++ = ((u08 *)&y)[1];
                *fileWriter++ = ((u08 *)&d)[0];
                *fileWriter++ = ((u08 *)&d)[1];

                if (AreNullTerminatedStringsEqual(edit->oldName, edit->newName, 16))
                {
                    u32 byte = (2 * index) >> 3;
                    u32 bit = (2 * index) & 7;

                    contigFlags[byte] |= (1 << bit);
                }

                if (edit->flags & editFlag_inverted)
                {
                    u32 byte = ((2 * index) + 1) >> 3;
                    u32 bit = ((2 * index) + 1) & 7;

                    contigFlags[byte] |= (1 << bit);
                }

                ++editStackPtr;
            }

            fileWriter += nContigFlags;
        }

        // waypoints
        {
            *fileWriter = (u08)nWayp;

            u32 ptr = nFileBytes;
            TraverseLinkedList(Waypoint_Editor->activeWaypoints.next, waypoint)
            {
                f32 x = node->coords.x;
                f32 y = node->coords.y;
                f32 z = node->z;
                u08 id = (u08)node->index;

                fileContents[--ptr] = id;
                fileContents[--ptr] = ((u08 *)&z)[3];
                fileContents[--ptr] = ((u08 *)&z)[2];
                fileContents[--ptr] = ((u08 *)&z)[1];
                fileContents[--ptr] = ((u08 *)&z)[0];
                fileContents[--ptr] = ((u08 *)&y)[3];
                fileContents[--ptr] = ((u08 *)&y)[2];
                fileContents[--ptr] = ((u08 *)&y)[1];
                fileContents[--ptr] = ((u08 *)&y)[0];
                fileContents[--ptr] = ((u08 *)&x)[3];
                fileContents[--ptr] = ((u08 *)&x)[2];
                fileContents[--ptr] = ((u08 *)&x)[1];
                fileContents[--ptr] = ((u08 *)&x)[0];
            }
        }
    
        u32 nBytesComp = nFileBytes + 128;
        u08 *compBuff = PushArrayP(Loading_Arena, u08, nBytesComp);
        u32 nCommpressedBytes = 0;

        if (!(nCommpressedBytes = (u32)libdeflate_deflate_compress(Compressor, (const void *)fileContents, nFileBytes, (void *)compBuff, nBytesComp)))
        {
            fprintf(stderr, "Could not compress save state file info the given buffer\n");
            exit(1);
        }

        FILE *file = fopen((const char *)SaveState_Path, "wb");
        fwrite(SaveState_Magic, 1, sizeof(SaveState_Magic), file);
        fwrite(&nCommpressedBytes, 1, 4, file);
        fwrite(&nFileBytes, 1, 4, file);
        fwrite(compBuff, 1, nCommpressedBytes, file);
        fclose(file);

        FreeLastPushP(Loading_Arena); // compBuff
        FreeLastPushP(Loading_Arena); // fileContents
    }
}

global_function
void
LoadState(u64 headerHash)
{
    if (!SaveState_Path)
    {
        SetSaveStatePaths();
    }
    
    if (SaveState_Path)
    {
        ForLoop(16)
        {
            u08 x = (u08)((headerHash >> (4 * index)) & 0xF);
            u08 c = 'a' + x;
            *(SaveState_Name + index) = c;
        }
        
        FILE *file = 0;
        if ((file = fopen((const char *)SaveState_Path, "rb")))
        {
            u08 magicTest[sizeof(SaveState_Magic)];

            u32 bytesRead = (u32)fread(magicTest, 1, sizeof(magicTest), file);
            if (bytesRead == sizeof(magicTest))
            {
                ForLoop(sizeof(SaveState_Magic))
                {
                    if (SaveState_Magic[index] != magicTest[index])
                    {
                        fclose(file);
                        file = 0;
                        break;
                    }
                }
            }
            else
            {
                fclose(file);
                file = 0;
            }
        }

        if (file)
        {
            u32 nBytesComp;
            u32 nBytesFile;
            fread(&nBytesComp, 1, 4, file);
            fread(&nBytesFile, 1, 4, file);

            u08 *fileContents = PushArrayP(Loading_Arena, u08, nBytesFile);
            u08 *compBuffer = PushArrayP(Loading_Arena, u08, nBytesComp);

            fread(compBuffer, 1, nBytesComp, file);
            if (libdeflate_deflate_decompress(Decompressor, (const void *)compBuffer, nBytesComp, (void *)fileContents, nBytesFile, NULL))
            {
                return;
            }
            FreeLastPushP(Loading_Arena); // comp buffer

            // settings
            {
                u08 settings = *fileContents++;
                
                theme th = (theme)(settings & 7);
                SetTheme(NK_Context, th);

                Contig_Name_Labels->on = settings & (1 << 4);
                Scale_Bars->on = settings & (1 << 5);
                Grid->on = settings & (1 << 6);
                Tool_Tip->on = settings & (1 << 7);
            }

            // colours
            {
                ForLoop(64)
                {
                    ((u08 *)Waypoint_Mode_Colours)[index] = *fileContents++;
                }

                ForLoop(80)
                {
                    ((u08 *)Edit_Mode_Colours)[index] = *fileContents++;
                }

                ForLoop(32)
                {
                    ((u08 *)Contig_Name_Labels)[index + 4] = *fileContents++;
                }

                ForLoop(32)
                {
                    ((u08 *)Scale_Bars)[index + 4] = *fileContents++;
                }

                ForLoop(16)
                {
                    ((u08 *)Grid)[index + 4] = *fileContents++;
                }

                ForLoop(32)
                {
                    ((u08 *)Tool_Tip)[index + 4] = *fileContents++;
                }
            }

            // colour map
            {
                u08 colourMap = *fileContents++;

                Color_Maps->currMap = (u32)colourMap;
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_BUFFER, Color_Maps->maps[Color_Maps->currMap]);
                glActiveTexture(GL_TEXTURE0);
            }

            // gamma
            {
                ForLoop(12)
                {
                    ((u08 *)Color_Maps->controlPoints)[index] = *fileContents++;
                }

                glUseProgram(Contact_Matrix->shaderProgram);
                glUniform3fv( Color_Maps->cpLocation, 1, Color_Maps->controlPoints);
            }

            // camera
            {
                ForLoop(12)
                {
                    ((u08 *)&Camera_Position)[index] = *fileContents++;
                }
            }

            // edits
            {
                u08 nEdits = *fileContents++;
                u08 *contigFlags = fileContents + (6 * nEdits);
                u32 nContigFlags = (((u32)nEdits * 2) + 7) >> 3;

                ForLoop(nEdits)
                {
                    u16 x;
                    u16 y;
                    s16 d;

                    ((u08 *)&x)[0] = *fileContents++;
                    ((u08 *)&x)[1] = *fileContents++;
                    ((u08 *)&y)[0] = *fileContents++;
                    ((u08 *)&y)[1] = *fileContents++;
                    ((u08 *)&d)[0] = *fileContents++;
                    ((u08 *)&d)[1] = *fileContents++;

                    u32 byte = (index * 2) >> 3;
                    u32 bit = (index * 2) & 7;
                    u32 sameContig  = contigFlags[byte] & (1 << bit);

                    byte = ((index * 2) + 1) >> 3;
                    bit = ((index * 2) + 1) & 7;
                    u32 invert  = contigFlags[byte] & (1 << bit);

                    pointui startPixels = {(u32)x, (u32)y};
                    s32 delta = (s32)d;
                    pointui finalPixels = {(u32)((s32)startPixels.x + delta), (u32)((s32)startPixels.y + delta)};
                    
                    u32 flags = (u32)rearrangeFlag_firstMove;
                    flags |= sameContig ? rearrangeFlag_internalContigLock : rearrangeFlag_allowNewContigCreation;

                    u32 originalContigId = Pixel_Contig_Lookup[startPixels.x];
                    u32 firstPixelOfOriginalContig = Min(startPixels.x, startPixels.y);
                    
                    while (firstPixelOfOriginalContig > 0 && Pixel_Contig_Lookup[firstPixelOfOriginalContig] == Pixel_Contig_Lookup[firstPixelOfOriginalContig - 1])
                    {
                        --firstPixelOfOriginalContig;
                    }

                    u32 name[16];
                    ForLoop2(16)
                    {
                        name[index2] = (Contigs + originalContigId)->name[index2];
                    }

                    RearrangeMap(startPixels.x, startPixels.y, delta, &flags);
                    flags &= ~(u32)rearrangeFlag_firstMove;
                    RearrangeMap(finalPixels.x, finalPixels.y, 0, &flags);
                    
                    if (invert) InvertMap(finalPixels.x, finalPixels.y);

                    flags = flags & rearrangeFlag_wasWholeContig ? editFlag_wasWholeContig : 0;
                    flags |= invert ? editFlag_inverted : 0;

                    AddMapEdit(delta, finalPixels, (u16)originalContigId, firstPixelOfOriginalContig, (u16)flags, name);
                }

                fileContents += nContigFlags;
            }

            // waypoints
            {
                u08 nWayp = *fileContents++;

                ForLoop(nWayp)
                {
                    f32 x;
                    f32 y;
                    f32 z;

                    ((u08 *)&x)[0] = *fileContents++;
                    ((u08 *)&x)[1] = *fileContents++;
                    ((u08 *)&x)[2] = *fileContents++;
                    ((u08 *)&x)[3] = *fileContents++;
                    ((u08 *)&y)[0] = *fileContents++;
                    ((u08 *)&y)[1] = *fileContents++;
                    ((u08 *)&y)[2] = *fileContents++;
                    ((u08 *)&y)[3] = *fileContents++;
                    ((u08 *)&z)[0] = *fileContents++;
                    ((u08 *)&z)[1] = *fileContents++;
                    ((u08 *)&z)[2] = *fileContents++;
                    ((u08 *)&z)[3] = *fileContents++;
                    u08 id = *fileContents++;
                    
                    AddWayPoint({x, y});
                    Waypoint_Editor->activeWaypoints.next->z = z;
                    Waypoint_Editor->activeWaypoints.next->index = (u32)id;
                }
            }

            Redisplay = 1;
            FreeLastPushP(Loading_Arena); // fileContents
        }
    }
}

MainArgs
{
    u32 initWithFile = 0;
    u08 currFile[256];
    u08 *currFileName = 0;
    u64 headerHash = 0;
    if (ArgCount >= 2)
    {
        initWithFile = 1;
        CopyNullTerminatedString((u08 *)ArgBuffer[1], currFile);
    }

    Mouse_Move.x = -1.0;
    Mouse_Move.y = -1.0;

    Tool_Tip_Move.pixels.x = 0;
    Tool_Tip_Move.pixels.y = 0;
    Tool_Tip_Move.worldCoords.x = 0;
    Tool_Tip_Move.worldCoords.y = 0;

    Mouse_Select.x = -1.0;
    Mouse_Select.y = -1.0;

    Edit_Pixels.pixels.x = 0;
    Edit_Pixels.pixels.y = 0;
    Edit_Pixels.worldCoords.x = 0;
    Edit_Pixels.worldCoords.y = 0;
    Edit_Pixels.editing = 0;

    Camera_Position.x = 0.0f;
    Camera_Position.y = 0.0f;
    Camera_Position.z = 1.0f;

    CreateMemoryArena(Working_Set, MegaByte(256));
    Thread_Pool = ThreadPoolInit(&Working_Set, 2); 

    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit())
    {
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 8);

    GLFWwindow* window = glfwCreateWindow(1080, 1080, "PretextView", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    NK_Context = PushStruct(Working_Set, nk_context);
    glfwSetWindowUserPointer(window, NK_Context);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(window, GLFWChangeFrameBufferSize);
    glfwSetWindowSizeCallback(window, GLFWChangeWindowSize);
    glfwSetCursorPosCallback(window, MouseMove);
    glfwSetMouseButtonCallback(window, Mouse);
    glfwSetScrollCallback(window, Scroll);
    glfwSetKeyCallback(window, KeyBoard);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_MULTISAMPLE);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef DEBUG
    glEnable              ( GL_DEBUG_OUTPUT );
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback( MessageCallback, 0 );
#endif

    s32 width, height, display_width, display_height;
    glfwGetWindowSize(window, &width, &height);
    glfwGetFramebufferSize(window, &display_width, &display_height);
    Screen_Scale.x = (f32)display_width/(f32)width;
    Screen_Scale.y = (f32)display_height/(f32)height;

    Setup();
    if (initWithFile)
    {
        UI_On = LoadFile((const char *)currFile, Loading_Arena, (char **)&currFileName, &headerHash) == ok ? 0 : 1;
    }
    else
    {
        UI_On = 1;
    }

    // file browser
    struct file_browser browser;
    struct media media;
    {
        media.icons.home = IconLoad(IconHome, IconHome_Size);
        media.icons.directory = IconLoad(IconFolder, IconFolder_Size);
        media.icons.computer = IconLoad(IconDrive, IconDrive_Size);
        media.icons.default_file = IconLoad(IconFile, IconFile_Size);
        media.icons.img_file = IconLoad(IconImage, IconImage_Size);
        MediaInit(&media);
        FileBrowserInit(&browser, &media);
    }
    
    {
        f64 mousex, mousey;
        glfwGetCursorPos(window, &mousex, &mousey);
        MouseMove(window, mousex, mousey);
    }
    
    Redisplay = 1;
    while (!glfwWindowShouldClose(window))
    {
        if (Redisplay)
        {
            Render();
            glfwSwapBuffers(window);
            Redisplay = 0;
        }

        if (Loading)
        {
            if (currFileName) SaveState(headerHash);
            LoadFile((const char *)currFile, Loading_Arena, (char **)&currFileName, &headerHash);
            glfwPollEvents();
            Loading = 0;
            Redisplay = 1;
        }

        if (UI_On)
        {
            f64 x, y;
            nk_input_begin(NK_Context);
            glfwPollEvents();
#if 0
            nk_input_key(NK_Context, NK_KEY_DEL, glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS);
            nk_input_key(NK_Context, NK_KEY_ENTER, glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS);
            nk_input_key(NK_Context, NK_KEY_TAB, glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS);
            nk_input_key(NK_Context, NK_KEY_BACKSPACE, glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS);
            nk_input_key(NK_Context, NK_KEY_LEFT, glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS);
            nk_input_key(NK_Context, NK_KEY_RIGHT, glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS);
            nk_input_key(NK_Context, NK_KEY_UP, glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS);
            nk_input_key(NK_Context, NK_KEY_DOWN, glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS);

            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
            {
                nk_input_key(NK_Context, NK_KEY_COPY, glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS);
                nk_input_key(NK_Context, NK_KEY_PASTE, glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS);
                nk_input_key(NK_Context, NK_KEY_CUT, glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS);
                nk_input_key(NK_Context, NK_KEY_CUT, glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);
                nk_input_key(NK_Context, NK_KEY_SHIFT, 1);
            } 
            else
            {
                nk_input_key(NK_Context, NK_KEY_COPY, 0);
                nk_input_key(NK_Context, NK_KEY_PASTE, 0);
                nk_input_key(NK_Context, NK_KEY_CUT, 0);
                nk_input_key(NK_Context, NK_KEY_SHIFT, 0);
            }
#endif
            nk_input_scroll(NK_Context, NK_Scroll);
            NK_Scroll.x = 0;
            NK_Scroll.y = 0;

            glfwGetCursorPos(window, &x, &y);
            x *= (f64)Screen_Scale.x;
            y *= (f64)Screen_Scale.y;

            nk_input_motion(NK_Context, (s32)x, (s32)y);
            nk_input_button(NK_Context, NK_BUTTON_LEFT, (s32)x, (s32)y, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
            nk_input_button(NK_Context, NK_BUTTON_MIDDLE, (s32)x, (s32)y, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
            nk_input_button(NK_Context, NK_BUTTON_RIGHT, (s32)x, (s32)y, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
            nk_input_end(NK_Context);

            s32 showFileBrowser = 0;
            s32 showAboutScreen = 0;
            static u32 currGroup1 = 0;
            static u32 currGroup2 = 0;
            static s32 currSelected1 = -1;
            static s32 currSelected2 = -1;
            {
                static nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCROLL_AUTO_HIDE | NK_WINDOW_TITLE;

                if (nk_begin_titled(NK_Context, "Options", currFileName ? (const char *)currFileName : "", nk_rect(Screen_Scale.x * 10, Screen_Scale.y * 10, Screen_Scale.x * 700, Screen_Scale.y * 350), flags))
                {
                    struct nk_rect bounds = nk_window_get_bounds(NK_Context);
                    bounds.w /= 8;
                    bounds.h /= 8;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 400), bounds))
                    { 
                        nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                        theme curr = Current_Theme;
                        ForLoop(THEME_COUNT)
                        {
                            curr = nk_option_label(NK_Context, (const char *)Theme_Name[index], curr == (theme)index) ? (theme)index : curr;
                        }
                        if (curr != Current_Theme)
                        {
                            SetTheme(NK_Context, curr);
                        }

                        nk_contextual_end(NK_Context); 
                    } 

                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                    showFileBrowser = nk_button_label(NK_Context, "Load File");
                    showAboutScreen = nk_button_label(NK_Context, "About");

                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                    bounds = nk_widget_bounds(NK_Context);
                    if ((nk_option_label(NK_Context, "Waypoint Mode", Global_Mode == mode_waypoint) ? 1 : 0) != (Global_Mode == mode_waypoint ? 1 : 0)) Global_Mode = Global_Mode == mode_waypoint ? mode_normal : mode_waypoint;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 750, Screen_Scale.y * 400), bounds))
                    {
                        struct nk_colorf colour_text = Waypoint_Mode_Colours->text;
                        struct nk_colorf colour_bg = Waypoint_Mode_Colours->bg;
                        struct nk_colorf colour_base = Waypoint_Mode_Colours->base;
                        struct nk_colorf colour_select = Waypoint_Mode_Colours->selected;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Waypoint Mode Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 4);
                        nk_label(NK_Context, "Base", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Selected", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Text", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Background", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 4);
                        colour_base = nk_color_picker(NK_Context, colour_base, NK_RGBA);
                        colour_select = nk_color_picker(NK_Context, colour_select, NK_RGBA);
                        colour_text = nk_color_picker(NK_Context, colour_text, NK_RGBA);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 4);
                        if (nk_button_label(NK_Context, "Default")) colour_base = Red_Full;
                        if (nk_button_label(NK_Context, "Default")) colour_select = Blue_Full;
                        if (nk_button_label(NK_Context, "Default")) colour_text = Yellow_Text_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Waypoint_Mode_Colours->text = colour_text;
                        Waypoint_Mode_Colours->bg = colour_bg;
                        Waypoint_Mode_Colours->selected = colour_select;
                        Waypoint_Mode_Colours->base = colour_base;

                        nk_contextual_end(NK_Context);
                    }

                    bounds = nk_widget_bounds(NK_Context);
                    if ((nk_option_label(NK_Context, "Edit Mode", Global_Mode == mode_edit) ? 1 : 0) != (Global_Mode == mode_edit ? 1 : 0)) Global_Mode = Global_Mode == mode_edit ? mode_normal : mode_edit;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 750, Screen_Scale.y * 400), bounds))
                    {
                        struct nk_colorf colour_text = Edit_Mode_Colours->fg;
                        struct nk_colorf colour_bg = Edit_Mode_Colours->bg;
                        struct nk_colorf colour_preSelect = Edit_Mode_Colours->preSelect;
                        struct nk_colorf colour_select = Edit_Mode_Colours->select;
                        struct nk_colorf colour_invSelect = Edit_Mode_Colours->invSelect;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Edit Mode Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 5);
                        nk_label(NK_Context, "Select", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Edit", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Invert", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Text", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Background", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 5);
                        colour_preSelect = nk_color_picker(NK_Context, colour_preSelect, NK_RGBA);
                        colour_select = nk_color_picker(NK_Context, colour_select, NK_RGBA);
                        colour_invSelect = nk_color_picker(NK_Context, colour_invSelect, NK_RGBA);
                        colour_text = nk_color_picker(NK_Context, colour_text, NK_RGBA);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 5);
                        if (nk_button_label(NK_Context, "Default")) colour_preSelect = Green_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_select = Blue_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_invSelect = Red_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_text = Yellow_Text_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Edit_Mode_Colours->fg = colour_text;
                        Edit_Mode_Colours->bg = colour_bg;
                        Edit_Mode_Colours->preSelect = colour_preSelect;
                        Edit_Mode_Colours->select = colour_select;
                        Edit_Mode_Colours->invSelect = colour_invSelect;

                        nk_contextual_end(NK_Context);
                    }

                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 1);
                    bounds = nk_widget_bounds(NK_Context);
                    Contig_Name_Labels->on = nk_check_label(NK_Context, "Contig Name Labels", (s32)Contig_Name_Labels->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 400), bounds))
                    {
                        struct nk_colorf colour_text = Contig_Name_Labels->fg;
                        struct nk_colorf colour_bg = Contig_Name_Labels->bg;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Contig Name Label Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 2);
                        nk_label(NK_Context, "Text", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Background", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 2);
                        colour_text = nk_color_picker(NK_Context, colour_text, NK_RGBA);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 2);
                        if (nk_button_label(NK_Context, "Default")) colour_text = Yellow_Text_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Contig_Name_Labels->fg = colour_text;
                        Contig_Name_Labels->bg = colour_bg;

                        nk_contextual_end(NK_Context);
                    }

                    bounds = nk_widget_bounds(NK_Context);
                    Scale_Bars->on = nk_check_label(NK_Context, "Scale Bars", (s32)Scale_Bars->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 400), bounds))
                    {
                        struct nk_colorf colour_text = Scale_Bars->fg;
                        struct nk_colorf colour_bg = Scale_Bars->bg;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Scale Bar Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 2);
                        nk_label(NK_Context, "Text", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Background", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 2);
                        colour_text = nk_color_picker(NK_Context, colour_text, NK_RGBA);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 2);
                        if (nk_button_label(NK_Context, "Default")) colour_text = Red_Text_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Scale_Bars->fg = colour_text;
                        Scale_Bars->bg = colour_bg;

                        nk_contextual_end(NK_Context);
                    }

                    bounds = nk_widget_bounds(NK_Context);
                    Grid->on = nk_check_label(NK_Context, "Grid", (s32)Grid->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 150, Screen_Scale.y * 400), bounds))
                    {
                        struct nk_colorf colour_bg = Grid->bg;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Grid Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 1);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Grid->bg = colour_bg;

                        nk_contextual_end(NK_Context);
                    }

#ifdef DEBUG
                    bounds = nk_widget_bounds(NK_Context);
                    Tiles->on = nk_check_label(NK_Context, "Tiles", (s32)Tiles->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 150, Screen_Scale.y * 400), bounds))
                    {
                        struct nk_colorf colour_bg = Tiles->bg;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Tiles Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 1);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Tiles->bg = colour_bg;

                        nk_contextual_end(NK_Context);
                    }
#endif

                    bounds = nk_widget_bounds(NK_Context);
                    Tool_Tip->on = nk_check_label(NK_Context, "Tool Tip", (s32)Tool_Tip->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 400), bounds))
                    {
                        struct nk_colorf colour_text = Tool_Tip->fg;
                        struct nk_colorf colour_bg = Tool_Tip->bg;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Tool Tip Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 2);
                        nk_label(NK_Context, "Text", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Background", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 2);
                        colour_text = nk_color_picker(NK_Context, colour_text, NK_RGBA);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 2);
                        if (nk_button_label(NK_Context, "Default")) colour_text = Yellow_Text_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Tool_Tip->fg = colour_text;
                        Tool_Tip->bg = colour_bg;

                        nk_contextual_end(NK_Context);
                    }

                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                    nk_label(NK_Context, "Gamma Min", NK_TEXT_LEFT);
                    s32 slider1 = nk_slider_float(NK_Context, 0, Color_Maps->controlPoints, 1.0f, 0.001f);
                    if (slider1)
                    {
                        Color_Maps->controlPoints[0] = Min(Color_Maps->controlPoints[0], Color_Maps->controlPoints[2]);
                    }

                    nk_label(NK_Context, "Gamma Mid", NK_TEXT_LEFT);
                    s32 slider2 = nk_slider_float(NK_Context, 0, Color_Maps->controlPoints + 1, 1.0f, 0.001f);

                    nk_label(NK_Context, "Gamma Max", NK_TEXT_LEFT);
                    s32 slider3 = nk_slider_float(NK_Context, 0, Color_Maps->controlPoints + 2, 1.0f, 0.001f);
                    if (slider3)
                    {
                        Color_Maps->controlPoints[2] = Max(Color_Maps->controlPoints[2], Color_Maps->controlPoints[0]);
                    }

                    if (slider1 || slider2 || slider3)
                    {
                        Color_Maps->controlPoints[1] = Min(Max(Color_Maps->controlPoints[1], Color_Maps->controlPoints[0]), Color_Maps->controlPoints[2]);
                        glUseProgram(Contact_Matrix->shaderProgram);
                        glUniform3fv( Color_Maps->cpLocation, 1, Color_Maps->controlPoints);
                    }

                    if (nk_tree_push(NK_Context, NK_TREE_TAB, "Colour Maps", NK_MINIMIZED))
                    {
                        nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                        u32 currMap = Color_Maps->currMap;
                        ForLoop(Color_Maps->nMaps)
                        {
                            currMap = nk_option_label(NK_Context, Color_Map_Names[index], currMap == index) ? index : currMap;
                            nk_image(NK_Context, Color_Maps->mapPreviews[index]);
                        }

                        if (currMap != Color_Maps->currMap)
                        {
                            Color_Maps->currMap = currMap;
                            glActiveTexture(GL_TEXTURE1);
                            glBindTexture(GL_TEXTURE_BUFFER, Color_Maps->maps[Color_Maps->currMap]);
                            glActiveTexture(GL_TEXTURE0);
                        }

                        nk_tree_pop(NK_Context);
                    }

                    if (File_Loaded)
                    {
                        {
                            u32 NPerGroup = Min(Number_of_Contigs_to_Display, 10);

                            s32 prevSelected1 = currSelected1;
                            s32 prevSelected2 = currSelected2;

                            nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 1);
                            if (nk_tree_push(NK_Context, NK_TREE_TAB, "Go To:", NK_MINIMIZED))
                            {
                                static f32 ratio[] = {0.4f, 0.4f};
                                nk_layout_row(NK_Context, NK_DYNAMIC, (s32)(Screen_Scale.y * 400.0f), 2, ratio);

                                if (nk_group_begin(NK_Context, "Select1", 0))
                                {
                                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 1);
                                    for (   u32 index = currGroup1;
                                            index < (currGroup1 + NPerGroup) && index < Number_of_Contigs_to_Display;
                                            ++index )
                                    {
                                        contig *cont = Contigs + Contig_Display_Order[index];

                                        currSelected1 = nk_option_label(NK_Context, (const char *)cont->name, currSelected1 == (s32)index) ? (s32)index : currSelected1;
                                    }
                                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                                    if (nk_button_label(NK_Context, "Prev"))
                                    {
                                        currGroup1 = currGroup1 >= NPerGroup ? (currGroup1 - NPerGroup) : (Number_of_Contigs_to_Display - NPerGroup);
                                    }
                                    if (nk_button_label(NK_Context, "Next"))
                                    {
                                        currGroup1 = currGroup1 < (Number_of_Contigs_to_Display - NPerGroup) ? (currGroup1 + NPerGroup) : 0;
                                    }
                                    nk_group_end(NK_Context);
                                }

                                if (nk_group_begin(NK_Context, "Select2", 0))
                                {
                                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 1);
                                    for (   u32 index = currGroup2;
                                            index < (currGroup2 + NPerGroup) && index < Number_of_Contigs_to_Display;
                                            ++index )
                                    {
                                        contig *cont = Contigs + Contig_Display_Order[index];

                                        currSelected2 = nk_option_label(NK_Context, (const char *)cont->name, currSelected2 == (s32)index) ? (s32)index : currSelected2;
                                    }
                                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                                    if (nk_button_label(NK_Context, "Prev"))
                                    {
                                        currGroup2 = currGroup2 >= NPerGroup ? (currGroup2 - NPerGroup) : (Number_of_Contigs_to_Display - NPerGroup);
                                    }
                                    if (nk_button_label(NK_Context, "Next"))
                                    {
                                        currGroup2 = currGroup2 < (Number_of_Contigs_to_Display - NPerGroup) ? (currGroup2 + NPerGroup) : 0;
                                    }
                                    nk_group_end(NK_Context);
                                }

                                nk_tree_pop(NK_Context);
                            }

                            if ((prevSelected1 != currSelected1 || prevSelected2 != currSelected2) && currSelected1 >= 0 && currSelected2 >= 0)
                            {
                                f32 fracx1 = 0.0f;
                                f32 fracx2 = 0.0f;
                                f32 fracy1 = 0.0f;
                                f32 fracy2 = 0.0f;

                                contig *cont;
                                u32 index = 0;
                                for (   ;
                                        index < (u32)currSelected1;
                                        ++index )
                                {
                                    cont = Contigs + Contig_Display_Order[index];
                                    fracx1 += cont->fractionalLength;
                                }
                                cont = Contigs + Contig_Display_Order[index];
                                fracx2 = fracx1 + cont->fractionalLength;

                                index = 0;
                                for (   ;
                                        index < (u32)currSelected2;
                                        ++index )
                                {
                                    cont = Contigs + Contig_Display_Order[index];
                                    fracy1 += cont->fractionalLength;
                                }
                                cont = Contigs + Contig_Display_Order[index];
                                fracy2 = fracy1 + cont->fractionalLength;

                                f32 x0 = fracx1 - 0.5f;
                                f32 x1 = fracx2 - 0.5f;
                                f32 y0 = 0.5f - fracy1;
                                f32 y1 = 0.5f - fracy2;

                                f32 rx = x1 - x0;
                                f32 ry = y0 - y1;
                                f32 range = Max(rx, ry);

                                Camera_Position.x = 0.5f * (x0 + x1);
                                Camera_Position.y = 0.5f * (y0 + y1);
                                Camera_Position.z = 1.0f / range;
                                ClampCamera();
                            }
                        }

                        {
                            nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 1);
                            if (nk_tree_push(NK_Context, NK_TREE_TAB, "Edits", NK_MINIMIZED))
                            {
                                u32 nEdits = Min(Edits_Stack_Size, Map_Editor->nEdits);
                                u32 editStackPtr = Map_Editor->editStackPtr == nEdits ? 0 : Map_Editor->editStackPtr;

                                f64 bpPerPixel = (f64)Total_Genome_Length / (f64)(Number_of_Textures_1D * Texture_Resolution);

                                ForLoop(nEdits)
                                {
                                    if (editStackPtr == nEdits)
                                    {
                                        editStackPtr = 0;
                                    }

                                    map_edit *edit = Map_Editor->edits + editStackPtr;

                                    char buff[64];
                                    u32 newPosition = Min(edit->finalPixels.x, edit->finalPixels.y) - edit->newContigStartPixel;

                                    stbsp_snprintf((char *)buff, 64, "Edit %d:", index + 1);
                                    nk_label(NK_Context, (const char *)buff, NK_TEXT_LEFT);

                                    if (edit->flags & editFlag_wasWholeContig)
                                    {
                                        stbsp_snprintf((char *)buff, 64, "       %s - full", (char *)edit->oldName);
                                        nk_label(NK_Context, (const char *)buff, NK_TEXT_LEFT);

                                        nk_label(NK_Context, edit->flags & editFlag_inverted ? (const char *)"       inverted and moved to" : 
                                                (const char *)"       moved to", NK_TEXT_LEFT);
                                    }
                                    else
                                    {
                                        u32 oldFrom = (u32)((s32)Min(edit->finalPixels.x, edit->finalPixels.y) - edit->delta) - edit->oldContigStartPixel;
                                        u32 oldTo = (u32)((s32)Max(edit->finalPixels.x, edit->finalPixels.y) - edit->delta) - edit->oldContigStartPixel;

                                        stbsp_snprintf((char *)buff, 64, "       %s - %$.2fbp to %$.2fbp",
                                                (char *)edit->oldName, (f64)oldFrom * bpPerPixel,
                                                (f64)oldTo * bpPerPixel);
                                        nk_label(NK_Context, (const char *)buff, NK_TEXT_LEFT);

                                        nk_label(NK_Context, edit->flags & editFlag_inverted ? (const char *)"       inverted and moved to" : 
                                                (const char *)"       moved to", NK_TEXT_LEFT);
                                    }

                                    stbsp_snprintf((char *)buff, 64, "       %s - %$.2fbp",
                                            (char *)edit->newName, (f64)newPosition * bpPerPixel);
                                    nk_label(NK_Context, (const char *)buff, NK_TEXT_LEFT);

                                    ++editStackPtr;
                                }

                                if (nEdits && nk_button_label(NK_Context, "Undo")) UndoMapEdit();

                                nk_tree_pop(NK_Context);
                            }
                        }
                        
                        {
                            nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 1);
                            if (nk_tree_push(NK_Context, NK_TREE_TAB, "Waypoints", NK_MINIMIZED))
                            {
                                nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                                char buff[4];

                                TraverseLinkedList(Waypoint_Editor->activeWaypoints.next, waypoint)
                                {
                                    stbsp_snprintf((char *)buff, sizeof(buff), "%d", node->index + 1);
                                    if (nk_button_label(NK_Context, (char *)buff))
                                    {
                                        Camera_Position.x = node->coords.x;
                                        Camera_Position.y = -node->coords.y;
                                        Camera_Position.z = node->z;
                                        ClampCamera();
                                    }
                                    
                                    if (nk_button_label(NK_Context, "Remove"))
                                    {
                                        waypoint *tmp = node->prev;
                                        RemoveWayPoint(node);
                                        node = tmp;
                                    }
                                }

                                nk_tree_pop(NK_Context);
                            }
                        }
                    }

                    nk_end(NK_Context);
                }

                if (FileBrowserRun(&browser, NK_Context, (u32)showFileBrowser))
                {
                    if (!File_Loaded || !AreNullTerminatedStringsEqual(currFile, (u08 *)browser.file))
                    {
                        if (FILE *test = TestFile(browser.file))
                        {
                            fclose(test);
                            CopyNullTerminatedString((u08 *)browser.file, currFile);
                            Loading = 1;
                            currGroup1 = 0;
                            currGroup2 = 0;
                            currSelected1 = -1;
                            currSelected2 = -1;
                        }
                    }
                }

                AboutWindowRun(NK_Context, (u32)showAboutScreen);

                void *cmds = nk_buffer_memory(&NK_Context->memory);
                if (memcmp(cmds, NK_Device->lastContextMemory, NK_Context->memory.allocated))
                {
                    memcpy(NK_Device->lastContextMemory, cmds, NK_Context->memory.allocated);
                    Redisplay = 1;
                }
                else
                {
                    nk_clear(NK_Context);
                }
            }
        }
        else
        {
            glfwWaitEvents();
        }
    }

    SaveState(headerHash);

    ThreadPoolWait(Thread_Pool);
    ThreadPoolDestroy(Thread_Pool);
    glfonsDelete(FontStash_Context);
    nk_font_atlas_clear(NK_Atlas);
    nk_free(NK_Context);
    nk_buffer_free(&NK_Device->cmds);
    glfwDestroyWindow(window);
    glfwTerminate();

    EndMain;
}
