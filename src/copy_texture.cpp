
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

#include "copy_texture.h"
#include "shaderSource.h"
#include "utilsPretextView.h"
#include <cmath>


Show_State::Show_State()
{
    zoomlevel = 1.0f;
    isDragging = false;
    lastMouseX = 0.0;
    lastMouseY = 0.0;
    show_menu_window = true;
}

Show_State::~Show_State() {}


void get_linear_mask(std::vector<f32>& linear_array)
{   
    u32 length = linear_array.size();
    if (length < 2)
    {
        fmt::print(stderr, "Error: link score calculation: length({}) < 2\n", length);
        assert(0);
    }
    for (u32 i = 0; i < length; i++)
    {
        linear_array[i] = std::sqrt(1.0f / (f32)(i+1)) ;
    }
    f32 sum = std::accumulate(linear_array.data(), linear_array.data()+length, 0.0f);
    for (int i  = 0 ; i < length; i++)
    {
        linear_array[i] /= sum;
    }
    return;
}


#ifdef DEBUG
    // Callback function for mouse scroll
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) 
    {   
        // pass the scroll event to the ImGui
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheelH += (f32)xoffset;
        io.MouseWheel += (f32)yoffset;

        if (io.WantCaptureMouse) return ;
        auto* show_state = reinterpret_cast<Show_State*>(glfwGetWindowUserPointer(window));
        if (show_state->show_menu_window)  return;
        f32* zoomlevel = &show_state->zoomlevel;
        // Adjust the zoom level based on the scroll input
        *zoomlevel *= (1.0 - yoffset * 0.05f);  // Change 0.1f to adjust sensitivity

        // Clamp the zoom level to prevent excessive zooming
        if (*zoomlevel < 1.0 / 32. / 4.) *zoomlevel = 1.0 / 32. / 4.;
        if (*zoomlevel > 1.0f) *zoomlevel = 1.f;
    }

    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {   
        // pass the mouse button event to the ImGui
        ImGuiIO& io = ImGui::GetIO();
        if (button >= 0 && button < IM_ARRAYSIZE(io.MouseDown)) {
            io.MouseDown[button] = (action == GLFW_PRESS);
        }

        if (io.WantCaptureMouse) return ;
        auto* show_state = reinterpret_cast<Show_State*>(glfwGetWindowUserPointer(window));
        if (show_state->show_menu_window)  return;
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                show_state->isDragging = true;
                glfwGetCursorPos(window, &show_state->lastMouseX, &show_state->lastMouseY); // Initial cursor position
            } else if (action == GLFW_RELEASE) {
                show_state->isDragging = false;
            }
        }
    }

    void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) 
    {
        // pass the cursor position to the ImGui
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2((f32)xpos, (f32)ypos);

        if (io.WantCaptureMouse) return ;
        auto* show_state = reinterpret_cast<Show_State*>(glfwGetWindowUserPointer(window));
        if (show_state->show_menu_window)  return;

        if (show_state->isDragging) {
            double dx = xpos - show_state->lastMouseX;
            double dy = ypos - show_state->lastMouseY;

            // Adjust the translation offset based on mouse movement (scale as needed)
            show_state->translationOffset.x += static_cast<f32>(dx) * 0.00175f * show_state->zoomlevel; // Adjust sensitivity with 0.01f
            show_state->translationOffset.y -= static_cast<f32>(dy) * 0.00175f * show_state->zoomlevel;

            show_state->lastMouseX = xpos;
            show_state->lastMouseY = ypos;
        }
    }
#endif // DEBUG


TexturesArray4AI::TexturesArray4AI(
    u32 num_textures_1d_, 
    u32 texture_resolution_, 
    char* fileName, 
    const map_contigs* Contigs)
    :num_textures_1d(num_textures_1d_), texture_resolution(texture_resolution_),
    is_copied_from_buffer(false), is_compressed(false), hic_shader_initilised(false)
{   
    
    this->frags = new Frag4compress(Contigs);

    this->compressed_extensions = new CompressedExtensions(1); // initialize the compressed_extensions with 1

    char* tmp = fileName;
    while (*tmp != '\0' && *tmp != '.') 
    {
        this->name[tmp - fileName] = *tmp; 
        tmp++; 
    }
    this->name[tmp - fileName] = '\0';
    this->num_pixels_1d = this->num_textures_1d * this->texture_resolution;
    if ((this->texture_resolution & (this->texture_resolution - 1)) != 0) 
    {
        fprintf(stderr, "The texture resolution (%d) is not power of 2\n", this->texture_resolution);
        assert(0);
    }
    this->texture_resolution_exp = (u32)std::log2(this->texture_resolution);
    this->total_num_textures = ((1 + this->num_textures_1d) * this->num_textures_1d ) >> 1;
    
    // 此处不再为copy textures申请空间，因为耗费太多内存
    // this->initialise_textures();

    // Shader setup and texture binding
    this->shaderProgram = CreateShader(FragmentSource_copyTexture.c_str(), VertexSource_copyTexture.c_str());

    // Setup vao, vbo, ebo
    f32 vertexpositions[] = {
        // position      texCoord
        -1.f, -1.f,     0.0f, 0.0f,
        1.f, -1.f,     1.0f, 0.0f,
        1.f,  1.f,     1.0f, 1.0f, 
        -1.f,  1.f,     0.0f, 1.0f,
    };
    u32 triangle_indices[] = {
        0, 1, 2, 
        2, 3, 0,
    };
    glGenVertexArrays(1, &this->vao);
    glBindVertexArray(this->vao);

    glGenBuffers(1, &this->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexpositions), vertexpositions, GL_STATIC_DRAW);

    glGenBuffers(1, &this->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(triangle_indices), triangle_indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void*)(2 * sizeof(f32)));
    glEnableVertexAttribArray(1);

    this->hic_shader_initilised = true;
}


void TexturesArray4AI::initialise_textures()
{   
    if (this->textures) this->clear_textures();
    this->textures = new u08*[this->total_num_textures];
    try
    {
        for (u32 i = 0; i < this->total_num_textures; i++)
        {
            this->textures[i] = new u08[this->texture_resolution * this->texture_resolution];
        }
    }
    catch(const std::exception& e)
    {   
        this->clear_textures();
        std::cerr << e.what() << " Allocate mem error\n";
        assert(0);
    }
}

void TexturesArray4AI::clear_textures()
{
    for (u32 i = 0; i < this->total_num_textures; i++)
    {
        if (this->textures && this->textures[i])
        {
            delete[] this->textures[i];
            this->textures[i] = nullptr;
        }
    }
    delete[] this->textures;
    this->textures = nullptr;
    is_copied_from_buffer = false;
}


void TexturesArray4AI::clear_compressed_hic()
{
    if (this->compressed_hic)
    {
        delete this->compressed_hic;
        this->compressed_hic = nullptr;
    }
    return;
}


TexturesArray4AI::~TexturesArray4AI()
{   
    this->clear_textures();
    this->clear_compressed_hic();

    if (frags)
    {
        delete frags;
        frags = nullptr;
    }
    
    if (compressed_extensions)
    {
        delete compressed_extensions;
        compressed_extensions = nullptr;
    }

    if (mass_centres)
    {
        delete mass_centres;
        mass_centres = nullptr;
    }

    if (this->hic_shader_initilised)
    {
        // glDeleteShader(this->shaderProgram);
        glDeleteBuffers(1, &this->vbo);
        glDeleteBuffers(1, &this->ebo);
        glDeleteVertexArrays(1, &this->vao);
        glDeleteProgram(this->shaderProgram);
    }
}

