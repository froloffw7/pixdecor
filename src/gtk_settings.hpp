#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cmath>
#include <wayfire/config/types.hpp>

#define DEFAULT_BUTTON_LAYOUT "menu:minimize,maximize,close"

static inline const std::string check_empty_button_layout(const std::string& layout_str)
{
	return layout_str.empty() ? DEFAULT_BUTTON_LAYOUT : layout_str;
}

namespace wf::pixdecor::css {

// Function to strip whitespace and quotes
std::string trim(const std::string& str);

bool is_number(const std::string& text);
bool parse(const std::string& str, int &value);

enum class align_t
{
    left = 0,
    center,
    right
};

[[nodiscard]]
bool parse(const std::string& css_text, align_t&);

[[nodiscard]]
bool parse(const std::string& str, double &value);

struct vec4_t {
    union {
        // ALL groups — clockwise, slot by slot:
        // x,y,z,w = top,right,bottom,left = top_left,top_right,bottom_right,bottom_left
        double v[4] = {0.0, 0.0, 0.0, 0.0};
        struct { double x, y, z, w; };
        struct { double top, right, bottom, left; };
        struct { double top_left, top_right, bottom_right, bottom_left; };
    };
    // Default constructor (now the compiler will again allow creating empty objects)
    vec4_t() = default;
    // In C++, int is implicitly converted to double
    vec4_t(double _x, double _y, double _z, double _w) : x(_x), y(_y), z(_z), w(_w) {}
    vec4_t(double value) { v[0] = v[1] = v[2] = v[3] = value; }

    bool operator==(const vec4_t& other) const {
        const double ε = 0.00001;
        // Since 'x/y/z/w' is double, use a small epsilon for comparison accuracy
        return (std::abs(x - other.x) + std::abs(y - other.y) + std::abs(z - other.z) + std::abs(w - other.w)) < ε;
    }
    vec4_t operator+(const vec4_t& other) const {
        // Since 'x/y/z/w' is double, use a small epsilon for comparison accuracy
        return vec4_t{x + other.x, y + other.y, z + other.z, w + other.w};
    }
};

using sides_t   = vec4_t;   // distance fields: read as .top/.left
using corners_t = vec4_t;   // radius fields: read as .top_left/...

// Universal function: parses both "margin: 0 10px" and "padding: 4px"
[[nodiscard]]
bool parse(const std::string& css_text, vec4_t&);

#ifndef __SETTINGS_TEST__
// use wf::color_t in plugin
using color_t = wf::color_t;

#else
// use color_t definition in tests
struct color_t {
    /** Red channel value */
    double r = 0.0;
    /** Green channel value */
    double g = 0.0;
    /** Blue channel value */
    double b = 0.0;
    /** Alpha channel value */
    double a = 0.0;

    color_t() = default;
};

#endif

[[nodiscard]]
bool parse_named(const std::string& token, color_t& color);

[[nodiscard]]
bool parse(const std::string& str, color_t& color);

// Gradient direction
enum class gradient_direction_t {
    none,
    to_top,
    to_bottom,
    to_left,
    to_right,
};

// Gradient stop
struct gradient_stop_t {
    double  offset; // 0.0 ... 1.0
    color_t color;
};

// Final background parsing result
struct background_t {
    bool                         with_gradient = false;
    color_t                      color; // no default value
    gradient_direction_t         direction = gradient_direction_t::none;
    std::vector<gradient_stop_t> stops;
};

[[nodiscard]]
bool parse(std::string css_input, background_t& background);

// Final typed structure of a single shadow
struct shadow_t {
    bool    is_inset      = false;
    double  x_offset      = 0.0;
    double  y_offset      = 0.0;
    double  blur_radius   = 0.0;
    double  spread_radius = 0.0;
    color_t color;
};

class box_shadow {
  public:
    // Main method: parses the entire box-shadow: -> "..." string into typed structures
    // Returns std::nullopt if the string is invalid or empty
    // Parses as "0 0 0 1px..."
    [[nodiscard]]
    static std::optional<std::vector<shadow_t>> parse(const std::string& input);
    [[nodiscard]]
    static bool find_y_offset(const std::vector<shadow_t>& shadows, double& y_offset);

  private:
    // Intermediate string structure for internal parsing
    static std::vector<std::string>  split(const std::string& shadows_list);
    static std::optional<shadow_t>   parse_single_shadow(const std::string& shadow_str);
};

}; // css namespace
