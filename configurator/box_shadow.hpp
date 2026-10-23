#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <cmath>

#include "css_theme.hpp" // CSS Parser

namespace css {

// Final typed structure for a single shadow
struct shadow_t {
    bool     is_inset;
    length_t x_offset;
    length_t y_offset;
    length_t blur_radius;
    length_t spread_radius;
    color_t  color;
};

class box_shadow {
  public:
    // Main method: parses the entire "box-shadow: ..." string into typed structures
    // Returns std::nullopt if the string is invalid or empty
    // Parses both "box-shadow: 0 0 0 1px..." and plain "0 0 0 1px..."
    static std::optional<std::vector<shadow_t>> parse(const std::string& input);

    // Serialize a shadow list back into a CSS string (without the "box-shadow:" prefix)
    static std::string format(const std::vector<shadow_t>& shadows);
    
    // Assemble a single shadow into a string
    static std::string format(const shadow_t& shadow);

  private:
    // Intermediate string structure for internal parsing
    struct raw_shadow_t {
        bool        is_inset;
        std::string x_offset;
        std::string y_offset;
        std::string blur_radius;
        std::string spread_radius;
        std::string color;
    };

    static std::vector<std::string> split_shadows(const std::string& shadows_list);
    static std::optional<raw_shadow_t> parse_raw_shadow(const std::string& shadow_str);
    
    // Methods for converting strings into types
    static std::optional<shadow_t> convert(const raw_shadow_t& raw);
    
};

};
