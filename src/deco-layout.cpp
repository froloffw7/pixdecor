#include <wayfire/core.hpp>
#include <wayfire/nonstd/reverse.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/toplevel.hpp>
#include <wayfire/util.hpp>

#include "deco-settings.hpp"
#include "deco-theme.hpp"
#include "deco-layout.hpp"

// #define __DEBUG_LAYOUT__

namespace wf::pixdecor
{

const std::string edges_as_string(uint32_t edges)
{
    std::string text;

    auto add_edge = [&text](const char* edge_text) {
        if (!text.empty()) text += '|';
        text += edge_text;
    };

    if (edges & WLR_EDGE_TOP)    add_edge("WLR_EDGE_TOP");
    if (edges & WLR_EDGE_BOTTOM) add_edge("WLR_EDGE_BOTTOM");
    if (edges & WLR_EDGE_LEFT)   add_edge("WLR_EDGE_LEFT");
    if (edges & WLR_EDGE_RIGHT)  add_edge("WLR_EDGE_RIGHT");

    if (text.empty()) {
        text = "WLR_EDGE_NONE";
    }
    return text;
}

title_area_t::title_area_t( const wf::geometry_t& g, std::function<void(const wf::geometry_t&)> damage_cb)
                          : area_t(g)
{
    title = std::make_unique<title_t>(std::bind(damage_cb, geometry));
}

void title_area_t::set_title_text(const std::string& title_text)
{
    title->set_text(title_text);
}

void title_area_t::set_titlebar_width(double width)
{
    title->set_total_width(width);
}

void title_area_t::render(const wf::scene::render_instruction_t& data, bool window_active)
{
    title->render(data, geometry, window_active);
}

// Implementation of the button_area_t constructor (fixed syntax and class name)
button_area_t::button_area_t(button_type_t btn_type, const wf::geometry_t& g, std::function<void(const wf::geometry_t&)> damage_cb)
             : area_t({g}) // Base constructor receives the type and empty geometry
{
    // Pass geometry at creation time
    // std::bind "bakes" geometry g into the parent damage_callback,
    // turning its signature from void(wf::geometry_t) into parameterless void().
    // When button_t calls damage_callback() without arguments, std::bind
    // automatically substitutes 'g' and passes it up to the parent handler.
    button = std::make_unique<button_t>(btn_type, std::bind(damage_cb, geometry));
	/* Alternative variant, collapsing the signature with reference passing
	 * button = std::make_unique<button_t>(
	 *     std::bind(damage_callback, std::ref(geometry)));
	 */
}

void button_area_t::set_geometry(const wf::geometry_t& g)
{
    // Override set_geometry: change only the position, do not touch the size!
    // geometry.width and geometry.height remain what set_type set
    geometry.x = g.x;
    geometry.y = g.y;
}

// Implementation in .cpp:
void button_area_t::set_hovered(bool hover)
{
    if (button) {
        button->set_hovered(hover);
    }
}

void button_area_t::set_clicked(bool click)
{
    if (button) {
        button->set_clicked(click);
    }
}

layout_action_t button_area_t::get_click_action()
{
    if (button) {
        switch (button->get_type())
        {
          case button_type_t::close:
            return layout_action_t::close;

          case button_type_t::maximize:
          case button_type_t::restore:
            return layout_action_t::toggle_maximize;

          case button_type_t::minimize:
            return layout_action_t::minimize;

          default:
            break;
        }
    }
    return layout_action_t::none;
}

void button_area_t::render(const wf::scene::render_instruction_t& data, bool window_active)
{
    button->render(data, geometry, window_active);
}

button_t& button_area_t::get_button()
{
    assert(button);

    return *button;
}

const char* resize_area_t::get_cursor_name() const
{
    return wlr_xcursor_get_resize_name(static_cast<wlr_edges>(edges));
}

pixdecor_layout_t::pixdecor_layout_t(std::function<void(wf::geometry_t)> damage_cb)
                 : damage_callback(damage_cb)
                 , cached_tiled_edges(0)
{}

pixdecor_layout_t::~pixdecor_layout_t()
{
    clear();
}

void pixdecor_layout_t::clear()
{
    title_area = nullptr; // Protection against accessing a deleted object
    layout_areas.clear();
}

bool parse_button_layout(const std::string& button_layout, bool maximized,
	std::vector<button_type_t>& buttons_left,
	std::vector<button_type_t>& buttons_right)
{
    buttons_left.clear();
    buttons_right.clear();

    // Find the separator between the left and right parts
    auto colon_pos = button_layout.find(':');

    // If there is no colon at all, by default everything goes left (or right, but traditionally left)
    std::string_view left_part;
    std::string_view right_part;

    if (colon_pos != std::string::npos) {
        left_part = std::string_view{button_layout.data(), colon_pos};
        right_part = std::string_view{button_layout.data() + colon_pos + 1, button_layout.size() - colon_pos - 1};
    } else {
        left_part = button_layout;
    }

    const std::unordered_map<std::string_view, button_type_t> button_map = {
        { "minimize",    button_type_t::minimize },
        { "maximize",    maximized ? button_type_t::restore : button_type_t::maximize },
        { "close",       button_type_t::close },
        { "menu",        button_type_t::menu },
        { "application", button_type_t::menu }, // Support application as a synonym for menu
    };

    std::string_view delimiters{" ,"};

    auto parse_tokens = [&](std::string_view part, std::vector<button_type_t>& target_vector) -> bool {
        size_t start = part.find_first_not_of(delimiters);
        while (start != std::string_view::npos) {
            size_t end = part.find_first_of(delimiters, start);
            std::string_view token = part.substr(start, end - start);

            if (!token.empty()) {
                auto it = button_map.find(token);
                if (it == button_map.end()) {
                    return false; // Unknown token — error
                }
                target_vector.push_back(it->second);
            }
            start = part.find_first_not_of(delimiters, end);
        }
        return true;
    };

    if (!parse_tokens(left_part, buttons_left) ||
		!parse_tokens(right_part, buttons_right)) {
        buttons_left.clear();
        buttons_right.clear();
        return false;
	}
    return true;
}

const wf::geometry_t pixdecor_layout_t::create_left_buttons(const std::vector<button_type_t>& buttons,
	const dimensionsf_t& dims, const css::sides_t& padding, double spacing, const dimensionsf_t& btn_dims)
{
	(void)dims;
    double x_offset{ padding.left };
    if (buttons.empty()) {
        return wf::geometry_t{x_offset, 0, 0, 0};
    }
    // double titlebar_height{ theme_t::get_titlebar_height(false) };
	double titlebar_height{ cached_titlebar_height };
	double area_height{ titlebar_height - padding.top - padding.bottom };
	// assert (area_height > 0)
    auto y_offset  = padding.top + 0.5 * (area_height - btn_dims.height);

    wf::geometry_t geometry {
        .x = x_offset,
		.y = -titlebar_height + y_offset, // within window width, outside window height
        .width  = btn_dims.width, .height = btn_dims.height };

    double total_width = 0;
    // Left buttons need no reverse, feed the stream straight into enumerate
    for (const auto& type : buttons)
    {
        auto button_area  = std::make_unique<button_area_t>(type, geometry, damage_callback);
        layout_areas.push_back(std::move(button_area));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(geometry, std::string("Button(left) ") + format(type) + " click area"));
#endif
        double step{ btn_dims.width };
        // Compare memory addresses: if this is not the last button in the vector, add padding
        if (&buttons.back() != &type) {
            step += spacing;
        }
        total_width += step;
        geometry.x  += step;
    }
	geometry = wf::geometry_t {
        .x      = x_offset,
        .y      = -titlebar_height + y_offset,
        .width  = total_width,
        .height = area_height };
#ifdef __DEBUG_LAYOUT__
	LOGW(fmt_geom(geometry, "Left buttons area geom="), " total_width=", total_width);
#endif
	return geometry;
}

