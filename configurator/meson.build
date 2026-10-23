# test/meson.build
libgio   = dependency('gio-2.0') # Add GIO (GResource is provided by it)
gtk3_dep = dependency('gtk+-3.0')

subdir('cssparser')
subdir('icon_rendering')

generator_sources = files('gtkdecor_generator.cpp',
                          'css_theme.cpp',
                          'box_shadow.cpp',
                          'css_helpers.cpp')
# Build the generator executable
configurator = executable(
    'gtkdecor_generator',
    generator_sources,
    dependencies: [ 
        cssparser_dep, # Pass our static library
        libgio,
        gtk3_dep ],
    install: true 
)


