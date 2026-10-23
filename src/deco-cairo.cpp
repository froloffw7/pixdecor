#include <wayfire/opengl.hpp>
#include <wayfire/plugins/common/cairo-util.hpp>

#include "deco-theme.hpp"
#include "deco-cairo.hpp"

// #define __DEBUG_CAIRO__
// #define __DEBUG_CAIRO_RENDER__

namespace wf::pixdecor
{

void title_t::set_text(const std::string& text) {
#ifdef __DEBUG_CAIRO__
    LOGW("New title text: ", text, " old: ", title_text);
#endif
    // Guard against redundant repaints if the app sends the same title
    if (text == title_text) return;

    // Update the text
    title_text = text;
    /*
     * Reset the texture (texture = {}):
     * 1. Creates a new "empty" texture (OpenGL ID = 0).
     * 2. Overwrites the current variable.
     * 3. On overwrite, the destructor of the old wf::owned_texture_t is called automatically.
     * 4. The destructor calls glDeleteTextures(), safely freeing video memory.
     * Result: no memory leaks, and the text is redrawn from scratch on the next frame.
     * On the next render() --> update_texture() call, a new one will be generated.
     */
    texture = {};

    // Notify Wayfire that the titlebar area needs to be repainted on screen
    if (damage_callback) {
        damage_callback();
    }
}

/**
 * Render the given text on a cairo_surface_t with the given size.
 * The caller is responsible for freeing the memory afterwards.
 */
cairo_surface_t* title_t::render_text()
{
    int px_w = px_round<int>(geometry.width  * scale);
    int px_h = px_round<int>(geometry.height * scale);

    if (px_w <= 0 || px_h <= 0) {
        return nullptr;
    }

    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, px_w, px_h);
    auto cr = cairo_create(surface);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Scale the context to work in logical pixels (geometry)
    if (scale > 0.0) {
        cairo_scale(cr, scale, scale);
    }

    PangoLayout* layout = pango_cairo_create_layout(cr);
    const auto font_desc = font_manager_t::get_font_description();
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, title_text.c_str(), title_text.size());
    // Ellipsis when width is exceeded
    pango_layout_set_width(layout, std::max(1.0, geometry.width) * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    // Text color depending on active state
    const color_t text_color{ active ? theme_t::titlebar.color : theme_t::titlebar.color_backdrop };
    cairo_set_source_rgba(cr, text_color.r, text_color.g, text_color.b, text_color.a);

    // Pango sizes in logical pixels
    int layout_w_pango, layout_h_pango;
    pango_layout_get_size(layout, &layout_w_pango, &layout_h_pango);
    double layout_w = (double)layout_w_pango / PANGO_SCALE;
    double layout_h = (double)layout_h_pango / PANGO_SCALE;

    double x = 0;

    const align_t text_align{ theme_t::titlebar.text_align };
    switch (text_align)
    {
    case align_t::left:
        x = 0;
        break;
    case align_t::center:
    default:
        // In title coordinates
        x = 0.5 * (total_width - layout_w) - geometry.x;
        break;
    case align_t::right:
        x = geometry.width - layout_w;
        break;
    }
#ifdef __DEBUG_CAIRO__
    LOGW(title_text, " | ", fmt_geom(geometry), " x=", x, " total_width=", total_width, " layout=", layout_w, ':', layout_h);
#endif
    if ((x + layout_w) > geometry.width)
        x = 0.5 * (geometry.width - layout_w);
    if (x < 0)
        x = 0;
    double y = 0.5 * (geometry.height - layout_h);

    cairo_translate(cr, x, y);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
    cairo_destroy(cr);

    return surface;
}

bool title_t::update_texture(const wf::geometry_t& geom,
                             double                target_scale,
                             bool                  window_active)
{
    int px_width  = px_round<int>(geom.width  * target_scale);
    int px_height = px_round<int>(geom.height * target_scale);

    if (px_width <= 0 || px_height <= 0) {
        texture = {};
        return false;
    }

    int old_px_width  = px_round<int>(geometry.width  * scale);
    int old_px_height = px_round<int>(geometry.height * scale);

    constexpr double ε = 0.001;

    if (px_width      == old_px_width &&
        px_height     == old_px_height &&
        window_active == active &&
        std::abs(target_scale - scale) < ε &&
        texture.get_texture() != 0)
    {
#ifdef __DEBUG_CAIRO__
        LOGW("Text texture unchanged");
#endif
        return true;
    }

    // Save the state
    geometry    = geom;
    scale       = target_scale;
    active      = window_active;

    // Call surface generation without parameters
    cairo_surface_t* surface = render_text();

    if (surface) {
        texture = wf::owned_texture_t{ surface };
        cairo_surface_destroy(surface);
#ifdef __DEBUG_CAIRO__
        LOGW("Text texture created");
#endif
        return true;
    } else {
        texture = {};
        return false;
    }
}