/* 
*/
u08 TexturesArray4AI::operator()(u32 row, u32 column) const
{   
    if (row>=num_pixels_1d || column>=num_pixels_1d) 
    {   
        fmt::print(
            stderr,
            "Index [{}, {}] is out of range [{}, {}], File:{}, line:{}\n", 
            row, 
            column, 
            num_pixels_1d - 1, 
            num_pixels_1d - 1,
            __FILE__,
            __LINE__
        );
        assert(0);
    }
    if (!this->textures) 
    {   
        char buff[512];
        snprintf(buff, sizeof(buff), "TexturesArray4AI is not initialized, "
            "please first copy the buffer textures to this->textures[], "
            "row: %d, column: %d\n", row, column);
        MY_CHECK(buff);
        assert(0);
    }
    if (row>column) std::swap(row, column);
    // cal texture_id 
    // index within the texture
    return textures[texture_id_cal(row>>texture_resolution_exp, column>>texture_resolution_exp, num_textures_1d)][(row & ((1<<texture_resolution_exp) - 1)) * texture_resolution + (column & ((1<<texture_resolution_exp) - 1))];
}


f32 TexturesArray4AI::get_textures_diag_mean(int shift)
{   
    if (std::abs(shift)>=num_pixels_1d)
    {   
        printf("Shift (%d) is out of range [%d, %d]\n", shift, 1 - num_pixels_1d, num_pixels_1d - 1);
        assert(0);
    }
    u32 sum = 0;
    int start = (shift>=0?0:-shift), end = (shift>=0 ? num_pixels_1d - shift : num_pixels_1d);
    for (u32 row = start; row < end; row++)
    {
        sum += (u32)(*this)(row, row + shift);
    }
    return sum / ((f32)num_pixels_1d - (f32)std::abs(shift));
}


void TexturesArray4AI::check_copied_from_buffer() const
{
    if (!is_copied_from_buffer || this->textures == nullptr) 
    {
        std::cerr << "TexturesArray4AI is not initialized, "
            "please first copy the buffer textures to this->textures[], "
            << std::endl;
        assert(0);
    }
    return ;
}

/* 
复制没有经过re-order的buffer到 this->textures 中
其实就是存储在.pretext 文件中的textures
*/
void TexturesArray4AI::copy_buffer_to_textures(
    const contact_matrix *contact_matrix_, 
    bool show_flag) 
{   

    this->initialise_textures(); // 申请空间 textures

    const GLuint &contact_matrix_textures = contact_matrix_->textures;
    if (is_copied_from_buffer) return;
    
    // Temporary texture setup
    GLuint temptexture;
    glGenTextures(1, &temptexture);
    glBindTexture(GL_TEXTURE_2D, temptexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, texture_resolution, texture_resolution, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    

    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temptexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) 
    {
        std::cerr << "Framebuffer is not complete!" << std::endl;
        assert(0);
    }

    s32 orignal_viewport[4];
    glGetIntegerv(GL_VIEWPORT, orignal_viewport);
    GLcall(glActiveTexture(GL_TEXTURE0));
    GLcall(glBindTexture(GL_TEXTURE_2D_ARRAY, contact_matrix_textures));
    GLcall(glUseProgram(shaderProgram));
    GLcall(glUniform1i(glGetUniformLocation(shaderProgram, "texArray"), 0));
    auto model = glm::mat4(1.0f);
    for (int layer = 0; layer < total_num_textures; ++layer) 
    {   
        GLcall(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer));
        GLcall(glViewport(0, 0, texture_resolution, texture_resolution));
        GLcall(glClearColor(0.3f, 0.3f, 0.3f, 1.0f));
        GLcall(glClear(GL_COLOR_BUFFER_BIT)); 
        GLcall(glUniform1i(glGetUniformLocation(this->shaderProgram, "layer"), layer));
        glUniformMatrix4fv(glGetUniformLocation(this->shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        GLcall(glBindVertexArray(this->vao));
        GLcall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));
        GLcall( glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer));
        GLcall(glReadPixels(0, 0, texture_resolution, texture_resolution, GL_RED, GL_UNSIGNED_BYTE, textures[layer]));
        if ((layer + 1) % (total_num_textures / 100) == 0)
        {
            printf("\rCopying tiles to cpu %.2f%% (Layer %d of %d)", ((f32)layer + 1.f) / (f32)total_num_textures * 100.0f, layer + 1, total_num_textures);
            fflush(stdout);
        }
    }
    printf("\n"); 

    is_copied_from_buffer = true;
    // Cleanup
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindBuffer(GL_VERTEX_ARRAY, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteFramebuffers(1, &framebuffer);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &temptexture);

    #ifdef DEBUG
        if (show_flag) show_collected_textures();
    #endif // DEBUG 

    // return back to the original viewport
    glViewport(orignal_viewport[0], orignal_viewport[1], orignal_viewport[2], orignal_viewport[3]);

    return ;
}


/*
This will copy the modified pixels into this->textures.
*/
void TexturesArray4AI::copy_buffer_to_textures_dynamic(
    const contact_matrix *contact_matrix_, 
    bool show_flag) 
{

    this->initialise_textures(); // 申请空间textures

    GLuint temptexture;
    glGenTextures(1, &temptexture);
    glBindTexture(GL_TEXTURE_2D, temptexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, texture_resolution, texture_resolution, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temptexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) 
    {
        std::cerr << "Framebuffer is not complete!" << std::endl;
        assert(0);
    }

    s32 orignal_viewport[4];
    glGetIntegerv(GL_VIEWPORT, orignal_viewport);

    auto model = glm::mat4(1.0f);
    glUseProgram(contact_matrix_->shaderProgram);
    glUniformMatrix4fv(contact_matrix_->matLocation, 1, GL_FALSE, glm::value_ptr(model));
    glBindTexture(GL_TEXTURE_2D_ARRAY, contact_matrix_->textures);
    u32 layer_cnt = 0, ptr_cnt = 0;
    for (u32 i = 0 ; i < this->num_textures_1d; i ++ )
    {
        for (u32 j = 0; j < this->num_textures_1d ; j ++ )
        {
            if ( i > j )
            {
                ptr_cnt++;
                continue;
            }
            GLcall(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer));
            // GLcall(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
            GLcall(glViewport(0, 0, texture_resolution, texture_resolution));
            GLcall(glClearColor(0.3f, 0.3f, 0.3f, 1.0f));
            GLcall(glClear(GL_COLOR_BUFFER_BIT));

            // GLcall(glUseProgram(this->shaderProgram));
            // GLcall(glBindTexture(GL_TEXTURE_2D_ARRAY, contact_matrix_->textures));
            // GLcall(glUniform1i(glGetUniformLocation(this->shaderProgram, "layer"), layer_cnt));
            // glUniformMatrix4fv(glGetUniformLocation(this->shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            // GLcall(glBindVertexArray(this->vao));
            // GLcall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));

            GLcall(glBindVertexArray(contact_matrix_->vaos[ptr_cnt])); 
            GLcall(glDrawRangeElements(GL_TRIANGLES, 0, 3, 6, GL_UNSIGNED_SHORT, NULL));

            // GLcall( glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer));
            // GLcall( glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
            GLcall(glReadPixels(0, 0, texture_resolution, texture_resolution, GL_RED, GL_UNSIGNED_BYTE, textures[layer_cnt]));
            if ((layer_cnt + 1) % (total_num_textures / 100) == 0)
            {
                printf("\rCopying tiles to cpu %.2f%% (Layer %d of %d)", ((f32)layer_cnt + 1.f) / (f32)total_num_textures * 100.0f, layer_cnt + 1, total_num_textures);
                fflush(stdout);
            }
            layer_cnt++;
            ptr_cnt++;
        }
    }
    printf("\n"); 
    if (layer_cnt != total_num_textures)
    {   
        std::stringstream ss;
        ss << "layer_cnt (" << layer_cnt << ") != total_num_textures (" << this->total_num_textures << ")";
        MY_CHECK(ss.str().c_str());
        assert(0);
    }
    

    is_copied_from_buffer = true;

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);

    #ifdef DEBUG
        if (show_flag) show_collected_textures();
    #endif // DEBUG

    glViewport(orignal_viewport[0], orignal_viewport[1], orignal_viewport[2], orignal_viewport[3]);
    return ;
}

