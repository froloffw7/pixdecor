#include <wayfire/plugin.hpp>
#include <wayfire/plugins/ipc/ipc-activator.hpp>
#include <wayfire/matcher.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/txn/transaction-manager.hpp>
#include <wayfire/render-manager.hpp>

#include "deco-surface.hpp"
#include "deco-theme.hpp"
#include "deco-shade.hpp"

// #define __DEBUG__PIXDECOR__

namespace wf::pixdecor
{

wf::view_matcher_t                                ignore_views{"pixdecor/ignore_views"};
wf::view_matcher_t                                always_decorate{"pixdecor/always_decorate"};
wf::ipc_activator_t                               pixdecor_toggle_shade{"pixdecor/shade_toggle"};
// wf::option_wrapper_t<wf::animation_description_t> shade_duration{"pixdecor/shade_duration"};
wf::option_wrapper_t<wf::keybinding_t>            shade_modifier{"pixdecor/shade_modifier"};

template<class F>
static void for_each_view(F&& fn)
{
    for (const auto& view : wf::get_core().get_all_views())
        fn(view);
}

template<class F>
// ***c++20*** requires std::invocable<F&, const view_info_t&>
// Invariant: fn does not destroy the view (close/unmap/destroy).
// Removing and attaching decorations is allowed: this is data on the toplevel.
// convention "fn(const view_info_t&)"
static void for_each_decorated_view(F&& fn)
{
    // lvalue argument → copy, rvalue → move == forwarding reference
    for_each_view([fn = std::forward<F>(fn)](const wayfire_view& view)
    {
        if (const view_info_t info = view_info_t::get(view); info.decorated())
            fn(info);
    });
}

static bool should_decorate_view(const wayfire_toplevel_view& view)
{
    return always_decorate.matches(view) ||
        (view->should_be_decorated() && !ignore_views.matches(view));
}

static void undecorate(const wayfire_toplevel_view& view)
{
    view->toplevel()->erase_data<simple_decorator_t>();
    auto& pending = view->toplevel()->pending();
    if (!pending.fullscreen && !pending.tiled_edges)
    {
        const wf::geometry_t& new_g{ wf::shrink_geometry_by_margins(
            pending.geometry, pending.margins) };
#ifdef __DEBUG__PIXDECOR__
        const wf::geometry_t& g{ pending.geometry };
        LOGW(fmt_geom(g, "undecorate(): old geometry"), fmt_geom(new_g, " new geometry"));
#endif
        pending.geometry = new_g;
    }

    pending.margins = {0, 0, 0, 0};
}

static void decorate(const wayfire_toplevel_view& view)
{
    auto toplevel{ view->toplevel() };

    if (!toplevel->get_data<simple_decorator_t>())
    {
        toplevel->store_data(std::make_unique<simple_decorator_t>(view));
    }
    auto deco{ toplevel->get_data<simple_decorator_t>() };
    auto& pending{ toplevel->pending() };
    pending.margins = deco->get_margins(pending);
#ifdef __DEBUG__PIXDECOR__
    const wf::decoration_margins_t& margins{ pending.margins };
    LOGW(fmt_margins(margins, "decorate(): margins"));
#endif
    if (!pending.fullscreen && !pending.tiled_edges)
    {
        const wf::geometry_t& new_g{ wf::expand_geometry_by_margins(
            pending.geometry, pending.margins) };
#ifdef __DEBUG__PIXDECOR__
        const wf::geometry_t& g{ pending.geometry };
        LOGW(fmt_geom(g, "old geometry"), fmt_geom(new_g, " new geometry"));
#endif
        pending.geometry = new_g;
    }
}

static void ensure_decoration(const wayfire_view& view)
{
    if (auto info{ view_info_t::get(view) }) {
        if (info.csd() && should_decorate_view(info.toplevel)) {
            decorate(info.toplevel);
            wf::get_core().tx_manager->schedule_object(info.toplevel->toplevel());
        } else if (info.decorated() && !should_decorate_view(info.toplevel)) {
            undecorate(info.toplevel);
            wf::get_core().tx_manager->schedule_object(info.toplevel->toplevel());
        }
    }
}

static auto update_colors = []
{
    for_each_decorated_view([](const view_info_t& info)
    {
        info.deco->update_colors();
        info.toplevel->damage();
    });
};


static bool needs_effect_tick()
{
    return theme_t::effect_animation_enabled ||
           (theme_t::effect_type == effect_t::smoke) ||
           (theme_t::effect_type == effect_t::ink);
}

static bool needs_hook()
{
    return theme_t::shade_enabled || needs_effect_tick();   // composition, not copy-paste
}

// Synchronization (on_theme_updated + option_changed_cb):
// registered == needed
static void sync_hook(wf::effect_hook_t& hook, bool& is_set)
{
    if (needs_hook() == is_set)
        return;                    // stable — including "needed and present" — do NOT touch

    for (auto& o : wf::get_core().output_layout->get_outputs())
    {
        if (is_set) o->render->rem_effect(&hook);
        else        o->render->add_effect(&hook, wf::OUTPUT_EFFECT_PRE);
    }
    is_set = !is_set; // entered only on mismatch — now they match
}

// Teardown (fini): remove unconditionally, regardless of "need"
static void remove_effect_hook(wf::effect_hook_t& hook, bool& is_set)
{
    if (!is_set) return;
    for (auto& o : wf::get_core().output_layout->get_outputs())
        o->render->rem_effect(&hook);
    is_set = false;
}

// pixdecor.cpp — A SINGLE dispatcher; actions belong to the plugin (the theme knows nothing about views)
static void apply(resync_t r)
{
    switch (r)
    {
    case resync_t::none:
        return;
    case resync_t::repaint:
        for_each_decorated_view([](const view_info_t& info)
        {
            info.toplevel->damage();
        });
        return;
    case resync_t::recreate_with_font:
        simple_decorator_t::update_font();
    case resync_t::recreate:
        for_each_decorated_view([](const view_info_t& info)
        {
            info.deco->recreate_frame();
            wf::get_core().tx_manager->schedule_object(info.toplevel->toplevel());
        });
        return;
    case resync_t::invalidate_icons:
        simple_decorator_t::reset_buttons();
        for_each_decorated_view([](const view_info_t& info)
        {
            info.deco->recreate_frame();
            wf::get_core().tx_manager->schedule_object(info.toplevel->toplevel());
        });    
    case resync_t::invalidate:
        simple_decorator_t::update_font();
        simple_decorator_t::reset_buttons();
        for_each_decorated_view([](const view_info_t& info)
        {
            info.deco->recreate_frame();
            undecorate(info.toplevel);
            decorate(info.toplevel);
            wf::get_core().tx_manager->schedule_object(info.toplevel->toplevel());            
        });
        return;
    }
}

auto create_shade_transformer(const wayfire_view& view) -> std::shared_ptr<pixdecor_shade_t>
{
    const auto node{ view->get_transformed_node() };
    const auto tr{ std::make_shared<pixdecor_shade_t>(view) };
    // std::shared_ptr<pixdecor_shade_t> tr{ std::make_shared<pixdecor_shade_t>(view, titlebar_height) };
    node->add_transformer(tr, wf::TRANSFORMER_2D, shade_transformer_name);

    return tr;
}

void rem_shade_transformer(const wayfire_view& view)
{
    const auto node{ view->get_transformed_node() };
    if (const auto tr{ node->get_transformer<pixdecor_shade_t>(shade_transformer_name) })
    {
        node->rem_transformer(tr);
        view->damage(); // jump "strip → full window": repaint the freed area
    }
        
}

auto get_shade_transformer(const wayfire_view& view) -> std::shared_ptr<pixdecor_shade_t>
{
    const auto node{ view->get_transformed_node() };
    const auto tr{ node->get_transformer<pixdecor_shade_t>(shade_transformer_name) };

    return tr;
}

// direction 1 - forward, 0 - reverse
void init_shade_transformer(const wayfire_view& view, shade_action_t action)
{
    if (!theme_t::shade_enabled || !view || !view->is_mapped())
        return;
#ifdef __DEBUG_SHADE__
    LOGW("init transformer: ", (action == shade_action_t::shade?"shade":"unshade"));
#endif
    if (action == shade_action_t::none)
        return;

    if (const auto tr{ get_shade_transformer(view) })
    {
        // If the transformer exists and was found — update and animate
        tr->init_animation(action);
        return;
    }
    // No transformer — nothing to unshade
    if (action != shade_action_t::shade)
        return;
    // No transformer and the window needs to be shaded — create it
    auto tr{ create_shade_transformer(view) };
    tr->init_animation(action);
}

void remove_shade_transformers()
{
    for_each_view([](const wayfire_view& view)
    {
        rem_shade_transformer(view);
    });
}

const char* effect_type_text(effect_t type);
const char* overlay_engine_text(overlay_t overlay);

class wayfire_pixdecor : public wf::plugin_interface_t
{
    wf::wl_idle_call  idle_update_views;
    wf::effect_hook_t pre_hook;
    bool hook_set = false;
    bool axis_set = false;