const wf::geometry_t pixdecor_layout_t::create_right_buttons(const std::vector<button_type_t>& buttons,
	const dimensionsf_t& dims, const css::sides_t& padding, double spacing, const dimensionsf_t& btn_dims)
{
    double x_offset{ dims.width - padding.right };
    if (buttons.empty()) {
        return wf::geometry_t{x_offset, 0, 0, 0};
    }
	double titlebar_height{ cached_titlebar_height };
	double area_height{ titlebar_height - padding.top - padding.bottom };
	// assert (area_height > 0)
    auto y_offset = padding.top + 0.5 * (area_height - btn_dims.height);
#ifdef __DEBUG_LAYOUT__
	LOGW(fmt_margins(padding, "padding"), " titlebar_height=", titlebar_height, " area_height=", area_height, " y_offset=", y_offset);
#endif
    wf::geometry_t geometry {
        .x =  x_offset - btn_dims.width,
		.y = -titlebar_height + y_offset, // within window width, outside window height
        .width  = btn_dims.width, .height = btn_dims.height };

    double total_width = 0;
    // for (auto type : wf::reverse(buttons))
    // views::reverse reverses the vector, and views::enumerate gives a pair [index, element]
    for (const auto& type : wf::reverse(buttons))
    {
        auto button_area  = std::make_unique<button_area_t>(type, geometry, damage_callback);
        layout_areas.push_back(std::move(button_area));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(geometry, std::string("Button(right) ") + format(type) + " click area"));
#endif
        double step{ btn_dims.width };
        // Compare memory addresses:
        // Add padding for the next (more left) button,
        // but only if this is not the last element in the reverse
        if (&buttons.front() != &type) {
			step += spacing;
		}
        total_width += step;
        if (&buttons.front() != &type) {
	        geometry.x  -= step;
		}
    }
	geometry = wf::geometry_t {
        .x      = geometry.x,
        .y      = -titlebar_height + y_offset,
        .width  = total_width,
        .height = area_height };