void title_t::render(const wf::scene::render_instruction_t& data,
                     const wf::geometry_t&                  geometry,
                     bool                                   window_active)
{
    if (!update_texture(geometry, data.target.scale, window_active)) {
        return;
    }

    OpenGL::render_texture(
        wf::gles_texture_t{ texture.get_texture() },
        data.target,
        geometry,
        glm::vec4(1.0f),
        OpenGL::RENDER_FLAG_CACHED);

    data.pass->custom_gles_subpass(data.target, [&] {
        wf::gles::for_each_scissor_rect(data.target, data.damage, [&] {
            OpenGL::draw_cached();
        });
    });

    OpenGL::clear_cached();
}

button_t::button_t(button_type_t btn_type, std::function<void()> damage_cb)
    : type(btn_type)
    , damage_callback(std::move(damage_cb))
{
    hover_animation.animate(0.0f, 0.0f);   // 1.0 = normal (instant, no animation)
    click_animation.animate(0.0f, 0.0f);   // 0.0 = not pressed
}
/*
 * === State management ===
 */
#if 0
void button_t::set_normal()
{
    if (is_clicked) {
        click_animation.animate(0.0f); // <-- Fixed the name
    } else if (is_hovered) {
        hover_animation.animate(0.0f); // <-- Fixed the name
    }
    is_clicked = false;
    is_hovered = false;
}
#endif
void button_t::set_hovered(bool hover)
{
    if (is_hovered == hover) return; // state did not change — do nothing

    is_hovered = hover;
    // If the button is held down, we don't touch the hover animation
    // (it is suppressed by the math in render)
    if (!is_clicked)
    {
        hover_animation.animate(is_hovered ? 1.0f : 0.0f);
    }

    add_idle_damage();
}
/**
 * Set whether the button is pressed or not.
 * Affects appearance.
 */
void button_t::set_clicked(bool clicked)
{
    if (is_clicked == clicked) return; // state did not change — do nothing

    is_clicked = clicked;

    // Start a smooth transition to clicked state (1.0) or back to 0.0
    click_animation.animate(is_clicked ? 1.0f : 0.0f); // <-- Fixed the name

    // IMPORTANT: When releasing a click, the hover must be restored to the correct state,
    // since during the click the mouse may have stayed on the button or left it
    if (!is_clicked)
    {
        hover_animation.animate(is_hovered ? 1.0f : 0.0f);
    }

    add_idle_damage();
}

