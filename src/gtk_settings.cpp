#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <regex>

#include "gtk_settings.hpp"

namespace wf::pixdecor::css {

// --- Helper utility methods ---
// Trim whitespace at the edges; if the remainder is entirely wrapped in MATCHING quotes
// ("..." or '...') — remove the wrapper. Unpaired ones stay in place:
// a typo should be visible, not swallowed.
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last  = str.find_last_not_of(" \t\r\n");

    std::string out = str.substr(first, last - first + 1);

    if (out.length() >= 2) {
        const char q = out.front();
        if ((q == '"' || q == '\'') && out.back() == q)
            out = out.substr(1, out.length() - 2);   // one layer, not recursive
    }
    return out;
}

bool is_number(const std::string& text) {
    if (text.empty()) return false; // empty string — not a number
    
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return std::isdigit(c);
    });
}

bool parse(const std::string& str, int& value) {
    try {
        // std::stoi will turn "12pt" into 12 and "1px" into 1
        value = std::stoi(str);
        return true;
    } catch (...) {
        return false; // In case the string was empty or invalid
    }
}

bool parse(const std::string& css_text, align_t& align) {

    if      (css_text == "left")   { align = align_t::left;   return true; }
    else if (css_text == "center") { align = align_t::center; return true; }
    else if (css_text == "right")  { align = align_t::right;  return true; }

    return false;
}

#if 0
// OK: "12px", "9pt", "0px", "-0.0pt"          — any value + unit
// OK: "0", "-0", "+0", "0.0", ".0", "00"      — bare zero, unit not needed
// Error: "12", "0.5", "5."
bool parse(const std::string& tok, double& value) {
    // Two alternatives:
    //   1: number + required unit (px|pt) — any value
    //   2: number of only zeros, WITHOUT a unit     — zero only
    // TODO 
    // add to regex: |em|rem
    // add parameter: parse(tok, value, font_px, root_px)
    // if (m[2] == "em")  value *= font_px;
    // if (m[2] == "rem") value *= root_px;
    static const std::regex re(
        R"(^(?:([+-]?(?:\d+(?:\.\d+)?|\.\d+))(px|pt)|([+-]?(?:0+(?:\.0+)?|\.0+)))$)");
    std::smatch m;
    if (!std::regex_match(tok, m, re))
        return false;

    if (m[3].matched) {           // alternative 2: the grammar GUARANTEES zero
        value = 0.0;
        return true;              // stod is not even needed
    }

    try {
        value = std::stod(m[1]);  // alternative 1: number with a unit
    } catch (...) {
        return false;
    }

    if (m[2] == "pt") value *= 96.0 / 72.0;
    return true;
}
#else
// Simplified variant
// "12px", "0", "-3.5", "9pt" -> px, 0.2s -> 200ms
bool parse(const std::string& tok, double &value) {
    static const std::regex re(
        R"(^\s*([+-]?(?:\d+\.?\d*|\.\d+))\s*(px|pt|ms|s)?\s*$)");
    std::smatch m;
    if (!std::regex_match(tok, m, re))
        return false;                 // unknown garbage/unit
    try {
        value = std::stod(m[1]);
        if (!m[2].matched && (value != 0.0))
            return false;
    } catch (...) {
        // parse error
        return false;
    }

    if      (m[2] == "pt") value *= 96.0 / 72.0; // 1pt = 4/3 px
    else if (m[2] == "s")  value *= 1000.0;

    return true;
}
#endif

bool parse(const std::string& css_text, vec4_t& insets) {

    std::stringstream ss(css_text);
    std::string token;
    std::vector<double> sizes;

    while (ss >> token) {
        double size;
        if (!parse(token, size))
            return false;
        sizes.push_back(size);
    }
    if (sizes.empty() || (sizes.size() > 4))
        return false;
    // W3C expansion rules are the same for margin and padding!
    switch (sizes.size()) {
        case 1: // margin: 10px; -> all sides 10px
            insets.x = insets.y = insets.z = insets.w = sizes[0];
            break;
            
        case 2: // opposite pairs, margin: 0 10px; -> top/bottom=0, right/left=10px
            insets.x = insets.z = sizes[0];
            insets.y = insets.w = sizes[1];
            break;
            
        case 3: // margin: 10px 5px 20px; -> top=10, right/left=5, bottom=20
            // left / bottom_left) mirrors (right / top_right)
            insets.x = sizes[0];
            insets.y = insets.w = sizes[1];
            insets.z = sizes[2];
            break;
            
        case 4: // margin: 1px 2px 3px 4px; -> top, right, bottom, left
            insets.x = sizes[0];
            insets.y = sizes[1];
            insets.z = sizes[2];
            insets.w = sizes[3];
            break;
    }
    return true;
}