#ifdef __DEBUG_LAYOUT__
	LOGW(fmt_geom(geometry, "Right buttons area geom="), " total_width=", total_width);
#endif
	return geometry;
}

/** Regenerate layout using the new size */
void pixdecor_layout_t::resize(const dimensionsf_t& dims, double titlebar_height)
{
#ifdef __DEBUG_LAYOUT__
    LOGW(fmt_dims(dims, "New layout size"));
#endif
    clear();
    // No client area
    if (!((dims.width > 0) && (dims.height > 0)))
    {
        return;
    }
    if (titlebar_height > 0)
    {
		cached_titlebar_height = titlebar_height;
        const wf::geometry_t move_geom{
            .x      = 0,
            .y      = -titlebar_height,
            .width  = dims.width,
            .height = titlebar_height };

        /* Move area: whole titlebar */
        layout_areas.push_back(std::make_unique<move_area_t>(move_geom));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(move_geom, "Move area (titlebar)"));
#endif
		std::vector<button_type_t> buttons_left, buttons_right;

		auto   padding{ theme_t::titlebar.padding };
		double start_x{ 0.0 };
        double end_x{ 0.0 };
		if (parse_button_layout(theme_t::button_layout,
				cached_tiled_edges != WLR_EDGE_NONE, buttons_left, buttons_right)) {
			const auto& icons{ theme_t::icons };
			const double spacing{ icons.spacing };
			const wf::dimensionsf_t button_dims{ icons.min_width, icons.min_width };

			auto button_left_area_geometry  = create_left_buttons(
				buttons_left, dims, padding, spacing, button_dims);
#ifdef __DEBUG_LAYOUT__
			LOGW(fmt_geom(button_left_area_geometry, "Left buttons area"));
#endif
			auto button_right_area_geometry = create_right_buttons(
				buttons_right, dims, padding, spacing, button_dims);
#ifdef __DEBUG_LAYOUT__
			LOGW(fmt_geom(button_right_area_geometry, "Right buttons area"));
#endif
			start_x = button_left_area_geometry.x + button_left_area_geometry.width;
			end_x   = button_right_area_geometry.x;
		}
        /* Title render area */
		auto text_padding{ theme_t::titlebar.text_padding };
        wf::geometry_t geometry = {
            .x      = start_x + text_padding.left,
            .y      = -titlebar_height + padding.top + text_padding.top,
            .width  = std::max<double>(1, end_x - start_x - text_padding.left - text_padding.right),
            .height = titlebar_height - padding.top - text_padding.top - text_padding.bottom - padding.bottom,
        };
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(geometry, "Title render area"));
#endif
		auto ta = std::make_unique<title_area_t>(geometry, damage_callback);
		// Save a raw pointer to the titlebar area (for setting the titlebar text)
		title_area = ta.get();
		title_area->set_titlebar_width(dims.width);
		// Apply the cached title
		title_area->set_title_text(cached_text);
		// 6. Hand ownership to the vector
		layout_areas.push_back(std::move(ta));
    }

    constexpr uint32_t TILED_EDGES_ALL =
        WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT;

    // Determine the resizable edges taking maximization and tiling settings into account
    uint32_t edges = theme_t::maximized_borders ? TILED_EDGES_ALL
												: (cached_tiled_edges ^ TILED_EDGES_ALL);

    auto bandwidth{ theme_t::input_size };
    wf::geometry_t input_geometry;
    /* 1. Corner resize zones (push BEFORE the straight strips for priority in find_area_at) */
    // Top-Left corner
    if ((edges & (WLR_EDGE_TOP | WLR_EDGE_LEFT)) == (WLR_EDGE_TOP | WLR_EDGE_LEFT))
    {
        input_geometry = {
            .x      = -bandwidth,
            .y      = -titlebar_height - bandwidth,
            .width  = bandwidth,
            .height = bandwidth };
        layout_areas.push_back(std::make_unique<resize_area_t>(
            static_cast<wlr_edges>(WLR_EDGE_TOP | WLR_EDGE_LEFT), input_geometry));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(input_geometry, "Top-Left corner resize area"));