// === Render ===
void button_t::render(const wf::scene::render_instruction_t& data,
                      wf::geometry_t geometry,
                      bool window_active)
{
    const auto& pair   = button_textures_manager_t::get_pair(type, data.target.scale);
    const auto& states = window_active ? pair.active : pair.inactive;

#ifdef __DEBUG_CAIRO_RENDER__
    LOGW("btn: hover=", (double)hover_animation, " click=", (double)click_animation);
#endif
    // 1. Animations and frame invalidation
    if (hover_animation.running() || click_animation.running())
    {
        add_idle_damage();
    }

    // Lambda to draw a single layer with a given opacity
    auto render_pass = [&](const wf::owned_texture_t& texture, float alpha)
    {
        if (alpha <= 0.001f) return;

        // Check whether there is any underlying texture object at all
        auto raw_tex = texture.get_texture();
        if (!raw_tex) {
            LOGE("Attempting to render an empty/null texture!");
            return;
        }
        OpenGL::render_texture(
            wf::gles_texture_t{ texture.get_texture() },
            data.target, geometry,
            {1.0f, 1.0f, 1.0f, alpha},
            OpenGL::RENDER_FLAG_CACHED
        );

        data.pass->custom_gles_subpass(data.target, [&] {
            wf::gles::for_each_scissor_rect(data.target, data.damage, [&] {
                OpenGL::draw_cached();
            });
        });

        OpenGL::clear_cached();
    };

/*  Truth table
┌───────────────────┬──────────┬──────────┬───────────┬───────────┬───────────┬───────────────┐
│ State             │    hover │    click │    normal │   hovered │   clicked │     On screen │
├───────────────────┼──────────┼──────────┼───────────┼───────────┼───────────┼───────────────┤
│ idle              │    **0** │        0 │         1 │         0 │         0 │      normal ✓ │
│ hover             │    **1** │        0 │         0 │         1 │         0 │     hovered ✓ │
│ click outside     │      0   │        1 │         0 │         0 │         1 │     clicked ✓ │
│ click under cursor│      1   │        1 │         0 │         0 │         1 │     clicked ✓ │
│ transitions       │ animation │ animation │ crossfade │ crossfade │ crossfade │ Σ weights = 1 ✓ │
└───────────────────┴──────────┴──────────┴───────────┴───────────┴───────────┴───────────────┘
*/
#if 0
    // 2. Compute layer opacity weights (0.0 .. 1.0)
    float click_val   = click_animation;
    float base_weight = 1.0f - click_val;

    float alpha_normal  = base_weight * (1.0f - (float)hover_animation);
    float alpha_hovered = base_weight * (float)hover_animation;
    float alpha_clicked = click_val;
    // before the icons, one draw:
    const double h = 1.0 - (double)hover_animation;          // 0 = normal, 1 = hovered
    const color_t bg = lerp(bg_normal, bg_hover, h);         // linear RGBA interpolation
    OpenGL::render_rectangle(button_geometry, to_wf_color(bg), projection);
#ifdef __DEBUG_CAIRO_RENDER__
    LOGW("Render button ", format(type), " normal=", alpha_normal, " hovered=", alpha_hovered, " clicked=", alpha_clicked);
#endif
    // 3. Sequential layer drawing
    render_pass(states.normal,  alpha_normal);
    render_pass(states.hovered, alpha_hovered);  // Check: states.hover or states.hovered
    render_pass(states.clicked, alpha_clicked);
#else
    const float h = (float)hover_animation;
    const float c = (float)click_animation;

    if (c < 0.99f)   // base layers until clicked covers everything
    {
        if (h < 0.99f) render_pass(states.normal,  1.0f); // bottom — always opaque
        if (h > 0.01f) render_pass(states.hovered, h);
    }
    if (c > 0.01f)
    {
        render_pass(states.clicked, c);                   // on top of everything
    }
#endif
}

void button_t::add_idle_damage()
{
    idle_damage.run_once([=] ()
    {
        damage_callback();
    });
}

// ============================================================================
// 1. Data structures (GPU VRAM textures)
// ============================================================================

// ============================================================================
// 2. Helper CPU/Cairo drawing (temporary surfaces) and upload
// ============================================================================
namespace cairo_helpers
{
struct surface_deleter_t {
    void operator()(cairo_surface_t* s) const {
        if (s) cairo_surface_destroy(s);
    }
};

using scoped_cairo_surface = std::unique_ptr<cairo_surface_t, surface_deleter_t>;

static void rounded_rect(cairo_t* cr, const color_t& color, const wf::geometry_t& geom, double radius)
{
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

    radius = std::min(radius, 0.5 * std::min(geom.width, geom.height)); // safety
    if (radius > 0.05 * std::min(geom.width, geom.height)) {
        constexpr double π = 3.14159265358979323846;

        cairo_new_sub_path(cr);
        cairo_arc(cr, geom.x + geom.width - radius, geom.y + radius,               radius, -π / 2, 0);
        cairo_arc(cr, geom.x + geom.width - radius, geom.y + geom.height - radius, radius, 0, π / 2);
        cairo_arc(cr, geom.x + radius,              geom.y + geom.height - radius, radius, π / 2, π);
        cairo_arc(cr, geom.x + radius,              geom.y + radius,               radius, π, 3 * π / 2);
        cairo_close_path(cr);
    } else {
        cairo_rectangle(cr, geom.x, geom.y, geom.width, geom.height);
    }
    cairo_fill(cr);
}

// Vector fallback: sizes and background — following the load_png recipe.
// A PNG icon and a fallback glyph MUST look identical:
// in the theme, close comes from PNG and menu from the fallback — their sizes match by construction.
static cairo_surface_t* draw_vector_icon(button_type_t button,
    const wf::dimensions_t& dims, double scale, double image_size,
    const color_t& glyph_color, double line_thickness, /* logical px */
    const color_t& bg_color)
{
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dims.width, dims.height);
    cairo_t* cr = cairo_create(surface);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    constexpr double ε = 0.001;
    // 1. Target glyph size — literally the load_png formula:
    //    declared image_size (0 = the whole box), clamped to the box
    const double box = std::min(dims.width, dims.height);
    double glyph = (image_size > ε) ? image_size * scale : box;
    glyph = std::min(glyph, box);

