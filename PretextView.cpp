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

#define PretextView_Version "PretextView Version 0.0.3"

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
    in vec2 texcoord;
    out vec2 Texcoord;
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
    in vec2 Texcoord;
    out vec4 outColor;
    uniform sampler2D tex;
    uniform samplerBuffer colormap;
    uniform vec3 controlpoints;
    float bezier(float t)
    {
        float tsq = t * t;
        float omt = 1.0 - t;
        float omtsq = omt * omt;
        return((omtsq * controlpoints.x) + (2.0 * t * omt * controlpoints.y) + (tsq * controlpoints.z));
    }
    void main()
    {
        float value = bezier(texture(tex, Texcoord).r);
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
    f32 x, y;
    f32 s, t;
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
    GLuint **textures;
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

global_variable
quad_data *
Grid_Data;

global_variable
quad_data *
Select_Box_Data;

global_variable
quad_data *
Label_Box_Data;

global_variable
quad_data *
Scale_Bar_Data;

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
    u32 fg;
    nk_colorf bg;
};

#define Grey_Background {0.569f, 0.549f, 0.451f, 1.0f}
#define Yellow_Text glfonsRGBA(240, 185, 15, 255)
#define Yellow_Text_Float {0.941176471f, 0.725490196f, 0.058823529f, 1.0f}
#define Red_Text glfonsRGBA(240, 10, 5, 255)
#define Red_Text_Float {0.941176471f, 0.039215686f, 0.019607843f, 1.0f}

global_variable
ui_colour_element_bg *
Contig_Name_Labels;

global_variable
ui_colour_element_bg *
Scale_Bars;

struct
ui_colour_element
{
    u32 on;
    nk_colorf bg;
};

global_variable
ui_colour_element *
Grid;

global_variable
u32
UI_On = 0;

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
        if (button == GLFW_MOUSE_BUTTON_LEFT)
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
        (void)window;

        if (y != 0.0)
        {
            ZoomCamera(Max(Min((f32)y, 0.01f), -0.01f));
            Redisplay = 1;
        }  
    }
}

global_variable
u32
Number_of_Contigs;

global_variable
u32
Number_of_Contigs_to_Display;

#define Max_Number_of_Contigs_to_Display 2048

global_variable
f32 *
Contig_Fractional_Lengths;

