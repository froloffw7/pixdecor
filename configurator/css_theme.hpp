#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include <optional>

// Структура: [Одиночный Селектор] -> [Свойство] -> [Значение]

namespace css {

// Типы данных и функции для разбора CSS

int parse_numeric(const std::string& css_text);

enum class unit_t {
    none = 0,
    px,
    em,
    rem,
    ms,
};

    
// Строго типизированный размер (число + единица измерения)
struct length_t {
    float value = 0.0;
    unit_t unit = unit_t::none;

    bool operator==(const length_t& other) const {
        // Так как 'a' имеет тип float, используем небольшой эпсилон для точности сравнения
        return unit == other.unit && (std::abs(value - other.value) < 0.001f);
    }
};

struct unit {

    static std::optional<length_t> parse(const std::string& css_text);
    static const char * format(unit_t unit);
    static std::string format(const length_t&);

}; 

// Универсальная структура для margin и padding
struct edge_insets_t {
    length_t top;
    length_t right;
    length_t bottom;
    length_t left;

    bool operator==(const edge_insets_t& other) const {
        // Так как 'a' имеет тип float, используем небольшой эпсилон для точности сравнения
        return (top == other.top) && (right == other.right) && (bottom == other.bottom) && (left == other.left);
    }
};

struct edge_insets {

    // Универсальная функция: разберёт как "margin: 0 10px", так и "padding: 4px"
    static std::optional<edge_insets_t> parse(const std::string& css_text);
    static std::string format(const edge_insets_t&);

};// edge_insets namespace

enum color_format_t {
    RGB,
    RGBA,
    RRGGBB,
    RRGGBBAA,
};

// Структура для удобного хранения RGB
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

// Структура для хранения разобранных компонентов рамки
struct border_t {
    std::string width;
    std::string style;
    std::string color;
};

struct border
{
    // Основная функция разбора строки border
    static border_t parse(const std::string& css_text);
    static std::string format(const border_t& border);

  private:
    static bool is_width(const std::string& token);
    static bool is_style(const std::string& token);

}; // border namespace



// Направление градиента
enum class gradient_direction_t {
    none,
    to_top,
    to_bottom,
    to_left,
    to_right,
};

// Точка остановки градиента
struct gradient_stop_t {
    double  offset; // 0.0 ... 1.0
    color_t color;
};

// Финальный результат разбора фона
struct background_t {
    bool                         with_gradient = false;
    color_t                      color; // без значения по умолчанию
    gradient_direction_t         direction = gradient_direction_t::none;
    std::vector<gradient_stop_t> stops;
};

struct background
{
    // Основная функция разбора строки border
    static std::optional<background_t> parse(std::string css_text);
    static std::string format(const background_t& bg);

  private:
    static bool parse(const std::string& input_str, gradient_direction_t& direction);
};

using data_t = std::map<std::string, std::map<std::string, std::string>>;

data_t parse_and_ungroup(const std::string& css_content);

// Функция находит ВСЕ url(...) внутри строки и возвращает их списком
std::vector<std::string> extract_asset_paths_from_css(const std::string& css_text);


}; // css namespace

namespace fs {

std::string extract_filename_with_ext(const std::string& asset_path);
    
bool file_exists(const std::string& path);
// Вспомогательная функция для чтения обычного файла с диска в строку
std::string read_plain_file(const std::string& filepath);

bool ensure_directory_exists(const std::string& path);


}; // fs namespace