    // 3. Draw the glyph in a unit square [0..1],
    //    translate+scale centers it and stretches it to glyph —
    //    the same scheme load_png uses to place its source
    cairo_save(cr);
    cairo_translate(cr, 0.5 * (dims.width  - glyph),
                        0.5 * (dims.height - glyph));
    cairo_scale(cr, glyph, glyph);

    // 2. Background (load_png does not have it in its current form)
    if (bg_color.a > ε)
    {
        rounded_rect(cr, bg_color, {0, 0, 1.0, 1.0}, 0.5);
    }
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_source_rgba(cr, glyph_color.r, glyph_color.g,
                              glyph_color.b, glyph_color.a);
    // thickness is set in LOGICAL px -> physical -> normalized by CTM
    cairo_set_line_width(cr, line_thickness * scale / glyph);

    switch (button)
    {
      case button_type_t::close:
        cairo_move_to(cr, 0.25, 0.25);
        cairo_line_to(cr, 0.75, 0.75);
        cairo_move_to(cr, 0.75, 0.25);
        cairo_line_to(cr, 0.25, 0.75);
        cairo_stroke(cr);
        break;

      case button_type_t::minimize:
        cairo_move_to(cr, 0.25, 0.75);
        cairo_line_to(cr, 0.75, 0.75);
        cairo_stroke(cr);
        break;

      case button_type_t::maximize:
        cairo_rectangle(cr, 0.25, 0.25, 0.50, 0.50);
        cairo_stroke(cr);
        break;

      case button_type_t::restore:
        cairo_rectangle(cr, 0.375, 0.375, 0.25, 0.25);
        cairo_stroke(cr);
        break;

      case button_type_t::menu:                 // hamburger — TODO closed
        for (double y : {0.30, 0.50, 0.70})
        {
            cairo_move_to(cr, 0.25, y);
            cairo_line_to(cr, 0.75, y);
        }
        cairo_stroke(cr);
        break;
    }

    cairo_restore(cr);
    cairo_destroy(cr);
    return surface;
}

static cairo_surface_t* load_png(const std::string& path,
    const wf::dimensions_t& dims, double output_scale, double image_size)

{
    if (path.empty()) return nullptr;

    // 1. Load the source PNG (16x16/32x32) from disk
    cairo_surface_t* src = cairo_image_surface_create_from_png(path.c_str());
    if (cairo_surface_status(src) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(src);
        return nullptr;
    }

    const double src_width  = static_cast<double>(cairo_image_surface_get_width(src));
    const double src_height = static_cast<double>(cairo_image_surface_get_height(src));
    if ((src_width <= 0) || (src_height <= 0))
    {
        cairo_surface_destroy(src);
        return nullptr;    // same path as a corrupt PNG -> vector fallback
    }
    // 2. Create a transparent target of the given dimensions
    cairo_surface_t* dest = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dims.width, dims.height);
    cairo_t* cr = cairo_create(dest);

    // Fully clear the target surface to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    // Restore the standard layer compositing mode
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Foolproof limit in ANY mode: no larger than the box
    const double box_scale = std::min(dims.width  / src_width,
                                      dims.height / src_height);
    // Explicit size, if specified:
    const double want_scale = (image_size > 0)
        ? std::min(image_size * output_scale / src_width,
                   image_size * output_scale / src_height)
        : box_scale;
    const double s = std::min(want_scale, box_scale);

    double offset_x = 0.5 * (dims.width  - src_width  * s);
    double offset_y = 0.5 * (dims.height - src_height * s);