#endif
    }

    // Top-Right corner
    if ((edges & (WLR_EDGE_TOP | WLR_EDGE_RIGHT)) == (WLR_EDGE_TOP | WLR_EDGE_RIGHT))
    {
        input_geometry = {
            .x      = dims.width,
            .y      = -titlebar_height - bandwidth,
            .width  = bandwidth,
            .height = bandwidth };
        layout_areas.push_back(std::make_unique<resize_area_t>(
            static_cast<wlr_edges>(WLR_EDGE_TOP | WLR_EDGE_RIGHT), input_geometry));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(input_geometry, "Top-Right corner resize area"));
#endif
    }
    // Bottom-Left corner
    if ((edges & (WLR_EDGE_BOTTOM | WLR_EDGE_LEFT)) == (WLR_EDGE_BOTTOM | WLR_EDGE_LEFT))
    {
        input_geometry = {
            .x      = -bandwidth,
            .y      = dims.height,
            .width  = bandwidth,
            .height = bandwidth };
        layout_areas.push_back(std::make_unique<resize_area_t>(
            static_cast<wlr_edges>(WLR_EDGE_BOTTOM | WLR_EDGE_LEFT), input_geometry));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(input_geometry, "Bottom-Left corner resize area"));
#endif
    }
    // Bottom-Right corner
    if ((edges & (WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT)) == (WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT))
    {
        input_geometry = {
            .x      = dims.width,
            .y      = dims.height,
            .width  = bandwidth,
            .height = bandwidth };
        layout_areas.push_back(std::make_unique<resize_area_t>(
            static_cast<wlr_edges>(WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT), input_geometry));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(input_geometry, "Bottom-Right corner resize area"));
#endif
    }
    /* 2. Straight resize zones (sides) */
    // Top edge
    if (edges & WLR_EDGE_TOP)
    {
        input_geometry = {
            .x      = -bandwidth,
            .y      = -titlebar_height - bandwidth,
            .width  = bandwidth + dims.width + bandwidth,
            .height = bandwidth };
        layout_areas.push_back(std::make_unique<resize_area_t>(
            WLR_EDGE_TOP, input_geometry));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(input_geometry, "Top resize area"));
#endif
    }
    // Bottom edge
    if (edges & WLR_EDGE_BOTTOM)
    {
        input_geometry = {
            .x      = -bandwidth,
            .y      = dims.height,
            .width  = bandwidth + dims.width + bandwidth,
            .height = bandwidth };
        layout_areas.push_back(std::make_unique<resize_area_t>(
            WLR_EDGE_BOTTOM, input_geometry));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(input_geometry, "Bottom resize area"));