/*
存储一个matrix然后 rearrange

目前没有这样做，因为需要存两个textures... 目前还是从opengl的buffer往外copy
*/
void TexturesArray4AI::rearrange_textures(const u32* rearrange_index)
{
    if (!is_copied_from_buffer || this->textures == nullptr) 
    {
        std::cerr << "TexturesArray4AI is not initialized, "
            "please first copy the buffer textures to this->textures[], "
            << std::endl;
        assert(0);
    }
    u08** new_textures = new u08*[total_num_textures];
    for (u32 i = 0; i < total_num_textures; i++)
    {
        new_textures[i] = new u08[texture_resolution * texture_resolution];
    }
    u32 texture_id = 0, row_in_texture = 0, column_in_texture = 0, 
        row_re = 0, column_re =0,
        texture_id_re = 0, row_in_texture_re = 0, column_in_texture_re = 0;

    for (u32 row = 0; row < num_pixels_1d; row++)
    {
        for (u32 column = 0; column < num_pixels_1d; column++)
        {   
            /* 
            id= texture_id_cal(row>>texture_resolution_exp, column>>texture_resolution_exp, num_textures_1d)
            row_in_texture = row & ((1<<texture_resolution_exp) - 1)
            colomn_in_texture = column & ((1<<texture_resolution_exp) - 1)
            */
            texture_id = texture_id_cal(row>>texture_resolution_exp, column>>texture_resolution_exp, num_textures_1d);
            row_in_texture = row & ((1<<texture_resolution_exp) - 1);
            column_in_texture = column & ((1<<texture_resolution_exp) - 1);

            row_re = rearrange_index[row];
            column_re = rearrange_index[column];
            texture_id_re = texture_id_cal(row_re>>texture_resolution_exp, column_re>>texture_resolution_exp, num_textures_1d);
            row_in_texture_re = row_re & ((1<<texture_resolution_exp) - 1);
            column_in_texture_re = column_re & ((1<<texture_resolution_exp) - 1);
            new_textures[texture_id][row_in_texture * texture_resolution + column_in_texture] = textures[texture_id_re][row_in_texture_re * texture_resolution + column_in_texture_re];
        }
    }
    clear_textures();
    is_copied_from_buffer = true;
    textures = new_textures;
    return ;
}


void TexturesArray4AI::prepare_tmp_texture2d_array(GLuint& tmp_texture2d_array)
{   
    check_copied_from_buffer();

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &tmp_texture2d_array);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tmp_texture2d_array);
    glTexImage3D( // allocate the gpu memory for the texture
        GL_TEXTURE_2D_ARRAY, 
        0, 
        GL_R8, 
        texture_resolution, texture_resolution, total_num_textures, 
        0, 
        GL_RED, GL_UNSIGNED_BYTE, 
        nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // GL_LINEAR
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    for (int layer = 0; layer < total_num_textures; layer++)
    {   
        if (!textures[layer]) 
        {
            fprintf(stderr, "The texture[%d] is nullptr\n", layer);
            assert(0);
        }
        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY, 
            0,  // level
            0, 0, layer,  // offset
            texture_resolution, texture_resolution, 1, 
            GL_RED,  // format
            GL_UNSIGNED_BYTE,  // type
            textures[layer]);
    }
    return ;
}


void TexturesArray4AI::show_collected_texture()
{   
    u32 layer = 0;
    f32 rotation_degree = 0.0f;
    bool left_pressed=false, right_pressed = false, up_pressed = false, down_pressed = false;
    
    GLFWwindow* window = glfwGetCurrentContext();
    if(!window) assert(0);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) assert(0);
    glfwSwapInterval(1);
    glfwMakeContextCurrent(window);
    glfwSetWindowShouldClose(window, false);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    GLuint temptexture;
    prepare_tmp_texture2d_array(temptexture); // generate the texture 2d array and push textures into it, assign the id to temptexture
    glBindTexture(GL_TEXTURE_2D_ARRAY, temptexture);
    glUseProgram(shaderProgram);
    GLcall(glUniform1i(glGetUniformLocation(shaderProgram, "texArray"), 0));
    glDisable(GL_CULL_FACE);
    while (!glfwWindowShouldClose(window))
    {   
        glClearColor(0.3f, .3f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && !left_pressed) 
        {
            layer = (layer + total_num_textures - 1) % total_num_textures;
            left_pressed = true;
            printf("Showing tile %d of %d\n", layer + 1, total_num_textures);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE) left_pressed = false;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && !right_pressed) 
        {
            layer = (layer + 1) % total_num_textures;
            right_pressed = true;
            printf("Showing tile %d of %d\n", layer + 1, total_num_textures);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) right_pressed = false;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && !up_pressed) 
        {
            rotation_degree += 90.f;
            while (rotation_degree >= 360.f) rotation_degree -= 360.f;
            up_pressed = true;
            printf("Rotatio: %.1f\n", rotation_degree);
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_RELEASE) up_pressed = false;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && !down_pressed) 
        {
            rotation_degree -= 90.f;
            while (rotation_degree < -360.f) rotation_degree += 360.f;
            down_pressed = true;
            printf("Rotatio: %.1f\n", rotation_degree);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_RELEASE) down_pressed = false;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, temptexture);

        u32 row = 0;
        for (u32 i = layer; i<layer + 3; i ++ ) 
        {   
            u32 texture_id = texture_id_cal(row, i, num_textures_1d);
            auto model = glm::mat4(1.0f);
            // Rotate the model 90 degrees clockwise
            f32 angle = glm::radians(rotation_degree); // Negative for clockwise rotation
            
            model = glm::translate(model, glm::vec3(-1.0f + ((f32)(i - layer )+ 0.5) * 2.f / 3.0f, 0.0f, 0.0f)) * glm::scale(model, glm::vec3(1.0f/3.f, 1.0f/3.f, 1.0f)) * glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::scale(model, glm::vec3(1.0f, -1.0f, 1.0f));
            glUniform1i(glGetUniformLocation(shaderProgram, "layer"), texture_id);
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            GLcall(glBindVertexArray(vao));
            GLcall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glEnable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glDeleteTextures(1, &temptexture);
    return ;
}

