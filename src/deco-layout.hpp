#pragma once

#include <vector>
#include <wayfire/util.hpp>

#include "deco-cairo.hpp"

namespace wf::pixdecor
{

extern const std::string edges_as_string(uint32_t edges);
/**
 * Action which needs to be taken in response to an input event
 */
enum class layout_action_t
{
    none            = 0,
    /* Drag actions */
    move            = 1,
    resize          = 2,
    /* Button actions */
    close           = 3,
    toggle_maximize = 4,
    minimize        = 5,
    /* Axis actions */
    shade           = 6,
    unshade         = 7,
};

/**
 * Represents an area of the window parts which reacts to input events.
 */
struct area_t
{
  public:
    /**
     * Initialize a new decoration area with the given type and geometry
     */
    area_t(const wf::geometry_t& g) : geometry{ g } {}
    virtual ~area_t() = default;

    wf::geometry_t   get_geometry() const { return geometry; }
    virtual void     set_geometry(const wf::geometry_t& g) { geometry = g; }

    //virtual void   set_activited(bool active) { activated = active; }

    virtual const char*
                     get_cursor_name() const { return "default"; }
    // or comparison with AREA_MOVE
    virtual bool     can_move() const { return false; }
    // By default the area does NOT participate in resizing (returns empty geometry)
    virtual bool     can_resize() const { return false; }
    virtual uint32_t get_edges() const { return WLR_EDGE_NONE; }

    virtual bool     is_interactive() const { return false; }
    virtual void     set_hovered(bool hover) {}
    virtual void     set_clicked(bool click) {}

    virtual layout_action_t
                     get_click_action() { return layout_action_t::none; }

    virtual bool     is_renderable() const { return false; }
    virtual void     render(const wf::scene::render_instruction_t& data, bool win_active) {}

    virtual const char*
                     area_type() const { return "Base area"; }

  protected:
    wf::geometry_t  geometry;
    // updated in resize()
    wf::regionf_t   cached_region;
};

struct title_area_t : public area_t
{
  public:
    explicit    title_area_t(const wf::geometry_t& g, std::function<void(const wf::geometry_t&)> damage_cb);
    void        set_title_text(const std::string& title_text);
    void        set_titlebar_width(double width);
    bool        is_renderable() const { return true; }
    void        render(const wf::scene::render_instruction_t& data, bool win_active) override;

    const char* area_type() const { return "Title area"; }

  private:
    std::unique_ptr<title_t> title;
};

struct button_area_t : public area_t
{
  public:
    button_area_t(button_type_t btn_type, const wf::geometry_t& g, std::function<void(const wf::geometry_t&)> damage_cb);

    void        set_geometry(const wf::geometry_t& g) override;
    button_t&   get_button();

    bool        is_interactive() const { return true; }
    void        set_hovered(bool hover) override;
    void        set_clicked(bool click) override;
    virtual layout_action_t
                get_click_action();
    bool        is_renderable() const { return true; }
    // Here the method must be declared so it matches the implementation!
    void        render(const wf::scene::render_instruction_t& data, bool win_active) override;

    const char* area_type() const { return "Button area"; }

  private:
    std::unique_ptr<button_t> button;
};
// Support for padding around buttons (move_area_t / padding_area_t)
struct move_area_t : public area_t
{
  public:
    explicit move_area_t( const wf::geometry_t& g)
                        : area_t(g) {}

    bool         can_move() const override { return true; }

    const char*  area_type() const { return "Move area"; }
};

struct resize_area_t : public area_t
{
  public:
    resize_area_t( uint32_t _edges, const wf::geometry_t& g)
                 : area_t(g)
                 , edges(_edges) {}

    const char*  get_cursor_name() const override;

    bool         can_move() const override { return false; }
    bool         can_resize() const override { return true; }
    uint32_t     get_edges() const override { return edges; }

    const char*  area_type() const { return "Resize area"; }