global_variable
u32 **
Contig_Names;

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
        u32 ptr = 0;
        ForLoop(Number_of_Textures_1D)
        {
            ForLoop2(Number_of_Textures_1D)
            {
                u32 min = Min(index, index2);
                u32 max = Max(index, index2);

                glBindTexture(GL_TEXTURE_2D, Contact_Matrix->textures[min][max - min]);
                glBindVertexArray(Contact_Matrix->vaos[ptr++]);
                glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL);
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
            position += Contig_Fractional_Lengths[index];
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
            position += Contig_Fractional_Lengths[index];
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

    // Text Rendering
    if (Contig_Name_Labels->on || Scale_Bars->on || UI_On || Loading)
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
            fonsSetSize(FontStash_Context, 32.0f * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_CENTER | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Bold);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, Contig_Name_Labels->fg);

            f32 leftPixel = ModelXToScreen(-0.5f);
            f32 totalLength = 0.0f;
            f32 wy0 = ModelYToScreen(0.5f);

            ForLoop(Number_of_Contigs_to_Display)
            {
                totalLength += Contig_Fractional_Lengths[index];

                f32 rightPixel = ModelXToScreen(totalLength - 0.5f);

                if (rightPixel > 0.0f && leftPixel < width)
                {
                    const char *name = (const char *)Contig_Names[index];
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
                totalLength += Contig_Fractional_Lengths[index];
                f32 bottomPixel = ModelYToScreen(0.5f - totalLength);

                if (topPixel < height && bottomPixel > 0.0f)
                {
                    const char *name = (const char *)Contig_Names[index];
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

            u32 *labelsPerContig = PushArray(Working_Set, u32, Number_of_Contigs_to_Display);

            glViewport(0, 0, (s32)width, (s32)height);

            f32 lh = 0.0f;   

            fonsClearState(FontStash_Context);
            fonsSetSize(FontStash_Context, 20.0f * Screen_Scale.x);
            fonsSetAlign(FontStash_Context, FONS_ALIGN_CENTER | FONS_ALIGN_TOP);
            fonsSetFont(FontStash_Context, Font_Normal);
            fonsVertMetrics(FontStash_Context, 0, 0, &lh);
            fonsSetColor(FontStash_Context, Scale_Bars->fg);

            f32 leftPixel = ModelXToScreen(-0.5f);
            f32 rightPixel = ModelXToScreen(0.5f);
            f32 wy0 = ModelYToScreen(0.5);
            f32 offset = 45.0f * Screen_Scale.x;
            f32 y = Max(wy0, 0.0f) + offset;
            f32 totalLength = 0.0f;

            f32 bpPerPixel = (f32)((f64)Total_Genome_Length / (f64)(rightPixel - leftPixel));

            GLfloat *bg = (GLfloat *)&Scale_Bars->bg;
            static f32 scale = 1.0f / 255.0f;
            GLfloat fg[4];
            fg[0] = scale * (Scale_Bars->fg & 0xff);
            fg[1] = scale * ((Scale_Bars->fg >> 8) & 0xff);
            fg[2] = scale * ((Scale_Bars->fg >> 16) & 0xff);
            fg[3] = scale * ((Scale_Bars->fg >> 24) & 0xff);
            
            f32 scaleBarWidth = 4.0f * Screen_Scale.x;
            f32 tickLength = 3.0f * Screen_Scale.x;

            ForLoop(Number_of_Contigs_to_Display)
            {
                totalLength += Contig_Fractional_Lengths[index];
                rightPixel = ModelXToScreen(totalLength - 0.5f);

                f32 pixelLength = rightPixel - leftPixel;

                u32 labelLevels = SubDivideScaleBar(leftPixel, rightPixel, (leftPixel + rightPixel) * 0.5f, bpPerPixel);
                u32 labels = 0;
                ForLoop2(labelLevels)
                {
                    labels += (labels + 1);
                }
                labels = Min(labels, MaxTicksPerScaleBar);

                labelsPerContig[index] = labels;

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

                        glUniform4fv(Flat_Shader->colorLocation, 1, fg);

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
                totalLength += Contig_Fractional_Lengths[index];
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

            ChangeSize((s32)width, (s32)height);
            FreeLastPush(Working_Set);
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
Header_Decompressor;

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
load_file_result
LoadFile(const char *fileName, memory_arena *arena)
{
    // Test File
    FILE *file = TestFile(fileName);
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
        ForLoop(Number_of_Textures_1D)
        {
            glDeleteTextures((GLsizei)(Number_of_Textures_1D - index), Contact_Matrix->textures[index]);
        }

        glDeleteVertexArrays((GLsizei)(Number_of_Textures_1D * Number_of_Textures_1D), Contact_Matrix->vaos);
        glDeleteBuffers((GLsizei)(Number_of_Textures_1D * Number_of_Textures_1D), Contact_Matrix->vbos);

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
        if (libdeflate_deflate_decompress(Header_Decompressor, (const void *)compressionBuffer, nBytesHeaderComp, (void *)header, nBytesHeader, NULL))
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
        Number_of_Contigs = val32;

        Number_of_Contigs_to_Display = Min(Number_of_Contigs, Max_Number_of_Contigs_to_Display);

        Contig_Fractional_Lengths = PushArrayP(arena, f32, Number_of_Contigs);
        Contig_Names = PushArrayP(arena, u32*, Number_of_Contigs);

        ForLoop(Number_of_Contigs)
        {
            f32 frac;
            u32 name[16];

            ptr = (u08 *)&frac;
            ForLoop2(4)
            {
                *ptr++ = *header++;
            }
            Contig_Fractional_Lengths[index] = frac;

            ptr = (u08 *)name;
            ForLoop2(64)
            {
                *ptr++ = *header++;
            }
            Contig_Names[index] = PushArrayP(arena, u32, 16);
            ForLoop2(16)
            {
                Contig_Names[index][index2] = name[index2];
            }
        }

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
    }

    // Load Textures
    {
        InitialiseTextureBufferQueue(arena, Texture_Buffer_Queue, Bytes_Per_Texture, fileName);

        Contact_Matrix->textures = PushArrayP(arena, GLuint *, Number_of_Textures_1D);
        ForLoop(Number_of_Textures_1D)
        {
            Contact_Matrix->textures[index] = PushArrayP(arena, GLuint, Number_of_Textures_1D - index);
            ForLoop2(Number_of_Textures_1D - index)
            {
                Contact_Matrix->textures[index][index2] = (index << 8) | (index + index2);
                ThreadPoolAddTask(Thread_Pool, LoadTexture, (void *)(Contact_Matrix->textures[index] + index2));
            }
        }

        printf("Loading textures...\n");
        glActiveTexture(GL_TEXTURE0);
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

                glGenTextures(1, (Contact_Matrix->textures[index] + index2));
                glBindTexture(GL_TEXTURE_2D, Contact_Matrix->textures[index][index2]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)Number_of_MipMaps - 1);

                GLsizei resolution = (GLsizei)Texture_Resolution;
                for (   GLint level = 0;
                        level < (GLint)Number_of_MipMaps;
                        ++level )
                {
                    GLsizei nBytes = resolution * (resolution >> 1);
                    glCompressedTexImage2D(GL_TEXTURE_2D, level, GL_COMPRESSED_RED_RGTC1, resolution, resolution, 0, nBytes, texture);
                    resolution >>= 1;
                    texture += nBytes;
                }
                glBindTexture(GL_TEXTURE_2D, 0);

                printf("\r%3d/%3d (%1.2f%%) textures loaded from disk...", Texture_Ptr + 1, (Number_of_Textures_1D >> 1) * (Number_of_Textures_1D + 1), 100.0 * (f64)((f32)(Texture_Ptr + 1) / (f32)((Number_of_Textures_1D >> 1) * (Number_of_Textures_1D + 1))));
                fflush(stdout);

                AddTextureBufferToQueue(Texture_Buffer_Queue, (texture_buffer *)loadedTexture);
                FenceIn(Current_Loaded_Texture = 0);
                __atomic_fetch_add(&Texture_Ptr, 1, 0);
            }
        }

        printf("\n");
        CloseTextureBufferQueueFiles(Texture_Buffer_Queue);
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

                textureVertices[0].x = x - 0.5f;
                textureVertices[0].y = y - quadSize - 0.5f;
                textureVertices[0].s = cornerCoords[0];
                textureVertices[0].t = cornerCoords[1];

                textureVertices[1].x = x - 0.5f + quadSize;
                textureVertices[1].y = y - quadSize - 0.5f;
                textureVertices[1].s = 1.0f;
                textureVertices[1].t = 1.0f;

                textureVertices[2].x = x - 0.5f + quadSize;
                textureVertices[2].y = y - 0.5f;
                textureVertices[2].s = cornerCoords[1];
                textureVertices[2].t = cornerCoords[0];

                textureVertices[3].x = x - 0.5f;
                textureVertices[3].y = y - 0.5f;
                textureVertices[3].s = 0.0f;
                textureVertices[3].t = 0.0f;

                glGenBuffers(1, Contact_Matrix->vbos + ptr);
                glBindBuffer(GL_ARRAY_BUFFER, Contact_Matrix->vbos[ptr]);
                glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(tex_vertex), textureVertices, GL_STATIC_DRAW);

                glEnableVertexAttribArray(posAttrib);
                glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(tex_vertex), 0);
                glEnableVertexAttribArray(texAttrib);
                glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(tex_vertex), (void *)(2 * sizeof(GLfloat)));

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Quad_EBO);

                x += quadSize;
                ++ptr;
            }

            y -= quadSize;
            x = 0.0f;
        }
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
        PushGenericBuffer(&Scale_Bar_Data, 2 * Min(Number_of_Contigs_to_Display, 4) * (2 + MaxTicksPerScaleBar));
    }

    FenceIn(File_Loaded = 1);

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
    Header_Decompressor = libdeflate_alloc_decompressor();
    if (!Header_Decompressor)
    {
        fprintf(stderr, "Could not allocate decompressor\n");
        exit(1);
    }

    Texture_Buffer_Queue = PushStruct(Working_Set, texture_buffer_queue);
    
    // Contig Name Label UI
    {
        Contig_Name_Labels = PushStruct(Working_Set, ui_colour_element_bg);
        Contig_Name_Labels->on = 0;
        Contig_Name_Labels->fg = Yellow_Text;
        Contig_Name_Labels->bg = Grey_Background;
    }
    
    // Scale Bar UI
    {
        Scale_Bars = PushStruct(Working_Set, ui_colour_element_bg);
        Scale_Bars->on = 0;
        Scale_Bars->fg = Red_Text;
        Scale_Bars->bg = Grey_Background;
    }
   
    // Grid UI
    {
        Grid = PushStruct(Working_Set, ui_colour_element);
        Grid->on = 1;
        Grid->bg = Grey_Background;
    }

    // Contact Matrix Shader
    {
        Contact_Matrix = PushStruct(Working_Set, contact_matrix);
        Contact_Matrix->shaderProgram = CreateShader(FragmentSource_Texture, VertexSource_Texture);

        glUseProgram(Contact_Matrix->shaderProgram);
        glBindFragDataLocation(Contact_Matrix->shaderProgram, 0, "outColor");

        Contact_Matrix->matLocation = glGetUniformLocation(Contact_Matrix->shaderProgram, "matrix");
        glUniform1i(glGetUniformLocation(Contact_Matrix->shaderProgram, "tex"), 0);
        glUniform1i(glGetUniformLocation(Contact_Matrix->shaderProgram, "colormap"), 1);

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
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
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
                    Loading = 1;
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

    assert(dir);
    assert(count);
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
                    assert(results);
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

struct
about_window
{
    struct nk_image logo;
};

global_function
void
AboutWindowRun(about_window *about, struct nk_context *ctx, u32 show)
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
                (void)about;
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
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return nk_image_id((s32)tex);
}
//