#if 0
    constexpr double ε = 0.001;
    if (bg_color.a > ε)                       // idle transparent — skipped automatically
    {
        // PNG size
        const wf::geometry_t g { offset_x, offset_y, src_width, src_height };
        // Full size
        const wf::geometry_t g { 0, 0, static_cast<double>(dims.width), static_cast<double>(dims.height) };
        rounded_rect(cr, bg_color, g, radius * output_scale);
    }
#endif
    cairo_save(cr);
    cairo_translate(cr, offset_x, offset_y);
    cairo_scale(cr, s, s);
    cairo_set_source_surface(cr, src, 0, 0);                           // source
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST); // filter — ALWAYS AFTER the source
    cairo_paint(cr);
    cairo_restore(cr);
    // Free the memory
    cairo_destroy(cr);
    cairo_surface_destroy(src);
    // Return the finished surface of the given dimensions with the icon composited in
    return dest;
}

} // namespace cairo_helpers

// ============================================================================
// 3. Texture manager
// ============================================================================
// ─── deco-cairo.cpp ─────────────────────────────────────────────
wf::owned_texture_t button_textures_manager_t::load_texture(button_type_t type,
    button_state_t state, bool window_active, double scale,
    const wf::dimensions_t& size)
{
    const auto& icons{ theme_t::icons };
    const auto& icon{ icons.icon(type) };
    const auto& path{ icon.path(window_active, state) };

    cairo_helpers::scoped_cairo_surface surf{
        cairo_helpers::load_png(path, size, scale, icons.image_size) };

    if (!surf)   // PNG failed — vector fallback, colors for this state
    {
        LOGW(format(type), " fallback: no surface for path: ", path);

        const auto [glyph, bg] = icons.fallback_colors(state);   // lazy: only when needed
        surf.reset(cairo_helpers::draw_vector_icon(type, size, scale,
            icons.image_size, glyph, icons.line_thickness, bg));
    }

    wf::owned_texture_t texture{ surf.get() };   // pixels are copied in the ctor
    if (!texture.get_texture())
        LOGE("Failed to create owned_texture from surface for path: ", path);
    return texture;
}

button_pair_t button_textures_manager_t::load_pair(button_type_t type,
    double scale, wf::dimensions_t size)
{
    // 1. Load the active button state
    button_set_t active{
        load_texture(type, button_state_t::normal,  true, scale, size),
        load_texture(type, button_state_t::hovered, true, scale, size),
        load_texture(type, button_state_t::clicked, true, scale, size),
    };

    // 2. Load the inactive button state
    button_set_t inactive{
        load_texture(type, button_state_t::normal,  false, scale, size),
        load_texture(type, button_state_t::hovered, false, scale, size),
        load_texture(type, button_state_t::clicked, false, scale, size),
    };
    // Return the finished pair
    return button_pair_t{
        std::move(active),
        std::move(inactive) };
}

const button_textures_t& button_textures_manager_t::ensure(double scale)
{
    const int key = key_from_scale(scale);     // quantization — ONLY here
    auto it = cache_textures.find(key);
    if (it == cache_textures.end())
    {
        const auto& icons{ theme_t::icons };
        const double button_size{ icons.min_width }; // Validated at load time

        button_textures_t set;
        set.size = wf::dimensions_t{
            (int)std::lround(button_size * scale),
            (int)std::lround(button_size * scale) };

        set.close    = load_pair(button_type_t::close,    scale, set.size);
        set.minimize = load_pair(button_type_t::minimize, scale, set.size);
        set.maximize = load_pair(button_type_t::maximize, scale, set.size);
        set.restore  = load_pair(button_type_t::restore,  scale, set.size);
        set.menu     = load_pair(button_type_t::menu,     scale, set.size);
        // miss ⇒ the key definitely does not exist
        // emplace instead of insert_or_assign here is semantics, not style:
        // single-threaded "miss ⇒ no key" is guaranteed
        it = cache_textures.emplace(key, std::move(set)).first;
    }
    return it->second;
}

// Pure public method: give me a pair of textures for a given button type
const button_pair_t& button_textures_manager_t::get_pair(button_type_t type, double scale)
{
    const auto& set{ ensure(scale) };
    switch (type)
    {
        case button_type_t::close:    return set.close;
        case button_type_t::maximize: return set.maximize;
        case button_type_t::restore:  return set.restore;
        case button_type_t::minimize: return set.minimize;
        case button_type_t::menu:     return set.menu;
        default:                      break;
    }
    // Log a warning, but do not crash or freeze rendering
    LOGD("Attempting to get texture of non-existent button type!");
    static const button_pair_t dummy{};
    return dummy;
}