    wf::axis_callback shade_axis_cb;

    wf::signal::connection_t<wf::txn::new_transaction_signal> on_new_tx =
        [] (wf::txn::new_transaction_signal *ev)
    {
        // For each transaction, we need to consider what happens with participating views
        for (const auto& obj : ev->tx->get_objects())
        {
            if (auto toplevel{ std::dynamic_pointer_cast<wf::toplevel_t>(obj) })
            {
                // First check whether the toplevel already has decoration
                // In that case, we should just set the correct margins
                if (auto deco{ toplevel->get_data<simple_decorator_t>() })
                {
                    deco->update_decoration_size();
                    wf::decoration_margins_t margins = deco->get_margins(toplevel->pending());
#ifdef __DEBUG__PIXDECOR__
                    LOGW(fmt_margins(margins, "on_new_tx(): margins"));
#endif
                    toplevel->pending().margins = margins;
                    continue;
                }
                // Second case: the view is already mapped, or the transaction does not map it.
                // The view is not being decorated, so nothing to do here.
                if (toplevel->current().mapped || !toplevel->pending().mapped)
                {
                    continue;
                }
                // Third case: the transaction will map the toplevel.
                auto view = wf::find_view_for_toplevel(toplevel);
                wf::dassert(view != nullptr, "Mapping a toplevel means there must be a corresponding view!");
                if (should_decorate_view(view))
                {
                    decorate(view);
                }
            }
        }
    };

