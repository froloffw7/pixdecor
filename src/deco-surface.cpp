#include <wayfire/window-manager.hpp>
#include <wayfire/txn/transaction-manager.hpp>
#include <wayfire/scene-operations.hpp>

#include <linux/input-event-codes.h>

// #define __DEBUG_SURFACE__
// #define __DEBUG_SURFACE_RENDER__
// #define __DEBUG_SURFACE_SIGNALS__

#include "deco-theme.hpp"
#include "deco-layout.hpp"
#include "deco-shader.hpp"
#include "deco-surface.hpp"

namespace wf::pixdecor
{

void init_shade_transformer(const wayfire_view& view, shade_action_t action);
// void remove_shade_transformers();

/** Different types of window state */
enum class window_state_t: uint32_t
{
    none             = 0,
    active           = WIN_STATE_ACTIVE_BIT,
    maximized        = WIN_STATE_MAXIMIZED_BIT,
    active_maximized = WIN_STATE_ACTIVE_BIT | WIN_STATE_MAXIMIZED_BIT,
    minimized        = WIN_STATE_MINIMIZED_BIT,
    fullscreen       = WIN_STATE_FULLSCREEN_BIT,
};
// Allow bitwise AND for our enum class, operator &
constexpr window_state_t operator&(window_state_t a, window_state_t b) {
    return static_cast<window_state_t>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
}

constexpr window_state_t operator&(window_state_t a, uint32_t b) {
    return static_cast<window_state_t>(
        static_cast<uint32_t>(a) & b
    );
}

// Similarly, operator | (bitwise OR)
constexpr window_state_t operator|(window_state_t a, window_state_t b) {
    return static_cast<window_state_t >(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

constexpr window_state_t operator|(window_state_t a, uint32_t b) {
    return static_cast<window_state_t >(
        static_cast<uint32_t>(a) | b
    );
}
// Bitwise NOT (inversion)
constexpr window_state_t operator~(window_state_t a) {
    return static_cast<window_state_t>(~static_cast<uint32_t>(a));
}
// Operator |= (OR with assignment)
constexpr window_state_t& operator|=(window_state_t& a, window_state_t b) {
    a = a | b;
    return a;
}

constexpr window_state_t& operator|=(window_state_t& a, uint32_t b) {
    a = a | b;
    return a;
}
// Operator &= (AND with assignment)
constexpr window_state_t& operator&=(window_state_t& a, window_state_t b) { // or by value b
    a = a & b;
    return a;
}

constexpr window_state_t& operator&=(window_state_t& a, uint32_t b) {
    a = a & b;
    return a;
}

class simple_decoration_node_t : public wf::scene::node_t
                               , public wf::pointer_interaction_t
                               , public wf::touch_interaction_t
{
  private:
    std::weak_ptr<wf::toplevel_view_interface_t> _view;

    wf::signal::connection_t<wf::view_title_changed_signal> title_set =
        [=] (wf::view_title_changed_signal *ev)
    {
        if (auto view = _view.lock())
        {
            layout.set_title(view->get_title());
#ifdef __DEBUG_SURFACE_SIGNALS__
            LOGW("Title text changed: ", '"', view->get_title(), '"');
#endif
            // view->damage();
        }
    };

    pixdecor_layout_t           layout;
    window_state_t              state;

    wf::dimensionsf_t           size;

    double                      cached_titlebar_height;
    wf::decoration_margins_t    cached_margins;
    wf::geometry_t              cached_titlebar_geom;
    wf::geometry_t              cached_client_geom;
    uint32_t                    cached_tiled_edges;

    wf::regionf_t               cached_input_region;
    std::optional<wf::pointf_t> cursor_position;

    wf::wl_idle_call            idle_init;

    void sync_cache()
    {
        cached_titlebar_height = theme_t::get_titlebar_height(is_maximized());
        cached_margins  = theme_t::get_margins(is_maximized());
    }

  public:
    shader_t      shader;

    simple_decoration_node_t(wayfire_toplevel_view top_view)
        : node_t(false)
        , _view{top_view->weak_from_this()}
        , layout{ [=] (wf::geometry_t box)
            {
                wf::scene::damage_node(shared_from_this(), box);
            } }
        , state{window_state_t::none}
        , size{}
        , cached_titlebar_height{0.0}
        , cached_margins{}
        , cached_titlebar_geom{0,0,0,0}
        , cached_client_geom{0,0,0,0}
        , cached_tiled_edges{0}
    {
        sync_cache();
        // undecorate -> decorate drops title text
        // A direct call to layout.set_title() inside the constructor leads to a call to the λ
        // damage_callback, in title_t::set_text(), which uses shared_from_this(),
        // Calling shared_from_this() is forbidden until the object's constructor has fully
        // completed.
        // So we use a deferred call (idle)
        // to safely set the titlebar text.
        if (auto view = _view.lock())
        {
            std::string text = view->get_title(); 

            // 2. Add [this, text] to the lambda capture to pass the string inside
            idle_init.run_once([this, text] () {
                // Here it is safe to call shared_from_this() inside layout.set_title()
                layout.set_title(text);
            });
        }
        top_view->connect(&title_set);

        // make sure to hide frame if the view is fullscreen
        update_decoration_size();
        // instead of two assignments .x/x.y = FLT_MIN
        cursor_position.reset();
    }

    ~simple_decoration_node_t()
    {
        // remove_shade_transformers();
    }

    // Predicate functions for checking states
    constexpr bool is_active() const {
        return (state & window_state_t::active) == window_state_t::active;
    }

    constexpr bool is_minimized() const {
        return (state & window_state_t::minimized) == window_state_t::minimized;
    }

    constexpr bool is_maximized() const {
        return (state & window_state_t::maximized) == window_state_t::maximized;
    }

    constexpr bool is_fullscreen() const {
        return (state & window_state_t::fullscreen) == window_state_t::fullscreen;
    }

    // Functions for setting states
    void set_active(bool active) {
        if (active != is_active()) {
            if (active) state |= window_state_t::active;
            else        state &= ~window_state_t::active;

            if (auto view = _view.lock())
            {
                // node->update_colors(active);
                // layout.set_active(active);
                view->damage();
                // wf::scene::damage_node(this, get_bounding_box());
            }
        }
    }

    void set_minimized(bool minimized) {
        state &= ~WIN_STATE_EXCLUSIVE_MASK;
        if (minimized) {
            state |= window_state_t::minimized;
        }
    }

    void set_tiled(uint32_t edges) {
        // Safe check: is the view still alive
        if (auto view = _view.lock())
        {
            if (edges != cached_tiled_edges) {
                cached_tiled_edges = edges;
                // If at least one edge is snapped — set the maximized/tiled mask
                set_maximized(cached_tiled_edges != 0);
                //update_shadows_borders();
                cached_titlebar_height = theme_t::get_titlebar_height(is_maximized());
                cached_margins = theme_t::get_margins(is_maximized());
                layout.set_tiled_edges(cached_tiled_edges); // Update titlebar button geometry
            }
        }
    }

    void set_maximized(bool maximize) {
        state &= ~WIN_STATE_EXCLUSIVE_MASK;
        if (maximize) state |= window_state_t::maximized;
    }

    void set_fullscreen(bool fullscreen) {
        if (fullscreen != is_fullscreen()) {
            state &= ~WIN_STATE_EXCLUSIVE_MASK;
            if (fullscreen) {
                state |= window_state_t::fullscreen;
                cached_tiled_edges = 0;
                set_maximized(false);
                cached_titlebar_height = 0.0;
                cached_margins = wf::decoration_margins_t{};
            } else {
                cached_titlebar_height = theme_t::get_titlebar_height(is_maximized());
                cached_margins = theme_t::get_margins(is_maximized());
            }
            //update_shadows_borders();
            layout.set_fullscreen(fullscreen);
        }

    }

    double get_titlebar_height() const
    {
        return cached_titlebar_height;
    }

    const wf::decoration_margins_t& get_margins() const
    {
        return cached_margins;
    }

    const wf::geometry_t& get_client_geometry()
    {
        return cached_client_geom;
    }

    wf::pointf_t get_offset()
    {
        auto margins{ theme_t::get_margins(is_maximized()) };
        return wf::pointf_t{-margins.left, -(margins.top + theme_t::get_titlebar_height(is_maximized())) };
    }

    /* Whether the frame's damage intersects the area rectangle (local node coords).
     * pixman box: x1,y1 inclusive; x2,y2 exclusive. */
    static bool damage_intersects(const wf::regionf_t& damage, const wf::geometry_t& g)
    {
#if 0
        if ((g.width > 0) && (g.height > 0))
        {
            for (const auto& r : damage)
            {
                if ((g.x < r.x2) && (r.x1 < g.x + g.width) &&
                    (g.y < r.y2) && (r.y1 < g.y + g.height))
                {
                    return true;
                }
            }
        }
        return false;
#else
        return !(damage & g).empty();
#endif
    }

    void render(const wf::scene::render_instruction_t& data)
    {
        if (auto view = _view.lock()) {
            const wf::geometry_t geometry = wf::construct_box(get_offset(), size);
            float scale = 1.0f;
            if (auto output = view->get_output())
                scale = output->get_scale();

            const bool active = view->activated;
#ifdef __DEBUG_SURFACE_RENDER__
{
static int counter = 0;
//if (counter++ < 20) {
/*if (counter++ % 60 == 0)*/ {
    LOGW("node::render() #", counter, fmt_geom(geometry, " geometry"), " scale=", scale, fmt_bool(active, " active"));

    for (const auto& r : data.damage)
        LOGW("frame damage=", r.x1, "+", r.y1, ":", r.x2, "+", r.y2);}
counter++;
}
#endif
            auto renderables = layout.get_renderable_areas();

            wf::gles::run_in_context([&]
            {
                wf::gles::bind_render_buffer(data.target);
                // double, output coordinates
                const auto vg = view->get_geometry();
                const pointf_t pt { vg.x - geometry.x, vg.y - geometry.y };
                // Compute the physical position of the client area
                const double fx = std::floor(scale * pt.x); // + 0.5);
                const double fy = std::floor(scale * pt.y); // + 0.5);
                // Δ - for correction in the shader
                const wf::pointf_t micro_ofs{ pt.x - fx / scale, pt.y - fy / scale };
#ifdef __DEBUG_SURFACE_RENDER__
                LOGW(fmt_point(micro_ofs, " ==> micro_ofs"), fmt_geom(vg, " view "), fmt_point(pt, " client pt"),
                     " output scale=", scale);
#endif
                // scale = 8.0; // used for test shader positioning
                // → shader.render(data, geometry, ...)
                render_background(data, geometry - micro_ofs, scale, active);
                // render_background(data, geometry, scale, active);

                if (!theme_t::is_titlebar_visible(is_maximized()))  {
                    return;
                }
                /* Draw title & buttons */
                for (auto item : renderables)
                {
                    if (!damage_intersects(data.damage, item->get_geometry()))
                    {
                        continue;   // this frame does not touch the area
                    }
                    item->render(data, active);
                }
            });
        }
    }

    /**
     * Fill the given rectangle with the background color(s).
     *
     * @param fb The target framebuffer, must have been bound already
     * @param rectangle The rectangle to redraw.
     * @param scissor The GL scissor rectangle to use.
     * @param active Whether to use active or inactive colors
     * Don't use cursor position
     */
    void render_background(const wf::scene::render_instruction_t& data,
                           const wf::geometry_t& rectangle, float scale, bool active)
    {
        // 1) no engines — flat color, as now
        if ((theme_t::effect_type == effect_t::none) &&
            (theme_t::overlay_engine == overlay_t::none))
        {
            data.pass->custom_gles_subpass(data.target, [&] {
                wf::gles::for_each_scissor_rect(data.target, data.damage, [&] {
                    OpenGL::render_rectangle(rectangle, theme_t::decoration_color(active),
                        wf::gles::render_target_orthographic_projection(data.target));
                });
            });
            return;
        }
        // 2) texture update — by the theme engine
        if (theme_t::is_gtk_overlay()) {
            const auto tb = theme_t::get_titlebar_height(is_maximized());
            shader.update_gtk(data, rectangle, scale, active, tb, !is_maximized());
        } else {
            const auto tb     = theme_t::get_titlebar_height(is_maximized());
            const auto border = theme_t::get_border_size(is_maximized());
            const auto shadow = theme_t::get_shadow_size(is_maximized());
            shader.update_effects(data, rectangle, scale, active, cursor_position,
                                  tb, border, shadow);
        }
        
        // node::render()
        // 3) common texture output — identical for both engines
        shader.render(data, rectangle, scale);
    }

    std::optional<wf::scene::input_node_t> find_node_at(const wf::pointf_t& at) override
    {
        if (layout.contains(at))
        {
            return wf::scene::input_node_t{
                .node = this,
                .local_coords = at,    // absolute — the whole chain in one system
            };
        }

        return {};
    }

    pointer_interaction_t& pointer_interaction() override
    {
        return *this;
    }

    touch_interaction_t& touch_interaction() override
    {
        return *this;
    }

    class decoration_render_instance_t : public wf::scene::render_instance_t
    {
        simple_decoration_node_t*  node;
        wf::scene::damage_callback push_damage;

        wf::signal::connection_t<wf::scene::node_damage_signal> on_surface_damage =
            [=] (wf::scene::node_damage_signal *data)
        {
            push_damage(data->region);
        };

      public:
        decoration_render_instance_t(simple_decoration_node_t *_instance, wf::scene::damage_callback _callback)
        {
            node        = _instance;
            push_damage = _callback;
            node->connect(&on_surface_damage);
        }

        void schedule_instructions(std::vector<wf::scene::render_instruction_t>& instructions,
            const wf::render_target_t& target, wf::regionf_t& damage) override
        {
            // 1. Full node rectangle (including borders, titlebar, and shadow)
            const wf::geometry_t g{ node->get_bounding_box() };
#ifdef __DEBUG_SURFACE_RENDER__
            LOGW(fmt_geom(g, "node geom"), fmt_geom(node->get_client_geometry(), " client geom"));
#endif
            wf::regionf_t render_region(g);

            // 2. Subtract the client window area
            render_region ^= node->get_client_geometry();

            // 3. Shift to global scene coordinates
            // No further shift is needed given how the region is built
            // render_region += node->get_offset();

            // 4. Intersect with damage
            wf::regionf_t our_damage = damage & render_region;
            if (!our_damage.empty())
            {
                instructions.push_back(wf::scene::render_instruction_t{
                    .instance = this,
                    .target   = target,
                    .damage   = std::move(our_damage),
                });
            }
        }

        void render(const wf::scene::render_instruction_t& data) override
        {
#ifdef __DEBUG_SURFACE_RENDER__
{
static int c = 0;
// log rarely, but include damage size — "full frame" or "fragment":
if (c++ % 60 == 0) {
    int boxes = 0; double area = 0;
    for (auto& b : data.damage) { boxes++; area += (b.x2-b.x1)*(b.y2-b.y1); }
    LOGW("render() #", c, " damage: ", boxes, " boxes, area=", (int)area
        , fmt_geom(data.target.geometry, " target"));
}
}
#endif
            node->render(data);
        }
    };

    void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
        wf::scene::damage_callback push_damage, wf::output_t *output = nullptr) override
    {
        instances.push_back(std::make_unique<decoration_render_instance_t>(this, push_damage));
    }

    wf::geometry_t get_bounding_box() override
    {
        return wf::construct_box(get_offset(), size);
    }

    /* wf::compositor_surface_t implementation */
    void handle_pointer_enter(wf::pointf_t point) override
    {
        //point -= wf::pointf_t{get_offset()};
        layout.handle_motion(point.x, point.y);
    }

    void handle_pointer_leave() override
    {
        layout.handle_focus_lost();
        cursor_position.reset();
    }

    void handle_pointer_motion(wf::pointf_t to, uint32_t) override
    {
        //to -= wf::pointf_t{get_offset()};
        handle_action(layout.handle_motion(to.x, to.y));
        cursor_position = to;
    }

    void handle_pointer_button(const wlr_pointer_button_event& ev) override
    {
        if (ev.button != BTN_LEFT)
        {
            return;
        }

        handle_action(layout.handle_click_event(ev.state == WL_POINTER_BUTTON_STATE_PRESSED));
    }

    void handle_pointer_axis(const wlr_pointer_axis_event& ev) override
    {
        if (ev.orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
        {
            handle_action(layout.handle_axis_event(ev.delta));
        }
    }

    void handle_action(pixdecor_layout_t::action_response_t action)
    {
        if (auto view = _view.lock())
        {
            switch (action.action)
            {
              case layout_action_t::move:
                return wf::get_core().default_wm->move_request(view);

              case layout_action_t::resize:
                return wf::get_core().default_wm->resize_request(view, action.edges);

              case layout_action_t::close:
                return view->close();

              case layout_action_t::toggle_maximize:
                if (view->pending_tiled_edges())
                {
                    return wf::get_core().default_wm->tile_request(view, 0);
                } else
                {
                    return wf::get_core().default_wm->tile_request(view, wf::TILED_EDGES_ALL);
                }
                break;

              case layout_action_t::minimize:
                return wf::get_core().default_wm->minimize_request(view, true);
                break;

              case layout_action_t::shade:
#ifdef __DEBUG_SHADE__
                LOGW("layout_action_t::shade");
#endif
                init_shade_transformer(view, shade_action_t::shade);
                break;

              case layout_action_t::unshade:
#ifdef __DEBUG_SHADE__
                LOGW("layout_action_t::unshade");
#endif
                init_shade_transformer(view, shade_action_t::unshade);
                break;

              default:
                break;
            }
        }
    }

    void handle_touch_down(uint32_t time_ms, int finger_id, wf::pointf_t position) override
    {
        handle_touch_motion(time_ms, finger_id, position);
        handle_action(layout.handle_click_event());
    }

    void handle_touch_up(uint32_t time_ms, int finger_id, wf::pointf_t lift_off_position) override
    {
        handle_action(layout.handle_click_event(false));
        layout.handle_focus_lost();
    }

    void handle_touch_motion(uint32_t time_ms, int finger_id, wf::pointf_t position) override
    {
        //position -= wf::pointf_t{get_offset()};
        layout.handle_motion(position.x, position.y);
        cursor_position = position;
    }

    void recreate_frame()
    {
        sync_cache();

        update_decoration_size();

        // update_margins();

        if (auto view = _view.lock())
        {
            wf::geometry_t g(view->get_pending_geometry());

            dimensionsf_t dims(g.width, g.height);
            // cached_titlebar_height = theme_t::get_titlebar_height(is_maximized());

            if ((dims.width > 0) && (dims.height > 0)) {
                dims.width  -= (cached_margins.left + cached_margins.right);
                dims.height -= (cached_margins.top  + cached_margins.bottom
                              + cached_titlebar_height);
            }
#ifdef __DEBUG_SURFACE__
            LOGW(fmt_geom(g, "recreate_frame(): geom"), fmt_dims(dims, " layout size"));
#endif
            layout.resize(dims, cached_titlebar_height);
            cached_input_region = layout.calculate_region();
        }
    }

    bool size_changed(const wf::dimensionsf_t& new_dims)
    {
        double dx = new_dims.width  - size.width;
        double dy = new_dims.height - size.height;
        constexpr double ε = 0.01;
        // Euclidean distance (change radius):
        if (std::hypot(dx, dy) < ε) {
#ifdef __DEBUG_SURFACE__
            LOGW(fmt_dims(size, "Unchanged size"), " move only?");
#endif
            return false;
        }
        return true;
    }

    void update_decorations(const dimensionsf_t& dims)
    {
#ifdef __DEBUG_SURFACE__
        LOGW(fmt_dims(size, "Update decorations: old size"), fmt_dims(dims, " new size"),
            fmt_bool(is_maximized(), " maximized"), fmt_bool(is_fullscreen(), " fullscreen"));
#endif
        if (is_fullscreen()) {
            //if (auto view = _view.lock())
            //    update_shade_transformer(view);

            // In fullscreen mode there are no decorations — clear the input region
            cached_titlebar_geom = { 0, 0, 0, 0 };
            cached_client_geom.width  = dims.width;
            cached_client_geom.height = dims.height;
            layout.clear();
            cached_input_region.clear();
        } else {
            //if (auto view = _view.lock())
            //    update_shade_transformer(view);
            if (size_changed(dims)) {
                cached_titlebar_geom = { // Geometry relative to layout
                    .x      = 0,
                    .y      = -cached_titlebar_height,
                    .width  = dims.width,
                    .height = cached_titlebar_height };
                // Verify actual sizes before computing the client area
                if (dims.width > 0 && dims.height > 0) {
                    const wf::dimensionsf_t client_dims (
                        std::max(0.0, dims.width  - (cached_margins.left + cached_margins.right)),
                        std::max(0.0, dims.height - (cached_margins.top  + cached_margins.bottom + cached_titlebar_height)) );
                    cached_client_geom.width  = client_dims.width;
                    cached_client_geom.height = client_dims.height;
                    layout.resize(client_dims, cached_titlebar_height);
                    cached_input_region = layout.calculate_region();
                } else {
                    cached_client_geom.width  = 0;
                    cached_client_geom.height = 0;
                    layout.clear();
                    cached_input_region.clear();
                }
            }
        }
        size = dims;
    }

    void resize(const wf::dimensions_t& dims)
    {
        if (auto view = _view.lock())
        {
#if 0
            auto new_tiled_edges = view->pending_tiled_edges();
            // update_margins();

            if (new_tiled_edges != cached_tiled_edges) {
                cached_tiled_edges = new_tiled_edges;
                layout.set_tiled_edges(cached_tiled_edges);
                set_maximized(cached_tiled_edges != 0);
            }
            set_maximized(cached_tiled_edges != 0); // ??? Do we need this
#endif
            const wf::dimensionsf_t new_size(dims);
#if 0
            double dx = new_size.width  - size.width;
            double dy = new_size.height - size.height;
            constexpr double ε = 0.01;
            // Euclidean distance (change radius):
            if (std::hypot(dx, dy) < ε) {
                LOGW(fmt_dims(size, "resize(): size unchanged, move only"));
                return;
            }
#endif
            update_decorations(new_size);

            view->damage();
        }
    }

    void update_decoration_size()
    {
        if (auto view = _view.lock())
        {
            bool fullscreen = view->toplevel()->pending().fullscreen;
            auto edges = view->toplevel()->pending().tiled_edges;
            const wf::geometry_t& g(view->get_geometry());
#ifdef __DEBUG_SURFACE__
            LOGW(fmt_geom(g, "update_decoration_size(): geom"),
                " tiled egdes: ", edges_as_string(edges), fmt_bool(fullscreen, " fullscreen"));
#endif
            set_fullscreen(fullscreen);
            if (!fullscreen) {
                set_tiled(edges);
            }

            update_decorations({ g.width, g.height });

            view->damage();
        }
    }
};

simple_decorator_t::simple_decorator_t(wayfire_toplevel_view _view)
{
    view = _view;
    node = std::make_shared<simple_decoration_node_t>(view);
    node->resize(wf::dimensions(view->get_pending_geometry()));
    wf::scene::add_back(view->get_surface_root_node(), node);

    view->connect(&on_view_activated);
    view->connect(&on_view_geometry_changed);
    view->connect(&on_view_fullscreen);
    view->connect(&on_view_tiled);

    // this-> not required
    on_view_activated = [this] (auto)
    {
        const bool activated = view->activated;   // already the new state
#ifdef __DEBUG_SURFACE_SIGNALS__
        LOGW("activated: ", activated ? "focus GAINED" : "focus LOST");
#endif
        // backdrop transition: the texture must be recomputed (palette inside!)
    	node->set_active(activated);
        // wf::scene::damage_node(node, node->get_bounding_box());
    };

    on_view_geometry_changed = [this] (auto)
    {
        auto tiled_edges{ view->pending_tiled_edges() };
        const auto& g{ view->get_geometry() };
#ifdef __DEBUG_SURFACE_SIGNALS__
        LOGW(fmt_geom(g, "on changed geometry"), " tiled: ", edges_as_string(tiled_edges));
#endif
        node->set_tiled(tiled_edges);
        node->resize(wf::dimensions(g));
        wf::get_core().tx_manager->schedule_object(view->toplevel());
    };

    on_view_tiled = [this] (auto)
    {
        auto tiled_edges = view->pending_tiled_edges();
        const auto& g{ view->get_geometry() };
#ifdef __DEBUG_SURFACE_SIGNALS__
        LOGW("on tiled: ", edges_as_string(tiled_edges), fmt_geom(g));
#endif
        node->set_tiled(tiled_edges);
        node->resize(wf::dimensions(g));
        wf::get_core().tx_manager->schedule_object(view->toplevel());
    };

    on_view_fullscreen = [this] (auto)
    {
        const bool fullscreen = view->toplevel()->pending().fullscreen;   // already the new state
        const wf::geometry_t&g{ view->get_geometry() };
#ifdef __DEBUG_SURFACE_SIGNALS__
        LOGW("on fullscreen: ", fullscreen ? "FULLSCREEN" : "NORMAL", fmt_geom(g));
#endif
  	    node->set_fullscreen(fullscreen);
        if (!fullscreen)
        {
            node->resize(wf::dimensions(g));
            wf::get_core().tx_manager->schedule_object(view->toplevel());
        }
    };
}

simple_decorator_t::~simple_decorator_t()
{
    wf::scene::remove_child(node);
    node.reset();
}

void simple_decorator_t::recreate_frame()
{
    node->recreate_frame();
}

void simple_decorator_t::update_decoration_size()
{
    node->update_decoration_size();
}

void simple_decorator_t::update_colors()
{
    theme_t::update();
}

void simple_decorator_t::effect_changed()
{
    node->shader.effect_changed();
}

wf::decoration_margins_t simple_decorator_t::get_margins(const wf::toplevel_state_t& state)
{
    if (state.fullscreen)
    {
        return {0, 0, 0, 0};
    }
    auto top_height{ node->get_titlebar_height() };

    decoration_margins_t margins{ node->get_margins() };
    margins.top += top_height;

    return margins;
}

void simple_decorator_t::update_animation()
{
    const auto margins = get_margins(view->toplevel()->current());
    // decoration size is less than a pixel — nothing to animate
    // exit without waking up
    if ((margins.left < 1.0) && (margins.right  < 1.0) &&
        (margins.top  < 1.0) && (margins.bottom < 1.0))
        return;

    auto bbox    = node->get_bounding_box();

    wf::regionf_t region;
    region |= wf::geometry_t{bbox.x, bbox.y, bbox.width, margins.top};
    region |= wf::geometry_t{bbox.x, bbox.y, margins.left, bbox.height};
    region |= wf::geometry_t{bbox.x, bbox.y + bbox.height - margins.bottom, bbox.width, margins.bottom};
    region |= wf::geometry_t{bbox.x + bbox.width - margins.right, bbox.y, margins.right, bbox.height};
    wf::scene::damage_node(node, region);
#ifdef __DEBUG_SURFACE__
    LOGW("simple_decorator_t::update_animation()");
#endif
}

void simple_decorator_t::update_font()
{    
    // font_manager_t::reset();   // not GL, but keep symmetry with update()
    font_manager_t::update();
}

void simple_decorator_t::reset_buttons()
{
    button_textures_manager_t::invalidate();
}

}

