#include "deco-theme.hpp"
// #include "deco-button-manager.hpp"

namespace wf::pixdecor
{

// ─── Helper parsers (moved out of the anonymous namespace) ───
static effect_t parse_effect_type(const std::string &type)
{
    static const std::unordered_map<std::string, effect_t> map = {
        {"smoke",              effect_t::smoke},
        {"ink",                effect_t::ink},
        {"clouds",             effect_t::clouds},
        {"halftone",           effect_t::halftone},
        {"pattern",            effect_t::pattern},
        {"lava",               effect_t::lava},
        {"hex",                effect_t::hex},
        {"zebra",              effect_t::zebra},
        {"neural_network",     effect_t::neural_network},
        {"hexagon_maze",       effect_t::hexagon_maze},
        {"raymarched_truchet", effect_t::raymarched_truchet},
        {"neon_pattern",       effect_t::neon_pattern},
        {"neon_rings",         effect_t::neon_rings},
        {"deco",               effect_t::deco},
    };
    auto it = map.find(type);
    return (it != map.end()) ? it->second : effect_t::none;
}

static overlay_t parse_overlay_engine(const std::string &engine)
{
    static const std::unordered_map<std::string, overlay_t> map = {
        {"rounded_corners", overlay_t::rounded_corners},
        {"beveled_glass",   overlay_t::beveled_glass},
        {"gtk_theme",       overlay_t::gtk_theme},
    };
    auto it = map.find(engine);
    return (it != map.end()) ? it->second : overlay_t::none;
}

static titlebar_opts_t parse_titlebar_options(const std::string &opts)
{
    static const std::unordered_map<std::string, titlebar_opts_t> map = {
        {"windowed",  titlebar_opts_t::windowed},
        {"maximized", titlebar_opts_t::maximized},
        {"never",     titlebar_opts_t::never},
        {"always",    titlebar_opts_t::always},
    };
    auto it = map.find(opts);
    return (it != map.end()) ? it->second : titlebar_opts_t::always;
}

static align_t get_alignment(const std::string& text, align_t default_alignment)
{
    std::string trimmed = css::trim(text);
    align_t align;
    if (css::parse(trimmed, align))
        return align;
    return default_alignment;
}
#if 0
// ─── Build a snapshot of current settings from options ───
settings_t pixdecor_theme_t::build_settings_from_options()
{
    settings_t snap{};
    
    snap.ignore_views_string         = std::string(ini::ignore_views_string);
    snap.always_decorate_string      = std::string(ini::always_decorate_string);
    snap.titlebar_height             = int(ini::titlebar_height);
    snap.titlebar_font               = std::string(ini::title_font);
    snap.titlebar_text_align         = get_alignment(std::string(ini::title_text_align), align_t::center);
    snap.titlebar_options            = parse_titlebar_options(std::string(ini::titlebar_options));
    snap.border_size                 = static_cast<double>(int(ini::border_size));
    snap.input_size                  = static_cast<double>(int(ini::input_size));
    snap.maximized_borders           = bool(ini::maximized_borders);
    snap.maximized_shadows           = bool(ini::maximized_shadows);
    snap.color                       = wf::color_t(ini::color);
    snap.background_color            = wf::color_t(ini::background_color);
    snap.text_color                  = wf::color_t(ini::text_color);
    snap.background_text_color       = wf::color_t(ini::background_text_color);
    snap.line_color                  = wf::color_t(ini::line_color);
    snap.line_thickness              = double(ini::line_thickness);
    snap.left_button_spacing         = int(ini::left_button_spacing);
    snap.right_button_spacing        = int(ini::right_button_spacing);
    snap.left_button_x_offset        = int(ini::left_button_x_offset);
    snap.right_button_x_offset       = int(ini::right_button_x_offset);
    snap.button_y_offset             = int(ini::button_y_offset);
    snap.button_minimize_image       = std::string(ini::button_minimize_image);
    snap.button_maximize_image       = std::string(ini::button_maximize_image);
    snap.button_restore_image        = std::string(ini::button_restore_image);
    snap.button_close_image          = std::string(ini::button_close_image);
    snap.button_minimize_hover_image = std::string(ini::button_minimize_hover_image);
    snap.button_maximize_hover_image = std::string(ini::button_maximize_hover_image);
    snap.button_restore_hover_image  = std::string(ini::button_restore_hover_image);
    snap.button_close_hover_image    = std::string(ini::button_close_hover_image);
    snap.button_layout               = check_empty_button_layout(std::string(ini::button_layout));
    snap.button_animation_length     = int(ini::button_hover_duration);
    snap.effect_type                 = parse_effect_type(std::string(ini::effect_type));
    snap.overlay_engine              = parse_overlay_engine(std::string(ini::overlay_engine));
    snap.theme_name                  = std::string(ini::theme_name);
    snap.enable_effect_animation     = bool(ini::effect_animate);
    snap.effect_focused_only         = bool(ini::effect_focused_only);
    snap.effect_color                = wf::color_t(ini::effect_color);
    snap.blur_radius                 = int(ini::shadow_radius);
    snap.corner_radius               = int(ini::rounded_corner_radius);
    snap.shadow_color                = wf::color_t(ini::shadow_color);
    snap.enable_shade                = bool(ini::enable_shade);
    snap.csd_titlebar_height         = int(ini::csd_titlebar_height);
    snap.shadow_size                 = 2.0 * snap.blur_radius;
    
    return snap;
}
#endif
// ─── Initialize the change tracking system ───
void pixdecor_theme_t::init_updates()
{
    LOGI("Pixdecor: initializing live settings updates");
    
    // ── GROUP "Titlebar" → recreate ──
    auto titlebar_cb = [] { on_titlebar_changed(); };
    ini::title_font.set_callback(titlebar_cb);
    ini::title_text_align.set_callback(titlebar_cb);
    ini::titlebar_options.set_callback(titlebar_cb);
    ini::titlebar_height.set_callback(titlebar_cb);
    
    // ── GROUP "Colors" → repaint ──
    auto colors_cb = [] { on_colors_changed(); };
    ini::color.set_callback(colors_cb);
    ini::background_color.set_callback(colors_cb);
    ini::text_color.set_callback(colors_cb);
    ini::background_text_color.set_callback(colors_cb);
    ini::shadow_color.set_callback(colors_cb);
    ini::effect_color.set_callback(colors_cb);
    
    // ── GROUP "Geometry" → recreate ──
    auto geometry_cb = [] { on_geometry_changed(); };
    ini::border_size.set_callback(geometry_cb);
    ini::input_size.set_callback(geometry_cb);
    ini::rounded_corner_radius.set_callback(geometry_cb);
    ini::shadow_radius.set_callback(geometry_cb);
    ini::csd_titlebar_height.set_callback(geometry_cb);
    ini::maximized_borders.set_callback(geometry_cb);
    ini::maximized_shadows.set_callback(geometry_cb);
    
    // ── GROUP "Buttons: layout" → recreate ──
    auto buttons_cb = [] { on_buttons_changed(); };
    ini::button_layout.set_callback(buttons_cb);
    ini::left_button_spacing.set_callback(buttons_cb);
    ini::right_button_spacing.set_callback(buttons_cb);
    ini::left_button_x_offset.set_callback(buttons_cb);
    ini::right_button_x_offset.set_callback(buttons_cb);
    ini::button_y_offset.set_callback(buttons_cb);
    ini::button_hover_duration.set_callback(buttons_cb);
    
    // ── GROUP "Icons" (baked) → invalidate ──
    auto icons_cb = [] { on_icons_changed(); };
    ini::line_color.set_callback(icons_cb);
    ini::line_thickness.set_callback(icons_cb);
    ini::button_minimize_image.set_callback(icons_cb);
    ini::button_maximize_image.set_callback(icons_cb);
    ini::button_restore_image.set_callback(icons_cb);
    ini::button_close_image.set_callback(icons_cb);
    ini::button_minimize_hover_image.set_callback(icons_cb);
    ini::button_maximize_hover_image.set_callback(icons_cb);
    ini::button_restore_hover_image.set_callback(icons_cb);
    ini::button_close_hover_image.set_callback(icons_cb);
    
    // ── GROUP "Effects and overlays" → repaint / recreate ──
    auto effects_cb = [] { on_effects_changed(); };
    ini::effect_type.set_callback(effects_cb);
    ini::overlay_engine.set_callback(effects_cb);
    ini::effect_animate.set_callback(effects_cb);
    ini::effect_focused_only.set_callback(effects_cb);
    
    // ── GROUP "Policy" → none ──
    //auto policy_cb = [] { on_policy_changed(); };
    //ini::ignore_views_string.set_callback(policy_cb);
    //ini::always_decorate_string.set_callback(policy_cb);
    
    // ── Special: GTK theme name ──
    ini::theme_name.set_callback([] { on_theme_name_changed(); });
    
    // ── Initial snapshot ──
    // settings = build_settings_from_options();
    build_settings_from_options(settings);
    
    LOGI("Pixdecor: live settings updates initialized");
}

// ── Helper functions ──
static resync_t max_resync(resync_t a, resync_t b)
{
    return (static_cast<int>(a) >= static_cast<int>(b)) ? a : b;
}

static bool colors_not_equal(color_t a, color_t b)
{
    constexpr double ε = 0.01;
    if (std::abs(a.r - b.r) > ε) return true;
    if (std::abs(a.g - b.g) > ε) return true;
    if (std::abs(a.b - b.b) > ε) return true;
    if (std::abs(a.a - b.a) > ε) return true;
    return false;
}

// ─── GROUP "Titlebar" → recreate ───
void pixdecor_theme_t::on_titlebar_changed()
{
    resync_t r = resync_t::none;
    bool font_changed = false;
    
    std::string new_font = std::string(ini::title_font);
    align_t new_align = get_alignment(std::string(ini::title_text_align), align_t::center);
    titlebar_opts_t new_opts = parse_titlebar_options(std::string(ini::titlebar_options));
    int new_height = int(ini::titlebar_height);
    
    if (new_font != settings.titlebar_font) {
        r = resync_t::recreate;
        font_changed = true;
    }
    if (new_align != settings.titlebar_text_align)
        r = resync_t::recreate;
    if (new_opts != settings.titlebar_options)
        r = resync_t::recreate;
    if (new_height != settings.titlebar_height)
        r = resync_t::recreate;
    
    if (r == resync_t::none)
        return;
    
    settings.titlebar_font       = new_font;
    settings.titlebar_text_align = new_align;
    settings.titlebar_options    = new_opts;
    settings.titlebar_height     = new_height;
    
    if (settings.overlay_engine != overlay_t::gtk_theme) {
        if (font_changed) {
            r = resync_t::recreate_with_font;
            // font_description.reset();
        }
        emit_updated(r);
    }
}

// ─── GROUP "Colors" → repaint ───
void pixdecor_theme_t::on_colors_changed()
{
    resync_t r = resync_t::none;
    
    wf::color_t new_color = wf::color_t(ini::color);
    wf::color_t new_bg = wf::color_t(ini::background_color);
    wf::color_t new_text = wf::color_t(ini::text_color);
    wf::color_t new_bg_text = wf::color_t(ini::background_text_color);
    wf::color_t new_shadow = wf::color_t(ini::shadow_color);
    wf::color_t new_effect = wf::color_t(ini::effect_color);
    
    if (colors_not_equal(new_color, settings.color))
        r = max_resync(r, resync_t::repaint);
    if (colors_not_equal(new_bg, settings.background_color))
        r = max_resync(r, resync_t::repaint);
    if (colors_not_equal(new_text, settings.text_color))
        r = max_resync(r, resync_t::repaint);
    if (colors_not_equal(new_bg_text, settings.background_text_color))
        r = max_resync(r, resync_t::repaint);
    if (colors_not_equal(new_shadow, settings.shadow_color))
        r = max_resync(r, resync_t::repaint);
    if (colors_not_equal(new_effect, settings.effect_color))
        r = max_resync(r, resync_t::repaint);
    
    if (r == resync_t::none)
        return;
    
    settings.color                 = new_color;
    settings.background_color      = new_bg;
    settings.text_color            = new_text;
    settings.background_text_color = new_bg_text;
    settings.shadow_color          = new_shadow;
    settings.effect_color          = new_effect;
    
    if (settings.overlay_engine != overlay_t::gtk_theme) {
        emit_updated(r);
    }
}

// ─── GROUP "Geometry" → recreate ───
void pixdecor_theme_t::on_geometry_changed()
{
    resync_t r = resync_t::none;
    
    double new_border = static_cast<double>(int(ini::border_size));
    double new_input = static_cast<double>(int(ini::input_size));
    int new_corner = int(ini::rounded_corner_radius);
    int new_blur_radius = int(ini::shadow_radius);
    int new_csd = int(ini::csd_titlebar_height);
    bool new_max_borders = bool(ini::maximized_borders);
    bool new_max_shadows = bool(ini::maximized_shadows);
    
    if (new_border != settings.border_size)
        r = max_resync(r, resync_t::recreate);
    if (new_input != settings.input_size)
        r = max_resync(r, resync_t::recreate);
    if (new_corner != settings.corner_radius)
        r = max_resync(r, resync_t::recreate);
    if (new_blur_radius != settings.blur_radius)
        r = max_resync(r, resync_t::recreate);
    if (new_csd != settings.csd_titlebar_height)
        r = max_resync(r, resync_t::recreate);
    if (new_max_borders != settings.maximized_borders)
        r = max_resync(r, resync_t::recreate);
    if (new_max_shadows != settings.maximized_shadows)
        r = max_resync(r, resync_t::recreate);
    
    if (r == resync_t::none)
        return;
    
    settings.border_size           = new_border;
    settings.input_size            = new_input;
    settings.blur_radius           = new_blur_radius;
    settings.corner_radius         = new_corner;
    settings.csd_titlebar_height   = new_csd;
    settings.maximized_borders     = new_max_borders;
    settings.maximized_shadows     = new_max_shadows;
    settings.shadow_size           = 2.0 * new_blur_radius;
    
    if (settings.overlay_engine != overlay_t::gtk_theme) {
        emit_updated(r);
    }
}

// ─── GROUP "Buttons: layout" → recreate ───
void pixdecor_theme_t::on_buttons_changed()
{
    resync_t r = resync_t::none;
    
    std::string new_layout = check_empty_button_layout(std::string(ini::button_layout));
    int new_left_spacing = int(ini::left_button_spacing);
    int new_right_spacing = int(ini::right_button_spacing);
    int new_left_offset = int(ini::left_button_x_offset);
    int new_right_offset = int(ini::right_button_x_offset);
    int new_y_offset = int(ini::button_y_offset);
    int new_duration = int(ini::button_hover_duration);
    
    if (new_layout != settings.button_layout)
        r = max_resync(r, resync_t::recreate);
    if (new_left_spacing != settings.left_button_spacing)
        r = max_resync(r, resync_t::recreate);
    if (new_right_spacing != settings.right_button_spacing)
        r = max_resync(r, resync_t::recreate);
    if (new_left_offset != settings.left_button_x_offset)
        r = max_resync(r, resync_t::recreate);
    if (new_right_offset != settings.right_button_x_offset)
        r = max_resync(r, resync_t::recreate);
    if (new_y_offset != settings.button_y_offset)
        r = max_resync(r, resync_t::recreate);
    if (new_duration != settings.button_animation_length)
        r = max_resync(r, resync_t::recreate);
    
    if (r == resync_t::none)
        return;
    
    settings.button_layout           = new_layout;
    settings.left_button_spacing     = new_left_spacing;
    settings.right_button_spacing    = new_right_spacing;
    settings.left_button_x_offset    = new_left_offset;
    settings.right_button_x_offset   = new_right_offset;
    settings.button_y_offset         = new_y_offset;
    settings.button_animation_length = new_duration;

    if (settings.overlay_engine != overlay_t::gtk_theme) {
        emit_updated(r);
    }
}

// ─── GROUP "Icons" (baked) → invalidate ───
void pixdecor_theme_t::on_icons_changed()
{
    resync_t r = resync_t::none;
    
    wf::color_t new_line_color = wf::color_t(ini::line_color);
    double new_line_thickness = double(ini::line_thickness);
    
    std::array<std::string, 4> new_images = {
        std::string(ini::button_minimize_image),
        std::string(ini::button_maximize_image),
        std::string(ini::button_restore_image),
        std::string(ini::button_close_image)
    };
    std::array<std::string, 4> new_hovers = {
        std::string(ini::button_minimize_hover_image),
        std::string(ini::button_maximize_hover_image),
        std::string(ini::button_restore_hover_image),
        std::string(ini::button_close_hover_image)
    };
    
    if (colors_not_equal(new_line_color, settings.line_color))
        r = resync_t::recreate;
    if (new_line_thickness != settings.line_thickness)
        r = resync_t::recreate;
    
    // Compare the images
    std::array<std::string, 4> cur_images = {
        settings.button_minimize_image,
        settings.button_maximize_image,
        settings.button_restore_image,
        settings.button_close_image
    };
    std::array<std::string, 4> cur_hovers = {
        settings.button_minimize_hover_image,
        settings.button_maximize_hover_image,
        settings.button_restore_hover_image,
        settings.button_close_hover_image
    };
    
    for (int i = 0; i < 4; ++i) {
        if (new_images[i] != cur_images[i])
            r = resync_t::recreate;
        if (new_hovers[i] != cur_hovers[i])
            r = resync_t::recreate;
    }
    
    if (r == resync_t::none)
        return;
    
    settings.line_color                  = new_line_color;
    settings.line_thickness              = new_line_thickness;
    settings.button_minimize_image       = new_images[0];
    settings.button_maximize_image       = new_images[1];
    settings.button_restore_image        = new_images[2];
    settings.button_close_image          = new_images[3];
    settings.button_minimize_hover_image = new_hovers[0];
    settings.button_maximize_hover_image = new_hovers[1];
    settings.button_restore_hover_image  = new_hovers[2];
    settings.button_close_hover_image    = new_hovers[3];

    if (settings.overlay_engine != overlay_t::gtk_theme) {

        gtk_theme.icons.minimize.active_normal  = settings.button_minimize_image;
        gtk_theme.icons.minimize.active_hovered = settings.button_minimize_hover_image;
        gtk_theme.icons.maximize.active_normal  = settings.button_maximize_image;
        gtk_theme.icons.maximize.active_hovered = settings.button_maximize_hover_image;
        gtk_theme.icons.close.active_normal     = settings.button_close_image;
        gtk_theme.icons.close.active_hovered    = settings.button_close_hover_image;
        gtk_theme.icons.restore.active_normal   = settings.button_restore_image;
        if (gtk_theme.icons.restore.active_normal.empty())
            gtk_theme.icons.restore.active_normal = gtk_theme.icons.maximize.active_normal;
        gtk_theme.icons.restore.active_hovered  = settings.button_restore_hover_image;
        if (gtk_theme.icons.restore.active_hovered.empty())
            gtk_theme.icons.restore.active_hovered = gtk_theme.icons.maximize.active_hovered;

        apply_icon_fallback(gtk_theme.icons.minimize);
        apply_icon_fallback(gtk_theme.icons.maximize);
        apply_icon_fallback(gtk_theme.icons.restore);
        apply_icon_fallback(gtk_theme.icons.close);

        // button_textures_manager_t::invalidate();

        emit_updated(resync_t::invalidate_icons);
    }
}

// ─── GROUP "Effects and overlays" → repaint / recreate ───
void pixdecor_theme_t::on_effects_changed()
{
    resync_t r = resync_t::none;
    
    effect_t new_effect = parse_effect_type(std::string(ini::effect_type));
    overlay_t new_overlay = parse_overlay_engine(std::string(ini::overlay_engine));
    bool new_animate = bool(ini::effect_animate);
    bool new_focused_only = bool(ini::effect_focused_only);
    
    if (new_effect != settings.effect_type)
        r = max_resync(r, resync_t::repaint);  // shader reads it on the fly
    if (new_animate != settings.enable_effect_animation)
        r = max_resync(r, resync_t::repaint);
    if (new_focused_only != settings.effect_focused_only)
        r = max_resync(r, resync_t::repaint);
    if (new_overlay != settings.overlay_engine)
        r = max_resync(r, resync_t::invalidate);  // engine change
    
    if (r == resync_t::none)
        return;
    
    settings.effect_type             = new_effect;
    settings.overlay_engine          = new_overlay;
    settings.enable_effect_animation = new_animate;
    settings.effect_focused_only     = new_focused_only;
    
    // If the overlay changed, a full theme reload is required
    if (r == resync_t::invalidate) {
        update();  // call the existing method for a full reload
        return;    // update() will emit the signal itself
    }
    
    emit_updated(r);
}
#if 0
// ─── GROUP "Policy" → none ───
void pixdecor_theme_t::on_policy_changed()
{
    settings.ignore_views_string    = std::string(ini::ignore_views_string);
    settings.always_decorate_string = std::string(ini::always_decorate_string);
    
    emit_updated(resync_t::none);
}
#endif
// ─── Special: GTK theme name ───
void pixdecor_theme_t::on_theme_name_changed()
{
    settings.theme_name = std::string(ini::theme_name);
    
    // Switching the theme requires a full reload
    update();  // existing method
}

} // namespace wf::pixdecor
