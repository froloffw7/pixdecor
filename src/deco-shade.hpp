#pragma once

// #define __DEBUG_SHADE__

#include <wayfire/opengl.hpp>
#include <wayfire/util/duration.hpp>

namespace wf::pixdecor
{

//=====================================================================================
using deco_ptr_t = nonstd::observer_ptr<simple_decorator_t>;
/*********************************************************
 * Ownership (ALL FROM THE SOURCES)

    wayfire_view = wayfire_toplevel_view = observer_ptr
         — NON-OWNING. Copies are free; storing = dangling risk.
    Views: shared_ptr (tracking_allocator, enable_shared_from_this, create()).
    toplevel_t: shared_ptr in view; may outlive the view.
    get_all_views(): vector BY VALUE
        — snapshot of the list, does not hold objects. for_each invariant: fn does not destroy the view.
    damage() — virtual in view_interface_t: view->damage() == info.toplevel->damage() (same object, one override). Chain closed: links 1-3 verified against the source.
    Pattern: sync → observer; store → weak_ptr + lock(); own → shared.
 *****************************************************************************************/
// What is known about view: toplevel — always (otherwise there is no structure),
// deco — optional (CSD windows live without a decorator)
struct view_info_t
{
    wayfire_toplevel_view toplevel{};
    deco_ptr_t            deco{};

    explicit operator bool() const { return toplevel != nullptr; }  // valid window
    bool decorated() const         { return deco != nullptr; }      // SSD
    bool csd() const               { return toplevel && !deco; }    // CSD

    // Effective titlebar height: SSD — from the decorator, CSD — from the option.
    // Invariant: deco non-null => toplevel non-null, no check needed.
    int csd_titlebar_height() const
    {
        return deco ? 0 : theme_t::csd_titlebar_height;
    }
    static view_info_t get(const wayfire_view& view)
    {
        if (auto toplevel{ wf::toplevel_cast(view) })
            return { toplevel, toplevel->toplevel()->get_data<simple_decorator_t>() };
        return {};
    }
};

using namespace wf::scene;
using namespace wf::animation;

class progression_t : public duration_t
{
  public:
    using duration_t::duration_t;
    // The value is computed on every access:
    // transition = start + (end − start) × duration.get_plrogress()
    // Default initialization
    timed_transition_t transition{ *this };

    // Constructors for explicitly specifying ranges
    progression_t(std::shared_ptr<wf::config::option_t<wf::animation_description_t>> opt,
        double start, double end)
        : duration_t(opt), transition(*this, start, end)
    {}
    progression_t(const duration_t& other, double start, double end)
        : duration_t{ other }
        , transition(*this, start, end)
    {}
};

class pixdecor_shade_t : public wf::scene::view_2d_transformer_t
{
    wayfire_view             view;
    // API PROPERTY: a fresh progression before the first start() returns progress()==1.0
    // ("completed"), not 0.0. All progress readers before start (find_node_at,
    // progress_height) see a "fully collapsed" window. Accounted for in the constructor
    // (margins are consistent) and in the add/init_animation order (see create_shade_transformer).
    progression_t            progression{ ini::shade_animation, 0.0, 1.0 };
    wf::decoration_margins_t margins{};
    bool                     shaded = false; //

  public:

    pixdecor_shade_t(wayfire_view _view)
        : wf::scene::view_2d_transformer_t(_view)
        , view(_view)
    {
        if (const auto info{ view_info_t::get(view) })
            update_margins(info);
    }

    virtual ~pixdecor_shade_t() {}

    class simple_node_render_instance_t : public wf::scene::transformer_render_instance_t<transformer_base_node_t>
    {
        wf::signal::connection_t<node_damage_signal> on_surface_damage =
            [this] (node_damage_signal *data)
        {
            push_to_parent(data->region);
        };

        pixdecor_shade_t* self;
        damage_callback   push_to_parent;

      public:
        simple_node_render_instance_t(pixdecor_shade_t *_self, damage_callback _callback, wayfire_view view)
            : wf::scene::transformer_render_instance_t<transformer_base_node_t>
                (_self, _callback, view->get_output())
        {
            self = _self;
            push_to_parent = _callback;
            self->connect(&on_surface_damage);
        }

        ~simple_node_render_instance_t() {}

        void schedule_instructions(
            std::vector<render_instruction_t>& instructions,
            const wf::render_target_t& target, wf::regionf_t& damage)
        {
            // We want to render ourselves only, the node does not have children
            instructions.push_back(render_instruction_t{
                        .instance = this,
                        .target   = target,
                        .damage   = damage & self->get_bounding_box(),
                    });
        }
        // Two-pass variant. TODO - separate shader
        void render(const wf::scene::render_instruction_t& data)
        {
            wf::geometry_t geom;
            auto progress_height = self->progress_height(geom);

            gl_geometry dst_box = {
                .x1 = float(geom.x),
                .y1 = float(geom.y),
                .x2 = float(geom.x + geom.width),
                .y2 = float(geom.y + geom.height)};
#ifdef __DEBUG_SHADE_RENDER__
            LOGW(fmt_geom(geom, "src geom"), fmt_box(dst_box, " dst box"));
#endif
            auto top{ self->margins.top };
            auto height{ geom.height };
            geom.y      += top;
            geom.height  = progress_height;   // exact replica of the previous mutation
#ifdef __DEBUG_SHADE_RENDER__
            LOGW(fmt_geom(geom, " region geom"), ",height=", height, ",progress_height=", progress_height);
#endif
            auto src_tex = wf::scene::transformer_render_instance_t<transformer_base_node_t>::get_texture(1.0);
            auto region   = data.damage & geom;

            wf::gles::run_in_context([&]
            {
                wf::gles::bind_render_buffer(data.target);
                data.pass->custom_gles_subpass(data.target, [&]
                {
                    wf::gles::for_each_scissor_rect(data.target, region, [&] ()
                    {
                        OpenGL::render_transformed_texture(wf::gles_texture_t{ src_tex },
                            { .x1 = dst_box.x1,
                              .y1 = dst_box.y1 - float(height - progress_height),
                              .x2 = dst_box.x2,
                              .y2 = dst_box.y2 - float(height - progress_height) }, {},
                            wf::gles::render_target_orthographic_projection(data.target), glm::vec4(1.0), 0);
                    });
                });

                geom.y      -= top;
                geom.height  = top;
                region = data.damage & geom;

                data.pass->custom_gles_subpass(data.target, [&]
                {
                    wf::gles::for_each_scissor_rect(data.target, region, [&] ()
                    {
                        OpenGL::render_transformed_texture(wf::gles_texture_t{src_tex}, dst_box, {},
                            wf::gles::render_target_orthographic_projection(data.target), glm::vec4(1.0), 0);
                    });
                });

            });
        }
    };

