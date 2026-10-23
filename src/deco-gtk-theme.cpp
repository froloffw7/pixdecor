#include <fstream>
#include <filesystem>
#include <wayfire/util/log.hpp>

#include "deco-gtk-theme.hpp"

namespace fs = std::filesystem;

namespace wf::pixdecor {

static void set_icon(icon_t& icon, const std::string state, const std::string val) {
        if      (state == "active_normal")    icon.active_normal    = val;
        else if (state == "active_hovered")   icon.active_hovered   = val;
        else if (state == "active_clicked")   icon.active_clicked   = val;
        else if (state == "inactive_normal")  icon.inactive_normal  = val;
        else if (state == "inactive_hovered") icon.inactive_hovered = val;
        else if (state == "inactive_clicked") icon.inactive_clicked = val;
}

// maximum values from shadow set = borders
struct deco_params_t {
    double       x;       // shadow x_offset
    double       y;       // shadow y_offset
    double       radius;  // blur_radius
    double       spread;  // spread_radius
    css::sides_t width;   // window borders
};

static void calculate_shadows(const std::vector<css::shadow_t>& shadows, deco_params_t& p)
{
    double r = p.radius;
    for (size_t i = 0; i < shadows.size(); i++) {
        const auto& s = shadows[i];
        p.x = std::max(p.x, s.x_offset);
        p.y = std::max(p.y, s.y_offset);
        if (r < s.blur_radius) {
            r = s.blur_radius;
            p.radius = s.blur_radius;
            p.spread = s.spread_radius;
        }
    }
}

static void calculate_borders(const struct border_t& border, deco_params_t& p)
{
    const auto& width{ border.width };
    auto& w{ p.width };
    if (width.top    > w.top)    w.top    = width.top;
    if (width.right  > w.right)  w.right  = width.right;
    if (width.bottom > w.bottom) w.bottom = width.bottom;
    if (width.left   > w.left)   w.left   = width.left;
}

template <typename marg_t>
inline std::string fmt_margins(const marg_t& m, const std::string& header = "margins")
{
    std::ostringstream os;
    os << header << "=[" << m.left << '+' << m.right << '+' << m.bottom << '+' << m.top << ']';
    return os.str();
}

bool load_gtk_theme(const std::string& filepath, gtk_theme_t& settings) {
    fs::path path(filepath);
    // Check that the object exists AND is a regular file (not a directory)
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        return false;
    }
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    std::string current_section = "";

