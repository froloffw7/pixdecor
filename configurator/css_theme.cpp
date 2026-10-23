// #include <iostream>
#include <cstdint>
#include <string>
#include <format>
#include <vector>
#include <map>
#include <unordered_map>
#include <filesystem>
//#include <sstream>
#include <algorithm>
#include <regex>

#include <gio/gio.h>
#include <sys/stat.h>

#include <css_helpers.hpp>
#include "css_theme.hpp" // CSS Parser
#include "cssparser/css_parser.h" // CSS Parser

// Structure: [Single Selector] -> [Property] -> [Value]

namespace css {

// The formatting function lives in the same namespace
const char * unit::format(unit_t unit) {
    switch (unit) {
        //using enum unit_t; // C++20 feature to avoid writing unit_t:: everywhere
        case unit_t::px:  return "px";
        case unit_t::em:  return "em";
        case unit_t::rem: return "rem";
        case unit_t::ms:  return "ms";
        case unit_t::none:
        default:   return "";
    }
}

std::optional<length_t> unit::parse(const std::string& css_text) {
    std::string cleaned = css_text;
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace), cleaned.end());
    std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);

    if (cleaned == "0") {
        return length_t{0.0f, unit_t::none};
    }

    try {
        size_t idx = 0;
        float val = std::stof(cleaned, &idx);
        std::string unit_text = cleaned.substr(idx);

        unit_t unit{unit_t::px};
        if      (unit_text == "px")  unit = unit_t::px;
        else if (unit_text == "em")  unit = unit_t::em;
        else if (unit_text == "rem") unit = unit_t::rem;
        else if (unit_text == "ms")  unit = unit_t::ms;
        else if (unit_text.empty())  unit = unit_t::none;
        else return std::nullopt;

        return length_t{val, unit};
    } catch (...) {
        return std::nullopt;
    }
}

std::string unit::format(const length_t& len) {

	if (std::abs(len.value) < 0.001f) return "0";

    //std::ostringstream oss;
    // Strip redundant trailing zeros for cleaner CSS (for example, 4.5 instead of 4.500000)
    //oss << len.value;
    //return oss.str() + unit::format(len.unit);
    return cxx_format("{:g}", len.value) + format(len.unit);
}

std::optional<edge_insets_t> edge_insets::parse(const std::string& css_text) {
    edge_insets_t insets{};

    std::stringstream ss(css_text);
    std::string token;
    std::vector<length_t> lengths;

    while (ss >> token) {
        auto len = unit::parse(token);
        if (len) {
            lengths.push_back(*len);
        } else {
            return std::nullopt; // Syntax error — return zeros
        }
    }

    // W3C expansion rules are the same for margin and padding.
    switch (lengths.size()) {
        case 1: // margin: 10px; -> all sides 10px
            insets.top = insets.right = insets.bottom = insets.left = lengths[0];
            break;
            
        case 2: // margin: 0 10px; -> top/bottom=0, right/left=10px
            insets.top   = insets.bottom = lengths[0];
            insets.right = insets.left   = lengths[1];
            break;
            
        case 3: // margin: 10px 5px 20px; -> top=10, right/left=5, bottom=20
            insets.top    = lengths[0];
            insets.right  = insets.left = lengths[1];
            insets.bottom = lengths[2];
            break;
            
        case 4: // margin: 1px 2px 3px 4px; -> top, right, bottom, left
            insets.top    = lengths[0];
            insets.right  = lengths[1];
            insets.bottom = lengths[2];
            insets.left   = lengths[3];
            break;
            
        default:
        	return std::nullopt;
    }

    return insets;
}

std::string edge_insets::format(const edge_insets_t& insets)
{
    length_t zero{.value = 0.0f, .unit = unit_t::none};
    if (insets == edge_insets_t{zero, zero, zero, zero}) return "0";

    if ((insets.top == insets.right) && (insets.top == insets.bottom) && (insets.top == insets.left))
    	return unit::format(insets.top);

    if (insets.right == insets.left) {
        if (insets.top == insets.bottom)
		    return unit::format(insets.top) + ' '
		         + unit::format(insets.right);
		else
		    return unit::format(insets.top) + ' '
		         + unit::format(insets.right) + ' '
		         + unit::format(insets.bottom);
	}
    return unit::format(insets.top) + ' '
         + unit::format(insets.right) + ' '
         + unit::format(insets.bottom) + ' '
         + unit::format(insets.left);
}

