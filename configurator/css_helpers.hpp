#include <string>
#include <optional>
#include <cmath>

namespace css {

// Вспомогательная функция для очистки пробелов и переносов строк
std::string trim(const std::string& text);

int parse_numeric(const std::string& value);

#if __cplusplus < 202002L
    // === ДЛЯ C++17 ===
    // Обычная Си-функция, которая реализована в вашем .cpp через vsnprintf
    std::string cxx_format(std::string fmt, ...);
#else
    // === ДЛЯ C++20 ===
    // Просто перенаправляем имя cpp_format на оригинальный std::format.
    // Без макросов, без шаблонов, чисто силами компилятора!
#define cpp_format  std::format
#endif

}; // css namespace