#ifdef DEBUG
    void TexturesArray4AI::show_collected_textures()
    {
        GLuint tmp_texture2d_array;
        this->prepare_tmp_texture2d_array(tmp_texture2d_array);
        
        GLFWwindow* window = glfwGetCurrentContext();
        if(!window) assert(0);
        glfwSwapInterval(1);
        glfwMakeContextCurrent(window);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) assert(0);
        glfwSetWindowShouldClose(window, false);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();
        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        #ifdef __EMSCRIPTEN__
            ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
        #endif
            ImGui_ImplOpenGL3_Init((const char*)"#version 330");
        // Our state
        bool show_demo_window = false;
        bool show_another_window = false;
        glm::vec4 clear_color(0.45f, 0.55f, 0.60f, 1.00f);
        bool m_pressed = false;

        int width = 1080, height = 1080;
        glViewport(0, 0, width, height);
        glDisable(GL_CULL_FACE); // can also see the back face
        Show_State show_state;

        glfwSetWindowUserPointer(window, &show_state);
        // register the scroll and mouse move callback function
        glfwSetScrollCallback(window, scroll_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glUseProgram(shaderProgram);
        GLcall(glUniform1i(glGetUniformLocation(shaderProgram, "texArray"), 0));
        glm::mat4 model_not_flipped = glm::scale( // 控制显示大小
            glm::mat4(1.0f), 
            glm::vec3(
                1.0f/(f32)num_textures_1d, 
                -1.0f/(f32)num_textures_1d, // flipped on y axis 
                1.0f));
        glm::mat4 model_flipped = glm::scale( // 控制显示大小
            glm::rotate(model_not_flipped, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), 
            glm::vec3(1.0f, -1.0f, 1.0f));
        while (!glfwWindowShouldClose(window))
        {   
            int viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            f32 ratio_w_h = (f32)viewport[2] / (f32)viewport[3];
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            glBindBuffer(GL_TEXTURE_2D_ARRAY, tmp_texture2d_array);

            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glm::mat4 projection = glm::ortho( // 控制镜头远近
                -1.0f * ratio_w_h * show_state.zoomlevel, 1.0f * ratio_w_h * show_state.zoomlevel, 
                -1.0f * show_state.zoomlevel, 1.0f * show_state.zoomlevel, 
                -1.0f, 1.0f);
            
            glm::mat4 model_tmp;
            for (int row = 0; row < num_textures_1d; row++ )// 显示所有的 tile
            {
                for (int column = 0; column < num_textures_1d; column ++ )
                {   
                    // if (row > column) continue;
                    u32 layer = texture_id_cal((u32)row, (u32)column, num_textures_1d);
                    GLcall(glUniform1i(glGetUniformLocation(shaderProgram, "layer"), (int)layer));
                    model_tmp =  glm::translate( // 控制显示位置
                        glm::mat4(1.0f), 
                        glm::vec3( 
                            -1.0f + ((f32)column + 0.5f) * 2.0f / (f32)num_textures_1d + show_state.translationOffset.x,
                                1.0f - ((f32)row + 0.5f) * 2.0f / (f32)num_textures_1d + show_state.translationOffset.y,
                                0.0f)
                        ) * (row>column?model_flipped:model_not_flipped);
                    model_tmp = projection * model_tmp;
                    GLcall(glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model_tmp)));
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
            }

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)  glfwSetWindowShouldClose(window, true);
            if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) 
            {
                if (!m_pressed) show_state.show_menu_window = !show_state.show_menu_window;
                m_pressed = true;
            }
            if (glfwGetKey(window, GLFW_KEY_U) == GLFW_RELEASE) m_pressed = false;
            
            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
            if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);
            // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
            if (show_state.show_menu_window )
            {
                ImGui::Begin("Main menu");                          // Create a window called "Hello, world!" and append into it.
                ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

                ImGui::ColorEdit3("clear color", (f32*)&clear_color[0]); // Edit 3 floats representing a color
                ImGui::Text("Zoom level: %.4f", show_state.zoomlevel);
                ImGui::Text("Translation Offset: (%.2f, %.2f)", show_state.translationOffset.x, show_state.translationOffset.y);
                // ImGui::SameLine();
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
                ImGui::End();
            }

            // 3. Show another simple window.
            if (show_another_window)
            {
                ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me"))
                    show_another_window = false;
                ImGui::End();
            }

            // Rendering
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glDeleteTextures(1, &tmp_texture2d_array);
        return ;
    } 
#else 
    void TexturesArray4AI::show_collected_textures(){return ;}
#endif // DEBUG



void TexturesArray4AI::cal_maximum_number_of_shift(
    u32& D,
    std::vector<f32>& norm_diag_mean,
    const map_contigs* Contigs,
    f32 D_hic_ratio,
    u32 maximum_D,
    f32 min_hic_density)
{   
    f32 global_shift0_diag_mean = this->cal_diagonal_mean_within_fragments(D, Contigs);
    norm_diag_mean.push_back(global_shift0_diag_mean);
    f32 tmp_diag_mean = this->cal_diagonal_mean_within_fragments(++D, Contigs);
    while (
        tmp_diag_mean > min_hic_density && 
        tmp_diag_mean > D_hic_ratio * global_shift0_diag_mean && 
        D < maximum_D
    ) {
        norm_diag_mean.push_back(tmp_diag_mean);
        tmp_diag_mean = this->cal_diagonal_mean_within_fragments(++D, Contigs); 
    }

    if (D < 2)
    {
        fmt::print(stderr, 
            "Warning: D is less than 2, please check the Contigs of this sample.\n");
        assert(0);
    }
    return ;
}


void TexturesArray4AI::cal_compressed_hic(
    const map_contigs* Contigs,
    const extension_sentinel& Extensions,
    bool is_extension_required,
    bool is_massCenter_required,
    const SelectArea* select_area,
    const AutoCurationState* auto_curation_state,
    f32 D_hic_ratio, 
    u32 maximum_D, 
    f32 min_hic_density) 
{   
    check_copied_from_buffer();

    u08 using_select_area = (select_area && select_area->select_flag)?1:0;
    
    u08 cluster_flag = (auto_curation_state && auto_curation_state->num_clusters > 1)?1:0;

    // re-calculate the frags selected within this area
    frags->re_allocate_mem(
        Contigs,
        select_area, 
        false,  // used for cut
        cluster_flag);
    /*
    num_pixels_1d is the size of the Hi‑C texture (num_textures_1d × texture_resolution), often a fixed grid (e.g. 32768).
    frags->total_length is the sum of contig lengths in the map.
    So you can have a few extra pixels in the texture that are not assigned to any contig (padding / unused tail). 
    The old check required total_length == num_pixels_1d for a full‑map sort, which is too strict and triggered 
    the assert even when the data is consistent.*/
        const u32 expected_len =
        using_select_area ? select_area->get_selected_len(Contigs, cluster_flag) : num_pixels_1d;

    if (frags->total_length > num_pixels_1d)
    {
        fmt::print(
            stderr,
            "\n[Compress Hic] error: frags->total_length({}) > num_pixels_1d({}) — contigs extend past the Hi-C map. file:{}, line:{}\n\n",
            frags->total_length,
            num_pixels_1d,
            __FILE__,
            __LINE__);
        assert(0);
    }

    if (frags->total_length != expected_len)
    {
        if (!using_select_area && frags->total_length < num_pixels_1d)
        {
            fmt::print(
                "\n[Compress Hic] note: frags->total_length({}) < num_pixels_1d({}) (full area) — map has unused trailing padding; continuing. file:{}, line:{}\n\n",
                frags->total_length,
                num_pixels_1d,
                __FILE__,
                __LINE__);
        }
        else
        {
            fmt::print(
                "\n[Compress Hic] warning: frags->total_length({}) != expected_len({}) ({}) ({}). file:{}, line:{}\n\n",
                frags->total_length,
                expected_len,
                (using_select_area ? "selected area" : "full area"),
                cluster_flag ? "clustering" : "not clustering",
                __FILE__,
                __LINE__);
            assert(0);
        }
    }
    // clean the memory of compressed_hic_mx
    if (compressed_hic)
    {
        compressed_hic->re_allocate_mem(
            frags->num, 
            frags->num, 
            5);
    }
    else 
    {
        compressed_hic = new Matrix3D<f32>(
            frags->num, 
            frags->num, 
            5);
    }
    if (mass_centres) 
    {
        delete mass_centres;
        mass_centres = nullptr;
    }

    const u32 num_interaction_to_cal = (frags->num * (frags->num - 1))>>1;

    if (is_extension_required)
    {
        // compress the extensions
        compressed_extensions->re_allocate_mem(frags->num);
        cal_compressed_extension(Extensions); 
    }

    if (is_massCenter_required)
    {
        // cal the mass center
        mass_centres = new MassCentre[frags->num * frags->num];
        u32 cnt = 0;
        for (u32 i = 0; i < frags->num; ++i)
        {   
            for (u32 j = i+1; j < frags->num; ++j)
            {   
                cal_mass_centre(
                    frags->startCoord[i],
                    frags->startCoord[j],
                    frags->length[i],
                    frags->length[j],
                    mass_centres + i * frags->num + j);
                mass_centres[j * frags->num + i].row = mass_centres[i * frags->num + j].col;
                mass_centres[j * frags->num + i].col = mass_centres[i * frags->num + j].row;
                ++cnt;
                if (cnt % (num_interaction_to_cal / 100) == 0)
                {
                    fprintf(stdout, "\rCalculating mass center %.2f%%, [%d/%d]", (f32)cnt / (f32)num_interaction_to_cal * 100.0f, cnt, num_interaction_to_cal);
                    fflush(stdout);
                }
            }
        }
        fprintf(stdout, "\n");
    }
    
    // cal the average hic interation 
    for (u32 i = 0 ; i < frags->num; i++)
    {
        for (u32 j = i; j < frags->num; j++)
        {   
            if (frags->length[i] < 1u || frags->length[j] < 1u)
            {
                (*compressed_hic)(i, j, 4) = (*compressed_hic)(j, i, 4) = 0.f;
            }
            else
            {
                (*compressed_hic)(i, j, 4) = (*compressed_hic)(j, i, 4) = cal_block_mean(
                    frags->startCoord[i], 
                    frags->startCoord[j], 
                    frags->length[i], 
                    frags->length[j])/255.f;
            }
        }
    }

    // cal the maximum number of shift
    u32 D=0;
    std::vector<f32> norm_diag_mean = {};
    this->cal_maximum_number_of_shift(
        D, 
        norm_diag_mean,
        Contigs,
        D_hic_ratio,
        maximum_D,
        min_hic_density);

    Values_on_Channel buffer_values_on_channel;
    u32 cnt = 0;
    for (u32 i = 0; i < frags->num; i++)
    {
        for (u32 j = i+1; j < frags->num; j++)
        {   
            this->get_interaction_score(
                    frags->startCoord[i], 
                    frags->startCoord[j], 
                    frags->length[i], 
                    frags->length[j], 
                    D,
                    norm_diag_mean, 
                    buffer_values_on_channel);
            for (u32 channel = 0; channel < 4 ; channel ++ )
            {
                (*compressed_hic)(i, j, channel) = (*compressed_hic)(j, i, Switch_Channel_Symetric[channel]) = buffer_values_on_channel.c[channel];
            }
            if (num_interaction_to_cal > 100 && ++cnt % (num_interaction_to_cal / 100)==0)
            {
                printf("\rCalculating compressed_hic %.2f%%", (f32)(cnt) / (f32)num_interaction_to_cal * 100.0f);
                fflush(stdout);
            }
        }
    }
    printf("\n");
    is_compressed = true;

    #ifdef DEBUG
        if (is_extension_required && is_massCenter_required) output_compressed_hic_massCetre_extension_for_python();
    #endif // DEBUG

}