bool parse_named(const std::string& token, color_t& color) {
    // Static hash table with standard CSS / GTK colors
    // values rounded to 2 digits (e.g. 128/255 = 0.501960784 ~ 0.5)
    static const std::unordered_map<std::string, color_t> named_colors = {
        {"transparent", {0,    0,    0,    0.0}},
        {"black",       {0,    0,    0,    1.0}},
        {"white",       {1.0,  1.0,  1.0,  1.0}},
        {"red",         {1.0,  0,    0,    1.0}},
        {"lime",        {0,    1.0,  0,    1.0}},
        {"blue",        {0,    0  ,  1.0,  1.0}},
        {"yellow",      {1.0,  1.0,  0,    1.0}},
        {"cyan",        {0,    1.0,  1.0,  1.0}},
        {"magenta",     {1.0,  0,    1.0,  1.0}},
        {"aqua",        {0,    1.0,  1.0,  1.0}},
        {"fuchsia",     {1.0,  0,    1.0,  1.0}},
        {"silver",      {0.75, 0.75, 0.75, 1.0}},
        {"gray",        {0.5,  0.5,  0.5,  1.0}},
        {"maroon",      {0.5,  0,    0,    1.0}},
        {"olive",       {0.5,  0.5,  0,    1.0}},
        {"green",       {0,    0.5,  0,    1.0}},
        {"purple",      {0.5,  0,    0.5,  1.0}},
        {"teal",        {0,    0.5,  0.5,  1.0}},
        {"navy",        {0,    0,    0.5,  1.0}},
        {"orange",      {1.0,  0.65, 0,    1.0}},
        {"grey",        {0.5,  0.5,  0.5,  1.0}}, // In case of British spelling
    };

    auto it = named_colors.find(token);
    if (it != named_colors.end()) {
        color = it->second;
        return true;
    }
    return false;
}

/// Final (?) version
bool parse(const std::string& str, color_t& color) {
    std::string cleaned = trim(str);
    std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);

    if (cleaned.empty()) return false;

    if (cleaned == "initial" || cleaned == "none") {
        color = color_t{}; // Default transparent
        return true;
    }

    // 1. Parse rgb() and rgba()
    // in parse(color): allow spaces, and — static! (so the regex is not built on every call)
    static const std::regex rgba_regex(
        R"(rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*(\d+(?:\.\d+)?|\.\d+)\s*)?\))");
    std::smatch match;

    if (std::regex_match(cleaned, match, rgba_regex)) {
        try {
            color_t c; // So that color remains untouched if an exception is thrown
            // Extract channels by capture group indices (1, 2, 3)
            c.r = std::clamp(std::stoi(match[1].str()) / 255., 0.0, 1.0);
            c.g = std::clamp(std::stoi(match[2].str()) / 255., 0.0, 1.0);
            c.b = std::clamp(std::stoi(match[3].str()) / 255., 0.0, 1.0);
            if (match[4].matched && !match[4].str().empty()) {
                c.a = std::clamp(std::stod(match.str(4)), 0.0, 1.0);
            } else {
                c.a = 1.0;
            }
            color = c;
            return true;
        } catch (...) {}
    }
    // 2. Parse HEX (#ff...)
    else if (cleaned[0] == '#') {
        cleaned.erase(0, 1);
        // strictness: stoul does not check the tail, "#12345z" must not be a color
        if (cleaned.find_first_not_of("0123456789abcdef") != std::string::npos)
            return false;
        try {
            uint8_t red, green, blue, alpha;
            // Format #RRGGBBAA (8 digits)
            if (cleaned.length() == 8) {
                uint32_t rgba_value = std::stoul(cleaned, nullptr, 16);
                red   = (rgba_value >> 24) & 0xFF;
                green = (rgba_value >> 16) & 0xFF;
                blue  = (rgba_value >> 8) & 0xFF;
                alpha = rgba_value & 0xFF;
            }
            // Format #RRGGBB (6 digits)
            else if (cleaned.length() == 6) {
                uint32_t rgb_value = std::stoul(cleaned, nullptr, 16);
                red   = (rgb_value >> 16) & 0xFF;
                green = (rgb_value >> 8) & 0xFF;
                blue  = rgb_value & 0xFF;
                alpha = 255;
            }
            // Format #RGBA (4 digits, e.g. #f008)
            else if (cleaned.length() == 4) {
                uint32_t rgba_value = std::stoul(cleaned, nullptr, 16);
                red   = ((rgba_value >> 12) & 0xF) * 17;
                green = ((rgba_value >> 8) & 0xF) * 17;
                blue  = ((rgba_value >> 4) & 0xF) * 17;
                alpha = (rgba_value & 0xF) * 17;
            }
            // Format #RGB (3 digits)
            else if (cleaned.length() == 3) {
                uint32_t rgb_value = std::stoul(cleaned, nullptr, 16);
                red   = ((rgb_value >> 8) & 0xF) * 17;
                green = ((rgb_value >> 4) & 0xF) * 17;
                blue  = (rgb_value & 0xF) * 17;
                alpha = 255;
            } else {
                return false;
            }
            color.r = red   / 255.;
            color.g = green / 255.;
            color.b = blue  / 255.;
            color.a = alpha / 255.;
            return true;
        } catch (...) {
        }
    } else {
        // 3. Handle named colors
        return parse_named(cleaned, color);
    }
    return false;
}