// Returns true for a named color and writes its value directly into out_color
bool color::parse_named_color(const std::string& token, color_t& out_color) {
    // Static lookup table of standard CSS / GTK colors
    static const std::unordered_map<std::string, color_t> named_colors = {
        {"transparent", {0,   0,   0,   0.0f}},
        {"black",       {0,   0,   0,   1.0f}},
        {"white",       {255, 255, 255, 1.0f}},
        {"red",         {255, 0,   0,   1.0f}},
        {"lime",        {0,   255, 0,   1.0f}},
        {"blue",        {0,   0, 255,   1.0f}},
        {"yellow",      {255, 255, 0,   1.0f}},
        {"cyan",        {0,   255, 255, 1.0f}},
        {"magenta",     {255, 0,   255, 1.0f}},
        {"silver",      {192, 192, 192, 1.0f}},
        {"gray",        {128, 128, 128, 1.0f}},
        {"maroon",      {128, 0,   0,   1.0f}},
        {"olive",       {128, 128, 0,   1.0f}},
        {"green",       {0,   128, 0,   1.0f}},
        {"purple",      {128, 0,   128, 1.0f}},
        {"teal",        {0,   128, 128, 1.0f}},
        {"navy",        {0,   0,   128, 1.0f}},
        {"orange",      {255, 165, 0,   1.0f}},
        {"grey",        {128, 128, 128, 1.0f}}, // For the British spelling
        {"initial",     {0,   0,   0,   1.0f}},
    };

    auto it = named_colors.find(token);
    if (it != named_colors.end()) {
        out_color = it->second;
        return true;
    }
    return false;
}

