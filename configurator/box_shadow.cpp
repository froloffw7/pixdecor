#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>
#include <iomanip>

#include "box_shadow.hpp"
#include "css_helpers.hpp"

namespace css {

std::vector<std::string> box_shadow::split_shadows(const std::string& shadows_list) {
    std::vector<std::string> result;
    std::string current;
    int paren_count = 0;

    for (char ch : shadows_list) {
        if (ch == '(') paren_count++;
        else if (ch == ')') paren_count--;

        if (ch == ',' && paren_count == 0) {
            result.push_back(trim(current));
            current.clear();
        } else {
            current += ch;
        }
    }
    if (!current.empty()) {
        result.push_back(trim(current));
    }
    return result;
}

std::optional<box_shadow::raw_shadow_t> box_shadow::parse_raw_shadow(const std::string& shadow_str) {
    std::string cleaned = trim(shadow_str);
    if (cleaned.empty()) return std::nullopt;

    std::vector<std::string> tokens;
    std::string current_token;
    int paren_count = 0;

    for (char ch : cleaned) {
        if (ch == '(') paren_count++;
        else if (ch == ')') paren_count--;

        if (std::isspace(ch) && paren_count == 0) {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
        } else {
            current_token += ch;
        }
    }
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }

    if (tokens.size() < 2) return std::nullopt;

    raw_shadow_t raw{
        .is_inset = false,
        .x_offset{"0"},
        .y_offset{"0"},
        .blur_radius{"0"},
        .spread_radius{"0"},
        .color{"transparent"}};

    std::vector<std::string> numeric_tokens;

    for (const auto& token : tokens) {
        if (token == "inset") {
            raw.is_inset = true;
        } else if (token.find("rgb") != std::string::npos || 
                   token.front() == '#' || 
                   token == "transparent" ||
                   //(std::isalpha(token) && token != "px" && token != "em" && token != "rem")) {
                   (!token.empty() && std::all_of(token.begin(), token.end(),
                   [](unsigned char c) {
                       return std::isalpha(c);
                   }) && token != "px" && token != "em" && token != "rem")) {
            raw.color = token;
        } else {
            numeric_tokens.push_back(token);
        }
    }

    if (numeric_tokens.size() < 2 || numeric_tokens.size() > 4) {
        return std::nullopt; 
    }

    raw.x_offset = numeric_tokens[0];
    raw.y_offset = numeric_tokens[1];
    
    if (numeric_tokens.size() >= 3) raw.blur_radius = numeric_tokens[2];
    if (numeric_tokens.size() == 4) raw.spread_radius = numeric_tokens[3];

    return raw;
}

std::optional<shadow_t> box_shadow::convert(const raw_shadow_t& raw) {
    shadow_t shadow;

    auto x      = unit::parse(raw.x_offset);
    auto y      = unit::parse(raw.y_offset);
    auto blur   = unit::parse(raw.blur_radius);
    auto spread = unit::parse(raw.spread_radius);
    auto color  = color::parse(raw.color);

    if (!x || !y || !blur || !spread || !color) {
        return std::nullopt; 
    }
    shadow.is_inset      = raw.is_inset;
    shadow.x_offset      = *x;
    shadow.y_offset      = *y;
    shadow.blur_radius   = *blur;
    shadow.spread_radius = *spread;
    shadow.color         = *color;

    return shadow;
}

std::optional<std::vector<shadow_t>> box_shadow::parse(const std::string& input) {
    
    std::vector<shadow_t> result{};

    if (input.empty()) return result;
    
    std::string str = trim(input);
    
    if (!str.empty() && str.back() == ';') {
        str.pop_back();
        str = trim(str);
    }
    std::string shadow_text = str;
    
    size_t colon_pos = str.find(':');
    if (colon_pos != std::string::npos) {
        std::string prefix = trim(str.substr(0, colon_pos));
        // Пытаемся распарсить префикс. Если это не box-shadow, строка может быть просто 
        // объявлением значений без заголовка, содержащим случайное двоеточие (например, внутри rgba)
        if (prefix == "box-shadow") {
            shadow_text = trim(str.substr(colon_pos + 1));
        }
    }

    if (shadow_text.empty()) return std::nullopt;

    auto raw_shadows = split_shadows(shadow_text);

    for (const auto& raw_shadow : raw_shadows) {
        auto raw = parse_raw_shadow(raw_shadow);
        if (!raw) return std::nullopt;
        
        auto shadow = convert(*raw);
        if (!shadow) return std::nullopt;

        result.push_back(*shadow);
    }

    return result;
}

std::string box_shadow::format(const shadow_t& shadow) {
    std::string res;
    if (shadow.is_inset) res += "inset ";
    
    res += unit::format(shadow.x_offset) + " "
         + unit::format(shadow.y_offset);
         
    // В CSS можно опускать blur и spread, если они равны 0, 
    // но если spread не нулевой, мы обязаны вывести и blur.
    if (shadow.blur_radius.value != 0.0f || shadow.spread_radius.value != 0.0f) {
        res += " " + unit::format(shadow.blur_radius);
    }
    if (shadow.spread_radius.value != 0.0f) {
        res += " " + unit::format(shadow.spread_radius);
    }
    res += " " + color::format(shadow.color);
    
    return res;
}

std::string box_shadow::format(const std::vector<shadow_t>& shadows) {
    std::string res;
    
    for (size_t i = 0; i < shadows.size(); ++i) {
        res += format(shadows[i]);
        if (i + 1 < shadows.size()) {
            res += ", ";
        }
    }
    return res;
}

}; // css namespace