    while (std::getline(file, line)) {
        line = css::trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        // Capture the sections
        if (line[0] == '[' && line.back() == ']') {
            // Move the settings to backdrop
            if      (current_section == "titlebar")
                settings.titlebar.border_backdrop = settings.titlebar.border;
            else if (current_section == "decoration")
                settings.decoration_backdrop = settings.decoration;
                
            current_section = line.substr(1, line.length() - 2);
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = css::trim(line.substr(0, eq_pos));
        std::string val = css::trim(line.substr(eq_pos + 1));

        bool unsupported_key = false;

        auto set_or_log = [&](auto& field) {
            if (!css::parse(val, field))
                LOGE("[", current_section, "] bad value: ", key, " = ", val);
        };

        if (current_section == "main") {
            if      (key == "theme-name")    settings.theme_name = val;
            else if (key == "font-name")     settings.font_name = val;
            else unsupported_key = true;
        } 
        else if (current_section == "titlebar") {
            if      (key == "font-family")      settings.titlebar.font_family = val;
            else if (key == "font-size")        settings.titlebar.font_size = val;
            else if (key == "font-weight")      settings.titlebar.font_weight = val;
            else if (key == "button-layout")    settings.titlebar.button_layout = check_empty_button_layout(val);
            else if (key == "color")            set_or_log(settings.titlebar.color);
            else if (key == "background-color") set_or_log(settings.titlebar.background.color);
            else if (key == "background")       set_or_log(settings.titlebar.background);
            else if (key == "border-color")     set_or_log(settings.titlebar.border.color);
            else if (key == "border-width")     set_or_log(settings.titlebar.border.width);
            else if (key == "border-radius")    set_or_log(settings.titlebar.border.radius);
            else if (key == "border-style")     settings.titlebar.border.style = val;
            else if (key == "min-height")       set_or_log(settings.titlebar.min_height);
            else if (key == "padding")          set_or_log(settings.titlebar.padding);
            else if (key == "text-align")       set_or_log(settings.titlebar.text_align);
            else if (key == "text-padding")     set_or_log(settings.titlebar.text_padding);
            else if (key == "box-shadow") {
                // USING THE CLASS: Parse the titlebar inner shadow
                auto parsed = css::box_shadow::parse(val);
                if (parsed) {
                    settings.titlebar.shadows = *parsed;
                } else {
                    LOGE("Error: box-shadow ", val);
                }
            } else {
                unsupported_key = true;
            }
        }
        else if (current_section == "titlebar:backdrop") {
            if      (key == "color")            set_or_log(settings.titlebar.color_backdrop);
            else if (key == "background-color") set_or_log(settings.titlebar.background_backdrop.color);
            else if (key == "background")       set_or_log(settings.titlebar.background_backdrop);
            // ignore other border settings
            else if (key == "border-color")     set_or_log(settings.titlebar.border_backdrop.color);
            else unsupported_key = true;
        }
        else if (current_section == "decoration") {
            if      (key == "border-color")  set_or_log(settings.decoration.border.color);
            else if (key == "border-width")  set_or_log(settings.decoration.border.width);
            else if (key == "border-radius") set_or_log(settings.decoration.border.radius);
            else if (key == "border-style")  settings.decoration.border.style = val;
            else if (key == "box-shadow") {
                // USING THE CLASS: Parse the active window shadow
                auto parsed = css::box_shadow::parse(val);
                if (parsed) {
                    settings.decoration.shadows = *parsed;
                } else {
                    LOGE("Error: box-shadow ", val);
                }
            } else {
                unsupported_key = true;
            }
        }
        else if (current_section == "decoration:backdrop") {
            // ignore other border settings
            if      (key == "border-color")  set_or_log(settings.decoration_backdrop.border.color);
            else if (key == "border-width")  set_or_log(settings.decoration_backdrop.border.width);
            else if (key == "border-radius") set_or_log(settings.decoration_backdrop.border.radius);
            else if (key == "border-style")  settings.decoration_backdrop.border.style = val;
            else if (key == "box-shadow") {
                // USING THE CLASS: Parse the inactive window shadow
                auto parsed = css::box_shadow::parse(val);
                if (parsed) {
                    settings.decoration_backdrop.shadows = *parsed;
                } else {
                    LOGE("Error: box-shadow ", val);
                }
            } else {
                unsupported_key = true;
            }
        }
        else if (current_section == "icons") { 
            if      (key == "image-size") set_or_log(settings.icons.image_size);
            else if (key == "min-width")  set_or_log(settings.icons.min_width);
            else if (key == "spacing")    set_or_log(settings.icons.spacing);
            else if (key == "padding")    set_or_log(settings.icons.padding);
            else if (key == "color")        set_or_log(settings.icons.color);
            else if (key == "color:hover")  set_or_log(settings.icons.color_hovered);
            else if (key == "color:active") set_or_log(settings.icons.color_clicked);
            else if (key == "background-color")        set_or_log(settings.icons.background_color);
            else if (key == "background-color:hover")  set_or_log(settings.icons.background_color_hovered);
            else if (key == "background-color:active") set_or_log(settings.icons.background_color_clicked);

            else if (key.rfind("close_", 0) == 0) {
                std::string state = key.substr(6);
                set_icon(settings.icons.close, state, val);
            }
            else if (key.rfind("minimize_", 0) == 0) {
                std::string state = key.substr(9);
                set_icon(settings.icons.minimize, state, val);
            }
            else if (key.rfind("maximize_", 0) == 0) {
                std::string state = key.substr(9);
                set_icon(settings.icons.maximize, state, val);
            }
            else if (key.rfind("restore_", 0) == 0) {
                std::string state = key.substr(8);
                set_icon(settings.icons.restore, state, val);
            } else {
                unsupported_key = true;
            }
        }
        if (unsupported_key)
            LOGW("In section ", current_section, " unsupported key: `", key, " = ", val, '`');
    }
    // Theme loader (boundary = data normalization, our principle):
    auto& titlebar{ settings.titlebar };
    auto& icons{ settings.icons};
    // default PNG
    if (icons.image_size <= 0) icons.image_size = 16.0;
    // default of a TYPICAL BUTTON, not PNG
    if (icons.min_width < icons.image_size) icons.min_width = icons.image_size;
    double height_increase = titlebar.padding.top + titlebar.border.width.top;
        if ((titlebar.shadows.size() > 0) && titlebar.shadows[0].is_inset)
            height_increase += titlebar.shadows[0].y_offset;
    height_increase = 2.0 * std::max(height_increase, titlebar.padding.bottom + titlebar.border.width.bottom);
    // Initial value, comparison is safe
    if (titlebar.min_height == 0.0) {
        LOGW("Titlebar minimum height not set");
        if (!titlebar.font_size.empty()) {
            double font_size;
            if (css::parse(titlebar.font_size, font_size)) {// 12pt -> 16px
                double height = 1.5 * font_size + height_increase +
                    titlebar.text_padding.top + titlebar.text_padding.bottom;
                titlebar.height = std::round(height);
                LOGW("Estimated titlebar height: ", titlebar.height);
            } else {
                LOGE("Titlebar font size definition error");
                /* Minimum height for titlebar */
                // titlebar.height = MIN_BAR_HEIGHT;
            }
        }
    } else {
        titlebar.height = titlebar.min_height;
    }
    if (titlebar.height < (icons.min_width + height_increase)) {
        titlebar.height = icons.min_width + height_increase;
        LOGW("Titlebar height increased: ", titlebar.height);
    }
    int delta_px = std::lround(titlebar.height - icons.min_width);
    // Increase height by 1px to center the icons
    if ((delta_px % 2) != 0) titlebar.height += 1.0;
    titlebar.height = std::round(titlebar.height);
    LOGW("Titlebar height: ", titlebar.height);
    
    deco_params_t params{};
    calculate_shadows(settings.decoration.shadows, params);
    calculate_shadows(settings.decoration_backdrop.shadows, params);
    auto r = params.radius;
    // GTK3:gtk/gtkcairoblur.c
    constexpr double π = 3.14159265358979323846; // Source - https://stackoverflow.com/a/49778398
    // Gaussian scale factor (cappa)
    constexpr double κ = (3.0 * std::sqrt(2.0 * π) / 4.0);
    //
    double σ = r * κ * 0.5;
    double shadow_size = std::ceil(3.0 * σ + params.spread);
    LOGW("GTK Theme loaded OK shadow_size=", shadow_size, "(σ=", σ, ")", " off_x=", params.x, " off_y=", params.y, " spread=", params.spread);
    
    calculate_borders(settings.decoration.border, params);
    calculate_borders(settings.decoration_backdrop.border, params);
    // Calculate decoration margins
    auto& margins{ settings.margins };
    if (shadow_size < 1.0) shadow_size = 1.0; // increase 1px for rendering
    margins.left   = shadow_size - params.x + params.width.left;
    margins.right  = shadow_size + params.x + params.width.right;
    margins.bottom = shadow_size + params.y + params.width.bottom;
    margins.top    = shadow_size - params.y + params.width.top;

    LOGW(fmt_margins(margins, "Decoration margins"), fmt_margins(params.width," borders"));

    return true;
}

const char* format(button_type_t type)
{
    switch (type)
    {
      case button_type_t::close:    return "BUTTON_CLOSE";
      case button_type_t::maximize: return "BUTTON_MAXIMIZE";
      case button_type_t::restore:  return "BUTTON_RESTORE";
      case button_type_t::minimize: return "BUTTON_MINIMIZE";
      case button_type_t::menu:     return "BUTTON_MENU";
      default:                      return "UNKNOWN BUTTON";
    }
}

const char* format(button_state_t state)
{
    switch (state)
    {
      case button_state_t::normal:  return "normal";
      case button_state_t::hovered: return "hovered";
      case button_state_t::clicked: return "clicked";
      default:                      return "unknown";
    }
}

void print_width_info(const std::string& label, const css::sides_t& vec) {
    LOGW(label, ":"
             , " top=",    vec.top
             , " right=",  vec.right
             , " bottom=", vec.bottom
             , " left=",   vec.left);
}

void print_radius_info(const std::string& label, const css::corners_t& vec) {
    LOGW(label, ":"
              , " top_left=", vec.top_left
              , " top_right=", vec.top_right
              , " bottom_right=", vec.bottom_right
              , " bottom_left=", vec.bottom_left);
}

void print_color_info(const std::string& label, const color_t& color) {
    LOGW(label, " rgba("
              , (int)(color.r * 255.), "," 
              , (int)(color.g * 255.), "," 
              , (int)(color.b * 255.), ","
              ,       color.a, ")");
}

void print_align_info(const std::string& label, css::align_t align) {
	std::string align_text("none");
	switch (align) {
	case css::align_t::left:   align_text = "left";   break;
	case css::align_t::center: align_text = "center"; break;
	case css::align_t::right:  align_text = "right";  break;
	}
    LOGW(label, " :", align_text);
}


void print_background_info(const std::string& label, const css::background_t& bg)
{                              ;
    std::string dir_name = "none";
    if (bg.direction == css::gradient_direction_t::to_top)    dir_name = "to_top";
    if (bg.direction == css::gradient_direction_t::to_bottom) dir_name = "to_bottom";
    if (bg.direction == css::gradient_direction_t::to_left)   dir_name = "to_left";
    if (bg.direction == css::gradient_direction_t::to_right)  dir_name = "to_right";

    LOGW("  [", label, "]: With Gradient: ", (bg.with_gradient ? "YES" : "NO"),
        "\n\t\tFallback RGBA: (",
        (int)(bg.color.r * 255.), ", ", (int)(bg.color.g * 255.), ", ",
        (int)(bg.color.b * 255.), ", ", bg.color.a, ")",
        "\n\t\tDirection: ", dir_name,
        "\n\t\tColor Stops (", bg.stops.size(), "):");
    for (size_t i = 0; i < bg.stops.size(); ++i) {
        LOGW("    [", i, "] Offset: ", bg.stops[i].offset,
            " | RGBA: (",
            (int)(bg.stops[i].color.r * 255.), ", ",
            (int)(bg.stops[i].color.g * 255.), ", ",
            (int)(bg.stops[i].color.b * 255.), ", ",
            bg.stops[i].color.a, ")");
    }
}


// Helper to print shadow parse results to the console
void print_shadow_info(const std::string& label, const std::vector<css::shadow_t>& shadows) {
    LOGW("  [", label, "]: Found sub-shadows: ", shadows.size());
    for (size_t i = 0; i < shadows.size(); i++) {
        const auto& s = shadows[i];
        LOGW( "\n    Shadow #", i + 1, (s.is_inset ? " [Inner (inset)]" : " [Outer]"), "\n"
            , "      Offset X:    ", s.x_offset, "\n"
            , "      Offset Y:    ", s.y_offset, "\n"
            , "      Blur:   ", s.blur_radius, "\n"
            , "      Spread: ", s.spread_radius);
        print_color_info("      RGBA color:", s.color);
    }
}

void print_gtk_theme(const gtk_theme_t& s) {
    LOGW("=== GTK THEME CONFIGURATION ===");
    
    // [main]
    LOGW("[main]");
    LOGW("  theme-name: ", s.theme_name);
    LOGW("  font-name:  ", s.font_name);

    // [titlebar]
    LOGW("[titlebar]");
    LOGW("  font-family:   ", s.titlebar.font_family);
    LOGW("  font-size:     ", s.titlebar.font_size);
    LOGW("  font-weight:   ", s.titlebar.font_weight);
    LOGW("  button-layout: ", s.titlebar.button_layout);
    LOGW("  min-height:    ", s.titlebar.min_height);
    print_width_info("  padding", s.titlebar.padding);
    
    print_color_info("  color", s.titlebar.color);
    if (s.titlebar.background.with_gradient)
        print_background_info("  background", s.titlebar.background);
    else
        print_color_info("  background_color", s.titlebar.background.color);
    print_color_info("  border.color", s.titlebar.border.color);
    LOGW("  border.style: ", s.titlebar.border.style);
    print_width_info("  border.width", s.titlebar.border.width);
    print_radius_info("  border.radius", s.titlebar.border.radius);

    print_width_info("  titlebar.text-padding", s.titlebar.text_padding);
    print_align_info("  titlebar.text-align", s.titlebar.text_align);

    print_shadow_info("titlebar.box-shadow", s.titlebar.shadows);

    // [titlebar:backdrop]
    LOGW("[titlebar:backdrop]");
    print_color_info("  color", s.titlebar.color_backdrop);
    if (s.titlebar.background_backdrop.with_gradient)
        print_background_info("  background", s.titlebar.background_backdrop);
    else
        print_color_info("  background_color", s.titlebar.background_backdrop.color);
    print_color_info("  border.color", s.titlebar.border_backdrop.color);
    LOGW("  border.style: ", s.titlebar.border_backdrop.style);
    print_width_info("  border.width", s.titlebar.border_backdrop.width);
    print_radius_info("  border.radius", s.titlebar.border_backdrop.radius);

    // [decoration]
    LOGW("[decoration]");
    print_color_info("  border.color", s.decoration.border.color);
    print_width_info("  border.width", s.decoration.border.width);
    print_radius_info("  border.radius", s.decoration.border.radius);
    LOGW("  border.style: ", s.decoration.border.style);
    print_shadow_info("decoration.box-shadow", s.decoration.shadows);

    // [decoration:backdrop]
    LOGW("[decoration:backdrop]");
    print_color_info("  border.color", s.decoration_backdrop.border.color);
    print_width_info("  border.width", s.decoration_backdrop.border.width);
    print_radius_info("  border.radius", s.decoration_backdrop.border.radius);
    LOGW("  border.style: ", s.decoration_backdrop.border.style);
    print_shadow_info("decoration_backdrop.box-shadow", s.decoration_backdrop.shadows);

    // [icons]
    LOGW("[icons]");
    LOGW("  min-width: ", s.icons.min_width);
    LOGW("  spacing:   ", s.icons.spacing);
    print_width_info("  icons.padding", s.icons.padding);
    
    print_color_info("  color",        s.icons.color);
    print_color_info("  color:hover",  s.icons.color_hovered);
    print_color_info("  color:active", s.icons.color_clicked);
    print_color_info("  background-color",        s.icons.background_color);
    print_color_info("  background-color:hover",  s.icons.background_color_hovered);
    print_color_info("  background-color:active", s.icons.background_color_clicked);
    
    auto print_icon = [](const std::string& name, const icon_t& ic) {
        LOGW("  icon [", name, "]:");
        LOGW("    active_normal:    ", ic.active_normal);
        LOGW("    active_hovered:   ", ic.active_hovered);
        LOGW("    active_clicked:   ", ic.active_clicked);
        LOGW("    inactive_normal:  ", ic.inactive_normal);
        LOGW("    inactive_hovered: ", ic.inactive_hovered);
        LOGW("    inactive_clicked: ", ic.inactive_clicked);
    };

    print_icon("close", s.icons.close);
    print_icon("minimize", s.icons.minimize);
    print_icon("maximize", s.icons.maximize);
    print_icon("restore", s.icons.restore);

    LOGW("===============================");
}

};