std::optional<color_t> color::parse(std::string color_str) {
    color_t color{0, 0, 0, 0.0f}; // default to transparent

    // 1a. Strip whitespace
    color_str.erase(std::remove_if(color_str.begin(), color_str.end(), ::isspace), color_str.end());
    if (color_str.empty()) {
        return std::nullopt;
    }
    // 1b. Convert to lowercase
    std::transform(color_str.begin(), color_str.end(), color_str.begin(), ::tolower);

    // 3. Handle HEX
    if (color_str.front() == '#') {
        color_str.erase(0, 1); // Remove '#'

        try {
            // #RRGGBBAA format (8 digits)
            if (color_str.length() == 8) {
                uint32_t rgba_value = std::stoul(color_str, nullptr, 16);
                color.r = (rgba_value >> 24) & 0xFF;
                color.g = (rgba_value >> 16) & 0xFF;
                color.b = (rgba_value >> 8) & 0xFF;
                color.a = (rgba_value & 0xFF) / 255.f;
            }
            // #RRGGBB format (6 digits)
            else if (color_str.length() == 6) {
                uint32_t rgb_value = std::stoul(color_str, nullptr, 16);
                color.r = (rgb_value >> 16) & 0xFF;
                color.g = (rgb_value >> 8) & 0xFF;
                color.b = rgb_value & 0xFF;
                color.a = 1.0f;
            }
            // #RGBA format (4 digits, for example #f008)
            else if (color_str.length() == 4) {
                uint32_t rgba_value = std::stoul(color_str, nullptr, 16);
                color.r = ((rgba_value >> 12) & 0xF) * 17;
                color.g = ((rgba_value >> 8) & 0xF) * 17;
                color.b = ((rgba_value >> 4) & 0xF) * 17;
                color.a = ((rgba_value & 0xF) * 17) / 255.f;
            }
            // #RGB format (3 digits)
            else if (color_str.length() == 3) {
                uint32_t rgb_value = std::stoul(color_str, nullptr, 16);
                color.r = ((rgb_value >> 8) & 0xF) * 17;
                color.g = ((rgb_value >> 4) & 0xF) * 17;
                color.b = (rgb_value & 0xF) * 17;
                color.a = 1.0f;
            }
        } catch (...) {
            // On a parse error, return black as the default
            return std::nullopt;
        }
    } else if (color_str.rfind("rgb", 0) == 0) {
        // 2. HANDLE RGB / RGBA
        // Universal expression:
        // The regex looks for 3 required numbers, then an optional separator (comma, space or slash)
        // and alpha (fraction or percentage)
        // 1st pattern: old comma-separated syntax (supports both rgb(r,g,b) and rgba(r,g,b,a))
        std::regex legacy_regex(R"(rgba?\((\d+),(\d+),(\d+)(?:,([\d\.]+))?\))");
        // ***gtkdecor*** std::regex rgba_regex(R"(rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+))?\s*\))");
        // 2nd pattern: new CSS syntax with spaces and a slash / (for example, rgb(r g b / a))
        std::regex modern_regex(R"(rgba?\((\d+)\s+(\d+)\s+(\d+)(?:\s*/\s*([\d\.]+%?))?\))");

        std::smatch match;
        bool matched = false;

        if (std::regex_match(color_str, match, legacy_regex)) {
            matched = true;
        } else if (std::regex_match(color_str, match, modern_regex)) {
            matched = true;
        }

        if (matched) {
            try {
                // Extract the channels by capture group indices (1, 2, 3)
                color.r = static_cast<uint8_t>(std::stoi(match[1].str()));
                color.g = static_cast<uint8_t>(std::stoi(match[2].str()));
                color.b = static_cast<uint8_t>(std::stoi(match[3].str()));

                // Check the 4th group (alpha channel)
                if (match[4].matched && !match[4].str().empty()) {
                    std::string alpha_str = match[4].str();

                    if (alpha_str.back() == '%') { // Percentage format (for example, 50%)
                        alpha_str.pop_back();
                        float percent = std::stof(alpha_str);
                        color.a = percent / 100.0f;
                    } else { // Fraction format (for example, 0.5) or integer (for example, 128)
                        float alpha_val = std::stof(alpha_str);

                        // Important: if alpha is passed to rgba() as an integer > 1 (for example, 128),
                        // that is the old/non-standard format. In standard CSS alpha always ranges from 0.0 to 1.0.
                        // We check for both cases:
                        if (alpha_val > 1.0f && alpha_str.find('.') == std::string::npos) {
                            // If this is an integer greater than 1 (for example, 128), take it as is
                            color.a = std::max(0.0f, std::min(255.0f, alpha_val)) / 255.f;
                        } else {
                            // Otherwise treat it as a fraction from 0.0 to 1.0
                            color.a = std::max(0.0f, std::min(1.0f, alpha_val));
                        }
                    }
                } else {
                    color.a = 1.0f; // If there was no alpha, the color is opaque
                }
            } catch (...) {
                // On a parse error, return black as the default
                return std::nullopt;
;
            }
        }
    } else {
        // 2. Handle named colors
        if (!parse_named_color(color_str, color))
            return std::nullopt;

    }

    return color;
}

std::string color::format(const color_t& color, color_format_t fmt) {
    std::string color_str;

    if (fmt == RGB) {
        return cxx_format("rgb({},{},{})",
               static_cast<int>(color.r),
               static_cast<int>(color.g),
               static_cast<int>(color.b));
    } else if (fmt == RGBA) {
        return cxx_format("rgba({},{},{},{:#.2g})",
               static_cast<int>(color.r),
               static_cast<int>(color.g),
               static_cast<int>(color.b),
                                color.a);
    } else if (fmt == RRGGBB) {
        return cxx_format("#{:02x}{:02x}{:02x}",
               color.r,
               color.g,
               color.b);
    } else if (fmt == RRGGBBAA) {
        return cxx_format("#{:02x}{:02x}{:02x}{:02x}",
               color.r,
               color.g,
               color.b,
               std::lround(color.a * 255.f));
    }
    return std::string("");
}

// Helper function to check whether a token is a border width
bool border::is_width(const std::string& token) {
    if (token.empty()) return false;
    // If it starts with a digit, it is a width (1px, 2em, 0.5px, etc.)
    return std::isdigit(static_cast<unsigned char>(token[0])) || token[0] == '.';
}

// Helper function to check whether a token is a border style
bool border::is_style(const std::string& token) {
    const std::vector<std::string> styles = {
        "none", "hidden", "solid", "dashed", "dotted",
        "double", "groove", "ridge", "inset", "outset"
    };
    return std::find(styles.begin(), styles.end(), token) != styles.end();
}

