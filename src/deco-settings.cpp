#include "deco-settings.hpp"
#include "gtk_settings.hpp"

namespace wf::pixdecor
{
namespace ini
{
//wf::option_wrapper_t<std::string> ignore_views_string{"pixdecor/ignore_views"};
//wf::option_wrapper_t<std::string> always_decorate_string{"pixdecor/always_decorate"};

wf::option_wrapper_t<int>         titlebar_height{"pixdecor/titlebar_height"};
wf::option_wrapper_t<std::string> title_font{"pixdecor/title_font"};
wf::option_wrapper_t<std::string> title_text_align{"pixdecor/title_text_align"};
wf::option_wrapper_t<std::string> titlebar_options{"pixdecor/titlebar"};
wf::option_wrapper_t<int>         border_size{"pixdecor/border_size"};
wf::option_wrapper_t<int>         input_size{"pixdecor/input_size"};
wf::option_wrapper_t<bool>        maximized_borders{"pixdecor/maximized_borders"};
wf::option_wrapper_t<bool>        maximized_shadows{"pixdecor/maximized_shadows"};
wf::option_wrapper_t<wf::color_t> color{"pixdecor/fg_color"};
wf::option_wrapper_t<wf::color_t> background_color{"pixdecor/bg_color"};
wf::option_wrapper_t<wf::color_t> text_color{"pixdecor/fg_text_color"};
wf::option_wrapper_t<wf::color_t> background_text_color{"pixdecor/bg_text_color"};
wf::option_wrapper_t<wf::color_t> line_color{"pixdecor/button_color"};
wf::option_wrapper_t<double>      line_thickness{"pixdecor/button_line_thickness"};
wf::option_wrapper_t<int>         left_button_spacing{"pixdecor/left_button_spacing"};
wf::option_wrapper_t<int>         right_button_spacing{"pixdecor/right_button_spacing"};
wf::option_wrapper_t<int>         left_button_x_offset{"pixdecor/left_button_x_offset"};
wf::option_wrapper_t<int>         right_button_x_offset{"pixdecor/right_button_x_offset"};
wf::option_wrapper_t<int>         button_y_offset{"pixdecor/button_y_offset"};
wf::option_wrapper_t<std::string> button_minimize_image{"pixdecor/button_minimize_image"};
wf::option_wrapper_t<std::string> button_maximize_image{"pixdecor/button_maximize_image"};
wf::option_wrapper_t<std::string> button_restore_image{"pixdecor/button_restore_image"};
wf::option_wrapper_t<std::string> button_close_image{"pixdecor/button_close_image"};
wf::option_wrapper_t<std::string> button_minimize_hover_image{"pixdecor/button_minimize_hover_image"};
wf::option_wrapper_t<std::string> button_maximize_hover_image{"pixdecor/button_maximize_hover_image"};
wf::option_wrapper_t<std::string> button_restore_hover_image{"pixdecor/button_restore_hover_image"};
wf::option_wrapper_t<std::string> button_close_hover_image{"pixdecor/button_close_hover_image"};
wf::option_wrapper_t<std::string> button_layout{"pixdecor/button_layout"};
wf::option_wrapper_t<int>         button_hover_duration{"pixdecor/button_hover_duration"};

wf::option_wrapper_t<std::string> effect_type{"pixdecor/effect_type"};
wf::option_wrapper_t<std::string> overlay_engine{"pixdecor/overlay_engine"};
wf::option_wrapper_t<std::string> theme_name{"pixdecor/theme_name"};

wf::option_wrapper_t<bool>        effect_animate{"pixdecor/animate"};
wf::option_wrapper_t<bool>        effect_focused_only{"pixdecor/animate_focused_only"};
wf::option_wrapper_t<wf::color_t> effect_color{"pixdecor/effect_color"};

wf::option_wrapper_t<int>         rounded_corner_radius{"pixdecor/rounded_corner_radius"};
wf::option_wrapper_t<int>         shadow_radius{"pixdecor/shadow_radius"};
wf::option_wrapper_t<wf::color_t> shadow_color{"pixdecor/shadow_color"};
wf::option_wrapper_t<bool>        enable_shade{"pixdecor/enable_shade"};

wf::option_wrapper_t<int>         csd_titlebar_height{"pixdecor/csd_titlebar_height"};

wf::option_wrapper_t<wf::animation_description_t> shade_animation{"pixdecor/shade_duration"};

}

static
effect_t parse_effect_type(const std::string &type) {
    effect_t effect{ effect_t::none };
    
    if      (type == "smoke")              effect = effect_t::smoke;
    else if (type == "ink")                effect = effect_t::ink;
    else if (type == "clouds")             effect = effect_t::clouds;
    else if (type == "halftone")           effect = effect_t::halftone;
    else if (type == "pattern")            effect = effect_t::pattern;
    else if (type == "lava")               effect = effect_t::lava;
    else if (type == "hex")                effect = effect_t::hex;
    else if (type == "zebra")              effect = effect_t::zebra;
    else if (type == "neural_network")     effect = effect_t::neural_network;
    else if (type == "hexagon_maze")       effect = effect_t::hexagon_maze;
    else if (type == "raymarched_truchet") effect = effect_t::raymarched_truchet;
    else if (type == "neon_pattern")       effect = effect_t::neon_pattern;
    else if (type == "neon_rings")         effect = effect_t::neon_rings;
    else if (type == "deco")               effect = effect_t::deco;

    return effect;
}

static
overlay_t parse_overlay_engine(const std::string &engine) {
    overlay_t overlay{ overlay_t::none };
    
    if      (engine == "rounded_corners") overlay = overlay_t::rounded_corners;
    else if (engine == "beveled_glass")   overlay = overlay_t::beveled_glass;
    else if (engine == "gtk_theme")       overlay = overlay_t::gtk_theme;

    return overlay;
}

static 
titlebar_opts_t parse_titlebar_options(const std::string &opts) {
    titlebar_opts_t options{ titlebar_opts_t::always };
    
    if      (opts == "windowed")  options = titlebar_opts_t::windowed;
    else if (opts == "maximized") options = titlebar_opts_t::maximized;
    else if (opts == "never")     options = titlebar_opts_t::never;

    return options;
}

static align_t get_alignment(const std::string& text, align_t default_alignment)
{
    if (css::is_number(text) ) {
        // If parse only accepts int, create a temporary variable under the hood
        int value;
        if (!css::parse(text, value) || value != std::clamp(value, 0, 2)) {
            return default_alignment;
        }
        return static_cast<align_t>(value);
    } else {
        std::string align_str{ css::trim(text) };
        align_t align;
        if (!css::parse(align_str, align))
            return default_alignment;
        return align;
    }
}

void build_settings_from_options(settings_t &settings) {
    using namespace ini;
    
    settings_t new_s{
        //.ignore_views_string         = ignore_views_string,
        //.always_decorate_string      = always_decorate_string,

        .titlebar_height             = titlebar_height,
        .titlebar_font               = title_font,
        .titlebar_text_align         = get_alignment(title_text_align, align_t::center),
        .titlebar_options            = parse_titlebar_options(titlebar_options),
        .border_size                 = static_cast<double>(border_size),
        .input_size                  = static_cast<double>(input_size),
        .maximized_borders           = maximized_borders,
        .maximized_shadows           = maximized_shadows,
        .color                       = color,
        .background_color            = background_color,
        .text_color                  = text_color,
        .background_text_color       = background_text_color,
        .line_color                  = line_color,
        .line_thickness              = line_thickness,
        .left_button_spacing         = left_button_spacing,
        .right_button_spacing        = right_button_spacing,
        .left_button_x_offset        = left_button_x_offset,
        .right_button_x_offset       = right_button_x_offset,
        .button_y_offset             = button_y_offset,
        .button_minimize_image       = button_minimize_image,
        .button_maximize_image       = button_maximize_image,
        .button_restore_image        = button_restore_image,
        .button_close_image          = button_close_image,
        .button_minimize_hover_image = button_minimize_hover_image,
        .button_maximize_hover_image = button_maximize_hover_image,
        .button_restore_hover_image  = button_restore_hover_image,
        .button_close_hover_image    = button_close_hover_image,
        .button_layout               = check_empty_button_layout(button_layout),
        .button_animation_length     = button_hover_duration,

        .effect_type                 = parse_effect_type(effect_type),
        .overlay_engine              = parse_overlay_engine(overlay_engine),
        .theme_name                  = theme_name,
        .enable_effect_animation     = effect_animate,
        .effect_focused_only         = effect_focused_only,
        .effect_color                = effect_color,
        .blur_radius                 = shadow_radius,
        .corner_radius               = rounded_corner_radius,
        .shadow_color                = shadow_color,
        .enable_shade                = enable_shade,
        //.shade_modifier              = shade_modifier,
        .csd_titlebar_height         = csd_titlebar_height,
    };    
    /* Minimum height for titlebar */
    new_s.titlebar_height = std::max(new_s.titlebar_height, MIN_BAR_HEIGHT);
    if (new_s.overlay_engine != overlay_t::none) new_s.shadow_size = 2.0 * new_s.blur_radius;
    else                                         new_s.shadow_size = 0;

    settings = new_s;
}

void print_color_info(const std::string& label, const wf::color_t& color);

const char* effect_type_text(effect_t type)
{
	const char *text;
	switch (type) {
	case effect_t::none:            text = "none"; break;
    case effect_t::smoke:           text = "smoke"; break;
    case effect_t::ink:             text = "ink"; break;
    case effect_t::clouds:          text = "clouds"; break;
    case effect_t::halftone:        text = "halftone"; break;
    case effect_t::pattern:         text = "pattern"; break;
    case effect_t::lava:            text = "lava"; break;
    case effect_t::hex:             text = "hex"; break;
    case effect_t::zebra:           text = "zebra"; break;
    case effect_t::neural_network:  text = "neural_network"; break;
    case effect_t::hexagon_maze:    text = "hexagon_maze"; break;
    case effect_t::raymarched_truchet: text = "raymarched_truchet"; break;
    case effect_t::neon_pattern:    text = "neon_pattern"; break;
    case effect_t::neon_rings:      text = "neon_rings"; break;
    case effect_t::deco:            text = "deco"; break;
    default:                         text = "unknown effect type"; 
    }
    return text;
}

const char* overlay_engine_text(overlay_t overlay)
{
	const char *text;
	switch (overlay) {
	case overlay_t::none:            text = "none";            break;
    case overlay_t::rounded_corners: text = "rounded_corners"; break;
    case overlay_t::beveled_glass:   text = "beveled_glass";   break;
    case overlay_t::gtk_theme:       text = "gtk_theme";       break;
    default:                         text = "unknown overlay engine"; 
    }
    return text;
}

void print_settings(const settings_t& s) {
    LOGW("=== PIXDECOR SETTINGS CONFIGURATION ===");
    // ---- window selection for decoration ----
    //LOGW("  ignore_views_string:    ", s.ignore_views_string);
    //LOGW("  always_decorate_string: ", s.always_decorate_string);

    // ---- window titlebar ----
    LOGW("  titlebar_height:       ", s.titlebar_height);
    LOGW("  titlebar_font:         ", s.titlebar_font);
    LOGW("  titlebar_text_align:   ", static_cast<int>(s.titlebar_text_align));
    LOGW("  titlebar_options:      ", static_cast<uint32_t>(s.titlebar_options));

    LOGW("  border_size:           ", s.border_size);
    LOGW("  input_size:            ", s.input_size);

    // ---- maximization ----
    LOGW("  maximized_borders:     ", s.maximized_borders ? "true" : "false");
    LOGW("  maximized_shadows:     ", s.maximized_shadows ? "true" : "false");

    // ---- colors ----
    print_color_info("  fg_color", s.color);
    print_color_info("  background_color", s.background_color);
    print_color_info("  text_color", s.text_color);
    print_color_info("  background_text_color", s.background_text_color);
    print_color_info("  line_color", s.line_color);
    LOGW("  line_thickness: ", s.line_thickness);

    // ---- buttons: layout and padding ----
    LOGW("  button_layout:         ", s.button_layout);
    LOGW("  left_button_spacing:   ", s.left_button_spacing);
    LOGW("  right_button_spacing:  ", s.right_button_spacing);
    LOGW("  left_button_x_offset:  ", s.left_button_x_offset);
    LOGW("  right_button_x_offset: ", s.right_button_x_offset);
    LOGW("  button_y_offset:       ", s.button_y_offset);
    LOGW("  button_hover_duration: ", s.button_animation_length);

    // ---- buttons: images ----
    LOGW("  button_minimize_image:       ", s.button_minimize_image);
    LOGW("  button_maximize_image:       ", s.button_maximize_image);
    LOGW("  button_restore_image:        ", s.button_restore_image);
    LOGW("  button_close_image:          ", s.button_close_image);
    LOGW("  button_minimize_hover_image: ", s.button_minimize_hover_image);
    LOGW("  button_maximize_hover_image: ", s.button_maximize_hover_image);
    LOGW("  button_restore_hover_image:  ", s.button_restore_hover_image);
    LOGW("  button_close_hover_image:    ", s.button_close_hover_image);

    // ---- engines and effects ----
    LOGW("  effect_type:           ", effect_type_text(s.effect_type));
    LOGW("  overlay_engine:        ", overlay_engine_text(s.overlay_engine));
    LOGW("  gtk_theme:             ", s.theme_name);
    LOGW("  effect_animation:      ", s.enable_effect_animation ? "enabled" : "disabled");
    LOGW("  aminate_focused_only:  ", s.effect_focused_only ? "enabled" : "disabled");
    print_color_info("  effect_color", s.effect_color);
    LOGW("  blur_radius:           ", s.blur_radius);
    LOGW("  corner_radius:         ", s.corner_radius);
    print_color_info("  shadow_color", s.shadow_color);
    LOGW("  enable_shade:          ", s.enable_shade ? "true" : "false");

    // ---- misc ----
    LOGW("  csd_titlebar_height:   ", s.csd_titlebar_height);

    LOGW("=======================================");
}

//-----------------------------------------------------------------------------------------

const std::string shade_transformer_name = "pixdecor_shade";

}
