# cssparser/meson.build

cssparser_sources = files(
    'css_denest.cpp',
    'css_parser.cpp',
    'css_properties.cpp',
    'css_utils.cpp',
)

cssparser_inc = include_directories('.')

# Use static_library instead of library to force building a .a archive
cssparser_lib = static_library(
    'cssparser',
    cssparser_sources,
    include_directories: cssparser_inc,
    dependencies: libgio, # 2. Pass it here so that theme_parser.cpp can be compiled
    install: false # The plugin's static library does not need to be installed system-wide
)

cssparser_dep = declare_dependency(
    link_with: cssparser_lib,
    include_directories: cssparser_inc
)