#endif
    }
    // Left edge
    if (edges & WLR_EDGE_LEFT)
    {
        input_geometry = {
            .x      = -bandwidth,
            .y      = -titlebar_height - bandwidth,
            .width  = bandwidth,
            .height = bandwidth + titlebar_height + dims.height + bandwidth };
        layout_areas.push_back(std::make_unique<resize_area_t>(
            WLR_EDGE_LEFT, input_geometry));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(input_geometry, "Left resize area"));
#endif
    }
    // Right edge
    if (edges & WLR_EDGE_RIGHT)
    {
        input_geometry = {
            .x      = dims.width,
            .y      = -titlebar_height - bandwidth,
            .width  = bandwidth,
            .height = bandwidth + titlebar_height + dims.height + bandwidth };
        layout_areas.push_back(std::make_unique<resize_area_t>(
            WLR_EDGE_RIGHT, input_geometry));
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(input_geometry, "Border(right) resize area"));
#endif
    }
    if (theme_t::is_rounded_corners_overlay()) // TODO
    {
#if 0
        /* Shadow - top */
        border_geometry = {0, 0, w, radius * 2};
        layout_areas.push_back(std::make_unique<area_t>(
            AREA_SHADOW, border_geometry));

        /* Shadow - bottom */
        border_geometry = {0, h - radius * 2, w, radius * 2};
        layout_areas.push_back(std::make_unique<area_t>(
            AREA_SHADOW, border_geometry));

        /* Shadow - left */
        border_geometry = {0, radius * 2, radius * 2, h - radius * 4};
        layout_areas.push_back(std::make_unique<area_t>(
            AREA_SHADOW, border_geometry));

        /* Shadow - right */
        border_geometry = {w - radius * 2, radius * 2, radius * 2, h - radius * 4};
        layout_areas.push_back(std::make_unique<area_t>(
            AREA_SHADOW, border_geometry));
#endif
    }
}

/**
 * @return The decoration areas which need to be rendered, in top to bottom
 *  order.
 */
std::vector<nonstd::observer_ptr<area_t>> pixdecor_layout_t::get_renderable_areas()
{
    std::vector<nonstd::observer_ptr<area_t>> renderable;
    for (auto& area : layout_areas)
    {
        if (area->is_renderable())
        {
            renderable.push_back({area});
        }
    }
    return renderable;
}

wf::regionf_t pixdecor_layout_t::calculate_region() const
{
    wf::regionf_t r{};

    for (auto& area : layout_areas)
    {
        auto   g = area->get_geometry();

        if ((g.width > 0) && (g.height > 0))
        {
            r |= g;
        }
#ifdef __DEBUG_LAYOUT__
        LOGW(fmt_geom(g, "Layout region geometry"));
#endif
    }
    return r;
}

/** Handle motion event to (x, y) relative to the decoration */
pixdecor_layout_t::action_response_t pixdecor_layout_t::handle_motion(int x, int y)
{
	auto previous_area = find_area_at(current_input);
	auto current_area  = find_area_at({x, y});
	auto origin_area   = find_area_at(grab_origin);

	/* Move — only if the grab STARTED on a move area.
	 * Clicking a button + dragging on the titlebar must not move the window. */
	if (current_area && (current_area == previous_area) && is_grabbed &&
		current_area->can_move() && origin_area && origin_area->can_move())
	{
		is_grabbed = false;
		double_click_at_release = false; // drag kills the double-click gesture
		return { layout_action_t::move, 0 };
	} else
	{
		if (current_area != previous_area)
		{
			if (previous_area)
				previous_area->set_hovered(false);
			if (current_area)
				current_area->set_hovered(true);
		}
		/* While the mouse button is held: the pressed look lives on the button,
		 * WHERE the press started, and only while the cursor is over it.
		 * Left — released (the animation reverses), returned — pressed again. */
		if (is_grabbed && origin_area)
		{
			origin_area->set_clicked(current_area == origin_area);
		}
	}
	current_input = {x, y};
	update_cursor();
	return { layout_action_t::none, 0 };
}

/**
 * Handle press or release event.
 * @param pressed Whether the event is a press(true) or release(false)
 *  event.
 * @return The action which needs to be carried out in response to this
 *  event.
 * */
