#include <string>
#include <optional>
#include <cmath>

namespace css {

// Helper function for trimming whitespace and line breaks
std::string trim(const std::string& text);

int parse_numeric(const std::string& value);

#if __cplusplus < 202002L
    // === FOR C++17 ===
    // A regular C function, implemented in your .cpp via vsnprintf
    std::string cxx_format(std::string fmt, ...);
#else
    // === FOR C++20 ===
    // Simply redirect the name cpp_format to the original std::format.
    // No macros, no templates — just the compiler.
#define cpp_format  std::format
#endif

}; // css namespace

