gtk3_dep = dependency('gtk+-3.0')

renderer_sources = files('gtk_icon_renderer.cpp')

# Build the icon renderer executable
configurator = executable(
    'gtk_icon_renderer',
    renderer_sources,
    dependencies: [ 
        gtk3_dep ],
    install: true 
)