    wf::signal::connection_t<wf::view_decoration_state_updated_signal> on_decoration_state_changed =
        [] (wf::view_decoration_state_updated_signal *ev)
    {
        if (auto toplevel{ wf::toplevel_cast(ev->view) })
        {
            undecorate(toplevel);
        }
        ensure_decoration(ev->view);
    };

    // allows criteria containing maximized or floating check
    wf::signal::connection_t<wf::view_tiled_signal> on_view_tiled =
        [] (wf::view_tiled_signal *ev)
    {
        ensure_decoration(ev->view);
    };

    wf::signal::connection_t<wf::view_app_id_changed_signal> on_app_id_changed =
        [] (wf::view_app_id_changed_signal *ev)
    {
        ensure_decoration(ev->view);
    };

    wf::signal::connection_t<wf::view_title_changed_signal> on_title_changed =
        [] (wf::view_title_changed_signal *ev)
    {
        ensure_decoration(ev->view);
    };

    wf::signal::connection_t<wf::output_added_signal> on_output_added =
        [this] (wf::output_added_signal *ev)
    {
        if (hook_set)
        {
            ev->output->render->add_effect(&pre_hook, wf::OUTPUT_EFFECT_PRE);
        }
    };

    wf::signal::connection_t<wf::output_pre_remove_signal> on_output_pre_remove =
        [] (wf::output_pre_remove_signal *ev)
    {
        for_each_view([ev] (const wayfire_view& v)
        {
            // A shaded window is leaving a dying output — unshade it.
            // Handler context — not a transformer method, idle is not needed.
            if (v->get_output() == ev->output) // ONLY views of the dying output
                rem_shade_transformer(v);
        });
    };

    wf::signal::connection_t<wf::output_removed_signal> on_output_removed =
        [this] (wf::output_removed_signal *ev)
    {
        ev->output->render->rem_effect(&pre_hook);
    };

    wf::signal::connection_t<theme_updated_signal> on_theme_updated =
        [this] (theme_updated_signal *ev)      // ← member, initialized by lambda
    {
#ifdef __DEBUG__PIXDECOR__
        LOGW("Effect: ", effect_type_text(theme_t::effect_type));
        LOGW("Overlay: ", overlay_engine_text(theme_t::overlay_engine));
#endif        
        // Phase 0 — global infrastructure (everything is idempotent)
        sync_hook(pre_hook, hook_set);
        sync_shade();
        // Phase 1 — policy: who is decorated at all
        for_each_view(ensure_decoration);
        // Phase  2: policy + views
        apply(ev->resync);
        // Phase 3 — shaded windows: repaint
        for_each_view([] (const wayfire_view& v)
        {
            if (get_shade_transformer(v))
                v->damage();
        });
    };
    // member: sees shade_axis_cb and axis_set
    void sync_shade()
    {
        if (theme_t::shade_enabled == axis_set)
            return;
        if (theme_t::shade_enabled)
            wf::get_core().bindings->add_axis(shade_modifier, &shade_axis_cb);
        else {
            wf::get_core().bindings->rem_binding(&shade_axis_cb);

            for_each_view([] (const wayfire_view& v)
            { rem_shade_transformer(v); }); // plugin context — no idle, directly
        }
        axis_set = theme_t::shade_enabled;
    }

