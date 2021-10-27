/*
Copyright (c) 2021 Ed Harry, Wellcome Sanger Institute

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

#define String_(x) #x
#define String(x) String_(x)

#define PretextView_Version "PretextView Version " String(PV) 

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
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
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
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#define NK_PRIVATE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_STANDARD_IO
#define NK_ZERO_COMMAND_MEMORY
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_INCLUDE_STANDARD_VARARGS
//#define NK_KEYSTATE_BASED_INPUT
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
    float BiLinear( vec3 inCoord, vec2 texSize, float lod )
    {
        vec2 texelSize = 1.0 / texSize;

        vec3 coord = pixLookup(inCoord);
        
        float p0q0 = textureLod(tex, coord, lod).r;
        float p1q0 = textureLod(tex, pixLookup(inCoord + vec3(texelSize.x, 0, 0)), lod).r;

        float p0q1 = textureLod(tex, pixLookup(inCoord + vec3(0, texelSize.y, 0)), lod).r;
        float p1q1 = textureLod(tex, pixLookup(inCoord + vec3(texelSize, 0)), lod).r;

        float a = fract( inCoord.x * texSize.x ); // Get Interpolation factor for X direction.
                        // Fraction near to valid data.
	float b = fract( inCoord.y * texSize.y );// Get Interpolation factor for Y direction.
        
	float pInterp_q0 = mix( p0q0, p1q0, a ); // Interpolates top row in X direction.
        float pInterp_q1 = mix( p0q1, p1q1, a ); // Interpolates bottom row in X direction.

        return mix( pInterp_q0, pInterp_q1, b ); // Interpolate in Y direction.
    }
    void main()
    {
        float mml = mip_map_level(Texcoord.xy * textureSize(tex, 0).xy);
        
        float floormml = floor(mml);

        float f1 = BiLinear(Texcoord, textureSize(tex, 0).xy, floormml);
	float f2 = BiLinear(Texcoord, textureSize(tex, 0).xy, floormml + 1);

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

global_variable
const
GLchar *
VertexSource_EditablePlot = R"glsl(
    #version 330
    in float position;
    uniform usamplerBuffer pixrearrangelookup;
    uniform samplerBuffer yvalues;
    uniform float ytop;
    uniform float yscale;
    void main()
    {
        float x = position;
        float realx = texelFetch(pixrearrangelookup, int(x)).x;
        x /= textureSize(pixrearrangelookup);
        x -= 0.5;
        float y = texelFetch(yvalues, int(realx)).x;
        y *= yscale;
        y += ytop;

        gl_Position = vec4(x, y, 0.0, 1.0);
    }
)glsl";

global_variable
const
GLchar *
FragmentSource_EditablePlot = R"glsl(
    #version 330
    out vec4 outColor;
    uniform vec4 color;
    void main()
    {
        outColor = color;
    }
)glsl";

// https://blog.tammearu.eu/posts/gllines/
global_variable
const
GLchar *
GeometrySource_EditablePlot = R"glsl(
    #version 330
    layout (lines) in;
    layout (triangle_strip, max_vertices = 4) out;

    uniform mat4 matrix;
    uniform float linewidth;

    void main()
    {
        vec3 start = gl_in[0].gl_Position.xyz;
        vec3 end = gl_in[1].gl_Position.xyz;
        vec3 lhs = cross(normalize(end-start), vec3(0.0, 0.0, -1.0));

        lhs *= linewidth*0.0007;

        gl_Position = matrix * vec4(start+lhs, 1.0);
        EmitVertex();
        gl_Position = matrix * vec4(start-lhs, 1.0);
        EmitVertex();
        gl_Position = matrix * vec4(end+lhs, 1.0);
        EmitVertex();
        gl_Position = matrix * vec4(end-lhs, 1.0);
        EmitVertex();
        EndPrimitive();
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
editable_plot_shader
{
    GLuint shaderProgram;
    GLuint yValuesBuffer;
    GLuint yValuesBufferTex;
    GLint matLocation;
    GLint colorLocation;
    GLint yScaleLocation;
    GLint yTopLocation;
    GLint lineSizeLocation;
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

#ifdef Internal
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
Contig_ColourBar_Data;

global_variable
quad_data *
Scaff_Bar_Data;

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
Number_of_Pixels_1D;

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
    u32 selecting;
    pointui selectPixels;
    u32 snap;
};

global_variable
edit_pixels
Edit_Pixels;

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
    f32 size;
};

#define Grey_Background {0.569f, 0.549f, 0.451f, 1.0f}
#define Yellow_Text_Float {0.941176471f, 0.725490196f, 0.058823529f, 1.0f}
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

global_variable
u32
Waypoints_Always_Visible = 1;

global_variable
u08
Scaffs_Always_Visible = 1;

struct
ui_colour_element
{
    u32 on;
    nk_colorf bg;
    f32 size;
};

global_variable
ui_colour_element *
Grid;

global_variable
ui_colour_element *
Contig_Ids;

#ifdef Internal
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
waypoint_mode_data
{
    nk_colorf base;
    nk_colorf selected;
    nk_colorf text;
    nk_colorf bg;
    f32 size;
};

global_variable
waypoint_mode_data *
Waypoint_Mode_Data;

struct
scaff_mode_data
{
    nk_colorf text;
    nk_colorf bg;
    f32 size;
};

global_variable
scaff_mode_data *
Scaff_Mode_Data;

global_variable
u32
UI_On = 0;

enum
global_mode
{
    mode_normal = 0,
    mode_edit = 1,
    mode_waypoint_edit = 2,
    mode_scaff_edit = 3
};

global_variable
global_mode
Global_Mode = mode_normal;

#define Edit_Mode (Global_Mode == mode_edit)
#define Normal_Mode (Global_Mode == mode_normal)
#define Waypoint_Edit_Mode (Global_Mode == mode_waypoint_edit)
#define Scaff_Edit_Mode (Global_Mode == mode_scaff_edit)

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

global_function
s32
RearrangeMap(u32 pixelFrom, u32 pixelTo, s32 delta, u32 snap = 0);

global_function
void
InvertMap(u32 pixelFrom, u32 pixelTo);

global_variable
u32
Global_Edit_Invert_Flag = 0;

global_variable
u08
Scaff_Painting_Flag = 0;

global_variable
u16
Scaff_Painting_Id = 0;

struct
original_contig
{
    u32 name[16];
    u16 *contigMapPixels;
    u32 nContigs;
    u32 pad;
};

global_variable
original_contig *
Original_Contigs;

global_variable
u32
Number_of_Original_Contigs;

struct
contig
{
    u16 originalContigId;
    u16 length;
    u16 startCoord;
    u16 scaffId;
};

struct
contigs
{
    u08 *contigInvertFlags;
    contig *contigs;
    u32 numberOfContigs;
    u32 pad;
};

global_variable
contigs *
Contigs;

global_function
u08
IsContigInverted(u32 index)
{
    return(Contigs->contigInvertFlags[index >> 3] & (1 << (index & 7)));
}

#define Max_Number_of_Contigs 4096

struct
map_state
{
    u16 *contigIds;
    u16 *originalContigIds;
    u16 *contigRelCoords;
    u16 *scaffIds;
};

global_variable
map_state *
Map_State;

global_function
void
UpdateContigsFromMapState()
{
    u16 lastScaffID = Map_State->scaffIds[0];
    u16 scaffId = lastScaffID ? 1 : 0;
    u16 lastId = Map_State->originalContigIds[0];
    u16 lastCoord = Map_State->contigRelCoords[0];
    u32 contigPtr = 0;
    u16 length = 0;
    u16 startCoord = lastCoord;
    u08 inverted = Map_State->contigRelCoords[1] < lastCoord;
    Map_State->contigIds[0] = 0;
    
    u32 pixelIdx = 0;
    ForLoop(Number_of_Original_Contigs) (Original_Contigs + index)->nContigs = 0;
    ForLoop(Number_of_Pixels_1D - 1)
    {
        if (contigPtr == Max_Number_of_Contigs) break;

        ++length;

        pixelIdx = index + 1;
        u16 id = Map_State->originalContigIds[pixelIdx];
        u16 coord = Map_State->contigRelCoords[pixelIdx];

        if (id != lastId || (inverted && coord != (lastCoord - 1)) || (!inverted && coord != (lastCoord + 1)))
        {
            (Original_Contigs + lastId)->contigMapPixels[(Original_Contigs + lastId)->nContigs++] = pixelIdx - 1 - (length >> 1);

            contig *cont = Contigs->contigs + contigPtr++;
            cont->originalContigId = lastId;
            cont->length = length;
            cont->startCoord = startCoord;

            u16 thisScaffID = Map_State->scaffIds[pixelIdx - 1];
            cont->scaffId = thisScaffID ? ((thisScaffID == lastScaffID) ? (scaffId) : (++scaffId)) : 0;
            lastScaffID = thisScaffID;

            if (IsContigInverted(contigPtr - 1))
            {
                if (!inverted) Contigs->contigInvertFlags[(contigPtr - 1) >> 3] &= ~(1 << ((contigPtr - 1) & 7));
            }
            else
            {
                if (inverted) Contigs->contigInvertFlags[(contigPtr - 1) >> 3] |= (1 << ((contigPtr - 1) & 7));
            }

            startCoord = coord;
            length = 0;
            if (pixelIdx < (Number_of_Pixels_1D - 1)) inverted = Map_State->contigRelCoords[pixelIdx + 1] < coord;
        }

        Map_State->contigIds[pixelIdx] = (u16)contigPtr;
        lastId = id;
        lastCoord = coord;
    }

    if (contigPtr < Max_Number_of_Contigs)
    {
        (Original_Contigs + lastId)->contigMapPixels[(Original_Contigs + lastId)->nContigs++] = pixelIdx - 1 - (length >> 1);

        ++length;
        contig *cont = Contigs->contigs + contigPtr++;
        cont->originalContigId = lastId;
        cont->length = length;
        cont->startCoord = startCoord;
            
        u16 thisScaffID = Map_State->scaffIds[pixelIdx];
        cont->scaffId = thisScaffID ? ((thisScaffID == lastScaffID) ? (scaffId) : (++scaffId)) : 0;
        
        if (IsContigInverted(contigPtr - 1))
        {
            if (!inverted) Contigs->contigInvertFlags[(contigPtr - 1) >> 3] &= ~(1 << ((contigPtr - 1) & 7));
        }
        else
        {
            if (inverted) Contigs->contigInvertFlags[(contigPtr - 1) >> 3] |= (1 << ((contigPtr - 1) & 7));
        }
    }

    Contigs->numberOfContigs = contigPtr;
}

global_function
void
AddMapEdit(s32 delta, pointui finalPixels, u32 invert);

global_function
void
RebuildContig(u16 pixel)
{
    for (;;)
    {
        u16 contigId = Map_State->contigIds[pixel];
        u16 origContigId = Map_State->originalContigIds[pixel];

        u32 top = (u32)pixel;
        while (top && (Map_State->contigIds[top - 1] == contigId)) --top;

        u32 bottom = (u32)pixel;
        while ((bottom < (Number_of_Pixels_1D - 1)) && (Map_State->contigIds[bottom + 1] == contigId)) ++bottom;

        if (IsContigInverted(contigId))
        {
            InvertMap(top, bottom);
            AddMapEdit(0, {top, bottom}, 1);
            continue;
        }

        u08 fragmented = 0;
        ForLoop(Number_of_Pixels_1D)
        {
            if ((Map_State->contigIds[index] != contigId) && (Map_State->originalContigIds[index] == origContigId))
            {
                fragmented = 1;
                break;
            }
        }

        if (fragmented)
        {
            u16 contigTopCoord = Map_State->contigRelCoords[top];
            if (contigTopCoord)
            {
                u32 otherPixel = 0;
                ForLoop(Number_of_Pixels_1D)
                {
                    if ((Map_State->originalContigIds[index] == origContigId) && (Map_State->contigRelCoords[index] == (contigTopCoord - 1)))
                    {
                        otherPixel = index;
                        break;
                    }
                }

                u08 invert = !otherPixel || (Map_State->contigIds[otherPixel - 1] != Map_State->contigIds[otherPixel]);
                u32 otherPixel2 = otherPixel;

                if (invert)
                {
                    while ((otherPixel < (Number_of_Pixels_1D - 1)) && (Map_State->contigIds[otherPixel + 1] == Map_State->contigIds[otherPixel2])) ++otherPixel;
                }
                else
                {
                    while (otherPixel2 && (Map_State->contigIds[otherPixel2 - 1] == Map_State->contigIds[otherPixel])) --otherPixel2;
                }

                s32 delta = (s32)top - (s32)otherPixel;
                if (delta > 0) --delta;
                else delta = (s32)top - (s32)otherPixel2;
                pointui finalPixels = {(u32)((s32)otherPixel2 + delta), (u32)((s32)otherPixel + delta)};
                RearrangeMap(otherPixel2, otherPixel, delta);
                if (invert) InvertMap(finalPixels.x, finalPixels.y);
                AddMapEdit(delta, finalPixels, invert);
            }
            else
            {
                u16 contigBottomCoord = Map_State->contigRelCoords[bottom];

                u32 otherPixel = 0;
                ForLoop(Number_of_Pixels_1D)
                {
                    if ((Map_State->originalContigIds[index] == origContigId) && (Map_State->contigRelCoords[index] == (contigBottomCoord + 1)))
                    {
                        otherPixel = index;
                        break;
                    }
                }

                u08 invert = (otherPixel == (Number_of_Pixels_1D - 1)) || (Map_State->contigIds[otherPixel + 1] != Map_State->contigIds[otherPixel]);
                u32 otherPixel2 = otherPixel;

                if (!invert)
                {
                    while ((otherPixel < (Number_of_Pixels_1D - 1)) && (Map_State->contigIds[otherPixel + 1] == Map_State->contigIds[otherPixel2])) ++otherPixel;
                }
                else
                {
                    while (otherPixel2 && (Map_State->contigIds[otherPixel2 - 1] == Map_State->contigIds[otherPixel])) --otherPixel2;
                }

                s32 delta = (s32)bottom - (s32)otherPixel2;
                if (delta < 0) ++delta;
                else delta = (s32)bottom - (s32)otherPixel;
                pointui finalPixels = {(u32)((s32)otherPixel2 + delta), (u32)((s32)otherPixel + delta)};
                RearrangeMap(otherPixel2, otherPixel, delta);
                if (invert) InvertMap(finalPixels.x, finalPixels.y);
                AddMapEdit(delta, finalPixels, invert);
            }

            continue;
        }
        else break;
    }
}

struct
map_edit
{
    u16 finalPix1;
    u16 finalPix2;
    s16 delta;
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

#define Edits_Stack_Size 32768
#define Waypoints_Stack_Size 128

struct
map_editor
{
    map_edit *edits;
    u32 nEdits;
    u32 editStackPtr;
    u32 nUndone;
    u32 pad;
};

global_variable
map_editor *
Map_Editor;

#define Waypoints_Quadtree_Levels 5

struct
waypoint_quadtree_level
{
#ifdef Internal
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

#ifdef Internal
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
#ifdef Internal
GetWaypointsWithinRectange(point2f lowerBound, point2f size, waypoint ***bufferPtr, u32 reset = 1);
#else
GetWaypointsWithinRectange(point2f lowerBound, point2f size, waypoint ***bufferPtr);
#endif

global_function
void
UpdateWayPoint(waypoint *wayp, point2f coords);

global_function
void
MoveWayPoints(map_edit *edit, u32 undo = 0);

global_function
void
AddMapEdit(s32 delta, pointui finalPixels, u32 invert)
{
    ++Map_Editor->nEdits;
    Map_Editor->nUndone = 0;

    map_edit *edit = Map_Editor->edits + Map_Editor->editStackPtr++;

    if (Map_Editor->editStackPtr == Edits_Stack_Size)
    {
        Map_Editor->editStackPtr = 0;
    }

    u16 pix1 = (u16)(invert ? Max(finalPixels.x, finalPixels.y) : Min(finalPixels.x, finalPixels.y));
    u16 pix2 = (u16)(invert ? Min(finalPixels.x, finalPixels.y) : Max(finalPixels.x, finalPixels.y));

    edit->delta = (s16)delta;
    edit->finalPix1 = pix1;
    edit->finalPix2 = pix2;
}

global_function
void
UpdateScaffolds()
{
    ForLoop(Number_of_Pixels_1D) Map_State->scaffIds[index] = (Contigs->contigs + Map_State->contigIds[index])->scaffId;
}

global_function
void
UndoMapEdit()
{
    if (Map_Editor->nEdits && !Edit_Pixels.editing)
    {
        --Map_Editor->nEdits;
        ++Map_Editor->nUndone;

        if (!Map_Editor->editStackPtr)
        {
            Map_Editor->editStackPtr = Edits_Stack_Size + 1;
        }

        map_edit *edit = Map_Editor->edits + (--Map_Editor->editStackPtr);

        if (edit->finalPix1 > edit->finalPix2)
        {
            InvertMap((u32)edit->finalPix1, (u32)edit->finalPix2);
        }

        u32 start = Min(edit->finalPix1, edit->finalPix2);
        u32 end = Max(edit->finalPix1, edit->finalPix2);

        RearrangeMap(start, end, -edit->delta);

        UpdateScaffolds();
    }
}

global_function
void
RedoMapEdit()
{
    if (Map_Editor->nUndone && !Edit_Pixels.editing)
    {
        ++Map_Editor->nEdits;
        --Map_Editor->nUndone;

        map_edit *edit = Map_Editor->edits + Map_Editor->editStackPtr++;

        if (Map_Editor->editStackPtr == Edits_Stack_Size)
        {
            Map_Editor->editStackPtr = 0;
        }

        u32 start = Min(edit->finalPix1, edit->finalPix2);
        u32 end = Max(edit->finalPix1, edit->finalPix2);

        RearrangeMap((u32)((s32)start - edit->delta), (u32)((s32)end - edit->delta), edit->delta);
        
        if (edit->finalPix1 > edit->finalPix2)
        {
            InvertMap((u32)edit->finalPix1, (u32)edit->finalPix2);
        }
        
        UpdateScaffolds();
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
        tmp = {(u32)edit->finalPix1, (u32)edit->finalPix2};
        delta = -(s32)edit->delta;
        finalPixels = {(u32)((s32)tmp.x + delta), (u32)((s32)tmp.y + delta)};
    }
    else
    {
        tmp = {(u32)((s32)edit->finalPix1 - (s32)edit->delta), (u32)((s32)edit->finalPix2 - (s32)edit->delta)};
        delta = (s32)edit->delta;
        finalPixels = {(u32)edit->finalPix1, (u32)edit->finalPix2};
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

    f32 ooNPixels = (f32)(1.0 / (f64)Number_of_Pixels_1D);
    point2f editRangeModel = {((f32)editRange.x * ooNPixels) - 0.5f, ((f32)editRange.y * ooNPixels) - 0.5f};
    f32 dRange = editRangeModel.y - editRangeModel.x;

    waypoint **searchBuffer = PushArray(Working_Set, waypoint*, Waypoints_Stack_Size);
    waypoint **bufferEnd = searchBuffer;

    GetWaypointsWithinRectange({editRangeModel.x, -0.5f}, {dRange, 0.999f}, &bufferEnd);
    GetWaypointsWithinRectange({-0.5f, editRangeModel.x}, {0.999f, dRange}, &bufferEnd
#ifdef Internal
, 0
#endif
            );

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

                    if (edit->finalPix1 > edit->finalPix2)
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

                    if (edit->finalPix1 > edit->finalPix2)
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
#ifdef Internal
                    toSearch[index] = 0;
#endif
                    break;
                }
            }
        }
   
#ifdef Internal
        ForLoop(4)
        {
            if (!toSearch[index]) level->children[index]->show = 1;
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

#ifdef Internal
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
GetWaypointsWithinSquare(point2f lowerBound, f32 size, waypoint ***bufferPtr
#ifdef Internal        
        , u32 reset = 1)
#else
        )
#endif
{
#ifdef Internal
    if (reset) TurnOffDrawingForQuadTreeLevel();
#endif
    GetWaypointsWithinRectange(Waypoint_Editor->quadtree, lowerBound, {size, size}, bufferPtr);
}

global_function
void
GetWaypointsWithinRectange(point2f lowerBound, point2f size, waypoint ***bufferPtr
#ifdef Internal        
        , u32 reset)
#else
        )
#endif
{
#ifdef Internal
    if (reset) TurnOffDrawingForQuadTreeLevel();
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

        wayp->next = Waypoint_Editor->activeWaypoints.next;
        if (Waypoint_Editor->activeWaypoints.next) Waypoint_Editor->activeWaypoints.next->prev = wayp;
        Waypoint_Editor->activeWaypoints.next = wayp;
        wayp->prev = &Waypoint_Editor->activeWaypoints;
        wayp->index = wayp->next ? wayp->next->index + 1 : 0;

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
        wayp->next = Waypoint_Editor->freeWaypoints.next;
        if (Waypoint_Editor->freeWaypoints.next) Waypoint_Editor->freeWaypoints.next->prev = wayp;
        Waypoint_Editor->freeWaypoints.next = wayp;
        wayp->prev = &Waypoint_Editor->freeWaypoints;

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
            static s32 netDelta = 0;

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

            u32 nPixels = Number_of_Pixels_1D;

            u32 pixel1 = (u32)((f64)nPixels * (0.5 + (f64)wx));
            u32 pixel2 = (u32)((f64)nPixels * (0.5 + (f64)wy));

            u16 contig = Map_State->contigIds[pixel1];

            if (!Edit_Pixels.editing && !Edit_Pixels.selecting && Map_State->contigIds[pixel2] != contig)
            {
                u32 testPixel = pixel1;
                u32 testContig = contig;
                while (testContig == contig)
                {
                    testContig = pixel1 < pixel2 ? Map_State->contigIds[++testPixel] : Map_State->contigIds[--testPixel];
                    if (testPixel == 0 || testPixel >= (nPixels - 1)) break;
                }
                pixel2 = pixel1 < pixel2 ? testPixel - 1 : testPixel + 1;
            }
            
            if (Edit_Pixels.selecting)
            {
                Edit_Pixels.selectPixels.x = Max(pixel1, Max(Edit_Pixels.selectPixels.x, Edit_Pixels.selectPixels.y));
                while(  Edit_Pixels.selectPixels.x < (Number_of_Pixels_1D - 1) &&
                        Map_State->contigIds[Edit_Pixels.selectPixels.x] == Map_State->contigIds[1 + Edit_Pixels.selectPixels.x])
                {
                    ++Edit_Pixels.selectPixels.x;
                }

                Edit_Pixels.selectPixels.y = Min(pixel1, Min(Edit_Pixels.selectPixels.x, Edit_Pixels.selectPixels.y));
                while(  Edit_Pixels.selectPixels.y > 0 &&
                        Map_State->contigIds[Edit_Pixels.selectPixels.y] == Map_State->contigIds[Edit_Pixels.selectPixels.y - 1])
                {
                    --Edit_Pixels.selectPixels.y;
                }

                pixel1 = Edit_Pixels.selectPixels.x;
                pixel2 = Edit_Pixels.selectPixels.y;
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

                diff = forward ? Min(diffx, diffy) : Max(diffx, diffy);
                
                //newX = (s32)Edit_Pixels.pixels.x + diff;
                //newY = (s32)Edit_Pixels.pixels.y + diff;
                
                diff = RearrangeMap(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y, diff, Edit_Pixels.snap);
                netDelta += diff;

                newX = (s32)Edit_Pixels.pixels.x + diff;
                newY = (s32)Edit_Pixels.pixels.y + diff;
                
                Edit_Pixels.pixels.x = (u32)newX;
                Edit_Pixels.pixels.y = (u32)newY;

                Edit_Pixels.worldCoords.x = (f32)(((f64)((2 * Edit_Pixels.pixels.x) + 1)) / ((f64)(2 * nPixels))) - 0.5f;
                Edit_Pixels.worldCoords.y = (f32)(((f64)((2 * Edit_Pixels.pixels.y) + 1)) / ((f64)(2 * nPixels))) - 0.5f;
            }
            else
            {
                if (netDelta || Global_Edit_Invert_Flag)
                {
                    AddMapEdit(netDelta, Edit_Pixels.pixels, Global_Edit_Invert_Flag);
                    netDelta = 0;
                }
                
                wx = (f32)(((f64)((2 * pixel1) + 1)) / ((f64)(2 * nPixels))) - 0.5f;
                wy = (f32)(((f64)((2 * pixel2) + 1)) / ((f64)(2 * nPixels))) - 0.5f;

                Edit_Pixels.pixels.x = pixel1;
                Edit_Pixels.pixels.y = pixel2;
                Edit_Pixels.worldCoords.x = wx;
                Edit_Pixels.worldCoords.y = wy;
                
                Global_Edit_Invert_Flag = 0;
            }

            redisplay = 1;
        }
        else if (Tool_Tip->on || Waypoint_Edit_Mode || Scaff_Edit_Mode)
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

            wx = Max(-0.5f, Min(0.5f, wx));
            wy = Max(-0.5f, Min(0.5f, wy));

            u32 nPixels = Number_of_Textures_1D * Texture_Resolution;

            u32 pixel1 = (u32)((f64)nPixels * (0.5 + (f64)wx));
            u32 pixel2 = (u32)((f64)nPixels * (0.5 + (f64)wy));

            Tool_Tip_Move.pixels.x = pixel1;
            Tool_Tip_Move.pixels.y = pixel2;
            Tool_Tip_Move.worldCoords.x = wx;
            Tool_Tip_Move.worldCoords.y = wy;

            if (Waypoint_Edit_Mode)
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
            else if (Scaff_Edit_Mode && Scaff_Painting_Flag)
            {
                if (Scaff_Painting_Flag == 1)
                {
                    if (!Scaff_Painting_Id)
                    {
                        if (Map_State->scaffIds[Tool_Tip_Move.pixels.x])
                        {
                            Scaff_Painting_Id = Map_State->scaffIds[Tool_Tip_Move.pixels.x];
                        }
                        else
                        {
                            u16 max = 0;
                            ForLoop(Number_of_Pixels_1D) max = Max(max, Map_State->scaffIds[index]);
                            Scaff_Painting_Id = max + 1;
                        }
                    }

                    if (Map_State->scaffIds[Tool_Tip_Move.pixels.x] != Scaff_Painting_Id)
                    {
                        u32 pixel = Tool_Tip_Move.pixels.x;
                        u16 contigId = Map_State->contigIds[pixel];
                        Map_State->scaffIds[pixel] = Scaff_Painting_Id;

                        u32 testPixel = pixel;
                        while (testPixel && (Map_State->contigIds[testPixel - 1] == contigId)) Map_State->scaffIds[--testPixel] = Scaff_Painting_Id;

                        testPixel = pixel;
                        while ((testPixel < (Number_of_Pixels_1D - 1)) && (Map_State->contigIds[testPixel + 1] == contigId)) Map_State->scaffIds[++testPixel] = Scaff_Painting_Id;
                    }
                }
                else if (Scaff_Painting_Flag == 2)
                {
                    Scaff_Painting_Id = 0;
                    
                    if (Map_State->scaffIds[Tool_Tip_Move.pixels.x] != Scaff_Painting_Id)
                    {
                        u32 pixel = Tool_Tip_Move.pixels.x;
                        u16 contigId = Map_State->contigIds[pixel];
                        Map_State->scaffIds[pixel] = Scaff_Painting_Id;

                        u32 testPixel = pixel;
                        while (testPixel && (Map_State->contigIds[testPixel - 1] == contigId)) Map_State->scaffIds[--testPixel] = Scaff_Painting_Id;

                        testPixel = pixel;
                        while ((testPixel < (Number_of_Pixels_1D - 1)) && (Map_State->contigIds[testPixel + 1] == contigId)) Map_State->scaffIds[++testPixel] = Scaff_Painting_Id;
                    }
                }

                UpdateContigsFromMapState();
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
    }

    if (redisplay)
    {
        Redisplay = 1;
    }
}

global_variable
u08
Mouse_Invert = 0;

global_variable
u08
Deferred_Close_UI = 0;

global_function
void
Mouse(GLFWwindow* window, s32 button, s32 action, s32 mods)
{
    s32 primaryMouse = Mouse_Invert ? GLFW_MOUSE_BUTTON_RIGHT : GLFW_MOUSE_BUTTON_LEFT;
    s32 secondaryMouse = Mouse_Invert ? GLFW_MOUSE_BUTTON_LEFT : GLFW_MOUSE_BUTTON_RIGHT;    
    
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
            Deferred_Close_UI = 1;
        }
    }
    else
    {
        if (button == primaryMouse && Edit_Mode && action == GLFW_PRESS)
        {
            Edit_Pixels.editing = !Edit_Pixels.editing;
            MouseMove(window, x, y);
            if (!Edit_Pixels.editing) UpdateScaffolds();
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && Edit_Mode && action == GLFW_RELEASE && !Edit_Pixels.editing)
        {
            Edit_Pixels.editing = 1;
            Edit_Pixels.selecting = 0;
            MouseMove(window, x, y);
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && Edit_Mode && action == GLFW_PRESS && !Edit_Pixels.editing)
        {
            Edit_Pixels.selecting = 1;
            Edit_Pixels.selectPixels = Edit_Pixels.pixels;
            MouseMove(window, x, y);
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && Edit_Mode && Edit_Pixels.editing && action == GLFW_PRESS)
        {
            InvertMap(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y);
            Global_Edit_Invert_Flag = !Global_Edit_Invert_Flag;
            UpdateContigsFromMapState();

            Redisplay = 1;
        }
        else if (button == primaryMouse && Waypoint_Edit_Mode && action == GLFW_PRESS)
        {
            AddWayPoint(Tool_Tip_Move.worldCoords);
            MouseMove(window, x, y);
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && Waypoint_Edit_Mode && action == GLFW_PRESS)
        {
            if (Selected_Waypoint)
            {
                RemoveWayPoint(Selected_Waypoint);
                MouseMove(window, x, y);
            }
        }
        else if (button == primaryMouse && Scaff_Edit_Mode && action == GLFW_PRESS)
        {
            Scaff_Painting_Flag = 1;
            MouseMove(window, x, y);
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE && Scaff_Edit_Mode && action == GLFW_PRESS)
        {
            Scaff_Painting_Flag = 2;
            MouseMove(window, x, y);
        }
        else if ((button == GLFW_MOUSE_BUTTON_MIDDLE || button == primaryMouse) && Scaff_Edit_Mode && action == GLFW_RELEASE)
        {
            Scaff_Painting_Flag = 0;
            Scaff_Painting_Id = 0;
            MouseMove(window, x, y);
            UpdateScaffolds();
        }
        else if (button == secondaryMouse)
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
            Mouse_Move.x = Mouse_Move.y = -1;
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

        if (Edit_Mode || Tool_Tip->on || Waypoint_Edit_Mode)
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
SubDivideScaleBar(f32 left, f32 right, f32 middle, f32 bpPerPixel, f32 offset)
{
    u32 result = 0;

    if (left < right)
    {
        f32 length = right - left;
        f32 half = length * 0.5f;
        char buff[16];
        stbsp_snprintf(buff, 16, "%$.2f", (f64)(offset + (half * bpPerPixel)));
        f32 width = fonsTextBounds(FontStash_Context, middle, 0.0, buff, 0, NULL);
        f32 halfWidth = 0.5f * width;

        if ((middle + halfWidth) < right && (middle - halfWidth) > left)
        {
            u32 leftResult = SubDivideScaleBar(left, middle - halfWidth, (left + middle) * 0.5f, bpPerPixel, offset);
            u32 rightResult = SubDivideScaleBar(middle + halfWidth, right, (right + middle) * 0.5f, bpPerPixel, offset);
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

#ifdef Internal
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
ColourGenerator(u32 index, f32 *rgb)
{
#define RedFreq 1.666f
#define GreenFreq 2.666f
#define BlueFreq 3.666f

    rgb[0] = 0.5f * (sinf((f32)index * RedFreq) + 1.0f);
    rgb[1] = 0.5f * (sinf((f32)index * GreenFreq) + 1.0f);
    rgb[2] = 0.5f * (sinf((f32)index * BlueFreq) + 1.0f);
}

struct
graph
{
    u32 name[16];
    u32 *data;
    GLuint vbo;
    GLuint vao;
    editable_plot_shader *shader;
    f32 scale;
    f32 base;
    f32 lineSize;
    u32 on;
    nk_colorf colour;
};

enum
extension_type
{
    extension_graph,
};

global_variable
char
Extension_Magic_Bytes[][4] = 
{
    {'p', 's', 'g', 'h'}
};

struct
extension_node
{
    extension_type type;
    u32 pad;
    void *extension;
    extension_node *next;
};

struct
extension_sentinel
{
    extension_node *head;
    extension_node *tail;
};

global_variable
extension_sentinel
Extensions = {};

global_function
void
AddExtension(extension_node *node)
{
    node->next = 0;

    if (!Extensions.head)
    {
        Extensions.head = node;
        Extensions.tail = node;
    }
    else
    {
        Extensions.tail->next = node;
        Extensions.tail = node;
    }
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

        TraverseLinkedList(Extensions.head, extension_node)
        {
            switch (node->type)
            {
                case extension_graph:
                    {
                        graph *gph = (graph *)node->extension;
                        glUseProgram(gph->shader->shaderProgram);
                        glUniformMatrix4fv(gph->shader->matLocation, 1, GL_FALSE, mat);
                    }
                    break;
            }
        }
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

#ifdef Internal
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
    //if (Waypoint_Edit_Mode)
    {
        glUseProgram(Flat_Shader->shaderProgram);
        glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&QuadTrees->bg);

        u32 ptr = 0;
        vertex vert[4];

        f32 lineWidth = 0.001f / sqrtf(Camera_Position.z);

        DrawQuadTreeLevel(&ptr, Waypoint_Editor->quadtree, vert, lineWidth);
    }
#endif
   
    // Extensions
    {
        TraverseLinkedList(Extensions.head, extension_node)
        {
            switch (node->type)
            {
                case extension_graph:
                    {
                        graph *gph = (graph *)node->extension;

                        if (gph->on)
                        {
                            f32 factor1 = 1.0f / (2.0f * Camera_Position.z);
                            f32 factor2 = 2.0f / height;

                            f32 wy = (factor1 * (1.0f - (factor2 * (height - gph->base)))) + Camera_Position.y;

                            glUseProgram(gph->shader->shaderProgram);
                            glUniform4fv(gph->shader->colorLocation, 1, (GLfloat *)&gph->colour);
                            glUniform1f(gph->shader->yScaleLocation, gph->scale);
                            glUniform1f(gph->shader->yTopLocation, wy);
                            glUniform1f(gph->shader->lineSizeLocation, gph->lineSize / Camera_Position.z);

                            glBindBuffer(GL_ARRAY_BUFFER, gph->vbo);
                            glBindVertexArray(gph->vao);
                            glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)Number_of_Pixels_1D);
                        }
                    }
                    break;
            }
        }
    }

    // Grid
    if (File_Loaded && Grid->on)
    {
        glUseProgram(Flat_Shader->shaderProgram);
        glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&Grid->bg);

        u32 ptr = 0;
        vertex vert[4];

        f32 lineWidth = Grid->size / sqrtf(Camera_Position.z);
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
        ForLoop(Contigs->numberOfContigs - 1)
        {
            contig *cont = Contigs->contigs + index;

            position += ((f32)cont->length / (f32)Number_of_Pixels_1D);
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
        ForLoop(Contigs->numberOfContigs - 1)
        {
            contig *cont = Contigs->contigs + index;

            position += ((f32)cont->length / (f32)Number_of_Pixels_1D);
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

    // Contig Id Bars
    if (File_Loaded && Contig_Ids->on)
    {
        glUseProgram(Flat_Shader->shaderProgram);
        glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&Grid->bg);

        u32 ptr = 0;
        vertex vert[4];

        f32 barColour[4] = {1.0f, 1.0f, 1.0f, 1.0f};

        f32 lineWidth = Contig_Ids->size / sqrtf(Camera_Position.z);
        f32 position = 0.0f;

        f32 y = 0.5f;
        ForLoop(Contigs->numberOfContigs)
        {
            contig *cont = Contigs->contigs + index;

            position += ((f32)cont->length / (f32)Number_of_Pixels_1D);
            f32 py = y - lineWidth;
            y = 1.0f - position + (0.5f * (lineWidth - 1.0f));

            if (y < py)
            {
                u32 invert = IsContigInverted(index);

                vert[0].x = -py;
                vert[0].y = invert ? y : (py + lineWidth);
                vert[1].x = -py;
                vert[1].y = invert ? (y - lineWidth) : py;
                vert[2].x = -y;
                vert[2].y = invert ? (y - lineWidth) : py;
                vert[3].x = -y;
                vert[3].y = invert ? y : (py + lineWidth);

                ColourGenerator((u32)cont->originalContigId, (f32 *)barColour);
                glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&barColour);

                glBindBuffer(GL_ARRAY_BUFFER, Contig_ColourBar_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Contig_ColourBar_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
            }
        }
    }

    // Text / UI Rendering
    if (Contig_Name_Labels->on || Scale_Bars->on || Tool_Tip->on || UI_On || Loading || Edit_Mode || Waypoint_Edit_Mode || Waypoints_Always_Visible || Scaff_Edit_Mode || Scaffs_Always_Visible)
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
            fonsSetSize(FontStash_Context, Contig_Name_Labels->size * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_CENTER | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Bold);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, FourFloatColorToU32(Contig_Name_Labels->fg));

            f32 leftPixel = ModelXToScreen(-0.5f);
            f32 totalLength = 0.0f;
            f32 wy0 = ModelYToScreen(0.5f);

            ForLoop(Contigs->numberOfContigs)
            {
                contig *cont = Contigs->contigs + index;
                
                totalLength += (f32)((f64)cont->length / (f64)Number_of_Pixels_1D);

                f32 rightPixel = ModelXToScreen(totalLength - 0.5f);

                if (rightPixel > 0.0f && leftPixel < width)
                {
                    const char *name = (const char *)(Original_Contigs + cont->originalContigId)->name;
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

            ForLoop(Contigs->numberOfContigs)
            {
                contig *cont = Contigs->contigs + index;
                
                totalLength += (f32)((f64)cont->length / (f64)Number_of_Pixels_1D);

                f32 bottomPixel = ModelYToScreen(0.5f - totalLength);

                if (topPixel < height && bottomPixel > 0.0f)
                {
                    const char *name = (const char *)(Original_Contigs + cont->originalContigId)->name;
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

            glViewport(0, 0, (s32)width, (s32)height);

            f32 lh = 0.0f;   

            fonsClearState(FontStash_Context);
            fonsSetSize(FontStash_Context, Scale_Bars->size * Screen_Scale.x);
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
                        
            f32 scaleBarWidth = Scale_Bars->size * 4.0f / 20.0f * Screen_Scale.x;
            f32 tickLength = Scale_Bars->size * 3.0f / 20.0f * Screen_Scale.x;

            ForLoop(Contigs->numberOfContigs)
            {
                contig *cont = Contigs->contigs + index;
                
                totalLength += (f32)((f64)cont->length / (f64)Number_of_Pixels_1D);
                rightPixel = ModelXToScreen(totalLength - 0.5f);

                f32 pixelLength = rightPixel - leftPixel;
                f32 startCoord = (f32)((f64)(IsContigInverted(index) ? (cont->startCoord - cont->length) : cont->startCoord) * (f64)Total_Genome_Length / (f64)Number_of_Pixels_1D);

                u32 labelLevels = SubDivideScaleBar(leftPixel, rightPixel, (leftPixel + rightPixel) * 0.5f, bpPerPixel, startCoord);
                u32 labels = 0;
                ForLoop2(labelLevels)
                {
                    labels += (labels + 1);
                }
                labels = Min(labels, MaxTicksPerScaleBar);

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
                            stbsp_snprintf(buff, 16, "%$.2f", (f64)(startCoord + (pixelLength * (IsContigInverted(index) ? (1.0f - distance) : distance) * bpPerPixel)));
                            glUseProgram(UI_Shader->shaderProgram);
                            fonsDrawText(FontStash_Context, x, y + scaleBarWidth + tickLength + 1.0f, buff, 0);
                        }
                    }
                }

                leftPixel = rightPixel;
            }

            ChangeSize((s32)width, (s32)height);
        }
        
        // Waypoint Edit Mode
        if (File_Loaded && (Waypoint_Edit_Mode || Waypoints_Always_Visible))
        {
            u32 ptr = 0;
            vertex vert[4];

            glUseProgram(Flat_Shader->shaderProgram);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textNormalMat);
            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textNormalMat);

            glViewport(0, 0, (s32)width, (s32)height);
#define DefaultWaypointSize 18.0f
            glUseProgram(Flat_Shader->shaderProgram);
            glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Waypoint_Mode_Data->base);

            f32 lineWidth = Waypoint_Mode_Data->size / DefaultWaypointSize * 0.7f * Screen_Scale.x;
            f32 lineHeight = Waypoint_Mode_Data->size / DefaultWaypointSize * 8.0f * Screen_Scale.x;

            f32 lh = 0.0f;   

            u32 baseColour = FourFloatColorToU32(Waypoint_Mode_Data->base);
            u32 selectColour = FourFloatColorToU32(Waypoint_Mode_Data->selected);
            
            fonsClearState(FontStash_Context);
            fonsSetSize(FontStash_Context, Waypoint_Mode_Data->size * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Bold);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, baseColour);

            char buff[4];

            TraverseLinkedList(Waypoint_Editor->activeWaypoints.next, waypoint)
            {
                point2f screen = {ModelXToScreen(node->coords.x), ModelYToScreen(-node->coords.y)};
                point2f screenYRange = {ModelYToScreen(0.5f), ModelYToScreen(-0.5f)};

                glUseProgram(Flat_Shader->shaderProgram);
                if (node == Selected_Waypoint)
                {
                    glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Waypoint_Mode_Data->selected);
                }

                vert[0].x = screen.x - lineWidth;
                vert[0].y = screenYRange.x;
                vert[1].x = screen.x - lineWidth;
                vert[1].y = screenYRange.y;
                vert[2].x = screen.x + lineWidth;
                vert[2].y = screenYRange.y;
                vert[3].x = screen.x + lineWidth;
                vert[3].y = screenYRange.x;

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
                    glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Waypoint_Mode_Data->base);
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

            if (Waypoint_Edit_Mode && !UI_On)
            {
                fonsSetSize(FontStash_Context, 24.0f * Screen_Scale.x);
                fonsVertMetrics(FontStash_Context, 0, 0, &lh);
                fonsSetColor(FontStash_Context, FourFloatColorToU32(Waypoint_Mode_Data->text));

                f32 textBoxHeight = lh;
                textBoxHeight *= 4.0f;
                textBoxHeight += 3.0f;
                f32 spacing = 10.0f;

                char *helpText1 = (char *)"Waypoint Edit Mode";
                char *helpText2 = (char *)"W: exit";
                char *helpText3 = (char *)"Left Click: place";
                char *helpText4 = (char *)"Middle Click / Spacebar: delete";

                f32 textWidth = fonsTextBounds(FontStash_Context, 0, 0, helpText4, 0, NULL);

                glUseProgram(Flat_Shader->shaderProgram);
                glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Waypoint_Mode_Data->bg);

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
        }
       
        // Scaff Bars
        if (File_Loaded && (Scaff_Edit_Mode || Scaffs_Always_Visible))
        {
            glUseProgram(Flat_Shader->shaderProgram);
            glUniformMatrix4fv(Flat_Shader->matLocation, 1, GL_FALSE, textNormalMat);
            glUseProgram(UI_Shader->shaderProgram);
            glUniformMatrix4fv(UI_Shader->matLocation, 1, GL_FALSE, textNormalMat);

            glViewport(0, 0, (s32)width, (s32)height);

            u32 ptr = 0;
            vertex vert[4];
            f32 barColour[4] = {1.0f, 1.0f, 1.0f, 0.5f};

            f32 lh = 0.0f;   
            fonsClearState(FontStash_Context);
#define DefaultScaffSize 40.0f
            fonsSetSize(FontStash_Context, Scaff_Mode_Data->size * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_MIDDLE | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Bold);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);

            char buff[128];
            f32 position = 0.0f;
            f32 start = 0.0f;
            u16 scaffId = Contigs->contigs->scaffId;
            ForLoop(Contigs->numberOfContigs)
            {
                contig *cont = Contigs->contigs + index;

                if (cont->scaffId != scaffId)
                {
                    if (scaffId)
                    {
                        vert[0].x = ModelXToScreen(start - 0.5f);
                        vert[0].y = ModelYToScreen(0.5f - start);
                        vert[1].x = ModelXToScreen(start - 0.5f);
                        vert[1].y = ModelYToScreen(0.5f - position);
                        vert[2].x = ModelXToScreen(position - 0.5f);
                        vert[2].y = ModelYToScreen(0.5f - position);
                        vert[3].x = ModelXToScreen(position - 0.5f);
                        vert[3].y = ModelYToScreen(0.5f - start);

                        ColourGenerator((u32)scaffId, (f32 *)barColour);
                        u32 colour = FourFloatColorToU32(*((nk_colorf *)barColour)) | 0xff000000;

                        glUseProgram(Flat_Shader->shaderProgram);
                        glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&barColour);

                        glBindBuffer(GL_ARRAY_BUFFER, Scaff_Bar_Data->vbos[ptr]);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                        glBindVertexArray(Scaff_Bar_Data->vaos[ptr++]);
                        glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                        glUseProgram(UI_Shader->shaderProgram);
                        fonsSetColor(FontStash_Context, colour);

                        stbsp_snprintf(buff, sizeof(buff), "Scaffold %u", scaffId);
                        f32 textWidth = fonsTextBounds(FontStash_Context, 0, 0, buff, 0, NULL);
                        fonsDrawText(FontStash_Context, ModelXToScreen(0.5f * (position + start - 1.0f)) - (0.5f * textWidth), ModelYToScreen(0.5f - start) - lh, buff, 0);
                    }

                    start = position;
                    scaffId = cont->scaffId;
                }

                position += ((f32)cont->length / (f32)Number_of_Pixels_1D);
            }

            if (scaffId)
            {
                vert[0].x = ModelXToScreen(start - 0.5f);
                vert[0].y = ModelYToScreen(0.5f - start);
                vert[1].x = ModelXToScreen(start - 0.5f);
                vert[1].y = ModelYToScreen(0.5f - position);
                vert[2].x = ModelXToScreen(position - 0.5f);
                vert[2].y = ModelYToScreen(0.5f - position);
                vert[3].x = ModelXToScreen(position - 0.5f);
                vert[3].y = ModelYToScreen(0.5f - start);

                ColourGenerator((u32)scaffId, (f32 *)barColour);
                u32 colour = FourFloatColorToU32(*((nk_colorf *)barColour));

                glUseProgram(Flat_Shader->shaderProgram);
                glUniform4fv(Flat_Shader->colorLocation, 1, (GLfloat *)&barColour);

                glBindBuffer(GL_ARRAY_BUFFER, Scaff_Bar_Data->vbos[ptr]);
                glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                glBindVertexArray(Scaff_Bar_Data->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

                glUseProgram(UI_Shader->shaderProgram);
                fonsSetColor(FontStash_Context, colour);

                stbsp_snprintf(buff, sizeof(buff), "Scaffold %u", scaffId);
                f32 textWidth = fonsTextBounds(FontStash_Context, 0, 0, buff, 0, NULL);
                fonsDrawText(FontStash_Context, ModelXToScreen(0.5f * (position + start - 1.0f)) - (0.5f * textWidth), ModelYToScreen(0.5f - start) - lh, buff, 0);
            }

            if (Scaff_Edit_Mode && !UI_On)
            {
                fonsSetSize(FontStash_Context, 24.0f * Screen_Scale.x);
                fonsVertMetrics(FontStash_Context, 0, 0, &lh);
                fonsSetColor(FontStash_Context, FourFloatColorToU32(Scaff_Mode_Data->text));

                f32 textBoxHeight = lh;
                textBoxHeight *= 4.0f;
                textBoxHeight += 3.0f;
                f32 spacing = 10.0f;

                char *helpText1 = (char *)"Scaffold Edit Mode";
                char *helpText2 = (char *)"S: exit";
                char *helpText3 = (char *)"Left Click: place";
                char *helpText4 = (char *)"Middle Click / Spacebar: delete; D: delete all";

                f32 textWidth = fonsTextBounds(FontStash_Context, 0, 0, helpText4, 0, NULL);

                glUseProgram(Flat_Shader->shaderProgram);
                glUniform4fv(Flat_Shader->colorLocation, 1, (f32 *)&Scaff_Mode_Data->bg);

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
            fonsSetSize(FontStash_Context, Tool_Tip->size * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Normal);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, FourFloatColorToU32(Tool_Tip->fg));
           
            // Extension info, extra lines
            u32 nExtra = 0;
            f32 longestExtraLineLength = 0.0f;
            {
                if (Extensions.head)
                {
                    char buff[128];
                    TraverseLinkedList(Extensions.head, extension_node)
                    {
                        switch (node->type)
                        {
                            case extension_graph:
                                {
                                    graph *gph = (graph *)node->extension;
                                    if (gph->on)
                                    {
                                        glBindBuffer(GL_TEXTURE_BUFFER, Contact_Matrix->pixelRearrangmentLookupBuffer);
                                        u16 *buffer = (u16 *)glMapBufferRange(GL_TEXTURE_BUFFER, Tool_Tip_Move.pixels.x * sizeof(u16), sizeof(u16), GL_MAP_READ_BIT);

                                        stbsp_snprintf(buff, sizeof(buff), "%s: %$d", (char *)gph->name, gph->data[*buffer]);
                                        ++nExtra;
                                        longestExtraLineLength = Max(longestExtraLineLength, fonsTextBounds(FontStash_Context, 0, 0, buff, 0, NULL));

                                        glUnmapBuffer(GL_TEXTURE_BUFFER);
                                        glBindBuffer(GL_TEXTURE_BUFFER, 0);
                                    }
                                }
                                break;
                        }
                    }
                }
            }

            f32 textBoxHeight = lh;
            textBoxHeight *= (3.0f + (f32)nExtra);
            textBoxHeight += (2.0f + (f32)nExtra);

            u16 id1 = Map_State->originalContigIds[Tool_Tip_Move.pixels.x];
            u16 id2 = Map_State->originalContigIds[Tool_Tip_Move.pixels.y];
            u16 coord1 = Map_State->contigRelCoords[Tool_Tip_Move.pixels.x];
            u16 coord2 = Map_State->contigRelCoords[Tool_Tip_Move.pixels.y];
            
            f64 bpPerPixel = (f64)Total_Genome_Length / (f64)Number_of_Pixels_1D;

            char line1[64];
            char *line2 = (char *)"vs";
            char line3[64];

            stbsp_snprintf(line1, 64, "%s %$.2fbp", (Original_Contigs + id1)->name, (f64)coord1 * bpPerPixel);
            stbsp_snprintf(line3, 64, "%s %$.2fbp", (Original_Contigs + id2)->name, (f64)coord2 * bpPerPixel);

            f32 textWidth_1 = fonsTextBounds(FontStash_Context, 0, 0, line1, 0, NULL);
            f32 textWidth_2 = fonsTextBounds(FontStash_Context, 0, 0, line2, 0, NULL);
            f32 textWidth_3 = fonsTextBounds(FontStash_Context, 0, 0, line3, 0, NULL);
            f32 textWidth = Max(textWidth_1, textWidth_2);
            textWidth = Max(textWidth, textWidth_3);
            textWidth = Max(textWidth, longestExtraLineLength);

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

            glBindBuffer(GL_ARRAY_BUFFER, Tool_Tip_Data->vbos[0]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
            glBindVertexArray(Tool_Tip_Data->vaos[0]);
            glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);

            glUseProgram(UI_Shader->shaderProgram);
            fonsDrawText(FontStash_Context, ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing, 
                    ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing, line1, 0);
            fonsDrawText(FontStash_Context, ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing, 
                    ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing + lh + 1.0f, line2, 0);
            fonsDrawText(FontStash_Context, ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing, 
                    ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing + (2.0f * lh) + 2.0f, line3, 0);

            {
                if (Extensions.head)
                {
                    u32 count = 0;
                    char buff[128];
                    TraverseLinkedList(Extensions.head, extension_node)
                    {
                        switch (node->type)
                        {
                            case extension_graph:
                                {
                                    graph *gph = (graph *)node->extension;
                                    if (gph->on)
                                    {
                                        glBindBuffer(GL_TEXTURE_BUFFER, Contact_Matrix->pixelRearrangmentLookupBuffer);
                                        u16 *buffer = (u16 *)glMapBufferRange(GL_TEXTURE_BUFFER, Tool_Tip_Move.pixels.x * sizeof(u16), sizeof(u16), GL_MAP_READ_BIT);

                                        stbsp_snprintf(buff, sizeof(buff), "%s: %$d", (char *)gph->name, gph->data[*buffer]);

                                        fonsDrawText(FontStash_Context, ModelXToScreen(Tool_Tip_Move.worldCoords.x) + spacing, 
                                                ModelYToScreen(-Tool_Tip_Move.worldCoords.y) + spacing + ((2.0f + (f32)(++count)) * (lh + 1.0f)), buff, 0);

                                        glUnmapBuffer(GL_TEXTURE_BUFFER);
                                        glBindBuffer(GL_TEXTURE_BUFFER, 0);
                                    }
                                }
                                break;
                        }
                    }
                }
            }
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

                if (Edit_Pixels.editing && line1Done)
                {
                    pix1 = pix1 ? pix1 - 1 : (pix2 < (Number_of_Pixels_1D - 1) ? pix2 + 1 : pix2);
                }

                original_contig *cont = Original_Contigs + Map_State->originalContigIds[pix1];

                u32 nPixels = Number_of_Pixels_1D;
                f64 bpPerPixel = (f64)Total_Genome_Length / (f64)nPixels;

                f64 bpStart = bpPerPixel * (f64)Map_State->contigRelCoords[pix1];
                
                if (Edit_Pixels.editing)
                {
                    stbsp_snprintf(line2, 64, "%s[%$.2fbp]", cont->name, bpStart);
                }
                else if (line1Done)
                {
                    line1Done = 0;
                }
                
                if (!line1Done)
                {
                    f64 bpEnd = bpPerPixel * (f64)Map_State->contigRelCoords[pix2];
                    original_contig *cont2 = Original_Contigs + Map_State->originalContigIds[pix2];
                    stbsp_snprintf(line1, 64, "%s[%$.2fbp] to %s[%$.2fbp]", cont->name, bpStart, cont2->name, bpEnd);
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

                if (Edit_Pixels.snap)
                {
                    char *text = (char *)"Snap Mode On";
                    textWidth = fonsTextBounds(FontStash_Context, 0, 0, text, 0, NULL);
                    glUseProgram(Flat_Shader->shaderProgram);
                    vert[0].x = ModelXToScreen(min) - spacing - textWidth;
                    vert[0].y = ModelYToScreen(-min) + spacing;
                    vert[1].x = ModelXToScreen(min) - spacing - textWidth;
                    vert[1].y = ModelYToScreen(-min) + spacing + lh;
                    vert[2].x = ModelXToScreen(min) - spacing;
                    vert[2].y = ModelYToScreen(-min) + spacing + lh;
                    vert[3].x = ModelXToScreen(min) - spacing;
                    vert[3].y = ModelYToScreen(-min) + spacing;

                    glBindBuffer(GL_ARRAY_BUFFER, Edit_Mode_Data->vbos[ptr]);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(vertex), vert);
                    glBindVertexArray(Edit_Mode_Data->vaos[ptr++]);
                    glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
                    
                    glUseProgram(UI_Shader->shaderProgram);
                    fonsDrawText(FontStash_Context, ModelXToScreen(min) - spacing - textWidth, ModelYToScreen(-min) + spacing, text, 0);
                }

                {
                    fonsSetFont(FontStash_Context, Font_Bold);
                    fonsSetSize(FontStash_Context, 24.0f * Screen_Scale.x);
                    fonsVertMetrics(FontStash_Context, 0, 0, &lh);

                    textBoxHeight = lh;
                    textBoxHeight *= 6.0f;
                    textBoxHeight += 5.0f;
                    spacing = 10.0f;

                    char *helpText1 = (char *)"Edit Mode";
                    char *helpText2 = (char *)"E: exit, Q: undo, W: redo";
                    char *helpText3 = (char *)"Left Click: pickup, place";
                    char *helpText4 = (char *)"S: toggle snap mode";
                    char *helpText5 = (char *)"Middle Click / Spacebar: pickup whole contig";
                    char *helpText6 = (char *)"Middle Click / Spacebar (while editing): invert sequence";

                    textWidth = fonsTextBounds(FontStash_Context, 0, 0, helpText6, 0, NULL);

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
                    textY += (1.0f + lh);
                    fonsDrawText(FontStash_Context, width - spacing - textWidth, height - spacing - textBoxHeight + textY, helpText6, 0);
                }
            }
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
                            (GLint)(height - cmd->clip_rect.y - cmd->clip_rect.h),
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
CreateShader(const GLchar *fragmentShaderSource, const GLchar *vertexShaderSource, const GLchar *geometryShaderSource = 0)
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint geometryShader = geometryShaderSource ? glCreateShader(GL_GEOMETRY_SHADER) : 0;

    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    if (geometryShader) glShaderSource(geometryShader, 1, &geometryShaderSource, NULL);

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

    if (geometryShader)
    {
        glCompileShader(geometryShader);
        glGetShaderiv(geometryShader, GL_COMPILE_STATUS, &compiled);

        if (compiled == GL_FALSE)
        {
            PrintShaderInfoLog(geometryShader);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteShader(geometryShader);
            exit(1);
        }
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    if (geometryShader) glAttachShader(shaderProgram, geometryShader);
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
        if (geometryShader) glDetachShader(shaderProgram, geometryShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        if (geometryShader) glDeleteShader(geometryShader);
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
TestFile(const char *fileName, u64 *fileSize = 0)
{
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
            if (fileSize)
            {
                fseek(file, 0, SEEK_END);
                *fileSize = (u64)ftell(file);
                fseek(file, 0, SEEK_SET);
            }

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
u08
LoadState(u64 headerHash, char *path = 0);

global_function
load_file_result
LoadFile(const char *filePath, memory_arena *arena, char **fileName, u64 *headerHash)
{
    u64 fileSize = 0;

    FILE *file = TestFile(filePath, &fileSize);
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

        glDeleteVertexArrays((GLsizei)Contig_ColourBar_Data->nBuffers, Contig_ColourBar_Data->vaos);
        glDeleteBuffers((GLsizei)Contig_ColourBar_Data->nBuffers, Contig_ColourBar_Data->vbos);

        glDeleteVertexArrays((GLsizei)Scaff_Bar_Data->nBuffers, Scaff_Bar_Data->vaos);
        glDeleteBuffers((GLsizei)Scaff_Bar_Data->nBuffers, Scaff_Bar_Data->vbos);

        TraverseLinkedList(Extensions.head, extension_node)
        {
            switch (node->type)
            {
                case extension_graph:
                    {
                        graph *gph = (graph *)node->extension;
                        glDeleteVertexArrays(1, &gph->vao);
                        glDeleteBuffers(1, &gph->vbo);
                        glDeleteBuffers(1, &gph->shader->yValuesBuffer);
                        glDeleteTextures(1, &gph->shader->yValuesBufferTex);
                    }
                    break;
            }
        }

        Current_Loaded_Texture = 0;
        Texture_Ptr = 0;
        
        Mouse_Move.x = -1.0;
        Mouse_Move.y = -1.0;

        Camera_Position.x = 0.0f;
        Camera_Position.y = 0.0f;
        Camera_Position.z = 1.0f;
       
        Edit_Pixels.editing = 0;
        Global_Mode = mode_normal;

        Extensions = {};

        ResetMemoryArenaP(arena);
    }

    // File Contents
    {
        char *tmp = (char *)filePath;
#ifdef _WIN32
        char sep = '\\';
#else
        char sep = '/';
#endif

        while (*++tmp) {}
        while ((*--tmp != sep) && *tmp) {}

        *fileName = tmp + 1;

        u32 intBuff[16];
        PushStringIntoIntArray(intBuff, ArrayCount(intBuff), (u08 *)(*fileName));

        u32 nBytesHeaderComp;
        u32 nBytesHeader;
        fread(&nBytesHeaderComp, 1, 4, file);
        fread(&nBytesHeader, 1, 4, file);

        u08 *header = PushArrayP(arena, u08, nBytesHeader);
        u08 *compressionBuffer = PushArrayP(arena, u08, nBytesHeaderComp);

        fread(compressionBuffer, 1, nBytesHeaderComp, file);
        *headerHash = FastHash64(compressionBuffer, nBytesHeaderComp, FastHash64(intBuff, sizeof(intBuff), 0xbafd06832de619c2));
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
        Number_of_Original_Contigs = val32;

        Original_Contigs = PushArrayP(arena, original_contig, Number_of_Original_Contigs);
        f32 *contigFracs = PushArrayP(arena, f32, Number_of_Original_Contigs);
        ForLoop(Number_of_Original_Contigs)
        {
            f32 frac;
            u32 name[16];

            ptr = (u08 *)&frac;
            ForLoop2(4)
            {
                *ptr++ = *header++;
            }

            contigFracs[index] = frac;
            ptr = (u08 *)name;
            ForLoop2(64)
            {
                *ptr++ = *header++;
            }

            ForLoop2(16)
            {
                Original_Contigs[index].name[index2] = name[index2];
            }

            (Original_Contigs + index)->contigMapPixels = PushArrayP(arena, u16, Number_of_Pixels_1D);
            (Original_Contigs + index)->nContigs = 0;
        }

        u08 textureRes = *header++;
        u08 nTextRes = *header++;
        u08 mipMapLevels = *header;

        Texture_Resolution = Pow2(textureRes);
        Number_of_Textures_1D = Pow2(nTextRes);
        Number_of_MipMaps = mipMapLevels;

        Number_of_Pixels_1D = Number_of_Textures_1D * Texture_Resolution;
        
        Map_State = PushStructP(arena, map_state);
        Map_State->contigIds = PushArrayP(arena, u16, Number_of_Pixels_1D);
        Map_State->originalContigIds = PushArrayP(arena, u16, Number_of_Pixels_1D);
        Map_State->contigRelCoords = PushArrayP(arena, u16, Number_of_Pixels_1D);
        Map_State->scaffIds = PushArrayP(arena, u16, Number_of_Pixels_1D);
        memset(Map_State->scaffIds, 0, Number_of_Pixels_1D * sizeof(u16));
        f32 total = 0.0f;
        u16 lastPixel = 0;
        u16 relCoord = 0;
        ForLoop(Number_of_Original_Contigs)
        {
            total += contigFracs[index];
            u16 pixel = (u16)((f64)Number_of_Pixels_1D * (f64)total);
            
            relCoord = 0;
#ifdef RevCoords
            u16 tmp = pixel - lastPixel - 1;
#endif
            while (lastPixel < pixel)
            {
                Map_State->originalContigIds[lastPixel] = (u16)index;
                Map_State->contigRelCoords[lastPixel++] = 
#ifdef RevCoords
                    tmp - relCoord++;
#else
                    relCoord++;
#endif
            }
        }
        while (lastPixel < Number_of_Pixels_1D)
        {
            Map_State->originalContigIds[lastPixel] = (u16)(Number_of_Original_Contigs - 1);
            Map_State->contigRelCoords[lastPixel++] = relCoord++;
        }

        Contigs = PushStructP(arena, contigs);
        Contigs->contigs = PushArrayP(arena, contig, Max_Number_of_Contigs);
        Contigs->contigInvertFlags = PushArrayP(arena, u08, (Max_Number_of_Contigs + 7) >> 3);

        UpdateContigsFromMapState();

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

        // Extensions
        {
            u08 magicTest[sizeof(Extension_Magic_Bytes[0])];

            while ((u64)(currLocation + sizeof(magicTest)) < fileSize)
            {
                u32 bytesRead = (u32)fread(magicTest, 1, sizeof(magicTest), file);
                currLocation += bytesRead;
                if (bytesRead == sizeof(magicTest))
                {
                    ForLoop(ArrayCount(Extension_Magic_Bytes))
                    {
                        u08 foundExtension = 1;
                        u08 *magic = (u08 *)Extension_Magic_Bytes[index];
                        ForLoop2(sizeof(magicTest))
                        {
                            if (magic[index2] != magicTest[index2])
                            {
                                foundExtension = 0;
                                break;
                            }
                        }

                        if (foundExtension)
                        {
                            extension_type type = (extension_type)index;
                            u32 extensionSize = 0;
                            switch (type)
                            {
                                case extension_graph:
                                    {
                                        u32 compSize;
                                        fread(&compSize, 1, sizeof(u32), file);
                                        graph *gph = PushStructP(arena, graph);
                                        extension_node *node = PushStructP(arena, extension_node);
                                        u08 *dataPlusName = PushArrayP(arena, u08, ((sizeof(u32) * Number_of_Pixels_1D) + sizeof(gph->name) ));
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
                                        gph->data = (u32 *)(dataPlusName + sizeof(gph->name));
#pragma clang diagnostic pop                                       
                                        extensionSize += (compSize + sizeof(u32));
                                        u08 *compBuffer = PushArrayP(arena, u08, compSize);
                                        fread(compBuffer, 1, compSize, file);
                                        if (libdeflate_deflate_decompress(Decompressor, (const void *)compBuffer, compSize, (void *)dataPlusName, (sizeof(u32) * Number_of_Pixels_1D) + sizeof(gph->name), NULL))
                                        {
                                            FreeLastPushP(arena); // data
                                            FreeLastPushP(arena); // graph
                                            FreeLastPushP(arena); // node
                                        }
                                        else
                                        {
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
                                            u32 *namePtr = (u32 *)dataPlusName;
#pragma clang diagnostic pop
                                            ForLoop2(ArrayCount(gph->name))
                                            {
                                                gph->name[index2] = *(namePtr + index2);
                                            }

                                            node->type = type;
                                            node->extension = gph;
                                            AddExtension(node);
                                        }
                                        FreeLastPushP(arena); // compBuffer
                                    }
                                    break;
                            }
                            currLocation += extensionSize;
                        }
                    }
                }
                else
                {
                    break;
                }
            }
        }
        
        fclose(file);
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
        PushGenericBuffer(&Grid_Data, 2 * (Max_Number_of_Contigs + 1));
    }

    // Label Box Data
    {
        PushGenericBuffer(&Label_Box_Data, 2 * Max_Number_of_Contigs);
    }

    //Scale Bar Data
    {
        PushGenericBuffer(&Scale_Bar_Data, 4 * (2 + MaxTicksPerScaleBar));
    }

    //Contig Colour Bars
    {
        PushGenericBuffer(&Contig_ColourBar_Data, Max_Number_of_Contigs);
    }

    //Scaff Bars
    {
        PushGenericBuffer(&Scaff_Bar_Data, Max_Number_of_Contigs);
    }

    // Extensions
    {
        u32 exIndex = 0;
        TraverseLinkedList(Extensions.head, extension_node)
        {
            switch (node->type)
            {
                case extension_graph:
                    {
                        graph *gph = (graph *)node->extension;
#define DefaultGraphScale 0.2f
#define DefaultGraphBase 32.0f
#define DefaultGraphLineSize 1.0f
#define DefaultGraphColour {0.1f, 0.8f, 0.7f, 1.0f}
                        gph->scale = DefaultGraphScale;
                        gph->base = DefaultGraphBase;
                        gph->lineSize = DefaultGraphLineSize;
                        gph->colour = DefaultGraphColour;
                        gph->on = 0;

                        gph->shader = PushStructP(arena, editable_plot_shader);
                        gph->shader->shaderProgram = CreateShader(FragmentSource_EditablePlot, VertexSource_EditablePlot, GeometrySource_EditablePlot);

                        glUseProgram(gph->shader->shaderProgram);
                        glBindFragDataLocation(gph->shader->shaderProgram, 0, "outColor");
                        gph->shader->matLocation = glGetUniformLocation(gph->shader->shaderProgram, "matrix");
                        gph->shader->colorLocation = glGetUniformLocation(gph->shader->shaderProgram, "color");
                        gph->shader->yScaleLocation = glGetUniformLocation(gph->shader->shaderProgram, "yscale");
                        gph->shader->yTopLocation = glGetUniformLocation(gph->shader->shaderProgram, "ytop");
                        gph->shader->lineSizeLocation = glGetUniformLocation(gph->shader->shaderProgram, "linewidth");

                        glUniform1i(glGetUniformLocation(gph->shader->shaderProgram, "pixrearrangelookup"), 3);
                        glUniform1i(glGetUniformLocation(gph->shader->shaderProgram, "yvalues"), 4 + (s32)exIndex);

                        u32 nValues = Number_of_Pixels_1D;
                        f32 *xValues = PushArrayP(arena, f32, nValues);
                        f32 *yValues = PushArrayP(arena, f32, nValues);
                        
                        u32 max = 0;
                        ForLoop(Number_of_Pixels_1D)
                        {
                            max = Max(max, gph->data[index]);
                        }

                        ForLoop(Number_of_Pixels_1D)
                        {
                            xValues[index] = (f32)index;
                            yValues[index] = (f32)gph->data[index] / (f32)max;
                        }

                        glActiveTexture(GL_TEXTURE4 + exIndex++);

                        GLuint yVal, yValTex;

                        glGenBuffers(1, &yVal);
                        glBindBuffer(GL_TEXTURE_BUFFER, yVal);
                        glBufferData(GL_TEXTURE_BUFFER, sizeof(f32) * nValues, yValues, GL_STATIC_DRAW);

                        glGenTextures(1, &yValTex);
                        glBindTexture(GL_TEXTURE_BUFFER, yValTex);
                        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, yVal);

                        gph->shader->yValuesBuffer = yVal;
                        gph->shader->yValuesBufferTex = yValTex;
                        
                        glGenVertexArrays(1, &gph->vao);
                        glBindVertexArray(gph->vao);
                        glGenBuffers(1, &gph->vbo);
                        glBindBuffer(GL_ARRAY_BUFFER, gph->vbo);
                        glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * nValues, xValues, GL_STATIC_DRAW);

                        GLuint posAttrib = (GLuint)glGetAttribLocation(gph->shader->shaderProgram, "position");
                        glEnableVertexAttribArray(posAttrib);
                        glVertexAttribPointer(posAttrib, 1, GL_FLOAT, GL_FALSE, 0, 0);

                        FreeLastPushP(arena); // y
                        FreeLastPushP(arena); // x

                        glActiveTexture(GL_TEXTURE0);
                    }
                    break;
            }
        }
    }

#ifdef Internal
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
        Map_Editor->nUndone = 0;
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

    if (LoadState(*headerHash)) LoadState(*headerHash + 1);
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
#define DefaultNameLabelTextSize 32.0f
        Contig_Name_Labels->size = DefaultNameLabelTextSize;
    }
    
    // Scale Bar UI
    {
        Scale_Bars = PushStruct(Working_Set, ui_colour_element_bg);
        Scale_Bars->on = 0;
        Scale_Bars->fg = Red_Text_Float;
        Scale_Bars->bg = Grey_Background;
#define DefaultScaleBarSize 20.0f
        Scale_Bars->size = DefaultScaleBarSize;
    }
   
    // Grid UI
    {
        Grid = PushStruct(Working_Set, ui_colour_element);
        Grid->on = 1;
        Grid->bg = Grey_Background;
#define DefaultGridSize 0.00025f
        Grid->size = DefaultGridSize;
    }

    // Contig Ids UI
    {
        Contig_Ids = PushStruct(Working_Set, ui_colour_element);
        Contig_Ids->on = 1;
#define DefaultContigIdSize (DefaultGridSize * 3.0f)
        Contig_Ids->size = DefaultContigIdSize;
    }

    // Tool Tip UI
    {
        Tool_Tip = PushStruct(Working_Set, ui_colour_element_bg);
        Tool_Tip->on = 1;
        Tool_Tip->fg = Yellow_Text_Float;
        Tool_Tip->bg = Grey_Background;
#define DefaultToolTipTextSize 20.0f
        Tool_Tip->size = DefaultToolTipTextSize;
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
        Waypoint_Mode_Data = PushStruct(Working_Set, waypoint_mode_data);
        Waypoint_Mode_Data->base = Red_Full;
        Waypoint_Mode_Data->selected = Blue_Full;
        Waypoint_Mode_Data->text = Yellow_Text_Float;
        Waypoint_Mode_Data->bg = Grey_Background;
        Waypoint_Mode_Data->size = DefaultWaypointSize;
    }

    // Scaff Mode Colours
    {
        Scaff_Mode_Data = PushStruct(Working_Set, scaff_mode_data);
        Scaff_Mode_Data->text = Yellow_Text_Float;
        Scaff_Mode_Data->bg = Grey_Background;
        Scaff_Mode_Data->size = DefaultScaffSize;
    }

#ifdef Internal
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

    // Edit Mode Data
    {
        PushGenericBuffer(&Edit_Mode_Data, 12);
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

    Loading_Arena = PushStruct(Working_Set, memory_arena);
    CreateMemoryArena(*Loading_Arena, MegaByte(512));
    //Loading_Arena = PushSubArena(Working_Set, MegaByte(128));
}

#if 0
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
#endif

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

    u32 nPixels = Number_of_Pixels_1D;

    Assert(pixelFrom < nPixels);
    Assert(pixelTo < nPixels);
    
    u32 copySize = (pixelTo - pixelFrom + 1) >> 1;
    
    u16 *tmpBuffer = PushArray(Working_Set, u16, copySize);
    u16 *tmpBuffer2 = PushArray(Working_Set, u16, copySize);
    u16 *tmpBuffer3 = PushArray(Working_Set, u16, copySize);

    glBindBuffer(GL_TEXTURE_BUFFER, Contact_Matrix->pixelRearrangmentLookupBuffer);
    u16 *buffer = (u16 *)glMapBufferRange(GL_TEXTURE_BUFFER, 0, nPixels * sizeof(u16), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    if (buffer)
    {
        ForLoop(copySize)
        {
            tmpBuffer[index] = buffer[pixelFrom + index];
            tmpBuffer2[index] = Map_State->contigRelCoords[pixelFrom + index];
            tmpBuffer3[index] = Map_State->originalContigIds[pixelFrom + index];
        }

        ForLoop(copySize)
        {
            buffer[pixelFrom + index] = buffer[pixelTo - index];
            Map_State->contigRelCoords[pixelFrom + index] = Map_State->contigRelCoords[pixelTo - index];
            Map_State->originalContigIds[pixelFrom + index] = Map_State->originalContigIds[pixelTo - index];
        }

        ForLoop(copySize)
        {
            buffer[pixelTo - index] = tmpBuffer[index];
            Map_State->contigRelCoords[pixelTo - index] = tmpBuffer2[index];
            Map_State->originalContigIds[pixelTo - index] = tmpBuffer3[index];
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
    FreeLastPush(Working_Set); // tmpBuffer3

    UpdateContigsFromMapState();

    map_edit edit;
    edit.finalPix1 = (u16)pixelTo;
    edit.finalPix2 = (u16)pixelFrom;
    edit.delta = 0;
    MoveWayPoints(&edit);
}

global_function
s32
RearrangeMap(u32 pixelFrom, u32 pixelTo, s32 delta, u32 snap)
{
    if (pixelFrom > pixelTo)
    {
        u32 tmp = pixelFrom;
        pixelFrom = pixelTo;
        pixelTo = tmp;
    }

    u32 nPixels = Number_of_Pixels_1D;

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

    if (snap)
    {
        if (forward)
        {
            u32 target = GetRealBufferLocation(pixelTo + (u32)delta);
            u16 targetContigId = Map_State->contigIds[target] + (target == Number_of_Pixels_1D - 1 ? 1 : 0);
            if (targetContigId)
            {
                contig *targetContig = Contigs->contigs + targetContigId - 1;
                
                u16 targetCoord = IsContigInverted(targetContigId - 1) ? (targetContig->startCoord - targetContig->length + 1) : (targetContig->startCoord + targetContig->length - 1);
                while (delta > 0 && (Map_State->contigIds[target] != targetContigId - 1 || Map_State->contigRelCoords[target] != targetCoord))
                {
                    --target;
                    --delta;
                }
            }
            else
            {
                delta = 0;
            }
        }
        else
        {
            u32 target = GetRealBufferLocation((u32)((s32)pixelFrom + delta));
            u16 targetContigId = Map_State->contigIds[target];
            if (targetContigId < (Contigs->numberOfContigs - 1))
            {
                contig *targetContig = Contigs->contigs + (target ? targetContigId + 1 : 0);
                u16 targetCoord = targetContig->startCoord;
                while (delta < 0 && (Map_State->contigIds[target] != (target ? targetContigId + 1 : 0) || Map_State->contigRelCoords[target] != targetCoord))
                {
                    ++target;
                    ++delta;
                }
            }
            else
            {
                delta = 0;
            }
        }
    }

    if (delta)
    {
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
        u16 *tmpBuffer3 = PushArray(Working_Set, u16, copySize);
        u16 *tmpBuffer4 = PushArray(Working_Set, u16, copySize);

        glBindBuffer(GL_TEXTURE_BUFFER, Contact_Matrix->pixelRearrangmentLookupBuffer);
        u16 *buffer = (u16 *)glMapBufferRange(GL_TEXTURE_BUFFER, 0, nPixels * sizeof(u16), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

        if (buffer)
        {
            ForLoop(copySize)
            {
                tmpBuffer[index] = buffer[GetRealBufferLocation(index + startCopyFromRange)];
                tmpBuffer2[index] = Map_State->originalContigIds[GetRealBufferLocation(index + startCopyFromRange)];
                tmpBuffer3[index] = Map_State->contigRelCoords[GetRealBufferLocation(index + startCopyFromRange)];
                tmpBuffer4[index] = Map_State->scaffIds[GetRealBufferLocation(index + startCopyFromRange)];
            }

            if (forward)
            {
                ForLoop(nPixelsInRange)
                {
                    buffer[GetRealBufferLocation(pixelTo + (u32)delta - index)] = buffer[GetRealBufferLocation(pixelTo - index)];
                    Map_State->originalContigIds[GetRealBufferLocation(pixelTo + (u32)delta - index)] = Map_State->originalContigIds[GetRealBufferLocation(pixelTo - index)];
                    Map_State->contigRelCoords[GetRealBufferLocation(pixelTo + (u32)delta - index)] = Map_State->contigRelCoords[GetRealBufferLocation(pixelTo - index)];
                    Map_State->scaffIds[GetRealBufferLocation(pixelTo + (u32)delta - index)] = Map_State->scaffIds[GetRealBufferLocation(pixelTo - index)];
                }
            }
            else
            {
                ForLoop(nPixelsInRange)
                {
                    buffer[GetRealBufferLocation((u32)((s32)pixelFrom + delta) + index)] = buffer[GetRealBufferLocation(pixelFrom + index)];
                    Map_State->originalContigIds[GetRealBufferLocation((u32)((s32)pixelFrom + delta) + index)] = Map_State->originalContigIds[GetRealBufferLocation(pixelFrom + index)];
                    Map_State->contigRelCoords[GetRealBufferLocation((u32)((s32)pixelFrom + delta) + index)] = Map_State->contigRelCoords[GetRealBufferLocation(pixelFrom + index)];
                    Map_State->scaffIds[GetRealBufferLocation((u32)((s32)pixelFrom + delta) + index)] = Map_State->scaffIds[GetRealBufferLocation(pixelFrom + index)];
                }
            }

            ForLoop(copySize)
            {
                buffer[GetRealBufferLocation(index + startCopyToRange)] = tmpBuffer[index];
                Map_State->originalContigIds[GetRealBufferLocation(index + startCopyToRange)] = tmpBuffer2[index];
                Map_State->contigRelCoords[GetRealBufferLocation(index + startCopyToRange)] = tmpBuffer3[index];
                Map_State->scaffIds[GetRealBufferLocation(index + startCopyToRange)] = tmpBuffer4[index];
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
        FreeLastPush(Working_Set); // tmpBuffer3
        FreeLastPush(Working_Set); // tmpBuffer4

        UpdateContigsFromMapState();
        
        map_edit edit;
        edit.finalPix1 = (u16)GetRealBufferLocation((u32)((s32)pixelFrom + delta));
        edit.finalPix2 = (u16)GetRealBufferLocation((u32)((s32)pixelTo + delta));
        edit.delta = (s16)delta;
        MoveWayPoints(&edit);
    }

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

    if (Waypoint_Edit_Mode)
    {
        Global_Mode = mode_normal;
        if (Tool_Tip->on)
        {
            f64 mousex, mousey;
            glfwGetCursorPos(window, &mousex, &mousey);
            MouseMove(window, mousex, mousey);
        }
    }
    else if (Normal_Mode)
    {
        Global_Mode = mode_waypoint_edit;
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
u32
ToggleScaffMode(GLFWwindow* window)
{
    u32 result = 1;

    if (Scaff_Edit_Mode && !Scaff_Painting_Flag)
    {
        Global_Mode = mode_normal;
        Scaff_Painting_Flag = 0;
        Scaff_Painting_Id = 0;
        if (Tool_Tip->on)
        {
            f64 mousex, mousey;
            glfwGetCursorPos(window, &mousex, &mousey);
            MouseMove(window, mousex, mousey);
        }
    }
    else if (Normal_Mode)
    {
        Global_Mode = mode_scaff_edit;
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

#if 0
enum
MouseButon
{
    left,
    right,
    middle,
};

struct
KeyBindings
{
    s32 ui;
    s32 editMode;
    s32 waypointMode;
    s32 undoEdit;
    s32 redoEdit;
    s32 selectWholeSeq_key;
    MouseButon selectWholeSeq_mouse;

};
#endif

global_variable
s32
Windowed_Xpos, Windowed_Ypos, Windowed_Width, Windowed_Height;

#if 0
struct
GLFWKeyAndScancode
{
    s32 key;
    s32 scancode;
};

global_variable
GLFWKeyAndScancode
LastPressedKey = {0, 0};
#endif

global_variable
nk_keys
NK_Pressed_Keys[1024] = {};

global_variable
u32
NK_Pressed_Keys_Ptr = 0;

global_variable
u32
GatheringTextInput = 0;

global_function
void
KeyBoard(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods)
{
    if (!Loading && (action != GLFW_RELEASE || key == GLFW_KEY_SPACE))
    {
        if (UI_On)
        {
#if 0
            LastPressedKey.key = key;
            LastPressedKey.scancode = scancode;
#else
            (void)scancode;
#endif

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
#ifdef Internal
            else if (key == GLFW_KEY_ESCAPE && !mods) 
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
#endif
            /*nk_input_key(NK_Context, NK_KEY_DEL, glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS);
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
            }*/


            else if (key == GLFW_KEY_U)
            {
                Deferred_Close_UI = 1;
            }
            else if (action != GLFW_RELEASE && !GatheringTextInput)
            {
                nk_keys addKey = NK_KEY_NONE;
                switch (key)
                {
                    case GLFW_KEY_DELETE:
                        addKey = NK_KEY_DEL;
                        break;

                    case GLFW_KEY_ENTER:
                        addKey = NK_KEY_ENTER;
                        break;

                    case GLFW_KEY_TAB:
                        addKey = NK_KEY_TAB;
                        break;

                    case GLFW_KEY_BACKSPACE:
                        addKey = NK_KEY_BACKSPACE;
                        break;

                    case GLFW_KEY_LEFT:
                        addKey = NK_KEY_LEFT;
                        break;

                    case GLFW_KEY_RIGHT:
                        addKey = NK_KEY_RIGHT;
                        break;

                    case GLFW_KEY_UP:
                        addKey = NK_KEY_UP;
                        break;

                    case GLFW_KEY_DOWN:
                        addKey = NK_KEY_DOWN;
                        break;
                }

                if (addKey != NK_KEY_NONE)
                {
                    NK_Pressed_Keys[NK_Pressed_Keys_Ptr++] = addKey;
                    if (NK_Pressed_Keys_Ptr == ArrayCount(NK_Pressed_Keys)) NK_Pressed_Keys_Ptr = 0;
                }
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
                    if (Edit_Mode)
                    {
                        RedoMapEdit();
                    }
                    else
                    {
                        keyPressed = ToggleWaypointMode(window);
                    }
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
                
                case GLFW_KEY_SPACE:
                    if (Edit_Mode && !Edit_Pixels.editing && action == GLFW_PRESS)
                    {
                        Edit_Pixels.selecting = 1;
                        Edit_Pixels.selectPixels = Edit_Pixels.pixels;
                        f64 x, y;
                        glfwGetCursorPos(window, &x, &y);
                        MouseMove(window, x, y);
                    }
                    else if (Edit_Mode && !Edit_Pixels.editing && action == GLFW_RELEASE)
                    {
                        Edit_Pixels.editing = 1;
                        Edit_Pixels.selecting = 0;
                        f64 x, y;
                        glfwGetCursorPos(window, &x, &y);
                        MouseMove(window, x, y);
                    }
                    else if (Edit_Mode && Edit_Pixels.editing && action == GLFW_PRESS)
                    {
                        InvertMap(Edit_Pixels.pixels.x, Edit_Pixels.pixels.y);
                        Global_Edit_Invert_Flag = !Global_Edit_Invert_Flag;
                    }
                    else if (Waypoint_Edit_Mode && Selected_Waypoint && action == GLFW_PRESS)
                    {
                        f64 x, y;
                        glfwGetCursorPos(window, &x, &y);
                        RemoveWayPoint(Selected_Waypoint);
                        MouseMove(window, x, y);
                    }
                    else if (Scaff_Edit_Mode && (action == GLFW_PRESS || action == GLFW_RELEASE))
                    {
                        f64 x, y;
                        glfwGetCursorPos(window, &x, &y);
                        Scaff_Painting_Flag = action == GLFW_PRESS ? 2 : 0;
                        MouseMove(window, x, y);
                        if (action == GLFW_RELEASE) UpdateScaffolds();
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
                    if (Edit_Mode)
                    {
                        Edit_Pixels.snap = !Edit_Pixels.snap;
                    }
                    else
                    {
                        keyPressed = ToggleScaffMode(window);
                    }
                    break;

                case GLFW_KEY_D:
                    if (Scaff_Edit_Mode)
                    {
                        ForLoop(Contigs->numberOfContigs) (Contigs->contigs + index)->scaffId = 0;
                        UpdateScaffolds();
                    }
                    else keyPressed = 0;
                    break;

                case GLFW_KEY_I:
                    Contig_Ids->on = !Contig_Ids->on;
                    break;

                case GLFW_KEY_U:
                    UI_On = !UI_On;
                    ++NK_Device->lastContextMemory[0];
                    Mouse_Move.x = Mouse_Move.y = -1;
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
#ifdef Internal
                case GLFW_KEY_ESCAPE:
                    if (!mods)
                    {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }
                    break;
#endif
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
u32
StringIsLexBigger(char *string, char *toCompareTo)
{
    u32 result;
    u32 equal;

    do
    {
        equal = *string == *toCompareTo;
        result = *string > *(toCompareTo++);
    } while (equal && (*(string++) != '\0'));

    return(result);
}

global_function
void
CharArrayBubbleSort(char **list, u32 size)
{
    while (size > 1)
    {
        u32 newSize = 0;
        ForLoop(size - 1)
        {    
            if (StringIsLexBigger(list[index], list[index + 1]))
            {
                char *tmp = list[index];
                list[index] = list[index + 1];
                list[index + 1] = tmp;
                newSize = index + 1;
            }
        }
        size = newSize;
    }
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
    
    CharArrayBubbleSort(results, (u32)size);
    
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

global_variable
char
Save_State_Name_Buffer[1024] = {0};

global_variable
char
AGP_Name_Buffer[1024] = {0};

global_function
void
SetSaveStateNameBuffer(char *name)
{
    u32 ptr = 0;
    while (*name) 
    {
        AGP_Name_Buffer[ptr] = *name;
        Save_State_Name_Buffer[ptr++] = *name++;
    }
    
    u32 ptr1 = ptr;
    name = (char *)".savestate_1";
    while (*name) Save_State_Name_Buffer[ptr++] = *name++;
    Save_State_Name_Buffer[ptr] = 0;

    ptr = ptr1;
    name = (char *)".agp_1";
    while (*name) AGP_Name_Buffer[ptr++] = *name++;
    AGP_Name_Buffer[ptr] = 0;
}

global_function
u08
FileBrowserRun(const char *name, struct file_browser *browser, struct nk_context *ctx, u32 show, u08 save = 0)
{
#ifndef _WIN32
    char pathSep = '/';
#else
    char pathSep = '\\';
#endif
   
    struct nk_window *window = nk_window_find(ctx, name);
    u32 doesExist = window != 0;

    if (!show && !doesExist)
    {
        return(0);
    }

    if (show && doesExist && (window->flags & NK_WINDOW_HIDDEN))
    {
        window->flags &= ~(nk_flags)NK_WINDOW_HIDDEN;
        FileBrowserReloadDirectoryContent(browser, browser->directory);
    }

    u08 ret = 0;
    struct media *media = browser->media;
    struct nk_rect total_space;

    if (nk_begin(ctx, name, nk_rect(Screen_Scale.x * 50, Screen_Scale.y * 50, Screen_Scale.x * 800, Screen_Scale.y * 600),
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
            nk_layout_row_dynamic(ctx, Screen_Scale.y * 25.0f, 6);
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
        f32 endSpace = save ? 50.0f : 0;
        total_space = nk_window_get_content_region(ctx);
        nk_layout_row(ctx, NK_DYNAMIC, total_space.h - endSpace, 2, ratio);
        nk_group_begin(ctx, "Special", NK_WINDOW_NO_SCROLLBAR);
        {
            struct nk_image home = media->icons.home;
            struct nk_image computer = media->icons.computer;

            nk_layout_row_dynamic(ctx, Screen_Scale.y * 40.0f, 1);
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
            size_t i = 0, j = 0;//, k = 0;
            size_t rows = 0, cols = 0;
            size_t count = browser->dir_count + browser->file_count;
            f32 iconRatio[] = {0.05f, NK_UNDEFINED};

            cols = 1;
            rows = count / cols;
            for (   i = 0;
                    i <= rows;
                    i += 1)
            {
                {
                    size_t n = j + cols;
                    nk_layout_row(ctx, NK_DYNAMIC, Screen_Scale.y * 25.0f, 2, iconRatio);
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
                            nk_label(ctx, browser->directories[j], NK_TEXT_LEFT);
                        } 
                        else 
                        {
                            /* draw and execute files buttons */
                            struct nk_image *icon;
                            size_t fileIndex = ((size_t)j - browser->dir_count);
                            icon = MediaIconForFile(media,browser->files[fileIndex]);
                            if (nk_button_image(ctx, *icon))
                            {
                                if (save)
                                {
                                    strncpy(save == 2 ? AGP_Name_Buffer : Save_State_Name_Buffer, browser->files[fileIndex], sizeof(save == 2 ? AGP_Name_Buffer : Save_State_Name_Buffer));
                                }
                                else
                                {
                                    strncpy(browser->file, browser->directory, MAX_PATH_LEN);
                                    n = strlen(browser->file);
                                    strncpy(browser->file + n, browser->files[fileIndex], MAX_PATH_LEN - n);
                                    ret = 1;
                                }
                            }
                            nk_label(ctx,browser->files[fileIndex],NK_TEXT_LEFT);
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

        if (save)
        {
            Deferred_Close_UI = 0;

            nk_layout_row(ctx, NK_DYNAMIC, endSpace - 5.0f, 1, ratio + 1);
            nk_group_begin(ctx, "File", NK_WINDOW_NO_SCROLLBAR);
            {
                f32 fileRatio[] = {0.8f, 0.1f, NK_UNDEFINED};
                f32 fileRatio2[] = {0.62f, 0.1f, 0.18f, NK_UNDEFINED};
                nk_layout_row(ctx, NK_DYNAMIC, Screen_Scale.y * 35.0f, save == 2 ? 4 : 3, save == 2 ? fileRatio2 : fileRatio);

                u08 saveViaEnter = (nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, save == 2 ? AGP_Name_Buffer : Save_State_Name_Buffer, sizeof(save == 2 ? AGP_Name_Buffer : Save_State_Name_Buffer), 0) & NK_EDIT_COMMITED) ? 1 : 0;
                
                static u08 overwrite = 0;
                overwrite = (u08)nk_check_label(ctx, "Override", overwrite);
                
                static u08 singletons = 0;
                if (save == 2) singletons = (u08)nk_check_label(ctx, "Format Singletons", singletons);

                if (nk_button_label(ctx, "Save") || saveViaEnter)
                {
                    strncpy(browser->file, browser->directory, MAX_PATH_LEN);
                    size_t n = strlen(browser->file);
                    strncpy(browser->file + n, save == 2 ? AGP_Name_Buffer : Save_State_Name_Buffer, MAX_PATH_LEN - n);
                    ret = 1 | (overwrite ? 2 : 0) | (singletons ? 4 : 0);
                }

                nk_group_end(ctx);
            }
        }
        
        nk_style_set_font(ctx, &NK_Font->handle);
    }
    nk_end(ctx);
    return(ret);
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

    enum windowMode {showAcknowledgements, showLicence, showThirdParty};

    static windowMode mode = showAcknowledgements;

    if (nk_begin_titled(ctx, "About", PretextView_Version, nk_rect(Screen_Scale.x * 50, Screen_Scale.y * 50, Screen_Scale.x * 870, Screen_Scale.y * 610),
                NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE|NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE))
    {
        nk_menubar_begin(ctx);
        {
            nk_layout_row_dynamic(ctx, Screen_Scale.y * 35.0f, 3);
            if (nk_button_label(ctx, "Acknowledgements"))
            {
                mode = showAcknowledgements;
            }
            if (nk_button_label(ctx, "Licence"))
            {
                mode = showLicence;
            }
            if (nk_button_label(ctx, "Third Party Software"))
            {
                mode = showThirdParty;
            }
        }
        nk_menubar_end(ctx);

        struct nk_rect total_space = nk_window_get_content_region(ctx);
        f32 one = NK_UNDEFINED;
        nk_layout_row(ctx, NK_DYNAMIC, total_space.h, 1, &one);

        nk_group_begin(ctx, "About_Content", 0);
        {
            if (mode == showThirdParty)
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
                        nk_layout_row_static(ctx, Screen_Scale.y * (f32)sizes[0], (s32)(Screen_Scale.x * (f32)sizes[1]), 1);
                        len = (s32)StringLength(ThirdParty[licenceIndex]);
                        nk_edit_string(ctx, NK_EDIT_READ_ONLY | NK_EDIT_NO_CURSOR | NK_EDIT_SELECTABLE | NK_EDIT_MULTILINE, (char *)ThirdParty[licenceIndex], &len, len, 0);
                        nk_tree_pop(NK_Context);
                    }
                }
            }
            else if (mode == showAcknowledgements)
            {
                nk_layout_row_static(ctx, Screen_Scale.y * 500, (s32)(Screen_Scale.x * 820), 1);
                s32 len = sizeof(Acknowledgements);
                nk_edit_string(ctx, NK_EDIT_READ_ONLY | NK_EDIT_NO_CURSOR | NK_EDIT_SELECTABLE | NK_EDIT_MULTILINE, (char *)Acknowledgements, &len, len, 0);
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
            folder = (char *)"PretextView";
            folder2 = 0;
        }
        else
        {
            path = getenv("HOME");
            if (!path) path = getpwuid(getuid())->pw_dir;
            folder = (char *)".config";
            folder2 = (char *)"PretextView";
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
        PWSTR path = 0;
        char sep = '\\';
        HRESULT hres = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, &path);
        if (SUCCEEDED(hres))
        {
            PWSTR pathPtr = path;
            while ((*buff++ = *pathPtr++)) {}
            *(buff - 1) = sep;
            char *folder = (char *)"PretextView";
            while ((*buff++ = *folder++)) {}

            if (CreateDirectory((char *)buff_, NULL) || ERROR_ALREADY_EXISTS == GetLastError())
            {
                u32 n = StringLength((u08*)buff_);
                SaveState_Path = PushArray(Working_Set, u08, n + 18);
                CopyNullTerminatedString((u08*)buff_, SaveState_Path);
                SaveState_Path[n] = (u08)sep;
                SaveState_Name = SaveState_Path + n + 1;
                SaveState_Path[n + 17] = 0;
            }
        }

        if (path) CoTaskMemFree(path);
#endif
    }
}

global_variable
u08 SaveState_Magic[5] = {'p', 't', 's', 'x', '6'};

global_function
u08
SaveState(u64 headerHash, char *path = 0, u08 overwrite = 0)
{
    if (!path)
    {
        static u08 flipFlop = 0;
        headerHash += (u64)flipFlop;
        flipFlop = (flipFlop + 1) & 1;

        if (!SaveState_Path)
        {
            SetSaveStatePaths();
        }
    }
    
    if (path || SaveState_Path)
    {
        if (!path)
        {
            ForLoop(16)
            {
                u08 x = (u08)((headerHash >> (4 * index)) & 0xF);
                u08 c = 'a' + x;
                *(SaveState_Name + index) = c;
            }
        }
        
        u32 nEdits = Min(Edits_Stack_Size, Map_Editor->nEdits);
        u32 nWayp = Waypoint_Editor->nWaypointsActive;

        u32 nGraphPlots = 0;
        TraverseLinkedList(Extensions.head, extension_node)
        {
            switch (node->type)
            {
                case extension_graph:
                    {
                        ++nGraphPlots;
                    }
                    break;
            }
        }

        u16 nScaffs = 0;
        ForLoop(Contigs->numberOfContigs) if ((Contigs->contigs + index)->scaffId) ++nScaffs;

        u32 nFileBytes = 350 + (13 * nWayp) + (6 * nEdits) + ((nEdits + 7) >> 3) + (32 * nGraphPlots) + (4 * nScaffs);
        u08 *fileContents = PushArrayP(Loading_Arena, u08, nFileBytes);
        u08 *fileWriter = fileContents;

        // settings
        {
            u08 settings = (u08)Current_Theme;
            *fileWriter++ = settings;
           
            settings = 0;
            settings |= Waypoints_Always_Visible ? (1 << 0) : 0;
            settings |= Contig_Name_Labels->on ? (1 << 1) : 0;
            settings |= Scale_Bars->on ? (1 << 2) : 0;
            settings |= Grid->on ? (1 << 3) : 0;
            settings |= Contig_Ids->on ? (1 << 4) : 0;
            settings |= Tool_Tip->on ? (1 << 5) : 0;
            settings |= Mouse_Invert ? (1 << 6) : 0;
            settings |= Scaffs_Always_Visible ? (1 << 7) : 0;

            *fileWriter++ = settings;
        }
        
        // colours
        {
            ForLoop(32)
            {
                *fileWriter++ = ((u08 *)Scaff_Mode_Data)[index];
            }

            ForLoop(64)
            {
                *fileWriter++ = ((u08 *)Waypoint_Mode_Data)[index];
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

        // sizes
        {
            ForLoop(4)
            {
                *fileWriter++ = ((u08 *)Scaff_Mode_Data)[index + 32];
            }

            ForLoop(4)
            {
                *fileWriter++ = ((u08 *)Waypoint_Mode_Data)[index + 64];
            }

            ForLoop(4)
            {
                *fileWriter++ = ((u08 *)Contig_Name_Labels)[index + 36];
            }

            ForLoop(4)
            {
                *fileWriter++ = ((u08 *)Scale_Bars)[index + 36];
            }

            ForLoop(4)
            {
                *fileWriter++ = ((u08 *)Grid)[index + 20];
            }

            ForLoop(4)
            {
                *fileWriter++ = ((u08 *)Contig_Ids)[index + 20];
            }

            ForLoop(4)
            {
                *fileWriter++ = ((u08 *)Tool_Tip)[index + 36];
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
            ForLoop(4)
            {
                *fileWriter++ = ((u08 *)&nEdits)[index];
            }

            u32 editStackPtr = Map_Editor->editStackPtr == nEdits ? 0 : Map_Editor->editStackPtr;
            u32 nContigFlags = (nEdits + 7) >> 3;
            u08 *contigFlags = fileWriter + (6 * nEdits);
            memset(contigFlags, 0, nContigFlags);

            ForLoop(nEdits)
            {
                if (editStackPtr == nEdits)
                {
                    editStackPtr = 0;
                }
                
                map_edit *edit = Map_Editor->edits + editStackPtr;
                u16 x = (u16)((s32)edit->finalPix1 - edit->delta);
                u16 y = (u16)((s32)edit->finalPix2 - edit->delta);
                s16 d = (s16)edit->delta;

                *fileWriter++ = ((u08 *)&x)[0];
                *fileWriter++ = ((u08 *)&x)[1];
                *fileWriter++ = ((u08 *)&y)[0];
                *fileWriter++ = ((u08 *)&y)[1];
                *fileWriter++ = ((u08 *)&d)[0];
                *fileWriter++ = ((u08 *)&d)[1];

                if (edit->finalPix1 > edit->finalPix2)
                {
                    u32 byte = (index + 1) >> 3;
                    u32 bit = (index + 1) & 7;

                    contigFlags[byte] |= (1 << bit);
                }

                ++editStackPtr;
            }

            fileWriter += nContigFlags;
        }

        // waypoints
        {
            *fileWriter++ = (u08)nWayp;
#ifdef DEBUG
            u08 breakHere = 0;
            if (path)
                breakHere = 1;
            (void)breakHere;
#endif

            u32 ptr = 348 + (13 * nWayp) + (6 * nEdits) + ((nEdits + 7) >> 3);
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

            fileWriter += (13 * nWayp);
        }

        // scaffs
        {
            *fileWriter++ = ((u08 *)&nScaffs)[0];
            *fileWriter++ = ((u08 *)&nScaffs)[1];
            ForLoop(Contigs->numberOfContigs)
            {
                if ((Contigs->contigs + index)->scaffId)
                {
                    u16 cId = (u16)index;
                    u16 sId = (Contigs->contigs + index)->scaffId;
                    *fileWriter++ = ((u08 *)&cId)[0];
                    *fileWriter++ = ((u08 *)&cId)[1];
                    *fileWriter++ = ((u08 *)&sId)[0];
                    *fileWriter++ = ((u08 *)&sId)[1];
                }
            }
        }

        // extensions
        {
            TraverseLinkedList(Extensions.head, extension_node)
            {
                switch (node->type)
                {
                    case extension_graph:
                        {
                            graph *gph = (graph *)node->extension;
                            ForLoop(32)
                            {
                                *fileWriter++ = ((u08 *)gph)[index + 88];
                            }
                        }
                        break;
                }
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

        FILE *file;
        if (path)
        {
            if (!overwrite)
            {
                if ((file = fopen((const char *)path, "rb")))
                {
                    fclose(file);
                    return(1);
                }
            }

            if (!(file = fopen((const char *)path, "wb"))) return(1);

            fwrite(SaveState_Magic, 1, sizeof(SaveState_Magic) - 1, file);
            fwrite("5", 1, 1, file);
            fwrite(&headerHash, 1, 8, file);
        }
        else
        {
            file = fopen((const char *)SaveState_Path, "wb");
            fwrite(SaveState_Magic, 1, sizeof(SaveState_Magic), file);
        }
        
        fwrite(&nCommpressedBytes, 1, 4, file);
        fwrite(&nFileBytes, 1, 4, file);
        fwrite(compBuff, 1, nCommpressedBytes, file);
        fclose(file);

        FreeLastPushP(Loading_Arena); // compBuff
        FreeLastPushP(Loading_Arena); // fileContents

        if (!path)
        {
            u08 nameCache[17];
            CopyNullTerminatedString((u08 *)SaveState_Name, (u08 *)nameCache);

            *(SaveState_Name + 0) = 'p';
            *(SaveState_Name + 1) = 't';
            *(SaveState_Name + 2) = 'l';
            *(SaveState_Name + 3) = 's';
            *(SaveState_Name + 4) = 'n';
            *(SaveState_Name + 5) = '\0';

            file = fopen((const char *)SaveState_Path, "wb");
            fwrite((u08 *)nameCache, 1, sizeof(nameCache), file);
            fclose(file);
        }
    }

    return(0);
}

global_function
u08
LoadState(u64 headerHash, char *path)
{
    u08 oldSytle = 0;

    if (!path && !SaveState_Path)
    {
        SetSaveStatePaths();
    }
    
    if (path || SaveState_Path)
    {
        FILE *file = 0;
        u08 fullLoad = 1;
        
        if (path)
        {
            if ((file = fopen((const char *)path, "rb")))
            {
                u08 magicTest[sizeof(SaveState_Magic)];

                u32 bytesRead = (u32)fread(magicTest, 1, sizeof(magicTest), file);
                if (bytesRead == sizeof(magicTest))
                {
                    ForLoop(sizeof(SaveState_Magic) - 1)
                    {
                        if (SaveState_Magic[index] != magicTest[index])
                        {
                            fclose(file);
                            file = 0;
                            break;
                        }
                    }
                    if (file)
                    {
                        if (magicTest[sizeof(SaveState_Magic)-1] != '3' && magicTest[sizeof(SaveState_Magic)-1] != '5')
                        {
                            fclose(file);
                            file = 0;
                        }
                        else
                        {
                            oldSytle = magicTest[sizeof(SaveState_Magic)-1] == '3';
                            u64 hashTest;
                            bytesRead = (u32)fread(&hashTest, 1, sizeof(hashTest), file);
                            if (!(bytesRead == sizeof(hashTest) && hashTest == headerHash))
                            {
                                fclose(file);
                                file = 0;
                            }
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
        else
        {
            ForLoop(16)
            {
                u08 x = (u08)((headerHash >> (4 * index)) & 0xF);
                u08 c = 'a' + x;
                *(SaveState_Name + index) = c;
            }

            if ((file = fopen((const char *)SaveState_Path, "rb")))
            {
                u08 magicTest[sizeof(SaveState_Magic)];

                u32 bytesRead = (u32)fread(magicTest, 1, sizeof(magicTest), file);
                if (bytesRead == sizeof(magicTest))
                {
                    ForLoop(sizeof(SaveState_Magic) - 1)
                    {
                        if (SaveState_Magic[index] != magicTest[index])
                        {
                            fclose(file);
                            file = 0;
                            break;
                        }
                    }
                    if (file)
                    {
                        if (magicTest[sizeof(SaveState_Magic)-1] != '4' && magicTest[sizeof(SaveState_Magic)-1] != '6')
                        {
                            fclose(file);
                            file = 0;
                        }
                        oldSytle = magicTest[sizeof(SaveState_Magic)-1] == '4';
                    }
                }
                else
                {
                    fclose(file);
                    file = 0;
                }
            }
            else
            {
                fullLoad = 0;
                u08 nameCache[16];
                *(SaveState_Name + 0) = 'p';
                *(SaveState_Name + 1) = 't';
                *(SaveState_Name + 2) = 'l';
                *(SaveState_Name + 3) = 's';
                *(SaveState_Name + 4) = 'n';
                *(SaveState_Name + 5) = '\0';

                if ((file = fopen((const char *)SaveState_Path, "rb")))
                {
                    if (fread((u08 *)nameCache, 1, sizeof(nameCache), file) == sizeof(nameCache))
                    {
                        fclose(file);
                        file = 0;
                        ForLoop(16)
                        {
                            *(SaveState_Name + index) = nameCache[index];
                        }
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

                    }
                    else
                    {
                        fclose(file);
                        file = 0;
                    }
                }
            }
        }
        
        if (file)
        {
            u32 nBytesComp;
            u32 nBytesFile;
            fread(&nBytesComp, 1, 4, file);
            fread(&nBytesFile, 1, 4, file);
            u32 nBytesRead = 0;

            u08 *fileContents = PushArrayP(Loading_Arena, u08, nBytesFile);
            u08 *compBuffer = PushArrayP(Loading_Arena, u08, nBytesComp);

            fread(compBuffer, 1, nBytesComp, file);
            fclose(file);
            if (libdeflate_deflate_decompress(Decompressor, (const void *)compBuffer, nBytesComp, (void *)fileContents, nBytesFile, NULL))
            {
                FreeLastPushP(Loading_Arena); // comp buffer
                FreeLastPushP(Loading_Arena); // fileContents
                return(1);
            }
            FreeLastPushP(Loading_Arena); // comp buffer

            // settings
            {
                u08 settings = *fileContents++;
                theme th = (theme)settings;
                SetTheme(NK_Context, th);

                settings = *fileContents++;
                Waypoints_Always_Visible = settings & (1 << 0);
                Contig_Name_Labels->on = settings & (1 << 1);
                Scale_Bars->on = settings & (1 << 2);
                Grid->on = settings & (1 << 3);
                Contig_Ids->on = settings & (1 << 4);
                Tool_Tip->on = settings & (1 << 5);
                Mouse_Invert = settings & (1 << 6);
                Scaffs_Always_Visible = settings & (1 << 7);

                nBytesRead += 2;
            }

            // colours
            {
                ForLoop(32)
                {
                    ((u08 *)Scaff_Mode_Data)[index] = *fileContents++;
                }

                ForLoop(64)
                {
                    ((u08 *)Waypoint_Mode_Data)[index] = *fileContents++;
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

                nBytesRead += 288;
            }

            // sizes
            {
                ForLoop(4)
                {
                    ((u08 *)Scaff_Mode_Data)[index + 32] = *fileContents++;
                }

                ForLoop(4)
                {
                    ((u08 *)Waypoint_Mode_Data)[index + 64] = *fileContents++;
                }

                ForLoop(4)
                {
                    ((u08 *)Contig_Name_Labels)[index + 36] = *fileContents++;
                }

                ForLoop(4)
                {
                    ((u08 *)Scale_Bars)[index + 36] = *fileContents++;
                }

                ForLoop(4)
                {
                    ((u08 *)Grid)[index + 20] = *fileContents++;
                }

                ForLoop(4)
                {
                    ((u08 *)Contig_Ids)[index + 20] = *fileContents++;
                }

                ForLoop(4)
                {
                    ((u08 *)Tool_Tip)[index + 36] = *fileContents++;
                }

                nBytesRead += 28;
            }

            // colour map
            {
                u08 colourMap = *fileContents++;

                Color_Maps->currMap = (u32)colourMap;
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_BUFFER, Color_Maps->maps[Color_Maps->currMap]);
                glActiveTexture(GL_TEXTURE0);

                ++nBytesRead;
            }

            // gamma
            {
                ForLoop(12)
                {
                    ((u08 *)Color_Maps->controlPoints)[index] = *fileContents++;
                }
                nBytesRead += 12;

                glUseProgram(Contact_Matrix->shaderProgram);
                glUniform3fv(Color_Maps->cpLocation, 1, Color_Maps->controlPoints);
            }

            if (fullLoad)
            {
                // camera
                {
                    ForLoop(12)
                    {
                        ((u08 *)&Camera_Position)[index] = *fileContents++;
                    }
                    nBytesRead += 12;
                }

                // edits
                {
                    u32 nEdits  = Min(Edits_Stack_Size, Map_Editor->nEdits);
                    ForLoop(nEdits) UndoMapEdit();

                    ForLoop(4) ((u08 *)&nEdits)[index] = *fileContents++;
                    nBytesRead += 4;

                    u08 *contigFlags = fileContents + (6 * nEdits);
                    u32 nContigFlags = ((u32)nEdits + 7) >> 3;

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

                        u32 byte = (index + 1) >> 3;
                        u32 bit = (index + 1) & 7;
                        u32 invert  = contigFlags[byte] & (1 << bit);

                        pointui startPixels = {(u32)x, (u32)y};
                        s32 delta = (s32)d;
                        pointui finalPixels = {(u32)((s32)startPixels.x + delta), (u32)((s32)startPixels.y + delta)};

                        RearrangeMap(startPixels.x, startPixels.y, delta);
                        if (invert) InvertMap(finalPixels.x, finalPixels.y);

                        AddMapEdit(delta, finalPixels, invert);
                    }

                    fileContents += nContigFlags;
                    nBytesRead += (nContigFlags + (6 * nEdits));
                }

                // waypoints
                {
                    TraverseLinkedList(Waypoint_Editor->activeWaypoints.next, waypoint)
                    {
                        waypoint *tmp = node->prev;
                        RemoveWayPoint(node);
                        node = tmp;
                    }
                    u08 nWayp = *fileContents++;
                    ++nBytesRead;

                    if (oldSytle) fileContents += 2;
                    //nBytesRead += 2;

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

                        if (oldSytle && index == (nWayp - 1)) ((u08 *)&z)[3] = 0;

                        AddWayPoint({x, y});
                        Waypoint_Editor->activeWaypoints.next->z = z;
                        
                        if (!oldSytle || index != (nWayp - 1)) Waypoint_Editor->activeWaypoints.next->index = (u32)id;
                    }

                    if (oldSytle) fileContents -= 2;
                    
                    nBytesRead += (13 * nWayp);
                }

                // scaffs
                {
                    u16 nScaffs;
                    ((u08 *)&nScaffs)[0] = *fileContents++;
                    ((u08 *)&nScaffs)[1] = *fileContents++;

                    nBytesRead += 2;

                    if (nScaffs) ForLoop(Contigs->numberOfContigs) (Contigs->contigs + index)->scaffId = 0;

                    ForLoop(nScaffs)
                    {
                        u16 cId;
                        u16 sId;
                        ((u08 *)&cId)[0] = *fileContents++;
                        ((u08 *)&cId)[1] = *fileContents++;
                        ((u08 *)&sId)[0] = *fileContents++;
                        ((u08 *)&sId)[1] = *fileContents++;

                        (Contigs->contigs + cId)->scaffId = sId;
                    }

                    if (nScaffs) UpdateScaffolds();

                    nBytesRead += (4 * nScaffs);
                }

                // extensions
                {
                    TraverseLinkedList(Extensions.head, extension_node)
                    {
                        switch (node->type)
                        {
                            case extension_graph:
                                {
                                    if ((nBytesRead + 32) > nBytesFile) break;

                                    ForLoop(32)
                                    {
                                        ((u08 *)node->extension)[index + 88] = *fileContents++;
                                    }

                                    nBytesRead += 32;
                                }
                                break;
                        }
                    }
                }
            }

            Redisplay = 1;
            FreeLastPushP(Loading_Arena); // fileContents
        }
    }

    return(0);
}

global_variable
u32
Global_Text_Buffer[1024] = {0};

global_variable
u32
Global_Text_Buffer_Ptr = 0;

global_function
void
TextInput(GLFWwindow* window, u32 codepoint)
{
    if (!GatheringTextInput)
    {
        (void)window;
        
        Global_Text_Buffer[Global_Text_Buffer_Ptr++] = codepoint;
        
        if (Global_Text_Buffer_Ptr == ArrayCount(Global_Text_Buffer)) Global_Text_Buffer_Ptr = 0;
        //nk_input_unicode(NK_Context, codepoint); 
    }
}

global_function
void
CopyEditsToClipBoard(GLFWwindow *window)
{
    u32 nEdits = Min(Edits_Stack_Size, Map_Editor->nEdits);

    if (nEdits)
    {
        u32 bufferSize = 256 * nEdits;
        u08 *buffer = PushArray(Working_Set, u08, bufferSize);
        u32 bufferPtr = 0;

        u32 editStackPtr = Map_Editor->editStackPtr == nEdits ? 0 : Map_Editor->editStackPtr;

        f64 bpPerPixel = (f64)Total_Genome_Length / (f64)(Number_of_Textures_1D * Texture_Resolution);

        ForLoop(nEdits)
        {
            if (editStackPtr == nEdits)
            {
                editStackPtr = 0;
            }

            map_edit *edit = Map_Editor->edits + editStackPtr++;

            u16 start = Min(edit->finalPix1, edit->finalPix2);
            u16 end = Max(edit->finalPix1, edit->finalPix2);
            u16 to = start ? start - 1 : (end < (Number_of_Pixels_1D - 1) ? end + 1 : end);

            u32 oldFrom = Map_State->contigRelCoords[start];
            u32 oldTo = Map_State->contigRelCoords[end];
            u32 *name1 = (Original_Contigs + Map_State->originalContigIds[start])->name;
            u32 *name2 = (Original_Contigs + Map_State->originalContigIds[end])->name;
            u32 *newName = (Original_Contigs + Map_State->originalContigIds[to])->name;

            bufferPtr += (u32)stbsp_snprintf((char *)buffer + bufferPtr, (s32)(bufferSize - bufferPtr), "Edit %d:\n       %s[%$.2fbp] to %s[%$.2fbp]\n%s\n       %s[%$.2fbp]\n",
                    index + 1, (char *)name1, (f64)oldFrom * bpPerPixel, (char *)name2, (f64)oldTo * bpPerPixel,
                    edit->finalPix1 > edit->finalPix2 ? (const char *)"       inverted and moved to" : (const char *)"       moved to",
                    (char *)newName, (f64)Map_State->contigRelCoords[to] * bpPerPixel);
        }

        buffer[bufferPtr] = 0;
        glfwSetClipboardString(window, (const char *)buffer);
        
        FreeLastPush(Working_Set); // buffer
    }
}

global_function
void
GenerateAGP(char *path, u08 overwrite, u08 formatSingletons)
{
    FILE *file;
    if (!overwrite && (file = fopen((const char *)path, "rb")))
    {
        fclose(file);
        return;
    }

    if ((file = fopen((const char *)path, "w")))
    {
        char buffer[1024];

        stbsp_snprintf(buffer, sizeof(buffer), "##agp-version\t%s\n# DESCRIPTION: Generated by %s\n", formatSingletons ? "<NA>" : "2.1", PretextView_Version);
        fwrite(buffer, 1, strlen(buffer), file);

        if (formatSingletons)
        {
            stbsp_snprintf(buffer, sizeof(buffer), "# WARNING: Non-standard AGP, singletons maybe reverse-complemented\n");
            fwrite(buffer, 1, strlen(buffer), file);
        }

        u16 scaffId = 0;
        u64 scaffCoord_Start = 1;
        u32 scaffPart = 0;

        for (   u08 type = 0;
                type < 2;
                ++type)
        {
            ForLoop(Contigs->numberOfContigs)
            {
                contig *cont = Contigs->contigs + index;
                u08 invert = IsContigInverted(index);
                u16 startCoord = cont->startCoord - (invert ? (cont->length - 1) : 0);

                invert = (!formatSingletons && (!cont->scaffId || (index && (index < (Contigs->numberOfContigs - 1)) && (cont->scaffId != ((cont + 1)->scaffId)) && (cont->scaffId != ((cont - 1)->scaffId))) || (!index && (cont->scaffId != ((cont + 1)->scaffId))) || ((index == (Contigs->numberOfContigs - 1)) && (cont->scaffId != ((cont - 1)->scaffId))))) ? 0 : invert;
                
                u64 contRealStartCoord = (u64)((f64)startCoord / (f64)Number_of_Pixels_1D * (f64)Total_Genome_Length) + 1;
                u64 contRealEndCoord = (u64)((f64)(startCoord + cont->length) / (f64)Number_of_Pixels_1D * (f64)Total_Genome_Length);
                u64 contRealSize = contRealEndCoord - contRealStartCoord + 1;

                char *contName = (char *)((Original_Contigs + cont->originalContigId)->name);

                if (cont->scaffId && !type)
                {
                    if (cont->scaffId != scaffId)
                    {
                        scaffId = cont->scaffId;
                        scaffCoord_Start = 1;
                        scaffPart = 0;
                    }

                    u64 scaffCoord_End;

                    if (scaffPart)
                    {
                        scaffCoord_End = scaffCoord_Start + 99;
                        stbsp_snprintf(buffer, sizeof(buffer), "Scaffold_%u\t%" PRIu64 "\t%" PRIu64 "\t%u\tU\t100\tscaffold\tyes\tproximity_ligation\n", scaffId, scaffCoord_Start, scaffCoord_End, ++scaffPart);
                        fwrite(buffer, 1, strlen(buffer), file);

                        scaffCoord_Start = scaffCoord_End + 1;
                    }

                    scaffCoord_End = scaffCoord_Start + contRealSize - 1;
                    stbsp_snprintf(buffer, sizeof(buffer), "Scaffold_%u\t%" PRIu64 "\t%" PRIu64 "\t%u\tW\t%s\t%" PRIu64 "\t%" PRIu64 "\t%s\n", scaffId, scaffCoord_Start, scaffCoord_End, ++scaffPart, contName, contRealStartCoord, contRealEndCoord, invert ? "-" : "+");
                    fwrite(buffer, 1, strlen(buffer), file);

                    scaffCoord_Start = scaffCoord_End + 1;
                }
                else if (!cont->scaffId && type)
                {
                    stbsp_snprintf(buffer, sizeof(buffer), "Scaffold_%u\t1\t%" PRIu64 "\t1\tW\t%s\t%" PRIu64 "\t%" PRIu64 "\t%s\n", ++scaffId, contRealSize, contName, contRealStartCoord, contRealEndCoord, invert ? "-" : "+");
                    fwrite(buffer, 1, strlen(buffer), file);
                }
            }
        }

        fclose(file);
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

    Edit_Pixels.pixels.x = 0;
    Edit_Pixels.pixels.y = 0;
    Edit_Pixels.worldCoords.x = 0;
    Edit_Pixels.worldCoords.y = 0;
    Edit_Pixels.editing = 0;
    Edit_Pixels.selecting = 0;

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
    glfwSetCharCallback(window, TextInput);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_MULTISAMPLE);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback( MessageCallback, 0 );
#endif

    s32 width, height, display_width, display_height;
    glfwGetWindowSize(window, &width, &height);
    glfwGetFramebufferSize(window, &display_width, &display_height);
    Screen_Scale.x = (f32)display_width/(f32)width;
    Screen_Scale.y = (f32)display_height/(f32)height;

    // Cursors
    GLFWcursor *arrowCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    GLFWcursor *crossCursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);

    Setup();
    if (initWithFile)
    {
        UI_On = LoadFile((const char *)currFile, Loading_Arena, (char **)&currFileName, &headerHash) == ok ? 0 : 1;
        if (currFileName)
        {
            glfwSetWindowTitle(window, (const char *)currFileName);
            FenceIn(SetSaveStateNameBuffer((char *)currFileName));
        }
    }
    else
    {
        UI_On = 1;
    }

    // file browser
    struct file_browser browser;
    struct file_browser saveBrowser;
    struct file_browser loadBrowser;
    struct file_browser saveAGPBrowser;
    struct media media;
    {
        media.icons.home = IconLoad(IconHome, IconHome_Size);
        media.icons.directory = IconLoad(IconFolder, IconFolder_Size);
        media.icons.computer = IconLoad(IconDrive, IconDrive_Size);
        media.icons.default_file = IconLoad(IconFile, IconFile_Size);
        media.icons.img_file = IconLoad(IconImage, IconImage_Size);
        MediaInit(&media);
        FileBrowserInit(&browser, &media);
        FileBrowserInit(&saveBrowser, &media);
        FileBrowserInit(&loadBrowser, &media);
        FileBrowserInit(&saveAGPBrowser, &media);
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
            if (currFileName) SaveState(headerHash);
        }

        if (Loading)
        {
            if (currFileName) SaveState(headerHash);
            LoadFile((const char *)currFile, Loading_Arena, (char **)&currFileName, &headerHash);
            if (currFileName)
            {
                glfwSetWindowTitle(window, (const char *)currFileName);
                FenceIn(SetSaveStateNameBuffer((char *)currFileName));
            }
            glfwPollEvents();
            Loading = 0;
            Redisplay = 1;
        }

        if (UI_On)
        {
            glfwSetCursor(window, arrowCursor);
            f64 x, y;
            nk_input_begin(NK_Context);
            GatheringTextInput = 1;
            glfwPollEvents();

            /*nk_input_key(NK_Context, NK_KEY_DEL, glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS);
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
            }*/
            
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
          
            ForLoop(NK_Pressed_Keys_Ptr) nk_input_key(NK_Context, NK_Pressed_Keys[index], 1);
            NK_Pressed_Keys_Ptr = 0;
            
            ForLoop(Global_Text_Buffer_Ptr) nk_input_unicode(NK_Context, Global_Text_Buffer[index]);
            Global_Text_Buffer_Ptr = 0;

            GatheringTextInput = 0;
            nk_input_end(NK_Context);

            s32 showFileBrowser = 0;
            s32 showAboutScreen = 0;
            s32 showSaveStateScreen = 0;
            s32 showLoadStateScreen = 0;
            s32 showSaveAGPScreen = 0;
            static u32 currGroup1 = 0;
            static u32 currGroup2 = 0;
            static s32 currSelected1 = -1;
            static s32 currSelected2 = -1;
            {
                static nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCROLL_AUTO_HIDE | NK_WINDOW_TITLE;

                if (nk_begin_titled(NK_Context, "Options", currFileName ? (const char *)currFileName : "", nk_rect(Screen_Scale.x * 10, Screen_Scale.y * 10, Screen_Scale.x * 800, Screen_Scale.y * 350), flags))
                {
                    struct nk_rect bounds = nk_window_get_bounds(NK_Context);
                    bounds.w /= 8;
                    bounds.h /= 8;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 400), bounds))
                    { 
                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 2);
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

                    nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 5);
                    showFileBrowser = nk_button_label(NK_Context, "Load Map");
                    if (currFileName)
                    {
                        showSaveStateScreen = nk_button_label(NK_Context, "Save State");
                        showLoadStateScreen = nk_button_label(NK_Context, "Load State");
                        showSaveAGPScreen = nk_button_label(NK_Context, "Generate AGP");
                    }
                    showAboutScreen = nk_button_label(NK_Context, "About");

                    nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 3);
                    
                    bounds = nk_widget_bounds(NK_Context);
                    if ((nk_option_label(NK_Context, "Waypoint Edit Mode", Global_Mode == mode_waypoint_edit) ? 1 : 0) != (Global_Mode == mode_waypoint_edit ? 1 : 0)) Global_Mode = Global_Mode == mode_waypoint_edit ? mode_normal : mode_waypoint_edit;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 750, Screen_Scale.y * 420), bounds))
                    {
                        struct nk_colorf colour_text = Waypoint_Mode_Data->text;
                        struct nk_colorf colour_bg = Waypoint_Mode_Data->bg;
                        struct nk_colorf colour_base = Waypoint_Mode_Data->base;
                        struct nk_colorf colour_select = Waypoint_Mode_Data->selected;

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

                        Waypoint_Mode_Data->text = colour_text;
                        Waypoint_Mode_Data->bg = colour_bg;
                        Waypoint_Mode_Data->selected = colour_select;
                        Waypoint_Mode_Data->base = colour_base;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Size", NK_TEXT_CENTERED);
                        nk_slider_float(NK_Context, 1.0f, &Waypoint_Mode_Data->size, 2.0f * DefaultWaypointSize, 4.0f);
                        if (nk_button_label(NK_Context, "Default")) Waypoint_Mode_Data->size = DefaultWaypointSize;

                        nk_contextual_end(NK_Context);
                    }

                    bounds = nk_widget_bounds(NK_Context);
                    if ((nk_option_label(NK_Context, "Scaffold Edit Mode", Global_Mode == mode_scaff_edit) ? 1 : 0) != (Global_Mode == mode_scaff_edit ? 1 : 0)) Global_Mode = Global_Mode == mode_scaff_edit ? mode_normal : mode_scaff_edit;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 420), bounds))
                    {
                        struct nk_colorf colour_text = Scaff_Mode_Data->text;
                        struct nk_colorf colour_bg = Scaff_Mode_Data->bg;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Scaffold Mode Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 2);
                        nk_label(NK_Context, "Text", NK_TEXT_CENTERED);
                        nk_label(NK_Context, "Background", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 2);
                        colour_text = nk_color_picker(NK_Context, colour_text, NK_RGBA);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 2);
                        if (nk_button_label(NK_Context, "Default")) colour_text = Yellow_Text_Float;
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Scaff_Mode_Data->text = colour_text;
                        Scaff_Mode_Data->bg = colour_bg;

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Size", NK_TEXT_CENTERED);
                        nk_slider_float(NK_Context, 1.0f, &Scaff_Mode_Data->size, 2.0f * DefaultScaffSize, 4.0f);
                        if (nk_button_label(NK_Context, "Default")) Scaff_Mode_Data->size = DefaultScaffSize;

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

                    nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 2);
                    Waypoints_Always_Visible = nk_check_label(NK_Context, "Waypoints Always Visible", (s32)Waypoints_Always_Visible) ? 1 : 0;
                    Scaffs_Always_Visible = nk_check_label(NK_Context, "Scaffolds Always Visible", (s32)Scaffs_Always_Visible) ? 1 : 0;

                    bounds = nk_widget_bounds(NK_Context);
                    Contig_Name_Labels->on = nk_check_label(NK_Context, "Contig Name Labels", (s32)Contig_Name_Labels->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 420), bounds))
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

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Text Size", NK_TEXT_CENTERED);
                        nk_slider_float(NK_Context, 1.0f, &Contig_Name_Labels->size, 2.0f * DefaultNameLabelTextSize, 8.0f);
                        if (nk_button_label(NK_Context, "Default")) Contig_Name_Labels->size = DefaultNameLabelTextSize;
                        
                        nk_contextual_end(NK_Context);
                    }

                    bounds = nk_widget_bounds(NK_Context);
                    Scale_Bars->on = nk_check_label(NK_Context, "Scale Bars", (s32)Scale_Bars->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 420), bounds))
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

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Bar Size", NK_TEXT_CENTERED);
                        nk_slider_float(NK_Context, 10.0f, &Scale_Bars->size, 2.0f * DefaultScaleBarSize, 4.0f);
                        if (nk_button_label(NK_Context, "Default")) Scale_Bars->size = DefaultScaleBarSize;
                        
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
                        
                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Grid Size", NK_TEXT_CENTERED);
                        nk_slider_float(NK_Context, 0, &Grid->size, 2.0f * DefaultGridSize, DefaultGridSize / 20.0f);
                        if (nk_button_label(NK_Context, "Default")) Grid->size = DefaultGridSize;
                        
                        nk_contextual_end(NK_Context);
                    }

                    bounds = nk_widget_bounds(NK_Context);
                    Contig_Ids->on = nk_check_label(NK_Context, "ID Bars", (s32)Contig_Ids->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 150, Screen_Scale.y * 400), bounds))
                    {
                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "ID Bar Size", NK_TEXT_CENTERED);
                        nk_slider_float(NK_Context, 0, &Contig_Ids->size, 2.0f * DefaultContigIdSize, DefaultContigIdSize / 20.0f);
                        if (nk_button_label(NK_Context, "Default")) Contig_Ids->size = DefaultContigIdSize;
                        
                        nk_contextual_end(NK_Context);
                    }