  private:
    uint32_t edges;
};

// TODO
struct shadow_area_t : public area_t
{
};

class pixdecor_theme_t;
/**
 * Manages the layout of the decorations, i.e positioning of the title,
 * buttons, etc.
 *
 * Also dispatches the input events to the appropriate place.
 */
class pixdecor_layout_t
{
  public:
    /**
     * Create a new decoration layout for the given theme.
     * When the theme changes, the decoration layout needs to be created again.
     *
     * @param damage_callback The function to be called when a part of the
     * layout needs a repaint.
     */
    pixdecor_layout_t(std::function<void(wf::geometry_t)> damage_callback);
    ~pixdecor_layout_t();

    void clear();
    /** Regenerate layout using the new size */
    void resize(const dimensionsf_t& dims, double tilebar_height);
    /**
     * @return The decoration areas which need to be rendered, in top to bottom
     *  order.
     */
    std::vector<nonstd::observer_ptr<area_t>> get_renderable_areas();

    /** @return The combined region of all layout areas */
    wf::regionf_t calculate_region() const;

    /** Limit region to title bar area by region intersection
     *  @param region to limit by title area
     *  @return the intersection between region and the title bar area */
    wf::regionf_t limit_region(wf::regionf_t & region) const;

    struct action_response_t
    {
        layout_action_t action;
        /* For resizing action, determine the edges for resize request */
        uint32_t edges;
    };

    /** Handle motion event to (x, y) relative to the decoration */
    action_response_t handle_motion(int x, int y);

    /**
     * Handle press or release event.
     * @param pressed Whether the event is a press(true) or release(false)
     *  event.
     * @return The action which needs to be carried out in response to this
     *  event.
     */
    action_response_t handle_click_event(bool pressed = true);

    /**
     * Handle axis event.
     * @param delta The delta of the axis event denoting direction
     * @return The action which needs to be carried out in response to this
     *  event.
     */
    action_response_t handle_axis_event(int delta);

    /**
     * Handle focus lost event.
     */
    void handle_focus_lost();

    void set_tiled_edges(uint32_t edges) { cached_tiled_edges = edges; }

    void set_fullscreen(uint32_t edges) { cached_tiled_edges = edges; }

    // even simpler: contains without region allocations:
    bool contains(const wf::pointf_t& at) const;

    // 1. Store the text. If the area already exists — update it
    void set_title(const std::string& title_text)
    {
        // LOGW("New title text: ", title_text, " old: ", cached_text);
        cached_text = title_text;
        if (title_area) title_area->set_title_text(title_text);
    }

  private:
    std::function<void(wf::geometry_t)> damage_callback;

    std::vector<std::unique_ptr<area_t>> layout_areas;
    // Non-owning pointer (raw pointer) for fast access without searching the vector
    title_area_t* title_area = nullptr;

    uint32_t      cached_tiled_edges;
    std::string   cached_text;
    double        cached_titlebar_height;
    int           cached_input_size;

    bool          is_grabbed = false;
    /* Position where the grab has started */
    wf::point_t   grab_origin;
    /* Last position of the input */
    wf::point_t   current_input;
    /* double-click timer */
    wf::wl_timer<false> timer;
    bool double_click_at_release = false;

    /** Create buttons in the layout, and return their total geometry */
    const wf::geometry_t create_left_buttons(const std::vector<button_type_t>& buttons,
    	const dimensionsf_t& dims, const css::sides_t& padding, double spacing,
        const dimensionsf_t& btn_dims);
    const wf::geometry_t create_right_buttons(const std::vector<button_type_t>& buttons,
	    const dimensionsf_t& dims, const css::sides_t& padding, double spacing,
        const dimensionsf_t& btn_dims);

    /** Update the cursor based on @current_input */
    void update_cursor();
    /**
     * Find the layout area at the given coordinates, if any
     * @return The layout area or null on failure
     */
    nonstd::observer_ptr<area_t> find_area_at(wf::point_t point);
};

}