f32 TexturesArray4AI::cal_diagonal_mean_within_fragments(int shift, const map_contigs* Contigs)
{   
    u64 sum = 0;
    u32 cnt = 0, maxlen=0, start_coord = 0;
    for (int i = 0 ; i < Contigs->numberOfContigs; i ++ )
    {   
        maxlen = std::max(maxlen, Contigs->contigs_arr[i].length);
        Sum_and_Number tmp = this->get_fragement_diag_mean_square( 
            start_coord, 
            Contigs->contigs_arr[i].length, 
            shift );
        sum += tmp.sum;
        cnt += tmp.number; // number of pixels
    }
    if ((f32)cnt <= (f32)maxlen * 0.2)
    {
        return 0.f;
    }

    return (f32)sum / (f32)std::max(cnt, 1u);
}


void TexturesArray4AI::get_interaction_score(
    const u32& row, 
    const u32& column, 
    const u32& num_row, 
    const u32& num_column, 
    const u32& D,
    const std::vector<f32>& norm_diag_mean,
    Values_on_Channel& buffer_values_on_channel)
{
    check_copied_from_buffer();

    if (num_row < 1 || num_column < 1)
    {
        buffer_values_on_channel.initilize();
        return;
    }

    std::function<f32(f32, int)> score_cal = [&norm_diag_mean](const f32& mean, const int& i)->f32
        {   
            f32 dis = std::abs(mean - norm_diag_mean[i]);
            if (dis> norm_diag_mean[i]) return 0.f;
            else return 1.0f - dis / norm_diag_mean[i];
        };

    u32 number_of_scores = std::min(D-1, std::max(num_row, num_column));
    u32 len_of_weight_mask = std::min(D-1, (u32)(number_of_scores + 5) );  // 往外多算几个像素点，因此小的片段的link score分就会相对下降, 特别是很小的片段
    // f32 linear_weight[number_of_scores]; 
    // get_linear_mask(linear_weight, number_of_scores);  // assign the linear weight mask
    std::vector<f32> linear_weight(len_of_weight_mask, 0.f);
    get_linear_mask(linear_weight ); 

    buffer_values_on_channel.initilize();
    for (int channel = 0; channel < 4; channel++)
    {   
        for (int shift = 1; shift < number_of_scores+1; shift++)
        {
            Sum_and_Number tmp = get_interaction_block_diag_sum_number( 
                row, column, num_row, num_column, 
                shift, channel);
            f32 tmp_mean = (f32)tmp.sum / (f32)std::max(tmp.number, 1u);
            buffer_values_on_channel.c[channel] += linear_weight[shift-1] * score_cal(tmp_mean, shift);
        }
    }
    // delete[] linear_weight;
    return ;
}