    void update_margins(const view_info_t& info)
    {
        if (info.decorated())
        {
            /* SSD */
            margins = info.deco->get_margins(info.toplevel->toplevel()->pending());
        } else
        {
            /* CSD */
            auto bg = info.toplevel->get_surface_root_node()->get_bounding_box();
            auto vg = info.toplevel->get_geometry();
            margins = {
                .left  = vg.x - bg.x,
                .right = bg.width  - ((vg.x - bg.x) + vg.width),
                .bottom = bg.height - ((vg.y - bg.y) + vg.height),
                .top   = vg.y - bg.y + info.csd_titlebar_height() // +(!)
            };
        }
    }

    double progress_height(wf::geometry_t& bbox) const
    {
        bbox = get_children_bounding_box();
        const double p   = progression.progress();
        const double lim = margins.top + margins.bottom;
        //LOGW("progess =", p, ",lim=", lim);
        return bbox.height - p * (bbox.height - lim);
    }
    // THE ORDER IS INTENTIONAL: add_transformer comes BEFORE init_animation, so at the moment
    // of addition the scene hit-tests us with a fresh progression (==1.0) and sees an "already
    // collapsed" window: only the margins strip (titlebar + border) accepts input.
    // Cost: between "creation → end of animation" a scroll from the window body will not
    // find its target. Accepted. Unshading at this moment is done from the strip or via the toggle
    // (pixdecor_toggle_shade works on the active window, no pointer needed).
    std::optional<wf::scene::input_node_t> find_node_at(const wf::pointf_t& at) override
    {
        wf::geometry_t bbox;
        auto height = progress_height(bbox);
        // LOGW(fmt_point(at, "at"), fmt_geom(bbox, ",box"), ",progess height=", height);
        if (((at.y - bbox.y) > 0.0) && ((at.y - bbox.y) < height))
            return floating_inner_node_t::find_node_at(at);

        return {};
    }

    bool tick(const view_info_t& info)
    {
        // always update margins: they are read by render/find_node_at
        update_margins(info);
#ifdef __DEBUG_SHADE__
        LOGW(fmt_margins(margins, "tr tick margins"));
#endif
        if (!progression.running())
            return shaded;

        info.toplevel->damage();
        
        return true;
    }

    void gen_render_instances(std::vector<render_instance_uptr>& instances,
        damage_callback push_damage, wf::output_t *shown_on) override
    {
        instances.push_back(std::make_unique<simple_node_render_instance_t>(
            this, push_damage, view));
    }

    // shade_action_t action:
    // shade           = 0, // forward 0 -> 1
    // unshade         = 1, // reverse 1 -> 0
    void init_animation(shade_action_t action)
    {
        int current_dir = progression.get_direction();
        int dir_wanted  = action == shade_action_t::shade ? 1 : 0;
        bool is_running = progression.running();

        bool shade_state_wanted = action == shade_action_t::shade ? true : false;

#ifdef __DEBUG_SHADE__
        auto dir_str = [](int direction)
        {
            return direction != 0 ? "forward" : "reverse";

        };
        double progress = progression.progress();
        // Target boundary: 1.0 (forward) or 0.0 (backward)
        double target_limit = (dir_wanted != 0) ? 1.0 : 0.0;
        LOGW("init_animation: wanted=", dir_str(dir_wanted), ", current_dir=", dir_str(current_dir),
             ", progress=", progress, '(', progression.transition, ')',
             ", target=", target_limit, fmt_bool(is_running, ", running"));
#endif
        if (is_running)
        {
            // ← the target is fixed even while in flight,
            //   otherwise the flag goes stale after reverse()
            shaded = shade_state_wanted;
            // 1. If the animation is already running in the desired direction — let it continue
            // 2. If the animation is running but in the OPPOSITE direction — smoothly reverse it
            // The reverse() method correctly inverts the motion without resetting progress to 0
            if (current_dir != dir_wanted)
                progression.reverse();

            return;
        } else {
            if (shade_state_wanted != shaded) {
                // Start only if we were actually at rest
                shaded = shade_state_wanted;
                // If the current direction differs from the desired one, switch it before starting
                if (current_dir != dir_wanted)
                    progression.reverse();

                progression.start();
#ifdef __DEBUG_SHADE__
                LOGW("Animation started");
#endif
            }
        }
    }

    int get_direction()
    {
        return progression.get_direction();
    }
};

}
