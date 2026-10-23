#pragma once

#include <wayfire/opengl.hpp>
#include <GLES3/gl32.h>
#include <wayfire/scene-render.hpp>

namespace wf::pixdecor
{

class shader_t
{
    /** background effects */
    GLuint motion_program,
           diffuse1_program,
           diffuse2_program,
           project1_program,
           project2_program,
           project3_program,
           project4_program,
           project5_program,
           project6_program,
           advect1_program,
           advect2_program,
           render_program,
           render_overlay_program;

    GLuint texture,
           b0u, b0v, b0d, b1u, b1v, b1d,
           neural_network_texture;

    int    px_width;
    int    px_height;

    void create_programs();
    void destroy_programs();

    void create_textures();
    void destroy_textures();

    void dispatch_region(const wf::region_t& region);

    void render_region(GLuint program, const wf::region_t & region, const wf::dimensions_t & size);

    void recreate_textures();
    
    void upload_box_shadow_static(GLuint program); // TODO, const PluginSettings& s)

    void dump_box_shadow_shader(int Wp, int Hp, float scale);

  public:
    shader_t();
    ~shader_t();
    // GTK theme shader
    void update_gtk(const wf::scene::render_instruction_t& data,
                    const wf::geometry_t& rectangle, float scale,
                    bool activated, double title_height, bool render_decorations);
    // Other effects/overlays shaders
    void update_effects(const wf::scene::render_instruction_t& data,
                        const wf::geometry_t& rectangle, float scale,
                        bool activated, std::optional<wf::pointf_t> pt, 
                        int title_height, int border_size, int shadow_size);

    void render(const wf::scene::render_instruction_t& data,
                const wf::geometry_t& rectangle, float scale);

    void effect_changed();
};

}