MainArgs
{
    u32 initWithFile = 0;
    u08 currFile[256];
    if (ArgCount >= 2)
    {
        initWithFile = 1;
        CopyNullTerminatedString((u08 *)ArgBuffer[1], currFile);
    }

    Mouse_Move.x = -1.0;
    Mouse_Move.y = -1.0;

    Mouse_Select.x = -1.0;
    Mouse_Select.y = -1.0;

    Camera_Position.x = 0.0f;
    Camera_Position.y = 0.0f;
    Camera_Position.z = 1.0f;

    CreateMemoryArena(Working_Set, MegaByte(256));
    Thread_Pool = ThreadPoolInit(&Working_Set, 6); 

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

    s32 width, height, display_width, display_height;
    glfwGetWindowSize(window, &width, &height);
    glfwGetFramebufferSize(window, &display_width, &display_height);
    Screen_Scale.x = (f32)display_width/(f32)width;
    Screen_Scale.y = (f32)display_height/(f32)height;
    
    Setup();
    if (initWithFile)
    {
        UI_On = LoadFile((const char *)currFile, Loading_Arena) == ok ? 0 : 1;
    }
    else
    {
        UI_On = 1;
    }

    // about screen
    about_window about;
    //about.logo = IconLoad(AboutIcon, AboutIcon_Size);

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
            LoadFile((const char *)currFile, Loading_Arena);
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

                if (nk_begin(NK_Context, "Options", nk_rect(Screen_Scale.x * 10, Screen_Scale.y * 10, Screen_Scale.x * 700, Screen_Scale.y * 350), flags))
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

                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 1);
                    bounds = nk_widget_bounds(NK_Context);
                    Contig_Name_Labels->on = nk_check_label(NK_Context, "Contig Name Labels", (s32)Contig_Name_Labels->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 400), bounds))
                    {
                        static struct nk_colorf colour_text = Yellow_Text_Float;
                        static struct nk_colorf colour_bg = Grey_Background;
                       
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

                        Contig_Name_Labels->fg = glfonsRGBA(    (u08)(colour_text.r * 255.0f), 
                                                                (u08)(colour_text.g * 255.0f), 
                                                                (u08)(colour_text.b * 255.0f), 
                                                                (u08)(colour_text.a * 255.0f));

                        Contig_Name_Labels->bg = colour_bg;

                        nk_contextual_end(NK_Context);
                    }

                    bounds = nk_widget_bounds(NK_Context);
                    Scale_Bars->on = nk_check_label(NK_Context, "Scale Bars", (s32)Scale_Bars->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 300, Screen_Scale.y * 400), bounds))
                    {
                        static struct nk_colorf colour_text = Red_Text_Float;
                        static struct nk_colorf colour_bg = Grey_Background;
                       
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

                        Scale_Bars->fg = glfonsRGBA(    (u08)(colour_text.r * 255.0f), 
                                                                (u08)(colour_text.g * 255.0f), 
                                                                (u08)(colour_text.b * 255.0f), 
                                                                (u08)(colour_text.a * 255.0f));

                        Scale_Bars->bg = colour_bg;

                        nk_contextual_end(NK_Context);
                    }
                   
                    bounds = nk_widget_bounds(NK_Context);
                    Grid->on = nk_check_label(NK_Context, "Grid", (s32)Grid->on) ? 1 : 0;
                    if (nk_contextual_begin(NK_Context, 0, nk_vec2(Screen_Scale.x * 150, Screen_Scale.y * 400), bounds))
                    {
                        static struct nk_colorf colour_bg = Grey_Background;
                       
                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        nk_label(NK_Context, "Grid Colour", NK_TEXT_CENTERED);

                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 200, 1);
                        colour_bg = nk_color_picker(NK_Context, colour_bg, NK_RGBA);
                       
                        nk_layout_row_dynamic(NK_Context, Screen_Scale.y * 30, 1);
                        if (nk_button_label(NK_Context, "Default")) colour_bg = Grey_Background;

                        Grid->bg = colour_bg;

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

                    {
                        if (File_Loaded)
                        {
                            u32 NPerGroup = Min(Number_of_Contigs, 10);
                            
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
                                            index < (currGroup1 + NPerGroup) && index < Number_of_Contigs;
                                            ++index )
                                    {
                                        currSelected1 = nk_option_label(NK_Context, (const char *)Contig_Names[index], currSelected1 == (s32)index) ? (s32)index : currSelected1;
                                    }
                                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                                    if (nk_button_label(NK_Context, "Prev"))
                                    {
                                        currGroup1 = currGroup1 >= NPerGroup ? (currGroup1 - NPerGroup) : (Number_of_Contigs - NPerGroup);
                                    }
                                    if (nk_button_label(NK_Context, "Next"))
                                    {
                                        currGroup1 = currGroup1 < (Number_of_Contigs - NPerGroup) ? (currGroup1 + NPerGroup) : 0;
                                    }
                                    nk_group_end(NK_Context);
                                }

                                if (nk_group_begin(NK_Context, "Select2", 0))
                                {
                                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 1);
                                    for (   u32 index = currGroup2;
                                            index < (currGroup2 + NPerGroup) && index < Number_of_Contigs;
                                            ++index )
                                    {
                                        currSelected2 = nk_option_label(NK_Context, (const char *)Contig_Names[index], currSelected2 == (s32)index) ? (s32)index : currSelected2;
                                    }
                                    nk_layout_row_dynamic(NK_Context, (s32)(Screen_Scale.y * 30.0f), 2);
                                    if (nk_button_label(NK_Context, "Prev"))
                                    {
                                        currGroup2 = currGroup2 >= NPerGroup ? (currGroup2 - NPerGroup) : (Number_of_Contigs - NPerGroup);
                                    }
                                    if (nk_button_label(NK_Context, "Next"))
                                    {
                                        currGroup2 = currGroup2 < (Number_of_Contigs - NPerGroup) ? (currGroup2 + NPerGroup) : 0;
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

                                u32 index = 0;
                                for (   ;
                                        index < (u32)currSelected1;
                                        ++index )
                                {
                                    fracx1 += Contig_Fractional_Lengths[index];
                                }
                                fracx2 = fracx1 + Contig_Fractional_Lengths[index];

                                index = 0;
                                for (   ;
                                        index < (u32)currSelected2;
                                        ++index )
                                {
                                    fracy1 += Contig_Fractional_Lengths[index];
                                }
                                fracy2 = fracy1 + Contig_Fractional_Lengths[index];

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
           
            AboutWindowRun(&about, NK_Context, (u32)showAboutScreen);

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
        else
        {
            glfwWaitEvents();
        }
    }

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
