#include <css_helpers.hpp>
#include <algorithm>
#if __cplusplus < 202002L
#include <cstdarg> // Обязательно для va_list, va_start, va_end
#endif

namespace css {

// Вспомогательная функция для очистки пробелов и переносов строк
std::string trim(const std::string& text) {
    if (text.empty()) return std::string{};
	
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string{};
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, (last - first + 1));
}

int parse_numeric(const std::string& value) {
    try {
        // std::stoi превратит "12pt" в 12, а "1px" в 1
        return std::stoi(value);
    } catch (...) {
        return 0; // На случай, если строка была пустой или некорректной
    }
}

#if __cplusplus < 202002L

#include <cstdarg> // Обязательно для va_list, va_start, va_end

std::string cxx_format(std::string fmt, ...) {
    // 1. Быстро переводим синтаксис C++ в Си-синтаксис
    for (size_t pos = 0; (pos = fmt.find("{:")) != std::string::npos; ) {
        size_t end_pos = fmt.find('}', pos);
        fmt.replace(pos, end_pos - pos + 1, "%" + fmt.substr(pos + 2, end_pos - pos - 2));
    }
    
    size_t pos = 0;
    while ((pos = fmt.find("{}", pos)) != std::string::npos) {
        fmt.replace(pos, 2, "%d"); 
    }

    // 2. Инициализируем список аргументов Си
    va_list args;
    
    // Сначала измеряем размер итоговой строки
    va_start(args, fmt);
    int size_s = std::vsnprintf(nullptr, 0, fmt.c_str(), args);
    va_end(args);

    if (size_s <= 0) return "";

    // Выделяем память под строку
    std::string result(static_cast<size_t>(size_s), '\0');
    
    // Заполняем строку реальными данными
    va_start(args, fmt);
    std::vsnprintf(&result[0], size_s + 1, fmt.c_str(), args);
    va_end(args);
    
    return result;
}
#endif

}; // css namespace

