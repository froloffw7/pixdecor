#pragma once

#include <wayfire/option-wrapper.hpp>
#include <wayfire/toplevel.hpp>
#include <iomanip>

#define MIN_RESIZE_SIZE        3.0
// #define LARGE_ICON_THRESHOLD  20.0
#define MIN_BAR_HEIGHT        20

// DEBUG
// #define __DEBUG_RENDER__
// #define __DEBUG_SHADE__
// #define __DEBUG_SHADE_RENDER__

namespace wf::pixdecor
{
    
namespace ini
{
/**
 * All plugin option_wrapper_t instances — in one place.
 *
 * The best option to cambain all settings in deco-settings.hpp
 * should be use separate structure, like:
 *
 * struct pixdecor_settings_t
 * {
 *     // ---- geometry ----
 *     wf::option_wrapper_t<int> border_size{"pixdecor/border_size"};
 *     wf::option_wrapper_t<int> title_height{"pixdecor/title_height"};
 *     wf::option_wrapper_t<int> shadow_radius{"pixdecor/shadow_radius"};
 *     wf::option_wrapper_t<int> csd_titlebar_height{"pixdecor/csd_titlebar_height"};
 * ...
 *     wf::option_wrapper_t<double> box_shadow_scale{"pixdecor/box_shadow_scale"};        // output scale override
 * 
 *     // Access to the single instance.
 *     static pixdecor_settings_t& get()
 *     {
 *         static pixdecor_settings_t instance;   // C++11: thread-safe, lazy
 *         return instance;
 *     }
 * 
 * private:
 *     pixdecor_settings_t() = default;
 * };
 * 
 * // convenient shorthand for files:
 * inline pixdecor_settings_t& PIXS()
 * {
 *     return pixdecor_settings_t::get();
 * }
 * usage:
 * #include "deco-settings.hpp"
 * if (wf::pixdecor::PIXS().maximized_shadows) { ... }
 * auto engine = std::string(wf::pixdecor::PIXS().overlay_engine);
 *
 * Decoration files include deco-settings.hpp and use
 * these global names directly. Definitions (ownership) — deco-settings.cpp.
 *
 * RULE (initialization order): these objects are created when the .so is loaded.
 * Do NOT read them from constructors of other global objects —
 * the initialization order of translation units is undefined.
 * All reads in pixdecor happen at runtime (after plugin init) — keep it that way.
 */
// ---- window selection for decoration ----
//extern wf::option_wrapper_t<std::string> ignore_views_string;
//extern wf::option_wrapper_t<std::string> always_decorate_string;

// ---- window titlebar ----
extern wf::option_wrapper_t<int>         titlebar_height;    // px; 0 = auto by font
extern wf::option_wrapper_t<std::string> title_font;         // pango font description
extern wf::option_wrapper_t<std::string> title_text_align;   // 0/left, 1/center, 2/right
extern wf::option_wrapper_t<std::string> titlebar_options;   // always/never/windowed/maximized

// ---- window frames ----
extern wf::option_wrapper_t<int>         border_size;        // px
extern wf::option_wrapper_t<int>         input_size;         // frame zone width px

// ---- maximization ----
extern wf::option_wrapper_t<bool>        maximized_borders;
extern wf::option_wrapper_t<bool>        maximized_shadows;

// ---- colors ----
extern wf::option_wrapper_t<wf::color_t> color;                 // active frame
extern wf::option_wrapper_t<wf::color_t> background_color;      // inactive frame
extern wf::option_wrapper_t<wf::color_t> text_color;            // active text
extern wf::option_wrapper_t<wf::color_t> background_text_color; // inactive text
extern wf::option_wrapper_t<wf::color_t> line_color;            // fallback icon color
extern wf::option_wrapper_t<double>      line_thickness;        // line thickness (px)

// ---- buttons: layout ----
extern wf::option_wrapper_t<std::string> button_layout;         // "minimize:maximize,close"
extern wf::option_wrapper_t<int>         left_button_spacing;
extern wf::option_wrapper_t<int>         right_button_spacing;
extern wf::option_wrapper_t<int>         left_button_x_offset;
extern wf::option_wrapper_t<int>         right_button_x_offset;
extern wf::option_wrapper_t<int>         button_y_offset;
extern wf::option_wrapper_t<int>         button_hover_duration;

// ---- buttons: images (empty string = draw a cairo icon) ----
extern wf::option_wrapper_t<std::string> button_minimize_image;
extern wf::option_wrapper_t<std::string> button_maximize_image;
extern wf::option_wrapper_t<std::string> button_restore_image;
extern wf::option_wrapper_t<std::string> button_close_image;
extern wf::option_wrapper_t<std::string> button_minimize_hover_image;
extern wf::option_wrapper_t<std::string> button_maximize_hover_image;
extern wf::option_wrapper_t<std::string> button_restore_hover_image;
extern wf::option_wrapper_t<std::string> button_close_hover_image;

// ---- engines and effects ----
extern wf::option_wrapper_t<std::string> effect_type;         // none/smoke/ink/clouds/...
extern wf::option_wrapper_t<std::string> overlay_engine;      // none/rounded_corners/beveled_glass/box_shadow
extern wf::option_wrapper_t<std::string> theme_name;          // GTK theme name

extern wf::option_wrapper_t<bool>        effect_animate;      // "animate" option
extern wf::option_wrapper_t<bool>        effect_focused_only;
extern wf::option_wrapper_t<wf::color_t> effect_color;
extern wf::option_wrapper_t<int>         rounded_corner_radius;
extern wf::option_wrapper_t<int>         shadow_radius;
extern wf::option_wrapper_t<wf::color_t> shadow_color;
extern wf::option_wrapper_t<bool>        enable_shade;

// ---- misc ----
extern wf::option_wrapper_t<int>         csd_titlebar_height;

// ---- animation
//class wf::animation_description_t;
extern wf::option_wrapper_t<wf::animation_description_t> shade_animation;

}

extern const std::string shade_transformer_name;

// canonical: * 120.0; alternative - * 8.0 - 1/8;
static inline int key_from_scale(double scale)
{
    return (int)std::lround(scale * 120.0);
}

#if 0
static inline int px_round(float val)
{
    return (int)std::floor(val + 0.5f);
}
#endif
template<typename T>
static inline T px_round(float val)
{
    return static_cast<T>(std::floor(val + 0.5f));
}

enum class shade_action_t
{
    none            = 0,
    shade           = 1, // forward 0.0 -> 1.0
    unshade         = 2, // reverse 1.0 -> 0.0 
};

template <typename T, typename delta_t>
inline T shade_action_from_delta(delta_t delta)
{
    if      (delta < 0) return T::unshade;
    else if (delta > 0) return T::shade;
    return T::none;
}

namespace css { enum class align_t; }
using align_t = css::align_t;

static constexpr uint32_t TITLEBAR_WINDOWED_BIT    = (1U <<  0);
static constexpr uint32_t TITLEBAR_MAXIMIZED_BIT   = (2U <<  0);

enum class titlebar_opts_t
{
    never          = 0,
    windowed       = TITLEBAR_WINDOWED_BIT,
    maximized      = TITLEBAR_MAXIMIZED_BIT,
    always         = TITLEBAR_WINDOWED_BIT | TITLEBAR_MAXIMIZED_BIT,
};

static constexpr uint32_t WIN_STATE_ACTIVE_BIT     = (1U <<  0);
static constexpr uint32_t WIN_STATE_MAXIMIZED_BIT  = (1U <<  1);
static constexpr uint32_t WIN_STATE_MINIMIZED_BIT  = (1U << 16);
static constexpr uint32_t WIN_STATE_FULLSCREEN_BIT = (1U << 17);
static constexpr uint32_t WIN_STATE_EXCLUSIVE_MASK = (WIN_STATE_MAXIMIZED_BIT | WIN_STATE_MINIMIZED_BIT | WIN_STATE_FULLSCREEN_BIT);

enum class effect_t {
    none = 0,
    smoke,
    ink,
    clouds,
    halftone,
    pattern,
    lava,
    hex,
    zebra,
    neural_network,
    hexagon_maze,
    raymarched_truchet,
    neon_pattern,
    neon_rings,
    deco,
};

enum class overlay_t {
    none = 0,
    rounded_corners,
    beveled_glass,
    gtk_theme,
};

struct settings_t {
    //std::string        ignore_views_string;
    //std::string        always_decorate_string;

