#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include <optional>

// Structure: [Single Selector] -> [Property] -> [Value]

namespace css {

// Data types and functions for CSS parsing

int parse_numeric(const std::string& css_text);

enum class unit_t {
    none = 0,
    px,
    em,
    rem,
    ms,
};

    
// Strongly typed size (number + unit)
struct length_t {
    float value = 0.0;
    unit_t unit = unit_t::none;

    bool operator==(const length_t& other) const {
        // Since 'a' is a float, use a small epsilon for comparison accuracy
        return unit == other.unit && (std::abs(value - other.value) < 0.001f);
    }
};

struct unit {

    static std::optional<length_t> parse(const std::string& css_text);
    static const char * format(unit_t unit);
    static std::string format(const length_t&);

}; 

// Universal structure for margin and padding
struct edge_insets_t {
    length_t top;
    length_t right;
    length_t bottom;
    length_t left;

    bool operator==(const edge_insets_t& other) const {
        // Since 'a' is a float, use a small epsilon for comparison accuracy
        return (top == other.top) && (right == other.right) && (bottom == other.bottom) && (left == other.left);
    }
};

struct edge_insets {

    // Universal function: parses both "margin: 0 10px" and "padding: 4px"
    static std::optional<edge_insets_t> parse(const std::string& css_text);
    static std::string format(const edge_insets_t&);

};// edge_insets namespace

enum color_format_t {
    RGB,
    RGBA,
    RRGGBB,
    RRGGBBAA,
};

// Structure for storing RGB values
struct color_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    float   a;
};

struct color
{
    static std::optional<color_t> parse(std::string css_text);
    static std::string format(const color_t& color, color_format_t fmt = RGBA);

  private:
    static bool parse_named_color(const std::string& token, color_t& out_color);

}; // color struct

// Structure for storing the parsed border components
struct border_t {
    std::string width;
    std::string style;
    std::string color;
};

struct border
{
    // Main function for parsing a border string
    static border_t parse(const std::string& css_text);
    static std::string format(const border_t& border);

  private:
    static bool is_width(const std::string& token);
    static bool is_style(const std::string& token);

}; // border namespace



// Gradient direction
enum class gradient_direction_t {
    none,
    to_top,
    to_bottom,
    to_left,
    to_right,
};

// Gradient color stop
struct gradient_stop_t {
    double  offset; // 0.0 ... 1.0
    color_t color;
};

// Final result of background parsing
struct background_t {
    bool                         with_gradient = false;
    color_t                      color; // no default value
    gradient_direction_t         direction = gradient_direction_t::none;
    std::vector<gradient_stop_t> stops;
};

struct background
{
    // Main function for parsing a background string
    static std::optional<background_t> parse(std::string css_text);
    static std::string format(const background_t& bg);

  private:
    static bool parse(const std::string& input_str, gradient_direction_t& direction);
};

using data_t = std::map<std::string, std::map<std::string, std::string>>;

data_t parse_and_ungroup(const std::string& css_content);

// The function finds ALL url(...) inside the string and returns them as a list
std::vector<std::string> extract_asset_paths_from_css(const std::string& css_text);


}; // css namespace

namespace fs {

std::string extract_filename_with_ext(const std::string& asset_path);
    
bool file_exists(const std::string& path);
// Helper function for reading a regular file from disk into a string
std::string read_plain_file(const std::string& filepath);

bool ensure_directory_exists(const std::string& path);


}; // fs namespace