// Parse background
// Function to split a selector string by comma
static std::vector<std::string> split(const std::string& tokens_str) {
    std::vector<std::string> result;
    std::stringstream ss(tokens_str);
    std::string item;

    while (std::getline(ss, item, ',')) {
        std::string cleaned = trim(item);
        if (!cleaned.empty()) {
            result.push_back(cleaned);
        }
    }
    return result;
}


// Determine the gradient direction
static bool parse(const std::string& input_str, gradient_direction_t& direction) {
    const std::string dir_str = trim(input_str);

    // If the string is empty, set the CSS default and exit successfully
    if (dir_str.empty()) {
        direction = gradient_direction_t::to_bottom;
        return true;
    }
    // Static map of string-to-enum correspondences
    static const std::unordered_map<std::string_view, gradient_direction_t> direction_map = {
        {"to top",    gradient_direction_t::to_top},
        {"to bottom", gradient_direction_t::to_bottom},
        {"to left",   gradient_direction_t::to_left},
        {"to right",  gradient_direction_t::to_right}
    };

    // Look up the token in the map
    auto it = direction_map.find(dir_str);
    if (it != direction_map.end()) {
        direction = it->second;
        return true;
    }
    // If the token is not found in the map
    return false;
}

// Main background parsing function
bool parse(std::string css_input, background_t& background) {

    background_t b;

    std::string input_str = trim(css_input);

    // Find the start of the gradient declaration
    std::regex gradient_regex(R"(linear-gradient\s*\((.*)\))");
    std::smatch match;

    if (std::regex_search(input_str, match, gradient_regex)) {
        std::string inner_content = match.str(1);

        // Parse the fallback color written BEFORE the gradient
        std::string prefix = std::string(trim(match.prefix().str()));
        if (!prefix.empty()) {
            if (!parse(prefix, b.color))
                return false;
        } else {
            b.color = {0.0, 0.0, 0.0, 0.0}; // Default transparent
        }
        // Split the gradient parameters using your split_selectors function
        std::vector<std::string> tokens = split(inner_content);

        if (tokens.size() >= 2) {
        	gradient_stop_t stop;
            size_t color_index = 0;
            // Try to parse the direction via parse_direction
            b.direction = gradient_direction_t::to_bottom; // CSS default
            if (!parse(tokens[0], b.direction)) {
                // If the color does not decode - error!
                if (!parse(tokens[0], stop.color)) {
                    return false;
                }
                stop.offset = 0.0;
                b.stops.push_back(stop);
                color_index = 1;
            }
            // Populate the stops array
            // The first token is already processed
            size_t total_colors = tokens.size() - 1 + color_index;
            // std::cout << total_colors << "," << color_index << "\n"; 
            for (size_t i = 1; i < tokens.size(); i++) {
                if (parse(tokens[i], stop.color)) {
                    if (color_index > 0) {
                        stop.offset = static_cast<double>(color_index) / static_cast<double>(total_colors - 1);
                    } else {
                        stop.offset = 0.0;
                    }
                    b.stops.push_back(stop);
                    color_index++;
                } else {
                    return false; // If one of the colors inside the gradient is broken
                }
            }
        } else {
            return false; // Invalid number of gradient parameters
        }
        background = b;
	    background.with_gradient = true;
        return true;

    } else {
        // If there is no gradient — it is a regular solid color
        if (parse(input_str, background.color)) {
	        background.with_gradient = false;
	        background.direction = gradient_direction_t::none;
			return true;
        }
    }
    return false;
}


static
bool parse(const std::string& shadow_params, shadow_t& params) {

    std::stringstream ss(shadow_params);
    std::string token;
    std::vector<double> sizes;

    while (ss >> token) {
        double size;
        if (!parse(token, size))
            return false;
        sizes.push_back(size);
    }
    if ((sizes.size() < 2) || (sizes.size() > 4)) // box-shadow: 1px; - forbidden
        return false;

    switch (sizes.size()) {
        case 2: // box-shadow: 1px 2px; -> x_offset, y_offset
            params.x_offset      = sizes[0];
            params.y_offset      = sizes[1];
            params.blur_radius   = 0.0;
            params.spread_radius = 0.0;
            break;
            
        case 3: // box-shadow: 1px 2px 3px; -> x_offset, y_offset, blur_radius
            params.x_offset      = sizes[0];
            params.y_offset      = sizes[1];
            params.blur_radius   = sizes[2];
            params.spread_radius = 0.0;
            break;
            
        case 4: // box-shadow: 1px 2px 3px 4px; -> x_offset, y_offset, blur_radius, spread_radius
            params.x_offset      = sizes[0];
            params.y_offset      = sizes[1];
            params.blur_radius   = sizes[2];
            params.spread_radius = sizes[3];
            break;
    }

    return true;
}

