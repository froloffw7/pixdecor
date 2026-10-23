#pragma once

#include "gtk_settings.hpp" // Include your header file

namespace wf::pixdecor {

struct border_t {
    css::sides_t   width;  // │
    css::corners_t radius; // ├ only :not(backdrop)
    std::string    style;  // │
    color_t        color;
};

struct titlebar_t {
    // [titlebar] & [titlebar:backdrop]
    std::string                font_family;
    std::string                font_size;
    std::string                font_weight;
    std::string                button_layout;

    double                     height = 0.0;
    double                     min_height = 0.0;
    css::sides_t               padding;

    color_t                    color;
    // color_t                    background_color;
    css::background_t          background;
    border_t                   border;

    color_t                    color_backdrop;
    // color_t                    background_color_backdrop;
    css::background_t          background_backdrop;
    border_t                   border_backdrop;

    css::align_t               text_align;
    css::sides_t               text_padding;

    std::vector<css::shadow_t> shadows; // Parsed titlebar shadows
};

struct decoration_t {
    border_t                   border;
    // [decoration] & [decoration:backdrop]
    std::vector<css::shadow_t> shadows; // Parsed window shadows
};
// ──────────────────────────────────────────────── //
//
// ──────────────────────────────────────────────── //
enum class button_type_t
{
    close = 0,
    minimize,
    maximize,
    restore,
    menu
};

enum class button_state_t
{
    normal = 0,
    hovered,
    clicked,
};

extern const char* format(button_type_t  type);
extern const char* format(button_state_t state);

// --------- Structure for 6 states of a single button -------------
struct icon_t
{
    std::string active_normal,   active_hovered,   active_clicked;
    std::string inactive_normal, inactive_hovered, inactive_clicked;

    /** Icon by window and button state. Data is table-driven, not branch-driven. */
    const std::string& path(bool window_active, button_state_t state) const
    {
        static constexpr std::string icon_t::* table[2][3] = {
            { &icon_t::active_normal,   &icon_t::active_hovered,   &icon_t::active_clicked   },
            { &icon_t::inactive_normal, &icon_t::inactive_hovered, &icon_t::inactive_clicked },
        };
        return this->*table[window_active?0:1][static_cast<std::size_t>(state)];
    }
};

struct icons_t
{
    double             min_width{ 24.0 };  // normalized at load time (24)
    double             image_size{ 16.0 }; // declared icon size (16px)
    // buttons spacing
    double             spacing{ 6.0 };
    css::sides_t       padding;

    double             line_thickness{ 1.0 };
    
    color_t color,            color_hovered,            color_clicked;             // fallback glyph
    color_t background_color, background_color_hovered, background_color_clicked;  // fallback background

    icon_t close, minimize, maximize, restore, menu;

    /** Icon set by button type. The table must follow the enum order! */
    const icon_t& icon(button_type_t type) const
    {
        static constexpr icon_t icons_t::* table[] = {
            &icons_t::close, &icons_t::minimize, &icons_t::maximize,
            &icons_t::restore, &icons_t::menu,
        };
        const auto idx = static_cast<std::size_t>(type);
        return this->*table[std::min(idx, std::size(table) - 1)];   // clamp: a new enum value won't crash it
    }

    /** Fallback colors by button state. */
    struct fallback_colors_t
    {
        const color_t& glyph;
        const color_t& bg;
    };

    fallback_colors_t fallback_colors(button_state_t state) const
    {
        switch (state)
        {
          case button_state_t::hovered:
              return { color_hovered, background_color_hovered };
          case button_state_t::clicked:
              return { color_clicked, background_color_clicked  };
          default:
              return { color,         background_color          };
        }
    }
};

// Full plugin settings structure for Wayfire
struct gtk_theme_t {
    // [main]
    std::string         theme_name;
    std::string         font_name;

    titlebar_t          titlebar;

    decoration_t        decoration;
    decoration_t        decoration_backdrop;

    // [icons]
    icons_t             icons;
    //---- calculated settings ----
    css::sides_t        margins;
};

struct margins_t { // Cairo margins
    int top, right, bottom, left;
    // Default constructor (so you can write: margins_t m;)
    margins_t() = default;
    // Template constructor for initialization from vec4_t / sides_t, etc.
    template <typename T>
    margins_t(const T& other) {
        top    = static_cast<int>(std::lround(other.top));
        right  = static_cast<int>(std::lround(other.right));
        bottom = static_cast<int>(std::lround(other.bottom));
        left   = static_cast<int>(std::lround(other.left));
    }
    // Assignment operator (kept for cases where the object already exists)
    template <typename T>
    margins_t& operator=(const T& other) {
        top    = static_cast<int>(std::lround(other.top));
        right  = static_cast<int>(std::lround(other.right));
        bottom = static_cast<int>(std::lround(other.bottom));
        left   = static_cast<int>(std::lround(other.left));
        return *this;
    }
};

// Fast INI file parsing function
bool load_gtk_theme(const std::string& filepath, gtk_theme_t& settings);

void print_width_info(const std::string& label, const css::sides_t& vec);

void print_radius_info(const std::string& label, const css::corners_t& vec);

void print_color_info(const std::string& label, const color_t& color);

// Helper to print shadow parse results to the console
void print_shadow_info(const std::string& label, const std::vector<css::shadow_t>& shadows);

void print_gtk_theme(const gtk_theme_t& s);

};