// --- DIAGNOSTICS ---
static void debug_pango_font(const PangoFontDescription* font_desc)
{
    PangoFontMap*  map = pango_cairo_font_map_get_default();
    PangoContext* ctx = pango_font_map_create_context(map);
    LOGI("font parsed='", pango_font_description_to_string(font_desc), "'");
    if (PangoFont* f = pango_context_load_font(ctx, font_desc))
    {
        PangoFontDescription* real = pango_font_describe(f);
        LOGI("font: resolved='", pango_font_description_to_string(real), "'");
        pango_font_description_free(real);
        g_object_unref(f);
    }
    g_object_unref(ctx);
}

// font-weight: normal | bold | 100..900. NOT a length — unitless is legal here.
static bool parse_font_weight(const std::string& s, int& weight)
{
    std::string token = css::trim(s);
    std::transform(token.begin(), token.end(), token.begin(), ::tolower);

    if (css::is_number(token)) {
        int v = 0;
        css::parse(token, v);
        if (v < 100 || v > 900)
            return false; // reject bolder/lighter: they are relative
        weight = v;
        return true;
    } else {
        const std::unordered_map<std::string_view, int> weight_map = {
            { "thin",      100 },
            { "extralight",200 },
            { "light",     300 },
            { "regular",   400 },
            { "normal",    400 },
            { "medium",    500 },
            { "semibold",  600 },
            { "bold",      700 },
            { "extrabold", 800 },
            { "black",     900 },
        };
        auto it = weight_map.find(token);
        if (it == weight_map.end()) {
            return false; // Unknown token — error
        }
        weight = it->second;
        return true;
    }
}

void font_manager_t::update_font(const titlebar_t& titlebar)
{
    font_description.reset(pango_font_description_new());
    if (!titlebar.font_family.empty()) {
        // 1. Family — as is (for now; see the fc-match check below)
        pango_font_description_set_family(font_description.get(),
            titlebar.font_family.c_str());
    } else {
        pango_font_description_set_family(font_description.get(), "Sans");
        LOGW("Theme font not set. Sans will be used");
    }
    // 2. Size — through our strict parser, full round trip:
    //    "12pt" -> 16px -> back to points (96 dpi) -> native Pango semantics
    if (!titlebar.font_size.empty()) {
        double font_size;
        if (css::parse(titlebar.font_size, font_size)) // 12pt -> 16px
        {
            pango_font_description_set_size(font_description.get(),
                font_size * 72.0 / 96.0 * PANGO_SCALE);
        } else {
            LOGW("Pango font: bad size", titlebar.font_size);
        }
    }
    if (!titlebar.font_weight.empty()) {
        int font_weight;
        if (parse_font_weight(titlebar.font_weight, font_weight)) {
            pango_font_description_set_weight(font_description.get(),
                (PangoWeight)font_weight);
        } else {
            LOGW("Pango font: bad weight", titlebar.font_weight);
        }
    }
    debug_pango_font(font_description.get());
}

void font_manager_t::update_font(const std::string& font_str)
{
    // Other shader/effect or fallback
    font_description.reset(pango_font_description_from_string(font_str.c_str()));
    // Keep the diagnostics — they are cheap and just paid off
    debug_pango_font(font_description.get());
}

/** @return The available height for displaying the title */
/** @return The height of the system font in pixels */
double font_manager_t::get_font_height()
{
    // If the font has not been initialized yet — load it on the fly
    if (!font_description) {
        LOGE("Pango font description is not created.");
        return 0.0;
    }

    const auto font_desc = font_description.get();

    // Extra safety: pango itself can also return an empty/invalid object
    if (!font_desc) {
        LOGE("Pango font description is null.");
        return 0.0;
    }

    double font_height = pango_font_description_get_size(font_desc);

    if (!pango_font_description_get_size_is_absolute(font_desc))
    {
        font_height *= 96;
        font_height /= 72;
    }

    return font_height / PANGO_SCALE;
}

} // namespace wf::pixdecor