// Correctly splits multiple shadows by commas, ignoring commas inside rgba(...) / rgb(...)
std::vector<std::string> box_shadow::split(const std::string& shadows) {
    std::vector<std::string> result;
    std::string current;
    int paren_depth = 0;

    for (char ch : shadows) {
        if (ch == '(') paren_depth++;
        else if (ch == ')') paren_depth--;

        if (ch == ',' && paren_depth == 0) {
            std::string trimmed = trim(current);
            if (!trimmed.empty()) result.push_back(trimmed);
            current.clear();
        } else {
            current += ch;
        }
    }
    std::string trimmed = trim(current);
    if (!trimmed.empty()) result.push_back(trimmed);

    return result;
}

// --- Lexical parsing of a single shadow ---
std::optional<shadow_t> box_shadow::parse_single_shadow(const std::string& shadow_str) {
    std::string working_str = trim(shadow_str);
    if (working_str.empty()) return std::nullopt;

    bool        is_inset = false;
    std::string parameters;
    std::string color_str;

    // 1. Extract inset if present (it may be at the beginning or end)
    static const std::regex inset_regex(R"(\binset\b)", std::regex_constants::icase);
    if (std::regex_search(working_str, inset_regex)) {
        is_inset = true;
        // trim() is required, regex_replace() leaves a trailing space
        working_str = trim(std::regex_replace(working_str, inset_regex, ""));
    }
    if (working_str.empty()) return std::nullopt;       // consisted of a single "inset"

    // 2. Extract the color if present (supports rgba, rgb, hex, named)
    // The regex catches rgb/rgba functions with parentheses OR hex colors (#fff, #1a2b3c)
    // Does not search for named colors 
    // std::regex color_regex(R"((rgba?\(.+?\)|#[0-9a-fA-F]{3,8}))", std::regex_constants::icase);
    static const std::regex color_regex(R"((rgba?\([^)]*\)|#[0-9a-fA-F]{3,8}))",
                                        std::regex_constants::icase);
    std::smatch match;

    if (std::regex_search(working_str, match, color_regex)) { // function/hex anywhere,
        color_str = match.str(1);                             // including with spaces inside
        // format_first_only guards against errors like (missing comma between shadows):
        // "0 0 #fff #000" / "0 0 #fff 0 0 #000"
        parameters = std::regex_replace(working_str, color_regex, "",
                                        std::regex_constants::format_first_only);
    } else {
        size_t pos = working_str.rfind(' ');                  // named — at the end?
        if (pos != std::string::npos) {
            color_str = working_str.substr(pos + 1);
            parameters = working_str.substr(0, pos);
        } else {
            // Separator not found, error?
            return std::nullopt; 
        }
    }
    if (parameters.empty()) return std::nullopt;

    shadow_t shadow{.is_inset = is_inset};
    if (css::parse(color_str, shadow.color) &&
        css::parse(parameters, shadow)) {
        return shadow;
	}
    return std::nullopt; 
}

// --- Public interface methods ---

std::optional<std::vector<shadow_t>> box_shadow::parse(const std::string& input) {
    std::string clean_input = trim(input);
    if (clean_input.empty()) return std::nullopt;

    std::vector<shadow_t> shadows{};
    if (clean_input == "none" || clean_input == "initial")
        return shadows;   // success: zero shadows

    std::vector<std::string> raw_list = split(clean_input);

    for (const auto& raw_str : raw_list) {
        auto shadow = parse_single_shadow(raw_str);
        if (!shadow) return std::nullopt;

        shadows.push_back(*shadow);
    }

    if (shadows.empty()) return std::nullopt;
    return shadows;
}

bool box_shadow::find_y_offset(const std::vector<shadow_t>& shadows, double& y_offset) {
    for (const auto& s : shadows) {
        if (s.is_inset) continue;       // Ignore inner shadows
        if (s.color.a <= 0.0) continue; // Skip transparent ones
        
        // Skip border-like shadows (when it is a border, not a shadow)
        if (s.blur_radius == 0.0 && s.spread_radius > 0.0 && s.x_offset == 0.0 && s.y_offset == 0.0) {
            continue;
        }        // Skip border-like shadows (when it is a border, not a shadow)

        // Found the first valid shadow!
        y_offset = s.y_offset;
        return true;
    }
    return false; // If there are no valid shadows
}

};