  public:

    void init() override
    {
        shade_axis_cb = [this] (wlr_pointer_axis_event *ev)
        {
            if (ev->orientation != WL_POINTER_AXIS_VERTICAL_SCROLL)
                return false;

            auto view{  wf::get_core().get_cursor_focus_view() };
#ifdef __DEBUG_SHADE__
            LOGW("shade_axis_cb delta=", ev->delta, " focus=", view ? "view" : "null");
#endif
            if (auto info{ view_info_t::get(view) })
            {
                shade_action_t action = shade_action_from_delta<shade_action_t>(ev->delta);
                init_shade_transformer(view, action);
                return true;
            }
            return false;    // ← fix for bug #4 from the analysis: scrolling over the desktop is no longer swallowed
        };

        pre_hook = [] ()
        {
            const bool tick_effects = needs_effect_tick();
            const auto effect_focused_only = theme_t::effect_animate_focused_only;
            
            for_each_view([tick_effects,effect_focused_only](const wayfire_view& view)
            {
                if (const view_info_t info = view_info_t::get(view))
                {
                    if (tick_effects && info.decorated() &&
                        (!effect_focused_only || info.toplevel->activated))
                        info.deco->update_animation();

                    const auto node{ view->get_transformed_node() };
                    if (auto tr{ node->get_transformer<pixdecor_shade_t>(shade_transformer_name) })
                    {
                        // margins + damage-if-running => true
                        // false => "unshaded and finished — remove
                        if (!tr->tick(info))
                        {
                            // plugin context — no idle
                            node->rem_transformer(tr);
                        }
                    }
                }
            });
        };

        auto& ol = wf::get_core().output_layout;
        ol->connect(&on_output_added);
        ol->connect(&on_output_pre_remove);
        ol->connect(&on_output_removed);
        wf::get_core().connect(&on_decoration_state_changed);
        wf::get_core().tx_manager->connect(&on_new_tx);
        wf::get_core().connect(&on_app_id_changed);
        wf::get_core().connect(&on_title_changed);
        wf::get_core().connect(&on_view_tiled);

        theme_t::connect(&on_theme_updated);
        // "starts EMPTY: the first update must produce a full resync"
        theme_t::update(true);

        pixdecor_toggle_shade.set_handler([this] (wf::output_t *output, const wayfire_view& view)
        {
            if (!theme_t::shade_enabled)
                return false;

            if (auto info{ view_info_t::get(view) })
            {
                shade_action_t action = shade_action_t::shade;
                if (auto tr{ get_shade_transformer(view) })
                {
                    int direction = tr->get_direction();
                    // invert the direction forward / reverse
                    action = direction != 0 ? shade_action_t::unshade : shade_action_t::shade;
                }
#ifdef __DEBUG_SHADE__
                LOGW("pixdecor_toggle_shade");
#endif
                init_shade_transformer(view, action);
                return true;
            }
            return false;
        });
    }

    void fini() override
    {
        for_each_view([](const wayfire_view& view)
        {
            if (const auto& toplevel{ wf::toplevel_cast(view) })
            {
                undecorate(toplevel);
                wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
            }
        });

        on_output_added.disconnect();
        on_output_pre_remove.disconnect();
        on_output_removed.disconnect();
        on_decoration_state_changed.disconnect();
        on_new_tx.disconnect();
        on_app_id_changed.disconnect();
        on_title_changed.disconnect();
        on_view_tiled.disconnect();
        on_theme_updated.disconnect();

        remove_effect_hook(pre_hook, hook_set);
        if (axis_set)
            wf::get_core().bindings->rem_binding(&shade_axis_cb);
        axis_set = false;

        remove_shade_transformers();
        // So that after main() the static destructor does not call
        // glDeleteTextures on a dead context.
        simple_decorator_t::reset_buttons();
        // Unmounting: release resources derived from the theme
        // (GL caches — while the context is alive). Call from plugin fini(), once.
        // theme_t::reset();
    }
};

}

DECLARE_WAYFIRE_PLUGIN(wf::pixdecor::wayfire_pixdecor);

