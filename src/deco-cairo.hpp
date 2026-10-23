#pragma once

#include <wayfire/scene-render.hpp>
#include <wayfire/plugins/common/cairo-util.hpp>

#include "deco-settings.hpp"

namespace wf::pixdecor
{

class title_t
{
  public:
    /**
     * Create a new titlebar object
     * @param damage Callback to request a repaint when the texture is invalidated
     */
    title_t(std::function<void()> damage_cb)
        : damage_callback(std::move(damage_cb)) {}

    /**
     * Draw the titlebar into the target framebuffer
     *
     * @param data Wayfire Scene API render instruction
     * @param geometry Local titlebar geometry
     * @param title_text Current window title text
     * @param active Whether the window is focused
     * @param align_width Width for text alignment
     */
    void render(const wf::scene::render_instruction_t& data,
                const wf::geometry_t&                  geom,
                bool                                   window_active);

    void set_text(const std::string& text);

    void set_total_width(double width) { total_width = width; }

  private:
    wf::owned_texture_t texture;
    /**
     * Reset the texture cache (e.g., when the font or theme changes)
     */
    std::function<void()> damage_callback;

    // State cache to avoid redundant Cairo renders
    wf::geometry_t      geometry{0, 0, 0, 0};
    std::string         title_text;
    bool                active      = true;
    double              scale       = 0.0;
    double              total_width = 0.0;

    /**
     * Update the text OpenGL texture when parameters change
     */
    bool update_texture(const wf::geometry_t& geometry,
                        double                scale,
                        bool                  active);

    cairo_surface_t* render_text();
};

enum class button_type_t;
enum class button_state_t;

class button_t
{
  public:
    /**
     * Create a new button
     * @param damage_callback   A callback to execute when the button needs a
     * repaint. Damage won't be reported while render() is being called.
     */
    button_t(button_type_t btn_type, std::function<void()> damage_cb);

    /** @return The type of the button */
    button_type_t get_type() const { return type; }
    /**
     * Set the button hover state.
     * Affects appearance.
     */
    void set_hovered(bool hover);
    /**
     * Set whether the button is pressed or not.
     * Affects appearance.
     */
    void set_clicked(bool clicked);

    /**
     * Render the button on the given framebuffer at the given coordinates.
     * Precondition: set_butt<S-F7>on_type() has been called, otherwise result is no-op
     *
     * @param buffer The target framebuffer
     * @param geometry The geometry of the button, in logical coordinates
     * @param scissor The scissor rectangle to render.
     */
    void render(const wf::scene::render_instruction_t& data, wf::geometry_t geometry, bool active);
    // signature with baked-in geometry

  private:
    button_type_t type;

    // std::function<void(wf::geometry_t)> damage_callback;
    std::function<void()> damage_callback;

    /* Whether the button is currently being hovered */
    bool is_hovered = false;
    /* Whether the button is currently being held */
    bool is_clicked = false;
    /* The shade of button background to use. */
    wf::animation::simple_animation_t hover_animation{ ini::button_hover_duration,
        //wf::animation::smoothing::sigmoid };
        //wf::animation::smoothing::get_cubic_bezier(0.215, 0.61, 0.355, 1.0) };
        wf::animation::smoothing::get_cubic_bezier(0.25, 0.1, 0.25, 1.0) };
    wf::animation::simple_animation_t click_animation{ ini::button_hover_duration,
        //wf::animation::smoothing::sigmoid };
        // wf::animation::smoothing::get_cubic_bezier(0.215, 0.61, 0.355, 1.0) };
        wf::animation::smoothing::get_cubic_bezier(0.25, 0.1, 0.25, 1.0) };

    wf::wl_idle_call idle_damage;
    /** Damage button the next time the main loop goes idle */
    void add_idle_damage();
};


struct button_set_t
{
    wf::owned_texture_t normal;
    wf::owned_texture_t hovered;
    wf::owned_texture_t clicked;
};

struct button_pair_t
{
    button_set_t active;
    button_set_t inactive;
};

struct button_textures_t
{
    wf::dimensions_t size;
    button_pair_t    close;
    button_pair_t    maximize;
    button_pair_t    restore;
    button_pair_t    minimize;
    button_pair_t    menu;
};

class button_textures_manager_t
{
  private:
    // output scale -> texture set. Generated lazily, on actual render.
    inline static std::map<int, button_textures_t> cache_textures;
    // canonical: * 120.0; alternative - * 8.0 - 1/8;
    // static int key_from_scale(double scale) { return (int)std::lround(scale * 120.0); }
    // inline static wf::dimensions_t dims;

    static wf::owned_texture_t load_texture(button_type_t type,
        button_state_t state, bool window_active, double scale,
        const wf::dimensions_t& size);

    static button_pair_t load_pair(button_type_t type,
        double scale, wf::dimensions_t size);

    [[nodiscard]]
    static const button_textures_t& ensure(double scale);

  public:
    // theme_updated_signal → here. Theme change = all textures invalid.
    static void invalidate() noexcept { cache_textures.clear(); }
    // Pure public method: give me a pair of textures for a given button type
	// static const button_pair_t& get_pair(button_type_t type);
    static const button_pair_t& get_pair(button_type_t type, double scale);
   	// static const wf::dimensions_t& get_dimensions() { return dims; };
    static const auto& get_dimensions(double scale) { return ensure(scale).size; }

    static void load_textures(double scale) { (void)ensure(scale); }
};


class titlebar_t;

class font_manager_t
{
  public:
    static double get_font_height();

    [[nodiscard]]
    static const PangoFontDescription* get_font_description() noexcept
    {
        return font_description.get();
    }
    
    static void update()
    {
        if (theme_t::is_gtk_overlay())
            update_font(theme_t::titlebar);
        else
            update_font(theme_t::titlebar_font);
    }
    // static resync_t set_font_name(const std::string& v);
    static void reset()
    {
        font_description.reset();   // not GL, but keep symmetry with update()
    }
    
  private:
    inline static std::unique_ptr<PangoFontDescription,
        decltype(&pango_font_description_free)>
            font_description{nullptr, pango_font_description_free};

    static void update_font(const titlebar_t& titlebar);
    static void update_font(const std::string& font_str);
};

} // namespace wf::pixdecor

