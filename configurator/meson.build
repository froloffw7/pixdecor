# test/meson.build
libgio   = dependency('gio-2.0') # Добавляем GIO (GResource живет здесь)
gtk3_dep = dependency('gtk+-3.0')

subdir('cssparser')
subdir('icon_rendering')

generator_sources = files('gtkdecor_generator.cpp',
                          'css_theme.cpp',
                          'box_shadow.cpp',
                          'css_helpers.cpp')
# Собираем исполняемый файл теста
configurator = executable(
    'gtkdecor_generator',
    generator_sources,
    dependencies: [ 
        cssparser_dep, # Передаем нашу статическую библиотеку
        libgio,
        gtk3_dep ],
    install: true 
)


