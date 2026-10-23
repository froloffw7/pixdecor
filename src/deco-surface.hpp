#pragma once

#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>

namespace wf::pixdecor
{

class simple_decoration_node_t;

/**
 * A decorator object attached as custom data to a toplevel object.
 */
class simple_decorator_t : public wf::custom_data_t
{
    wayfire_toplevel_view view;
    std::shared_ptr<simple_decoration_node_t> node;
    
    wf::signal::connection_t<wf::view_activated_state_signal>  on_view_activated;
    wf::signal::connection_t<wf::view_geometry_changed_signal> on_view_geometry_changed;
    wf::signal::connection_t<wf::view_fullscreen_signal>       on_view_fullscreen;
    wf::signal::connection_t<wf::view_tiled_signal>            on_view_tiled;

  public:
    simple_decorator_t(wayfire_toplevel_view view);
    ~simple_decorator_t();
    void                     update_colors();
    void                     recreate_frame();
    void                     update_decoration_size();
    void                     update_animation();
    void                     effect_changed();

    wf::decoration_margins_t get_margins(const wf::toplevel_state_t& state);

    static void              update_font();
    static void              reset_buttons();
};

}