#ifdef Internal
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
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 420), bounds))
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

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Text Size", NK_TEXT_CENTERED);
                        nk_slider_float(NK_Context, 1.0f, &Tool_Tip->size, 2.0f * DefaultToolTipTextSize, 8.0f);
                        if (nk_button_label(NK_Context, "Default")) Tool_Tip->size = DefaultToolTipTextSize;

                        nk_contextual_end(NK_Context);
                    }

                    Mouse_Invert = nk_check_label(NK_Context, "Invert Mouse Buttons", (s32)Mouse_Invert) ? 1 : 0;

                    nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 2);
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

                    nk_layout_row_static(NK_Context, Screen_Scale.y * 30.0f, (s32)(Screen_Scale.x * 180), 1);
                    s32 defaultGamma = nk_button_label(NK_Context, "Default Gamma");
                    if (defaultGamma)
                    {
                        Color_Maps->controlPoints[0] = 0.0f;
                        Color_Maps->controlPoints[1] = 0.5f;
                        Color_Maps->controlPoints[2] = 1.0f;
                    }

                    if (slider1 || slider2 || slider3 || defaultGamma)
                    {
                        Color_Maps->controlPoints[1] = Min(Max(Color_Maps->controlPoints[1], Color_Maps->controlPoints[0]), Color_Maps->controlPoints[2]);
                        glUseProgram(Contact_Matrix->shaderProgram);
                        glUniform3fv( Color_Maps->cpLocation, 1, Color_Maps->controlPoints);
                    }

                    if (nk_tree_push(NK_Context, NK_TREE_TAB, "Colour Maps", NK_MINIMIZED))
                    {
                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 2);
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
                            nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 1);

                            u32 nEdits = Min(Edits_Stack_Size, Map_Editor->nEdits);
                            char buff[128];
                            stbsp_snprintf((char *)buff, sizeof(buff), "Edits (%u)", nEdits);
                            
                            if (nk_tree_push(NK_Context, NK_TREE_TAB, (char *)buff, NK_MINIMIZED))
                            {
                                u32 editStackPtr = Map_Editor->editStackPtr == nEdits ? 0 : Map_Editor->editStackPtr;

                                f64 bpPerPixel = (f64)Total_Genome_Length / (f64)(Number_of_Textures_1D * Texture_Resolution);

                                ForLoop(nEdits)
                                {
                                    if (editStackPtr == nEdits)
                                    {
                                        editStackPtr = 0;
                                    }

                                    map_edit *edit = Map_Editor->edits + editStackPtr;

                                    u16 start = Min(edit->finalPix1, edit->finalPix2);
                                    u16 end = Max(edit->finalPix1, edit->finalPix2);
                                    u16 to = start ? start - 1 : (end < (Number_of_Pixels_1D - 1) ? end + 1 : end);

                                    u32 oldFrom = Map_State->contigRelCoords[start];
                                    u32 oldTo = Map_State->contigRelCoords[end];
                                    u32 *name1 = (Original_Contigs + Map_State->originalContigIds[start])->name;
                                    u32 *name2 = (Original_Contigs + Map_State->originalContigIds[end])->name;
                                    u32 *newName = (Original_Contigs + Map_State->originalContigIds[to])->name;
                                    
                                    stbsp_snprintf((char *)buff, sizeof(buff), "Edit %d:", index + 1);
                                    nk_label(NK_Context, (const char *)buff, NK_TEXT_LEFT);
                                    stbsp_snprintf((char *)buff, sizeof(buff), "       %s[%$.2fbp] to %s[%$.2fbp]",
                                            (char *)name1, (f64)oldFrom * bpPerPixel,
                                            (char *)name2, (f64)oldTo * bpPerPixel);
                                    nk_label(NK_Context, (const char *)buff, NK_TEXT_LEFT);

                                    nk_label(NK_Context, edit->finalPix1 > edit->finalPix2 ? (const char *)"       inverted and moved to" : 
                                            (const char *)"       moved to", NK_TEXT_LEFT);

                                    stbsp_snprintf((char *)buff, sizeof(buff), "       %s[%$.2fbp]",
                                            (char *)newName, (f64)Map_State->contigRelCoords[to] * bpPerPixel);
                                    nk_label(NK_Context, (const char *)buff, NK_TEXT_LEFT);

                                    ++editStackPtr;
                                }

                                nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 2);
                                if (nEdits && nk_button_label(NK_Context, "Undo")) UndoMapEdit();
                                if (nEdits && nk_button_label(NK_Context, "Copy to Clipboard")) CopyEditsToClipBoard(window);

                                nk_tree_pop(NK_Context);
                            }
                        }
                        
                        {
                            nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 1);
                            if (nk_tree_push(NK_Context, NK_TREE_TAB, "Waypoints", NK_MINIMIZED))
                            {
                                nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 2);
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
                       
                        {
                            nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 1);
                            if (nk_tree_push(NK_Context, NK_TREE_TAB, "Input Sequences", NK_MINIMIZED))
                            {
                                nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 1);

                                ForLoop(Number_of_Original_Contigs)
                                {
                                    original_contig *cont = Original_Contigs + index;

                                    char buff[128];
                                    stbsp_snprintf((char *)buff, sizeof(buff), "%s (%u)", (char *)cont->name, cont->nContigs);

                                    if (nk_tree_push_id(NK_Context, NK_TREE_TAB, (char *)buff, NK_MINIMIZED, index))
                                    {
                                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 2);

                                        ForLoop2(cont->nContigs)
                                        {
                                            stbsp_snprintf((char *)buff, sizeof(buff), "%u", index2 + 1);
                                            if (nk_button_label(NK_Context, (char *)buff))
                                            {
                                                f32 pos = (f32)((f64)cont->contigMapPixels[index2] / (f64)Number_of_Pixels_1D) - 0.5f;
                                                Camera_Position.x = pos;
                                                Camera_Position.y = -pos;
                                            }

                                            if (nk_button_label(NK_Context, "Rebuild")) RebuildContig(cont->contigMapPixels[index2]);
                                        }
                                        
                                        nk_tree_pop(NK_Context);
                                    }
                                }

                                nk_tree_pop(NK_Context);
                            }
                        }

                        {
                            if (Extensions.head)
                            {
                                nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 1);
                                if (nk_tree_push(NK_Context, NK_TREE_TAB, "Extensions", NK_MINIMIZED))
                                {
                                    char buff[128];

                                    nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30.0f, 1);
                                    TraverseLinkedList(Extensions.head, extension_node)
                                    {
                                        switch (node->type)
                                        {
                                            case extension_graph:
                                                {
                                                    graph *gph = (graph *)node->extension;
                                                    
                                                    stbsp_snprintf(buff, sizeof(buff), "Graph: %s", (char *)gph->name);
                                                    
                                                    bounds = nk_widget_bounds(NK_Context);
                                                    gph->on = nk_check_label(NK_Context, buff, (s32)gph->on) ? 1 : 0;
                                                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 150, Screen_Scale.y * 600), bounds))
                                                    {
                                                        struct nk_colorf colour = gph->colour;

                                                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                                                        nk_label(NK_Context, "Plot Colour", NK_TEXT_CENTERED);

                                                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 1);
                                                        colour = nk_color_picker(NK_Context, colour, NK_RGBA);

                                                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                                                        if (nk_button_label(NK_Context, "Default")) colour = DefaultGraphColour;

                                                        gph->colour = colour;

                                                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                                                        nk_label(NK_Context, "Plot Height", NK_TEXT_CENTERED);
                                                        nk_slider_float(NK_Context, -16.0f * DefaultGraphBase, &gph->base, 32.0f * DefaultGraphBase, 16.0f);
                                                        if (nk_button_label(NK_Context, "Default")) gph->base = DefaultGraphBase;

                                                        nk_label(NK_Context, "Plot Scale", NK_TEXT_CENTERED);
                                                        nk_slider_float(NK_Context, 0, &gph->scale, 8.0f * DefaultGraphScale, 0.005f);
                                                        if (nk_button_label(NK_Context, "Default")) gph->scale = DefaultGraphScale;

                                                        nk_label(NK_Context, "Line Width", NK_TEXT_CENTERED);
                                                        nk_slider_float(NK_Context, 0.1f, &gph->lineSize, 2.0f * DefaultGraphLineSize, 0.001f);
                                                        if (nk_button_label(NK_Context, "Default")) gph->lineSize = DefaultGraphLineSize;

                                                        nk_contextual_end(NK_Context);
                                                    }
                                                } break;
                                        }
                                    }
                                    nk_tree_pop(NK_Context);
                                }
                            }
                        }
                    }

                    nk_end(NK_Context);
                }

                if (FileBrowserRun("Load Map", &browser, NK_Context, (u32)showFileBrowser))
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

                if (currFileName)
                {
                    u08 state;
                    if ((state = FileBrowserRun("Save State", &saveBrowser, NK_Context, (u32)showSaveStateScreen, 1))) 
                    {
                        SaveState(headerHash, saveBrowser.file, state & 2);
                        FileBrowserReloadDirectoryContent(&saveBrowser, saveBrowser.directory);
                    }

                    if ((state = FileBrowserRun("Generate AGP", &saveAGPBrowser, NK_Context, (u32)showSaveAGPScreen, 2))) 
                    {
                        FenceIn(GenerateAGP(saveAGPBrowser.file, state & 2, state & 4));
                        FileBrowserReloadDirectoryContent(&saveAGPBrowser, saveAGPBrowser.directory);
                    }

                    if (FileBrowserRun("Load State", &loadBrowser, NK_Context, (u32)showLoadStateScreen)) LoadState(headerHash, loadBrowser.file);
                }

                AboutWindowRun(NK_Context, (u32)showAboutScreen);

                if (Deferred_Close_UI)
                {
                    UI_On = 0;
                    ++NK_Device->lastContextMemory[0];
                    Redisplay = 1;
                    Deferred_Close_UI = 0;
                }

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
            glfwSetCursor(window, crossCursor);
        }

        if (!Redisplay) glfwWaitEvents();
    }

    if (currFileName) SaveState(headerHash);

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
