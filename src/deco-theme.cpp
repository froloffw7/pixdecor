#include <map>
#include <mutex>

#include <glib.h> // g_get_user_config_dir()

#include "deco-theme.hpp"

// #define __DEBUG_THEME__

namespace wf::pixdecor
{

pixdecor_theme_t theme;

void pixdecor_theme_t::apply_icon_fallback(icon_t &icon)
{
    // Active state fallbacks
    if (icon.active_hovered.empty()) {
        icon.active_hovered = icon.active_normal;
    }
    if (icon.active_clicked.empty()) {
        icon.active_clicked = icon.active_hovered;
    }
    // Inactive (backdrop) state fallbacks
    if (icon.inactive_normal.empty()) {
        icon.inactive_normal = icon.active_normal;
    }
    if (icon.inactive_hovered.empty()) {
        icon.inactive_hovered = icon.active_hovered;
    }
    if (icon.inactive_clicked.empty()) {
        icon.inactive_clicked = icon.active_clicked;
    }
}

void pixdecor_theme_t::update(bool subscribe)
{
    settings_t new_s;
    gtk_theme_t new_gtk_theme;
    //double shadow_size = 0;
    
    build_settings_from_options(new_s);
    if (new_s.overlay_engine == overlay_t::gtk_theme) {
        std::string theme_name{ new_s.theme_name }; // {"Arc-Dark"};
        if (theme_name.length() >= 2) {
            // strip matching quotes at the beginning and end
            const char q = theme_name.front();
            if ((q == '"' || q == '\'') && theme_name.back() == q)
                theme_name = theme_name.substr(1, theme_name.length() - 2);   // one layer, not recursive
        }
        const std::string theme_path = std::string(g_get_user_config_dir()) + "/wayfire/assets/" + theme_name;
#ifdef __DEBUG_THEME__
        LOGW("GTK Theme path: ", theme_path);
        // std::string settings_path{ theme_path + "/settings.ini" };
        const std::string settings_path = std::string("/tmp/wayfire/assets/")  + theme_name + "/settings.ini";
#else
        const std::string settings_path = theme_path + "/settings.ini";
#endif        
        if (load_gtk_theme(settings_path, new_gtk_theme)) {
            // TODO Titlebar shadow
            // new_s.titlebar_height = new_gtk_theme.titlebar.min_height;
            // font_manager_t::update_font(new_gtk_theme.titlebar);
        } else {
            LOGE("GTK Theme incorect. Check path:", theme_path, " Theme name:", theme_name);
            new_s.overlay_engine = overlay_t::none;
        }
    }
    if (new_s.overlay_engine != overlay_t::gtk_theme) {
        new_gtk_theme.titlebar.height         = new_s.titlebar_height;
        new_gtk_theme.titlebar.text_align     = new_s.titlebar_text_align;
        new_gtk_theme.titlebar.button_layout  = new_s.button_layout;
        new_gtk_theme.titlebar.color          = new_s.text_color;
        new_gtk_theme.titlebar.color_backdrop = new_s.text_color;
        new_gtk_theme.titlebar.background.with_gradient = false;
        new_gtk_theme.titlebar.background.color = new_s.background_text_color;
        new_gtk_theme.titlebar.background_backdrop.with_gradient = false;
        new_gtk_theme.titlebar.background_backdrop.color = new_s.background_text_color;
        // new_gtk_theme.shadow_pt.width        = new_s.border_size;
        new_gtk_theme.icons.minimize.active_normal  = new_s.button_minimize_image;
        new_gtk_theme.icons.minimize.active_hovered = new_s.button_minimize_hover_image;
        new_gtk_theme.icons.maximize.active_normal  = new_s.button_maximize_image;
        new_gtk_theme.icons.maximize.active_hovered = new_s.button_maximize_hover_image;
        new_gtk_theme.icons.close.active_normal     = new_s.button_close_image;
        new_gtk_theme.icons.close.active_hovered    = new_s.button_close_hover_image;
        new_gtk_theme.icons.restore.active_normal   = new_s.button_restore_image;
        if (new_gtk_theme.icons.restore.active_normal.empty())
            new_gtk_theme.icons.restore.active_normal = new_gtk_theme.icons.maximize.active_normal;
        new_gtk_theme.icons.restore.active_hovered  = new_s.button_restore_hover_image;
        if (new_gtk_theme.icons.restore.active_hovered.empty())
            new_gtk_theme.icons.restore.active_hovered = new_gtk_theme.icons.maximize.active_hovered;

        //shadow_size = new_s.shadow_size;
        // font_manager_t::update_font(new_s.titlebar_font);
    }
    /* input area for resizing */
    // new_s.input_size = std::min(std::max(new_s.input_size, MIN_RESIZE_SIZE), shadow_size);
    new_s.input_size = std::max(new_s.input_size, MIN_RESIZE_SIZE);

    apply_icon_fallback(new_gtk_theme.icons.minimize);
    apply_icon_fallback(new_gtk_theme.icons.maximize);
    apply_icon_fallback(new_gtk_theme.icons.restore);
    apply_icon_fallback(new_gtk_theme.icons.close);

    update(new_s, new_gtk_theme);
    
#ifdef __DEBUG_THEME__
    print_settings(settings);
    
    if (settings.overlay_engine == overlay_t::gtk_theme)
        print_gtk_theme(gtk_theme);
        
    LOGW("GTK Theme: ", is_gtk_overlay() ? new_s.theme_name : "NO");
#endif    
    if (subscribe) init_updates();
    
    emit_updated(resync_t::invalidate);
}

bool pixdecor_theme_t::is_titlebar_visible(bool is_maximized)
{
    if (overlay_engine == overlay_t::gtk_theme)
        return true;
    
    if (titlebar_opts == titlebar_opts_t::never)
        return false;

    if (titlebar_opts == titlebar_opts_t::always)
        return true;

    if (is_maximized) {
        if (titlebar_opts == titlebar_opts_t::windowed)
            return false;
    } else {
        if (titlebar_opts == titlebar_opts_t::maximized)
            return false;
    }
    return true;
}

double pixdecor_theme_t::get_titlebar_height(bool is_maximized)
{
    if (overlay_engine == overlay_t::gtk_theme) {
        return titlebar.height;
    }

    if ( (titlebar_opts == titlebar_opts_t::never) ||
        ((titlebar_opts == titlebar_opts_t::windowed) && is_maximized) ||
        ((titlebar_opts == titlebar_opts_t::maximized && !is_maximized)))
        return 0;

    // TODO titlebar.border.radius?
    return std::round(settings.titlebar_height);
}

/** @return The available border for resizing
 *  not used while overlay_t::gtk_theme 
 **/
double pixdecor_theme_t::get_border_size(bool is_maximized)
{
    if (is_maximized && !settings.maximized_borders)
        return 0;
    
    return settings.border_size;
}

double pixdecor_theme_t::get_shadow_size(bool is_maximized)
{
    if (settings.overlay_engine != overlay_t::gtk_theme) {
        return is_maximized
            ? (settings.maximized_shadows ? settings.shadow_size : 0)
            : settings.shadow_size;
    }
    return 0;
}

wf::decoration_margins_t pixdecor_theme_t::get_margins(bool is_maximized)
{
    wf::decoration_margins_t margins{0, 0, 0, 0};
    double border_size = get_border_size(is_maximized);

    if (overlay_engine == overlay_t::gtk_theme) {
        if (!is_maximized) {
            const auto& m{ gtk_theme.margins };
            margins = wf::decoration_margins_t {
                .left   = m.left,
                .right  = m.right,
                .bottom = m.bottom,
                .top    = m.top };
        }
    } else if (overlay_engine != overlay_t::none) {
        double shadow_size = get_shadow_size(is_maximized);
        margins = wf::decoration_margins_t {
            .left   = shadow_size + border_size,
            .right  = shadow_size + border_size,
            .bottom = shadow_size + border_size,
            .top    = shadow_size + border_size };
    } else {
        margins = wf::decoration_margins_t {
            .left   = border_size,
            .right  = border_size,
            .bottom = border_size,
            .top    = border_size };
    }
    // LOGW(fmt_margins(margins, "Theme margins"));
    return margins;
}


wf::color_t pixdecor_theme_t::decoration_color(bool active)
{
    return active ? settings.color : settings.background_color;
}


}