void TexturesArray4AI::cal_graphData(
    GraphData*& graph_data, 
    f32 selected_edges_ratio, 
    f32 random_selected_edges_ratio, 
    u32 minimum_edges, 
    u32 minimum_random_edges, 
    f32 length_threshold, 
    u32 num_node_properties, 
    u32 num_edge_properties)
{
    if (is_compressed == false || 
        compressed_hic == nullptr || 
        compressed_extensions == nullptr || 
        mass_centres == nullptr || 
        frags == nullptr) 
    {
        std::cerr << "The compressed hic is not calculated, please first calculate the compressed hic" << std::endl;
        assert(0);
    }

    // cal the number of nodes, egdes, num_node_properties, num_edge_properties
    u32 num_nodes=0, num_edges=0;
    /*
        num_nodes = total_number_of_nodes - filtered_out  ;  // 
        num_egdes = num_selected_edges + num_random_selected_edges;
        num_node_properties = 1 + 1 + 1 ... ;
        num_edge_properties = 4 + 1 + 2 ...  ;
    */

    std::vector<u32> selected_nodes_inx;
    for (u32 i = 0; i < frags->num; i++)
    {
        if (((f32)frags->length[i] / ((f32)frags->total_length + 1.0f)) > length_threshold)
        {
            num_nodes++;
            selected_nodes_inx.push_back(i);
        }
    }
    // sort the edges of the selected nodes and then select the first minimum_edges
    u32 num_full_edges = (num_nodes * (num_nodes - 1)) >> 1;
    // build the selected_edges_inx
    u32* edges_idx_to_original_nodesNum = new u32[num_full_edges * 2];
    u32* edges_idx_to_new_nodesNum = new u32[num_full_edges * 2];
    {
        u32 cnt = 0;
        for (u32 i = 0; i < num_nodes; i++)
        {
            for (u32 j = i + 1; j < num_nodes; j++)
            {   
                edges_idx_to_original_nodesNum[2*cnt    ] = selected_nodes_inx[i];
                edges_idx_to_original_nodesNum[2*cnt + 1] = selected_nodes_inx[j];
                edges_idx_to_new_nodesNum[2*cnt    ] = i;
                edges_idx_to_new_nodesNum[2*cnt + 1] = j;
                cnt ++ ;
            }
        }
        if (cnt != num_full_edges) 
        {
            fprintf(stderr, "The cnt(%d) != num_full_edges(%d)", cnt, num_full_edges);
            assert(0);
        }
    }

    std::vector<u32> selected_edges_idx;
    if ((f32) num_full_edges * (selected_edges_ratio + random_selected_edges_ratio) < (minimum_edges + minimum_random_edges)) // select all the edges
    {
        num_edges = num_full_edges;
        selected_edges_idx.resize(num_full_edges);
        for (u32 i = 0; i < num_full_edges; i++)
        {
            selected_edges_idx[i] = i;
        }
    }
    else // select the edges with highest average hic score
    {   
        u32 num_selected_edges = num_full_edges * selected_edges_ratio, num_random_selected_edges = num_full_edges * random_selected_edges_ratio;
        num_edges = num_selected_edges + num_random_selected_edges;
        // select #(selected_edges_ratio*num_full_edges) edges with the highest average hic score
        std::priority_queue<std::pair<f32, u32>, std::vector<std::pair<f32, u32>>, std::greater<std::pair<f32, u32>>> pq;  // score, id, put the smallest score on the top
        std::vector<u32> remainning_edges_idx;
        {
            u32 cnt = 0;
            for (u32 i = 0; i < num_nodes; i++)
            {
                for (u32 j = i + 1; j < num_nodes; j++)
                {   
                    f32 mean_score = (
                        (*compressed_hic)(selected_nodes_inx[i], selected_nodes_inx[j], 0) +
                        (*compressed_hic)(selected_nodes_inx[i], selected_nodes_inx[j], 1) + 
                        (*compressed_hic)(selected_nodes_inx[i], selected_nodes_inx[j], 2) + 
                        (*compressed_hic)(selected_nodes_inx[i], selected_nodes_inx[j], 3) ) / 4.f;
                    if (pq.size() < num_selected_edges)
                    {
                        pq.push(std::make_pair(mean_score, cnt));
                    }
                    else if (pq.top().first < mean_score)
                    {   
                        remainning_edges_idx.push_back(pq.top().second);
                        pq.pop();
                        pq.push(std::make_pair(mean_score, cnt));
                    }
                    cnt ++ ;
                }
            }
            if (cnt != num_full_edges) 
            {
                fprintf(stderr, "The cnt(%d) != num_full_edges(%d)", cnt, num_full_edges);
                assert(0);
            }
        }
        while (!pq.empty())
        {
            selected_edges_idx.push_back(pq.top().second);
            pq.pop();
        }
        
        std::random_device rd;
        std::default_random_engine random_engine(rd());
        std::shuffle(remainning_edges_idx.begin(), remainning_edges_idx.end(), random_engine);
        for (u32 i = 0; i < num_random_selected_edges && !remainning_edges_idx.empty(); i++)
        {
            selected_edges_idx.push_back(remainning_edges_idx.back());
            remainning_edges_idx.pop_back();
        }
    }

    // =============== build the graph_data ===============
    // new the memory for the graph_data
    graph_data = new GraphData(
        num_nodes, 
        num_edges * 2,  // bidirectional edges
        num_node_properties, 
        num_edge_properties, 
        selected_nodes_inx,
        frags); 
    printf(
        "num_selected_nodes: %d\nnum_selected_edges: %d\nnum_node_properties:%d\nnum_edge_properties: %d\n", 
        graph_data->get_num_nodes(), graph_data->get_num_edges(), 
        graph_data->get_num_node_properties(), graph_data->get_num_edge_properties());

    // assign the node properties
    for (u32 i = 0; i < num_nodes; i++)
    {   
        u32 original_frags_id = selected_nodes_inx[i];
        graph_data->nodes(i, 0)= (f32)frags->length[original_frags_id] / (f32)num_pixels_1d;       // frag length
        graph_data->nodes(i, 1) = compressed_extensions->data[0][original_frags_id];               // coverage
        graph_data->nodes(i, 2) = compressed_extensions->data[1][original_frags_id];               // repeat_density
        graph_data->nodes(i, 3) = (f32)(frags->total_length > 32768);                              // is_high_resolution
        graph_data->nodes(i, 4) = (f32)frags->startCoord[original_frags_id] / (f32)num_pixels_1d;  // start_locs
        graph_data->nodes(i, 5) = (f32)compressed_extensions->exist_flags[0];                      // coverage_exists
        graph_data->nodes(i, 6) = (f32)compressed_extensions->exist_flags[1];                      // repeat_density_exits
        graph_data->nodes(i, 7) = 1.0f / (f32)frags->num;                                          // 1.0 / node_num
    }

    // assign the edges idx and the edge properties
    for (u32 i = 0; i < num_edges; i++)
    {   
        u32 id = selected_edges_idx[i];
        graph_data->edges_index(0, 2 * i) = edges_idx_to_new_nodesNum[2*id];
        graph_data->edges_index(1, 2 * i) = edges_idx_to_new_nodesNum[2*id + 1];
        graph_data->edges_index(0, 2 * i + 1) = edges_idx_to_new_nodesNum[2*id + 1];
        graph_data->edges_index(1, 2 * i + 1) = edges_idx_to_new_nodesNum[2*id];

        u32 node_i = edges_idx_to_original_nodesNum[2*id], 
            node_j = edges_idx_to_original_nodesNum[2*id + 1];
        // node_i -> node_j
        graph_data->edges(2 * i, 0)     = (*compressed_hic)(node_i, node_j, 0);
        graph_data->edges(2 * i, 1)     = (*compressed_hic)(node_i, node_j, 1);
        graph_data->edges(2 * i, 2)     = (*compressed_hic)(node_i, node_j, 2);
        graph_data->edges(2 * i, 3)     = (*compressed_hic)(node_i, node_j, 3);
        graph_data->edges(2 * i, 4)     = (*compressed_hic)(node_i, node_j, 4);
        graph_data->edges(2 * i, 5)     = mass_centres[node_i * frags->num + node_j].row;
        graph_data->edges(2 * i, 6)     = mass_centres[node_i * frags->num + node_j].col; 
        // node_j -> node_i
        graph_data->edges(2 * i + 1, 0) = (*compressed_hic)(node_j, node_i, 0);
        graph_data->edges(2 * i + 1, 1) = (*compressed_hic)(node_j, node_i, 1);
        graph_data->edges(2 * i + 1, 2) = (*compressed_hic)(node_j, node_i, 2);
        graph_data->edges(2 * i + 1, 3) = (*compressed_hic)(node_j, node_i, 3);
        graph_data->edges(2 * i + 1, 4) = (*compressed_hic)(node_j, node_i, 4);
        graph_data->edges(2 * i + 1, 5) = mass_centres[node_j * frags->num + node_i].row;
        graph_data->edges(2 * i + 1, 6) = mass_centres[node_j * frags->num + node_i].col;
    }

    delete[] edges_idx_to_original_nodesNum;
    delete[] edges_idx_to_new_nodesNum;

    return ;

}

Sum_and_Number TexturesArray4AI::get_fragement_diag_mean_square(
    u32 offset, 
    u32 length,  
    int shift)
{   
    if (shift < 0 )
    {
        std::cerr << "The shift is less than 0" << std::endl;
        assert(0);
    }
    this->check_copied_from_buffer();
    if (shift >= length)
    {
        return {0, 0};
    }
    u64 sum = 0;
    u32 total_count = length - shift, row_id = 0, col_id = shift;
    while (total_count--)
    {
        sum += (u64)(*this)(
            offset + row_id++, 
            offset + col_id++);
    }
    return {sum, length - shift};
}



Sum_and_Number TexturesArray4AI::get_interaction_block_diag_sum_number(
    u32 offset_row, 
    u32 offset_column, 
    u32 num_row, 
    u32 num_column, 
    int shift,         // number of element from the angle of the block, 0 means no element, thus the minimum distance is 1
    int channel)