    int                titlebar_height;
    std::string        titlebar_font;
    align_t            titlebar_text_align;
    titlebar_opts_t    titlebar_options;
    double             border_size;
    double             input_size;
    bool               maximized_borders;
    bool               maximized_shadows;
    wf::color_t        color;
    wf::color_t        background_color;
    wf::color_t        text_color;
    wf::color_t        background_text_color;
    wf::color_t        line_color;
    double             line_thickness;
    int                left_button_spacing;
    int                right_button_spacing;
    int                left_button_x_offset;
    int                right_button_x_offset;
    int                button_y_offset;
    std::string        button_minimize_image;
    std::string        button_maximize_image;
    std::string        button_restore_image;
    std::string        button_close_image;
    std::string        button_minimize_hover_image;
    std::string        button_maximize_hover_image;
    std::string        button_restore_hover_image;
    std::string        button_close_hover_image;
    std::string        button_layout;
    int                button_animation_length;

    effect_t           effect_type;
    overlay_t          overlay_engine;
    std::string        theme_name;
    bool               enable_effect_animation;
    bool               effect_focused_only;
    wf::color_t        effect_color;
    int                blur_radius;
    int                corner_radius;
    wf::color_t        shadow_color;
    bool               enable_shade;
    int                csd_titlebar_height;
    //---- calculated settings ----
    double             shadow_size;
};

void build_settings_from_options(settings_t &);

void print_settings(const settings_t& s);

inline std::string fmt_bool(bool value, const std::string& header = "bool value")
{
    std::ostringstream os;
    os << header << '=' << (value ? "true" : "false");
    return os.str();
}

template<typename pt_t>
inline std::string fmt_point(const pt_t& point, const std::string& header = "point")
{
    std::ostringstream os;
    if constexpr (std::is_same_v<point_t, wf::pointf_t>)
        os << std::setprecision(6) << std::defaultfloat;
    os << header << '=' << point.x << '+' << point.y;
    return os.str();
}

template <typename dims_t>
inline std::string fmt_dims(const dims_t& dims, const std::string& header = "dims")
{
    std::ostringstream os;
    os << header << '=' << dims.width << ':' << dims.height;
    return os.str();
}

template<typename geom_t>
inline std::string fmt_geom(const geom_t& g, const std::string& header = "geom")
{
    std::ostringstream os;
    os << header << '=' << g.width << ':' << g.height << '+' << g.x << '+' << g.y;
    return os.str();
}

template <typename marg_t>
inline std::string fmt_margins(const marg_t& m, const std::string& header = "margins")
{
    std::ostringstream os;
    os << header << "=[" << m.left << '+' << m.right << '+' << m.bottom << '+' << m.top << ']';
    return os.str();
}

template<typename box_t>
inline std::string fmt_box(const box_t& b, const std::string& header = "box")
{
    std::ostringstream os;
    os << header << "=[" << b.x1 << '+' << b.y1 << '-' << b.x2 << '+' << b.y2 << ']';
       // nearby — rectangle in w:h+x+y
    os << fmt_geom(wf::geometry_t{b.x1, b.y1, b.x2 - b.x1, b.y2 - b.y1}, " rect");
    return os.str();
}

}
