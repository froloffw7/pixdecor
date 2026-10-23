#pragma once

#include "deco-settings.hpp"
#include "deco-gtk-theme.hpp"

namespace wf::pixdecor
{

// deco-theme.hpp — the theme owns the fields and the damage radius
enum class resync_t
{
    none,               // value did not change
    repaint,            // damage is enough (shader reads on the fly)
    recreate,           // recreate frames (geometry)
    recreate_with_font, // recreate frames (geometry) and font
    invalidate_icons,   // recreate icons
    invalidate,         // regenerate button textures + frames (baked)
};

// The theme changed. ev->resync — what the consumer must do.
struct theme_updated_signal
{
    resync_t resync = resync_t::none;
};

/**
 * A  class which manages the outlook of decorations.
 * It is responsible for determining the background colors, sizes, etc.
 */
class pixdecor_theme_t
{
  private:
    inline static settings_t  settings;
    inline static gtk_theme_t gtk_theme;

  public:
    /** Create a new theme with the default parameters */
    //pixdecor_theme_t() : font_description(nullptr, pango_font_description_free) { }
    //~pixdecor_theme_t() {}

    //inline static const std::string&        ignore_views_string{ settings.ignore_views_string };
    //inline static const std::string&        always_decorate_string{ settings.always_decorate_string };

	inline static const effect_t&           effect_type{ settings.effect_type };
    inline static const overlay_t&          overlay_engine{ settings.overlay_engine };
	inline static const bool&               effect_animation_enabled{ settings.enable_effect_animation };
    inline static const bool&               effect_animate_focused_only{ settings.effect_focused_only };
    inline static const bool&               shade_enabled{ settings.enable_shade };
//    inline static const int&                shade_animation_length{ settings.rounded_corner_radius };

    inline static const std::string&        titlebar_font{ settings.titlebar_font };
    inline static const titlebar_opts_t&    titlebar_opts{ settings.titlebar_options };
    // inline static const double&             border_size{ settings.border_size };
    inline static const int&                blur_radius{ settings.blur_radius };
    inline static const int&                corner_radius{ settings.corner_radius };
    inline static const bool&               maximized_borders{ settings.maximized_borders };
    inline static const bool&               maximized_shadows{ settings.maximized_shadows };
    inline static const int&                button_animation_length{ settings.button_animation_length };

    // Decoration/GTK Theme parameters
    inline static const titlebar_t&         titlebar{ gtk_theme.titlebar };
    inline static const std::string&        button_layout{ gtk_theme.titlebar.button_layout };
    inline static const icons_t&            icons{ gtk_theme.icons };
    inline static const decoration_t&       decoration{ gtk_theme.decoration };
    inline static const decoration_t&       decoration_backdrop{ gtk_theme.decoration_backdrop };
    inline static const css::sides_t&       gtk_margins{ gtk_theme.margins };

    /** @return The available border for resizing */
    inline static const double&             input_size{ settings.input_size };
    inline static const int&                csd_titlebar_height{ settings.csd_titlebar_height };

    inline static const wf::color_t&        effect_color{ settings.effect_color };
    inline static const wf::color_t&        shadow_color{ settings.shadow_color };

    [[nodiscard]]
    static const settings_t& get_settings() noexcept { return settings; }
    [[nodiscard]]
    static const gtk_theme_t& get_gtk_theme() noexcept { return gtk_theme; }

    static bool is_titlebar_visible(bool is_maximized);
    /** @return The available height for displaying the title */
    static double get_titlebar_height(bool is_maximized);
    /** @return The available border for rendering */
    static double get_border_size(bool is_maximized);

    static double get_shadow_size(bool is_maximized);
	static wf::decoration_margins_t get_margins(bool is_maximized);

    /** @return The decoration color */
    static wf::color_t decoration_color(bool active);

    static void update(bool subscribe = false);

	static constexpr bool has_effect() { return effect_type != effect_t::none; }
	static constexpr bool is_ink_effect() { return effect_type == effect_t::ink; }

	static constexpr bool has_overlay() { return overlay_engine != overlay_t::none; }
	static constexpr bool is_gtk_overlay() { return overlay_engine == overlay_t::gtk_theme; }
	static constexpr bool is_rounded_corners_overlay() { return overlay_engine == overlay_t::rounded_corners; }

    // static resync_t set_font_name(const std::string& v);
    // static resync_t set_line_color(const wf::color_t& v);
    // static resync_t set_shadow_color(const wf::color_t& v);

    // Subscription point for consumers:
    //[[nodiscard]]
    //static wf::signal::provider_t& events() noexcept { return provider; }
    static void connect(wf::signal::connection_t<theme_updated_signal> *callback) {
        provider.connect(callback); }

  private:

    inline static wf::signal::provider_t provider;
    
    static void emit_updated(resync_t r)
    {
        theme_updated_signal ev;  // stack
        ev.resync = r;
        provider.emit(&ev);       // callbacks are invoked here; ev dies on exit
    }

	static void update_font();

    static void update(const settings_t& new_s, const gtk_theme_t& new_theme) noexcept
    {
        static_assert(std::is_nothrow_move_assignable_v<settings_t>);
        settings = std::move(new_s);
        static_assert(std::is_nothrow_move_assignable_v<gtk_theme_t>);
        gtk_theme = std::move(new_theme);
    }
    // ── Initialize the change tracking system ──
    // Call once from the plugin init() after options are loaded
    static void init_updates();
    // static settings_t build_settings_from_options();
    
    // ── Group change handlers ──
    static void on_titlebar_changed();
    static void on_colors_changed();
    static void on_geometry_changed();
    static void on_buttons_changed();
    static void on_icons_changed();
    static void on_effects_changed();
    static void on_theme_name_changed();
    // static void on_policy_changed();

    static void apply_icon_fallback(icon_t &icon);

};

using theme_t = pixdecor_theme_t;

}