// Main function for parsing a border string
border_t border::parse(const std::string& border_value) {
    border_t border{
    .width{"0px"},
    .style{"none"},
    .color{"transparent"}};

    if (border_value.empty() || border_value == "none") {
        return border;
    }

    std::stringstream ss(border_value);
    std::string token;

    // Split the string into whitespace-separated tokens
    while (ss >> token) {
        // Convert to lowercase for reliable comparison
        std::string lower_token = token;
        std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        if (is_width(lower_token)) {
            border.width = token; // keep the original case (just in case)
        }
        else if (is_style(lower_token)) {
            border.style = token;
        }
        else {
            // If it is neither a width nor a style, then it is a color (#808080, rgb(...), transparent)
            border.color = token;
        }
    }

    return border;
}


// Background parsing
// Function that splits a selector string on commas
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
bool background::parse(const std::string& input_str, gradient_direction_t& direction) {
    const std::string dir_str = trim(input_str);

    // If the string is empty, set the CSS default value and exit successfully
    if (dir_str.empty()) {
        direction = gradient_direction_t::to_bottom;
        return true;
    }
    // Static map from strings to enum values
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

// Main function for parsing background
std::optional<background_t> background::parse(std::string css_input)
{
    background_t b;

    std::string input_str = trim(css_input);

    // Look for the start of the gradient declaration
    std::regex gradient_regex(R"(linear-gradient\s*\((.*)\))");
    std::smatch match;

    if (std::regex_search(input_str, match, gradient_regex)) {
        std::string inner_content = match.str(1);

        // Parse the fallback color written BEFORE the gradient
        std::string prefix = std::string(trim(match.prefix().str()));
        if (!prefix.empty()) {
            auto c = color::parse(prefix);
            if (!c) return std::nullopt;
            b.color = *c;
        } else {
            b.color = {0, 0, 0, 0}; // Default transparent
        }
        // Split the gradient parameters using your split_selectors function
        std::vector<std::string> tokens = split(inner_content);

        if (tokens.size() >= 2) {
        	gradient_stop_t stop;
            size_t color_index = 0;
            // Try to parse the direction via parse_direction
            b.direction = gradient_direction_t::to_bottom; // CSS default
            if (!parse(tokens[0], b.direction)) {
                auto c = color::parse(tokens[0]);
                if (!c) return std::nullopt; // If the color cannot be decoded, it is an error.
                stop.color = *c;
                stop.offset = 0.0;
                b.stops.push_back(stop);
                color_index = 1;
            }
            // Fill the stops array
            // The first token has already been processed
            size_t total_colors = tokens.size() - 1 + color_index;
            // std::cout << total_colors << "," << color_index << "\n"; 
            for (size_t i = 1; i < tokens.size(); i++) {
                auto c = color::parse(tokens[i]);
                if (!c) return std::nullopt; // If one of the colors inside the gradient is invalid
                stop.color = *c;
                if (color_index > 0) {
                    stop.offset = static_cast<double>(color_index) / static_cast<double>(total_colors - 1);
                } else {
                    stop.offset = 0.0;
                }
                b.stops.push_back(stop);
                color_index++;
            }
        } else {
            return std::nullopt; // Invalid number of gradient parameters
        }
        b.with_gradient = true;
        return b;

    } else {
        // If there is no gradient, this is a regular solid color
        auto c = color::parse(input_str);
        if (!c) return std::nullopt; // If one of the colors inside the gradient is invalid
        b.color = *c;
        b.with_gradient = false;
        b.direction = gradient_direction_t::none;
        return b;
    }
    return std::nullopt;
}

static const char * gradient_direction(gradient_direction_t dir) {
	if (dir == gradient_direction_t::to_top)    return "to top";
	if (dir == gradient_direction_t::to_bottom) return "to bottom";
	if (dir == gradient_direction_t::to_left)   return "to left";
	if (dir == gradient_direction_t::to_right)  return "to right";
	return "";
}

std::string background::format(const background_t& bg) {
    std::string bg_str = color::format(bg.color, bg.color.a < 1.0 ? RGBA : RRGGBB);

    if (bg.with_gradient) {
        bg_str += " linear-gradient(";
        auto dir = gradient_direction(bg.direction);
        if (strlen(dir) > 0) {
            bg_str += dir;
            bg_str += ',';
        }
        for (size_t i = 0; i < bg.stops.size(); i++) {
            bg_str += color::format(bg.stops[i].color, bg.stops[i].color.a < 1.0 ? RGBA : RRGGBB);
            if (i != (bg.stops.size() - 1)) bg_str += ',';
        }
        bg_str += ')';
    }

    return bg_str;
}

// Function that splits a selector string on commas
static
std::vector<std::string> split_selectors(const std::string& selectors_str) {
    std::vector<std::string> result;
    std::stringstream ss(selectors_str);
    std::string item;

    while (std::getline(ss, item, ',')) {
        std::string cleaned = trim(item);
        if (!cleaned.empty()) {
            result.push_back(cleaned);
        }
    }
    return result;
}

data_t parse_and_ungroup(const std::string& css_content) {
    data_t css_tree;

    css_parser parser;
    parser.parse_css(css_content);

    css_parser::token_t token;
    std::vector<std::string> current_selectors;

    // Temporary container for the current block's properties
    std::map<std::string, std::string> current_properties;

    std::string last_property = "";

    while ((token = parser.get_next_token()).type != css_parser::CSS_END) {
        // Print the token type and contents to the console
        // std::cout << "TOKEN TYPE: " << parser.get_type_name(token.type)
        //          << " | DATA: " << token.data << std::endl;

        if (token.type == css_parser::SEL_START) {
            current_selectors = split_selectors(token.data);
        }
        else if (token.type == css_parser::PROPERTY) {
            last_property = token.data;
        }
        else if (token.type == css_parser::VALUE) {
            if (!last_property.empty()) {
                // Store it for the current block
                current_properties[last_property] = token.data;
                last_property = ""; // Reset
            }
        }
        else if (token.type == css_parser::SEL_END) {
            // Assign the properties to the individual selectors
            for (const auto& selector : current_selectors) {
                for (const auto& [prop, val] : current_properties) {
                    css_tree[selector][prop] = val;
                }
            }
            current_selectors.clear();
            current_properties.clear();
        }
    }

    return css_tree;
}

// background-image : -gtk-scaled(url("assets/titlebutton-close-dark.png"),url("assets/titlebutton-close-dark@2.png"));
std::string
extract_asset_path_from_css(const std::string& css_value) {
    // The regex looks for the first occurrence of url("...") or url('...')
    std::regex url_regex(R"(url\([\"']?([^\"'\)]+)[\"']?\))");
    std::smatch match;

    if (std::regex_search(css_value, match, url_regex)) {
        return match.str(1); // Returns the clean path: "assets/titlebutton-close-dark.png"
    }
    return std::string{};
}

// The function finds ALL url(...) inside the string and returns them as a list
std::vector<std::string>
extract_asset_paths_from_css(const std::string& css_value) {
    std::vector<std::string> paths;

    // Regex for finding url("...") or url('...')
    std::regex url_regex(R"(url\([\"']?([^\"'\)]+)[\"']?\))");

    std::string::const_iterator search_start(css_value.cbegin());
    std::smatch match;

    while (std::regex_search(search_start, css_value.cend(), match, url_regex)) {
        paths.push_back(match.str(1)); // match.str(1) is the group inside the parentheses
        search_start = match.suffix().first; // move on to the next url()
    }

    return paths; // Returns either one path, or two (regular and @2), or an empty vector
}

}; // css namespace

namespace fs {

// Accepts "assets/titlebutton-close-dark.png"
// Returns "titlebutton-close-dark.png"
std::string
extract_filename_with_ext(const std::string& asset_path) {
    size_t last_slash = asset_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        // Cut everything starting from the character right after the last slash
        return asset_path.substr(last_slash + 1);
    }
    return asset_path; // If no slash was found, the path is already the filename
}


bool file_exists(const std::string& path) {
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

// Helper function for reading a regular file from disk into a string
std::string read_plain_file(const std::string& filepath) {
	std::ifstream file(filepath);
	if (!file.is_open()) return "";
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

bool
ensure_directory_exists(const std::string& path) {
    std::filesystem::path dir(path);

    // The .parent_path() method automatically strips the filename if you passed a file path
    // For example, from "~/.config/wayfire/assets/close.png" it will make "~/.config/wayfire/assets/"
    if (dir.has_filename() && dir.extension() != "") {
        dir = dir.parent_path();
    }

    try {
        // create_directories checks whether the path exists.
        // It returns true if it created new directories, and false if they already existed.
        std::filesystem::create_directories(dir);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        return false;
    }
}

}; // fs namespace