pixdecor_layout_t::action_response_t pixdecor_layout_t::handle_click_event(bool pressed)
{
    action_response_t response{ layout_action_t::none, 0 };

    if (pressed)
    {
        auto area = find_area_at(current_input);
        if (area) {
            if (area->can_move())
            {
                if (timer.is_connected())
                {
                    double_click_at_release = true;
                    timer.disconnect();
                } else
                {
                    timer.set_timeout(300, [] () { return false; });
                }
            }

            if (area->can_resize())
			{
                /* gesture handed to core — do not arm the grab (see above) */
                return { layout_action_t::resize, area->get_edges() };
			}
            area->set_clicked(true);
        }
        is_grabbed  = true;
        grab_origin = current_input;
    } else
    {
        if (is_grabbed)
        {
            is_grabbed = false;                     // ↑ always: release closes the grab
            auto begin_area = find_area_at(grab_origin);

            if (begin_area)
            {
                begin_area->set_clicked(false);     // clear the pressed state — before all exits
            }

            if (double_click_at_release)
            {
                double_click_at_release = false;    // consumed unconditionally

                if (begin_area && begin_area->can_move())
                {
                    return { layout_action_t::toggle_maximize, 0 };
                }
                /* stale — fall through to the normal branch */
            }

            auto end_area = find_area_at(current_input);   // needed only here
            if (begin_area && end_area && (end_area == begin_area))
            {
                response.action = begin_area->get_click_action();
            }
        }
    }
    return response;
}

pixdecor_layout_t::action_response_t pixdecor_layout_t::handle_axis_event(int delta)
{
	// LOGW("handle_axis_event delta=", delta);
	return { shade_action_from_delta<layout_action_t>(delta), 0 };
}

/**
 * Find the layout area at the given coordinates, if any
 * @return The layout area or null on failure
 */
nonstd::observer_ptr<area_t> pixdecor_layout_t::find_area_at(wf::point_t point)
{
    // Geometries are set in resize() at full width/height already,
	// no additional layout expansion is required.

    // Pass 1: Point interactive elements (buttons)
    for (auto& area_ptr : this->layout_areas)
    {
		if (!area_ptr->is_interactive()) continue;

        if (area_ptr->get_geometry() & point)
        {
            return nonstd::observer_ptr<area_t>(area_ptr.get());
        }
    }
    // Pass 2: Resize zones (edges and corners)
    for (auto& area_ptr : this->layout_areas)
    {
        if (area_ptr->can_resize() && (area_ptr->get_geometry() & point))
        {
            return nonstd::observer_ptr<area_t>(area_ptr.get());
        }
    }
    // Pass 3: Background move zones (the whole titlebar, paddings, title_area)
    for (auto& area_ptr : this->layout_areas)
    {
        if (area_ptr->can_move() && (area_ptr->get_geometry() & point))
        {
            return nonstd::observer_ptr<area_t>(area_ptr.get());
        }
    }
    return nullptr;
}

// even simpler: contains without region allocations:
bool pixdecor_layout_t::contains(const wf::pointf_t& at) const
{
    // operator&(geometry, pointf) — «point is INSIDE rect» (wayfire/geometry.hpp):
    // boundary does not count as a hit, zero-size areas do not catch.
    for (const auto& area_ptr : this->layout_areas)
        if (area_ptr->get_geometry() & at)
            return true;

    return false;
}


/** Update the cursor based on @current_input */
void pixdecor_layout_t::update_cursor()
{
    auto area = find_area_at(current_input);
    if (area) {
        wf::get_core().set_cursor(area->get_cursor_name());
    } else {
        wf::get_core().set_cursor("default");
    }
}

void pixdecor_layout_t::handle_focus_lost()
{
    if (is_grabbed)
    {
        is_grabbed = false;
        auto grab_area = find_area_at(grab_origin);
        if (grab_area)
            grab_area->set_clicked(false);
    }
    /* Focus lost — the double-click gesture is dead.
     * Reset BOTH halves of its state. */
    double_click_at_release = false;
    timer.disconnect();

    auto hover_area = find_area_at(current_input);
    if (hover_area)
        hover_area->set_hovered(false);
}

}
