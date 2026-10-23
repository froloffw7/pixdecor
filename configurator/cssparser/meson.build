# cssparser/meson.build

cssparser_sources = files(
    'css_denest.cpp',
    'css_parser.cpp',
    'css_properties.cpp',
    'css_utils.cpp',
)

cssparser_inc = include_directories('.')

# Используем static_library вместо library, чтобы принудительно собрать .a архив
cssparser_lib = static_library(
    'cssparser',
    cssparser_sources,
    include_directories: cssparser_inc,
    dependencies: libgio, # 2. Передаем её сюда, чтобы скомпилировать theme_parser.cpp
    install: false # Статическую библиотеку для плагина устанавливать в систему не нужно
)

cssparser_dep = declare_dependency(
    link_with: cssparser_lib,
    include_directories: cssparser_inc
)
