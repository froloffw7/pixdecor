#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <cmath>

#include "css_theme.hpp" // CSS Parser

namespace css {

// Финальная типизированная структура одной тени
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
    // Главный метод: парсит всю строку "box-shadow: ..." в типизированные структуры
    // Возвращает std::nullopt, если строка некорректна или пуста
    // Парсит как "box-shadow: 0 0 0 1px..." так и просто "0 0 0 1px..."
    static std::optional<std::vector<shadow_t>> parse(const std::string& input);

    // Обратная сборка списка теней в CSS-строку (без префикса "box-shadow:")
    static std::string format(const std::vector<shadow_t>& shadows);
    
    // Сборка одной тени в строку
    static std::string format(const shadow_t& shadow);

  private:
    // Промежуточная строковая структура для внутреннего парсинга
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
    
    // Методы конвертации строк в типы
    static std::optional<shadow_t> convert(const raw_shadow_t& raw);
    
};

};