/* calculate the diag (with shift) mean of matrix [offset_row:offset_row + num_row, offset_col:offset_col + num_col] */
/*
    there are 4 modes to calculate the diag mean
        0 ----- 1 
        |       |
        |       |
        3 ----- 2
*/
{
    check_copied_from_buffer();
    u32 max_num = std::max(num_row, num_column), min_num = std::min(num_row, num_column);
    if (shift <= 0 || shift > max_num) // check: shift is in the range
    {
        std::cerr << "Shift("<< shift<< ") should be within [1, "<< max_num << "]" << std::endl;
        assert(0);
    }

    // check: submatrix is in the range
    if (offset_row + num_row > num_pixels_1d || offset_column + num_column > num_pixels_1d)
    {
        fprintf(stderr, "offset_row: %d, offset_column: %d, num_row: %d, num_column: %d is out of [%d:%d, %d:%d]\n", offset_row, offset_column, num_row, num_column, 0, num_pixels_1d - 1, 0, num_pixels_1d - 1);
        assert(0);
    }
    
    u64 sum = 0; 
    u32 total_count = std::min(min_num, (u32)shift);
    if (channel == 0)
    {   
        u32 row_id = shift<=num_column? 0:shift-num_column, 
            col_id = shift<=num_column? shift-1:num_column-1;
        while (total_count--)
        {
            sum += (u64)(*this)(
                offset_row + row_id++, 
                offset_column + col_id--);
        }
    }
    else if (channel == 1)
    {
        u32 row_id = shift <= num_column? 0:shift - num_column, 
            col_id = shift <= num_column? num_column - shift:0;
        while (total_count--)
        {   
            sum += (u64)(*this)(
                offset_row + row_id++, 
                offset_column + col_id++);
        }
    } else if (channel ==2 ) 
    {   
        u32 row_id = shift<=num_column? num_row-1:num_row-1-(shift-num_column), 
            col_id = shift<=num_column? num_column-shift:0;
        while (total_count--)
        {
            sum += (u64)(*this)(
                offset_row + row_id--, 
                offset_column + col_id++);
        }
    } else if (channel == 3) 
    {   
        u32 row_id = shift<=num_column? num_row-1:num_row-1-(shift-num_column), 
            col_id = shift<=num_column? shift-1:num_column-1;
        while (total_count--)
        {   
            sum += (u64)(*this)(
                offset_row + row_id-- , 
                offset_column + col_id--);
        }
    }
    else 
    {   
        fprintf(stderr, "The channel(%d) is out of range [0, 3]\n", channel);
        assert(0);
    }

    return {sum, std::min(min_num, (u32)shift)};
}

f32 TexturesArray4AI::cal_block_mean(const u32 &offset_row, const u32 &offset_column, const u32 &num_row, const u32 &num_column) const
{   
    // Zero-length contigs: empty block (callers should skip when possible to avoid log noise).
    if (num_row < 1 || num_column < 1)
        return 0.f;
    u64 sum = 0;
    for (u32 i = offset_row; i < offset_row + num_row; i++)
    {
        for (u32 j = offset_column; j < offset_column + num_column; j++)
        {
            sum += (u64)(*this)(i, j);
        }
    }
    return (f32)sum / (f32)(num_row * num_column);
}


void cal_mask(f32* mask, u32 num)
{
    if (num == 1)
    {
        mask[0] = 1.0f;
        return ;
    }
    for (u32 i = 0; i < num; i++)
    {
        mask[i] = -1.0f + 2.0f / (f32)(num-1) * (f32)i;
        mask[i] = mask[i] * mask[i];
    }
}


template <typename T>
T sum_of_array(const u32 N, const T *X, const u32 incX)
{
    T sum = 0;
    if (N <= 0 || incX <= 0)
        return sum;

    for (u32 i = 0; i < N; i++) {
        sum +=  X[i * incX];
    }
    return sum;
}


void TexturesArray4AI::cal_mass_centre(const u32 &offset_row, const u32 &offset_column, const u32 &num_row, const u32 &num_column, MassCentre* mass_centre) 
{   
    if (num_row < 1 || num_column < 1)
    {
        std::cerr << "[cal_mass_centre] skipping empty block: num_row=" << num_row
                  << " num_column=" << num_column << " at offset (" << offset_row << "," << offset_column << ")\n";
        mass_centre->row = 0.5f;
        mass_centre->col = 0.5f;
        return;
    }
    if (num_row == 1 && num_column == 1)
    {
        mass_centre->row = 0.5f;
        mass_centre->col = 0.5f;
        return ;
    }

    f32* data_tmp = new f32[num_row * num_column];
    // f32* mask_col = new f32[num_column];
    // f32* mask_row = new f32[num_row];
    // cal_mask(mask_col, num_column);
    // cal_mask(mask_row, num_row);
    
    // set the data_tmp
    for (u32 i = 0; i < num_row; i++)
    {
        for (u32 j = 0; j < num_column; j++)
        {   
            f32* tmp = data_tmp + i * num_column + j;
            *tmp = (f32)(*this)(i + offset_row, j + offset_column) / 256.; // * mask_col[j] * mask_row[i];
            *tmp = *tmp / (1.0 - *tmp);
            *tmp = *tmp * *tmp;
        }
    }

    f32 data_sum = sum_of_array(num_row * num_column, data_tmp, 1); 
    if (data_sum < 1e-3)
    {
        mass_centre->row = 0.5f;
        mass_centre->col = 0.5f;
        delete[] data_tmp;
        // delete[] mask_col;
        // delete[] mask_row;
        return ;
    }

    f32 row_sum = 0.f, col_sum = 0.f;
    for (u32 i = 0; i < num_row; i++)
    {
        f32 row_tmp = sum_of_array(num_column, data_tmp + i * num_column, 1);
        row_sum += row_tmp * (f32)i;
    }
    for (u32 j = 0; j < num_column; j++)
    {
        f32 col_tmp = sum_of_array(num_row, data_tmp + j, num_column);
        col_sum += col_tmp * (f32)j;
    }
    delete[] data_tmp;
    // delete[] mask_col;
    // delete[] mask_row;
    mass_centre->row = num_row > 1 ? (row_sum / data_sum / (f32)(num_row - 1)) : 0.5f;
    mass_centre->col = num_column > 1 ? (col_sum / data_sum / (f32)(num_column - 1)) : 0.5f;
    
    return ;
}

void TexturesArray4AI::output_textures_to_file() const
{   
    check_copied_from_buffer();
    
    auto outputfunc = [&](std::ofstream& f, u32 row, u32 col, u32 row_num, u32 col_num){
        for (u32 i = row; i < row_num+row; i++)
        {
            for (u32 j = col; j < col_num+col; j++)
            {
                f << (u32)(*this)(i, j) << " ";
            }
            f << std::endl;
        }
    };

    u32 row_id = 0, col_id = 1;
    for (u32 row = row_id; row < row_id + 1; row ++ )
    {
        for (u32 col = col_id; col < col_id+1; col++)
        {   
            std::string filename = "python/test_data_from_openglbuffer/test_output_textures_to_file_" + std::string(name) + "_" + std::to_string(row) + "_" + std::to_string(col) + ".txt";
            std::ofstream out_file(filename);
            if (!out_file)
            {
                printf("Cannot open the file %s\n", filename.c_str());
                assert(0);
            }
            u32 start1 = frags->startCoord[row], 
                start2 = frags->startCoord[col], 
                length1 = frags->length[row],
                length2 = frags->length[col];
            outputfunc(out_file, start1, start2, length1, length2);
            out_file.close();
            printf("Finished writing the textures to file %s\n", filename.c_str());
        }
    }

    return ;
}

void TexturesArray4AI::output_compressed_hic_to_file() const
{   
    if (!is_compressed || compressed_hic == nullptr)
    {
        std::cerr << "The compressed hic is not calculated yet" << std::endl;
        assert(0);
    }

    for (u32 channel = 0; channel < 5 ; channel++)
    {
        std::ofstream out_file("python/test_data_from_openglbuffer/test_output_compressed_hic_to_file_" + std::string(name) + "_" + std::to_string(channel) + ".txt");
        for (u32 i = 0; i < compressed_hic->shape[0]; i++)
        {
            for (u32 j = 0; j < compressed_hic->shape[1]; j++)
            {
                out_file << (*compressed_hic)(i, j, channel) << " ";
            }
            out_file << std::endl;
        }
        out_file.close();
    }
    return ;
}

