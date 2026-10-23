#include <css_helpers.hpp>
#include <algorithm>
#if __cplusplus < 202002L
#include <cstdarg> // Required for va_list, va_start, va_end
#endif

namespace css {

// Helper function for trimming whitespace and line breaks
std::string trim(const std::string& text) {
    if (text.empty()) return std::string{};
	
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string{};
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, (last - first + 1));
}

int parse_numeric(const std::string& value) {
    try {
        // std::stoi will turn "12pt" into 12 and "1px" into 1
        return std::stoi(value);
    } catch (...) {
        return 0; // In case the string was empty or invalid
    }
}

#if __cplusplus < 202002L

#include <cstdarg> // Required for va_list, va_start, va_end

std::string cxx_format(std::string fmt, ...) {
    // 1. Quickly convert C++ syntax into C syntax
    for (size_t pos = 0; (pos = fmt.find("{:")) != std::string::npos; ) {
        size_t end_pos = fmt.find('}', pos);
        fmt.replace(pos, end_pos - pos + 1, "%" + fmt.substr(pos + 2, end_pos - pos - 2));
    }
    
    size_t pos = 0;
    while ((pos = fmt.find("{}", pos)) != std::string::npos) {
        fmt.replace(pos, 2, "%d"); 
    }

    // 2. Initialize the C argument list
    va_list args;
    
    // First measure the size of the resulting string
    va_start(args, fmt);
    int size_s = std::vsnprintf(nullptr, 0, fmt.c_str(), args);
    va_end(args);

    if (size_s <= 0) return "";

    // Allocate memory for the string
    std::string result(static_cast<size_t>(size_s), '\0');
    
    // Fill the string with the actual data
    va_start(args, fmt);
    std::vsnprintf(&result[0], size_s + 1, fmt.c_str(), args);
    va_end(args);
    
    return result;
}
#endif

}; // css namespace