void TexturesArray4AI::output_compressed_extension_to_file() const
{   
    if (!is_compressed || compressed_extensions == nullptr)
    {
        std::cerr << "The compressed extension is not calculated yet" << std::endl;
        assert(0);
    }

    for (u32 i = 0; i < compressed_extensions->num ; i ++ ){
        std::stringstream ss;
        ss << "python/test_data_from_openglbuffer/test_output_compressed_extension_to_file_" << std::string(name) << "_" << compressed_extensions->names[i] << ".txt";
        std::ofstream out_file(ss.str());
        for (u32 j = 0; j < compressed_extensions->num_frags; j++)
        {
            out_file << compressed_extensions->data[i][j] << " ";
        }
        out_file.close();
    }
}

void TexturesArray4AI::output_mass_centres_to_file() const
{
    if (mass_centres == nullptr)
    {
        std::cerr << "The mass centres are not calculated yet" << std::endl;
        assert(0);
    }

    std::ofstream out_file_row("python/test_data_from_openglbuffer/test_output_mass_centres_to_file_" + std::string(name) + "_row.txt");
    std::ofstream out_file_col("python/test_data_from_openglbuffer/test_output_mass_centres_to_file_" + std::string(name) + "_col.txt");
    for (u32 i = 0; i < frags->num; i++)
    {
        for (u32 j = 0; j < frags->num; j++)
        {
            out_file_row << mass_centres[i * frags->num + j].row << " ";
            out_file_col << mass_centres[i * frags->num + j].col << " ";
        }
        out_file_row << std::endl;
        out_file_col << std::endl;
    }
    out_file_row.close();
    out_file_col.close();
    return ;
}

static void fill_0_with_neighbor(u32* original_data, u32* processed_data, u32 len, float ratio = 0.009)
{   

    u32 num_zeros = 0;
    for (u32 i = 0; i < len; i++) {
        processed_data[i] = original_data[i];
        if (original_data[i] == 0) num_zeros++;
    }
    
    if ( (float)num_zeros < ratio * (float)len ) return ;
    
    for (s32 i = 0; i < len; i++)
    {
        if (original_data[i] != 0) continue;
        s32 left = i - 1, right = i + 1;
        while (left >= 0 && original_data[left] == 0) left--;
        while (right < len && original_data[right] == 0) right++;

        if (left >= 0 && right < len) processed_data[i] = (original_data[left] + original_data[right]) >> 1;
        else if (left >= 0) processed_data[i] = original_data[left];
        else if (right < len) processed_data[i] = original_data[right];
        else processed_data[i] = 0;
    }
}


static u32 top_k(u32* data, u32 len, u32 k)
{   
    std::priority_queue<u32, std::vector<u32>, std::greater<u32>> pq; // bottom greater
    
    for (u32 i = 0; i < len; i++)
    {
        if (pq.size() < k) pq.push(data[i]);
        else if (data[i] > pq.top())
        {
            pq.pop();
            pq.push(data[i]);
        }
    }
    return pq.top();
}


static u32 bottom_k(u32* data, u32 len, u32 k)
{
    std::priority_queue<u32, std::vector<u32>, std::less<u32>> pq; // bottom less
    for (u32 i = 0; i < len; i++)
    {
        if (pq.size() < k) pq.push(data[i]);
        else if (data[i] < pq.top())
        {
            pq.pop();
            pq.push(data[i]);
        }
    }
    return pq.top();
}


static f32 mean_cal(u32* data, u32 len)
{
    u64 sum = 0;
    for (u32 i = 0; i < len; i++) sum += data[i];
    return (f32)sum / (f32)std::max(len, 1u);
}



void TexturesArray4AI::cal_compressed_extension(const extension_sentinel &Extensions)
{   
    TraverseLinkedList(Extensions.head, extension_node) // traverse all the extensions
    {   
        s32 extension_id = -1;
        if (node->type != extension_graph) continue;
        graph* graph_tmp = (graph*)node->extension;
        std::string extension_name = std::string((char*)graph_tmp->name);
        for (u32 i = 0 ; i <compressed_extensions->num; i++)
        {
            if (extension_name == compressed_extensions->names[i] && compressed_extensions->exist_flags[i] == false)
            {
                extension_id = i;
                break;
            }
        }
        if (extension_id < 0) continue;;

        auto* processed_data = new u32[num_pixels_1d];

        // initialize, and fill the 0 with the average of the neighbors if the number of zeros exceeds the ratio = 0.009
        fill_0_with_neighbor(graph_tmp->data, processed_data, num_pixels_1d);

        // filter the first 0.01 and last 0.01 out
        u32 min_10 = bottom_k(processed_data, num_pixels_1d, std::max(num_pixels_1d / 100, 1u)),
            max_10 = top_k(processed_data, num_pixels_1d, std::max(num_pixels_1d / 100, 1u));
        for (u32 i = 0; i < num_pixels_1d; i++) 
        {
            if (processed_data[i] < min_10) processed_data[i] = min_10;
            else if (processed_data[i] > max_10) processed_data[i] = max_10;
        }

        // cal the mean of every fragment
        for (u32 i = 0; i<frags->num; i++) // traverse all the fragments
        {
            compressed_extensions->data[extension_id][i] = mean_cal(processed_data + frags->startCoord[i], frags->length[i]);
        }

        delete[] processed_data;

        // check if the extension is valid, if yes, set the flag to true
        for (u32 i = 0; i < frags->num; i++)
        {
            if (compressed_extensions->data[extension_id][i] > 0 && max_10 != min_10)
            {
                compressed_extensions->exist_flags[extension_id] = true;
                break;
            }
        }

        // normalize the extension
        if (compressed_extensions->exist_flags[extension_id]== false) continue;
        for (u32 i = 0; i < frags->num; i++)
        {
            compressed_extensions->data[extension_id][i] = (compressed_extensions->data[extension_id][i] - min_10) / (max_10 - min_10);
        }

    }
    return ;
}

void TexturesArray4AI::output_compressed_hic_massCetre_extension_for_python() const
{
    if (!is_compressed || compressed_hic == nullptr || compressed_extensions == nullptr || mass_centres == nullptr || frags == nullptr)
    {
        std::cerr << "The compressed hic is not calculated yet" << std::endl;
        assert(0);
    }

    #ifdef DEBUG// used for debugging
        output_textures_to_file(); 
        output_compressed_hic_to_file();
        output_compressed_extension_to_file();
        output_mass_centres_to_file();
        output_frags4AI_to_file();
    #endif // DEBUG

    return ;
}


void TexturesArray4AI::output_frags4AI_to_file() const
{
    std::ofstream out_file("python/test_data_from_openglbuffer/test_output_frags4AI_to_file_" + std::string(name) + ".txt");
    if (!out_file)
    {
        std::cerr << "Cannot open the file" << std::endl;
        assert(0);
    }

    out_file << "Num_frags: " << frags->num << std::endl;
    out_file << "Total_length: " << frags->total_length << std::endl;

    out_file << "Start_coord: ";
    for (u32 i = 0; i < frags->num; i++)
    {
        out_file << frags->startCoord[i] << " ";
    }
    out_file << std::endl;

    out_file << "Length: ";
    for (u32 i = 0; i < frags->num; i++)
    {
        out_file << frags->startCoord[i] << " ";
    }
    out_file << std::endl;

    out_file << "Inversed: ";
    for (u32 i = 0; i < frags->num; i++)
    {
        out_file << (u32)(frags->inversed[i]) << " ";
    }
    out_file << std::endl;

    out_file.close();
    return ;
}