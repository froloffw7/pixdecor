// #include "gtkdecor_generator.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <sys/stat.h>

#include "css_theme.hpp"  // CSS Parser
#include "box_shadow.hpp"
#include "css_helpers.hpp"

std::string color_scheme;

std::string get_gsettings_value(const std::string& schema_name, const std::string& key_name) {
	if (g_settings_schema_source_lookup(g_settings_schema_source_get_default(), schema_name.c_str(), TRUE) == nullptr) {
		return "";
	}
	GSettings* settings = g_settings_new(schema_name.c_str());
	char* value_c = g_settings_get_string(settings, key_name.c_str());
	std::string value(value_c ? value_c : "");
	g_free(value_c);
	g_object_unref(settings);
	return value;
}

bool
get_resource_path(const std::string& content, std::string& internal_css_path) {
    // Look for the @import url("resource://...") directive in the file text
    // The regex extracts everything after resource:// inside the parentheses and quotes
    std::regex import_regex(R"(@import\s+url\(\"resource:\/\/\/([^ \t\"]+)\"\);)");
    std::smatch match;

    if (std::regex_search(content, match, import_regex)) {
        // match[1] contains the clean internal path (for example: "org/gnome/arc-theme/gtk-main-dark.css")
        internal_css_path = "/" + match[1].str(); // Add a leading slash for GResource
        return true;
    }
    return false;
}

// The function takes "/org/gnome/arc-theme/gtk-main-dark.css"
// and returns "/org/gnome/arc-theme/"
std::string
extract_resource_prefix(const std::string& internal_css_path) {
    size_t last_slash = internal_css_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        // Strip the filename, leaving a path with leading and trailing slashes
        return internal_css_path.substr(0, last_slash + 1);
    }
    return "/"; // Fall back to the root path
}

bool
extract_gresource_to_file(const std::string& gresource_path,
                          const std::string& internal_asset_path,
                          const std::string& output_file_path)
{
    GError* error = nullptr;
    GResource* resource = nullptr;
	if (!gresource_path.empty()) {
	    // 1. Load the system resource bundle from disk
	    resource = g_resource_load(gresource_path.c_str(), &error);
	    if (error != nullptr) {
	        g_error_free(error);
	        return false;
	    }
	    g_resources_register(resource);
    }

    // 2. Fetch the raw image bytes by their internal path
    GBytes* bytes = g_resources_lookup_data(internal_asset_path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
    if (error != nullptr) {
        g_error_free(error);
        if (resource != nullptr) {
	        g_resources_unregister(resource);
	        g_resource_unref(resource);
        }
        return false;
    }

    // 3. Get a pointer to the data in memory and its exact size
    gsize size = 0;
    const char* raw_data = static_cast<const char*>(g_bytes_get_data(bytes, &size));

    // 4. Write the bytes "as is" in binary mode to the output file
    std::ofstream out_file(output_file_path, std::ios::binary);
    if (out_file.is_open()) {
        out_file.write(raw_data, size);
        out_file.close();
    } else {
        std::cerr << "Error: could not create file on disk: " << output_file_path << std::endl;
    }

    // Release the memory
    g_bytes_unref(bytes);
    if (resource != nullptr) {
	    g_resources_unregister(resource);
	    g_resource_unref(resource);
	}
    return true;
}

std::string load_css_from_gresource(const std::string& gresource_path, const std::string& internal_path) {
	GError* error = nullptr;
	GResource* resource = nullptr;

	if (!gresource_path.empty()) {
		resource = g_resource_load(gresource_path.c_str(), &error);
		if (error != nullptr) { g_error_free(error); return ""; }
	    g_resources_register(resource);
	}

	GBytes* bytes = g_resources_lookup_data(internal_path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
	if (error != nullptr) {
		g_error_free(error);
		if (resource != nullptr) {
			g_resources_unregister(resource);
			g_resource_unref(resource);
		}
		return "";
	}

	gsize size = 0;
	const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &size));
	std::string css_content(data, size);

	g_bytes_unref(bytes);
	if (resource != nullptr) {
		g_resources_unregister(resource);
		g_resource_unref(resource);
	}
	return css_content;
}

std::string preprocess_gtk_css(std::string css_content) {
	std::unordered_map<std::string, std::string> gtk_variables;
	std::regex define_regex(R"(@define-color\s+([a-zA-Z0-9_\-]+)\s+([^;]+);)");
	std::smatch match;

	std::string::const_iterator search_start(css_content.cbegin());
	while (std::regex_search(search_start, css_content.cend(), match, define_regex)) {
		std::string var_name = match.str(1);
		std::string var_value = css::trim(match.str(2));
		gtk_variables[var_name] = var_value;
		search_start = match.suffix().first;
	}

	css_content = std::regex_replace(css_content, define_regex, "");

	bool replaced = true;
	while (replaced) {
		replaced = false;
		for (const auto& [var_name, var_value] : gtk_variables) {
			std::regex usage_regex("@" + var_name + R"(\b)");
			if (std::regex_search(css_content, usage_regex)) {
				css_content = std::regex_replace(css_content, usage_regex, var_value);
				replaced = true;
			}
		}
	}
	return css_content;
}

std::string
get_theme_css(const std::string& theme_name, std::string& theme_path, std::string& resource_prefix) {
    // 1. Build the prioritized list of theme directories on disk
    std::vector<std::string> base_paths = {
        "/usr/share/themes/" + theme_name + "/gtk-3.0/",
        std::string(g_get_user_data_dir()) + "/themes/" + theme_name + "/gtk-3.0/",
        std::string(g_get_home_dir()) + "/.themes/" + theme_name + "/gtk-3.0/"
    };

    std::string css_content{};

    // Locate the theme directory and read its gtk.css file
    for (const auto& path : base_paths) {
        std::string css_path = path + "gtk.css";
        if (fs::file_exists(css_path)) {
            css_content = fs::read_plain_file(css_path);
            if (!css_content.empty()) {
                std::string internal_path;
                if (get_resource_path(css_content, internal_path)) {
                    std::string gresource_path = path + "gtk.gresource";
                    if (fs::file_exists(gresource_path)) {
                        std::cout << "Resource import detected: " << internal_path << std::endl;
                        std::cout << "Loading data from the bundle: " << gresource_path << " ... ";
                        css_content = load_css_from_gresource(gresource_path, internal_path);
                        if (css_content.empty()) {
                            std::cout << "FAILED" << std::endl;
                        } else {
                            std::cout << "OK" << std::endl;
                            theme_path      = path;
                            resource_prefix = extract_resource_prefix(internal_path);
                            break;
                        }
                    } else {
                        std::cerr << "file " << gresource_path  << " for theme " << theme_name << " does not exist" << std::endl;
                    }
                } else if (css_content.length() < 100) {
					std::cerr << "file " << css_path  << " for theme " << theme_name << " contains no data" << std::endl;
					if (theme_name == "Adwaita") {
					    if (!gtk_init_check(0, nullptr))
					        std::cerr << "Warning: Failed to start the display server, "
                   			"but the resources will still be available." << std::endl;

						if (color_scheme == "prefer-dark")
							internal_path = "/org/gtk/libgtk/theme/Adwaita/gtk-contained-dark.css";
						else
							internal_path = "/org/gtk/libgtk/theme/Adwaita/gtk-contained.css";
                        css_content = load_css_from_gresource("", internal_path);
                        if (css_content.empty()) {
                            std::cout << "FAILED" << std::endl;
                        } else {
                            std::cout << "OK" << std::endl;
                            theme_path      = path;
                            resource_prefix = extract_resource_prefix(internal_path);
                            break;
                        }
					}
                }
            }
        }
    }

    if (css_content.empty()) {
        theme_path      = "";
        resource_prefix = "";
        std::cerr << "Failed to find or read the gtk.css file for theme " << theme_name << std::endl;
        return "";
    }

	// 2. Handle precedence: look for the user configuration file
	std::string user_config_path = std::string(g_get_user_config_dir()) + "/gtk-3.0/gtk.css";
	if (fs::file_exists(user_config_path)) {
		std::string user_css_content = fs::read_plain_file(user_config_path);

		if (!user_css_content.empty()) {
			std::cout << "User styles detected in: " << user_config_path << std::endl;
			// Concatenate them: user styles override system styles by CSS order.
			css_content += "\n" + user_css_content;
		}
	}

    return preprocess_gtk_css(css_content);
}

// The function extracts "assets/titlebutton-close-dark.png" from a background-image string
std::string
extract_url_path(const std::string& css_value) {
    // The regex looks for the first occurrence of url("...") or url('...')
    std::regex url_regex(R"(url\([\"']?([^\"'\)]+)[\"']?\))");
    std::smatch match;

    if (std::regex_search(css_value, match, url_regex)) {
        return match.str(1); // Returns the clean path: "assets/titlebutton-close-dark.png"
    }
    return std::string{};
}

// The function finds ALL url(...) inside the string and returns them as a list
std::vector<std::string>
extract_url_paths(const std::string& css_value) {
    std::vector<std::string> paths;
    
    // Regex for finding url("...") or url('...')
    std::regex url_regex(R"(url\([\"']?([^\"'\)]+)[\"']?\))");
    
    std::string::const_iterator search_start(css_value.cbegin());
    std::smatch match;

    while (std::regex_search(search_start, css_value.cend(), match, url_regex)) {
        paths.push_back(match.str(1)); // match.str(1) is the group inside the parentheses
        search_start = match.suffix().first; // move on to the next url()
    }

    return paths; // Returns either one path, or two (regular and @2), or an empty vector
}

struct icon_paths_t {
    std::string normal;
    std::string hidpi;
};

bool extract_theme_icon(const std::string& theme_path,
                        const std::string& resource_prefix,
                        const std::string& css_img,
                        const std::string& plugin_theme_path,
                        icon_paths_t& dest_icons) // Changed to reference our structure
{
    std::vector<std::string> urls = extract_url_paths(css_img);
    if (urls.empty()) return false;
    
    for (size_t i = 0; i < urls.size(); i++) {
        std::string url = urls[i]; 
        std::string icon_path = "";

        if (url.empty()) continue;

        if (url[0] == '/') {
            icon_path = url;
        } else {
            std::string file_name = theme_path + url;
            if (fs::file_exists(file_name)) {
                icon_path = file_name;
            } else {
                std::string gresource_path = theme_path + "gtk.gresource";
                if (!fs::file_exists(gresource_path)) {
                	// return false;
                    gresource_path = "";
				}                
                std::string internal;
                if (url.rfind("resource://", 0) == 0) {
                    internal = url.substr(11);
                } else {
                    internal = resource_prefix + url;
                }
                
                std::string filename = fs::extract_filename_with_ext(internal);
                icon_path = plugin_theme_path + "/" + filename;
                extract_gresource_to_file(gresource_path, internal, icon_path);
            }
        }
        
        // Distribute the found asset based on whether it's the 1st (1x) or 2nd (2x) url() found
        if (i == 0) {
            dest_icons.normal = icon_path;
            std::cout << "  -> Extracted 1x icon: " << icon_path << "\n";
        } else if (i == 1) {
            dest_icons.hidpi = icon_path;
            std::cout << "  -> Extracted 2x icon: " << icon_path << "\n";
        }
    }
    return true;
}

std::string get_theme_icon_path_portable(const std::string& icon_theme_name,
	const std::string& icon_name, int icon_size = 24, int icon_scale = 1)
{
    std::string path = "";

    // 1. Create an isolated theme object. It is empty and NOT bound to the screen
    GtkIconTheme *icon_theme = gtk_icon_theme_new();

    // 2. In GTK 3, this is the function used to manually set the theme on a local object.
    // It also works on non-singleton objects.
    gtk_icon_theme_set_custom_theme(icon_theme, icon_theme_name.c_str());

    // 3. Build the symbolic icon name
    std::string symbolic_name = "window-" + icon_name;// + "-symbolic";

	GtkIconInfo *icon_info = gtk_icon_theme_lookup_icon_for_scale(
	    icon_theme, 
	    symbolic_name.c_str(), 
	    icon_size,                  // Original base size in pixels (unscaled)
	    icon_scale,                 // Scale: 2 for @2 (HiDPI), 1 for regular screens
        GtkIconLookupFlags(GTK_ICON_LOOKUP_NO_SVG | GTK_ICON_LOOKUP_FORCE_SYMBOLIC)
	    //GTK_ICON_LOOKUP_FORCE_SIZE  // Force the requested icon size
	);
    // 4b. Look for the vector SVG variant
    if (!icon_info) {
		icon_info = gtk_icon_theme_lookup_icon_for_scale(
		    icon_theme, 
		    symbolic_name.c_str(), 
		    icon_size,                  // Original base size in pixels (unscaled)
		    icon_scale,                 // Scale: 2 for @2 (HiDPI), 1 for regular screens
	        GtkIconLookupFlags(GTK_ICON_LOOKUP_FORCE_SVG | GTK_ICON_LOOKUP_FORCE_SYMBOLIC)
		    //GTK_ICON_LOOKUP_FORCE_SIZE  // Force the requested icon size
		);
#if 0
    GtkIconInfo *icon_info = gtk_icon_theme_lookup_icon(
        icon_theme, 
        symbolic_name.c_str(), 
        icon_size, // Base size
        GTK_ICON_LOOKUP_FORCE_SVG
    );
	// Request the icon with an explicit scale factor
    // Fallback lookup without the -symbolic suffix
    if (!icon_info) {
	    std::string alt_name = "window-" + icon_name;
        icon_info = gtk_icon_theme_lookup_icon(
            icon_theme, 
            alt_name.c_str(), 
            icon_size, 
            GTK_ICON_LOOKUP_FORCE_SVG
        );
		icon_info = gtk_icon_theme_lookup_icon_for_scale(
		    icon_theme, 
            alt_name.c_str(), 
		    icon_size,                  // Original base size in pixels (unscaled)
		    icon_scale,                 // Scale: 2 for @2 (HiDPI), 1 for regular screens
		    GTK_ICON_LOOKUP_FORCE_SIZE  // Force the requested icon size
		);
#endif
    }

    if (icon_info) {
        // Get the absolute path where GTK found the icon
        const char *filename = gtk_icon_info_get_filename(icon_info);
        if (filename) {
            printf("Icon '%s' found at path: %s\n", icon_name.c_str(), filename);
            path = filename;
        }
        g_object_unref(icon_info);
    } else {
        printf("Icon '%s' not found by the built-in lookup.\n", icon_name.c_str());
    }

    g_object_unref(icon_theme);

    return path;
}

std::string get_theme_icon_path(const std::string& icon_theme_name, const std::string& icon_name) {
	std::vector<std::string> sub_dirs = {
		"/scalable/ui/", "/scalable/actions/", "/scalable/status/", "/16x16/actions/", "/symbolic/ui/"
	};
	std::vector<std::string> base_paths = {
		"/usr/share/icons/" + icon_theme_name,
		std::string(g_get_user_data_dir()) + "/icons/" + icon_theme_name,
		"/usr/share/icons/Adwaita" // Fallback
	};
	for (const auto& base : base_paths) {
		for (const auto& sub : sub_dirs) {
			std::string full_path = base + sub + icon_name + "-symbolic.svg";
			if (fs::file_exists(full_path)) return full_path;
		}
	}
	return "";
}

bool add_key(std::ofstream& out, const std::string& key, const std::string& value) {
	if (key.empty() || value.empty()) return false;
		
	out << key << " = " << value << std::endl;
	return true;
}

bool add_color_key(std::ofstream& out, const std::string& key, const css::color_t& color) {
	if (key.empty()) return false;

   	out << key << " = " << css::color::format(color, color.a < 1.0 ? css::RGBA : css::RRGGBB) << std::endl;
	return true;
}

bool add_background_key(std::ofstream& out, const std::string& key, const css::background_t& bg) {
	if (key.empty()) return false;
		
	out << key << " = " << css::background::format(bg) << std::endl;
	return true;
}


bool add_key(std::ofstream& out, css::data_t& css_data, const std::string& selector, const std::string& key) {
	if (css_data[selector].count(key) > 0) {
		out << key << " = " << css_data[selector][key] << std::endl;
		return true;
	}
	return false;
}

bool add_edge_insets_key(std::ofstream& out, css::data_t& css_data, const std::string& selector, const std::string& key) {
	if (css_data[selector].count(key) > 0) {
		auto insets = css::edge_insets::parse(css_data[selector][key]);
		if (insets) {
			out << key << " = " << css::edge_insets::format(*insets) << std::endl;
			return true;
		}
	}
	return false;
}

bool add_color_key(std::ofstream& out, css::data_t& css_data, const std::string& selector, const std::string& key, const std::string& suffix = "") {
	if (css_data[selector].count(key) > 0) {
		auto c_opt  = css::color::parse(css_data[selector][key]);
		if (c_opt) {
			out << key + suffix << " = " << css::color::format(*c_opt, c_opt->a < 1.0 ? css::RGBA : css::RRGGBB) << std::endl;
			return true;
		}
	}
	return false;
}

bool add_border_keys(std::ofstream& out, css::data_t& css_data, const std::string& selector) {
	std::string key("border");
	if (css_data.count(selector) && (css_data[selector].count(key) > 0)) {
		std::string css_style = css_data[selector][key];
				  
		css::border_t b = css::border::parse(css_style);
#if 0
		if ((css::parse_numeric(b.width) == 0) ||
			(b.style == "none") || (b.color == "transparent"))
			return false;
			
		add_key(out, "border-width", std::to_string(css::parse_numeric(b.width)));
#endif
		auto insets = css::edge_insets::parse(b.width);
		if (insets) {
			out << "border-width" << " = " << css::edge_insets::format(*insets) << std::endl;
			add_key(out, "border-style", b.style);
			auto color = css::color::parse(b.color);
			if (color)
				add_key(out, "border-color", css::color::format(*color));
			return true;
		}
	}
	return false;
}

bool add_padding_key(std::ofstream& out, css::data_t& css_data, const std::string& selector, const std::string& out_key) {
    css::edge_insets_t padding;
    if (css_data[selector].count("padding") > 0) {
        auto p = css::edge_insets::parse(css_data[selector]["padding"]);
        if (p) padding = *p;
    } else {
        auto set_field = [&](const auto& key, auto& field) {
            std::string val = css_data[selector][key];
            // std::cout << "padding: " << val << "\n";
            auto len = css::unit::parse(val);
            if (len) field = *len;
        };
        set_field("padding-left",   padding.left);
        set_field("padding-top",    padding.top);
        set_field("padding-right",  padding.right);
        set_field("padding-bottom", padding.bottom);
	}
	if (padding == css::edge_insets_t{}) {
		return false;
	}
    add_key(out, out_key, css::edge_insets::format(padding));
	return true;
}

bool add_shadow_key(std::ofstream& out, css::data_t& css_data, const std::string& selector) {
	std::string key("box-shadow");
	if (css_data.count(selector) && (css_data[selector].count(key) > 0)) {
		std::string css_style = css_data[selector][key];
				  
		auto parsed = css::box_shadow::parse(css_style);
		if (parsed.has_value()) {
			add_key(out, key, css::box_shadow::format(*parsed));
			return true;
		}
	}
	return false;
} 

#if 0
void test_ini() {
// ~/.config/gtk-3.0/settings.ini (also check gtk-4.0 — they may differ):
const std::string prefer_dark =
    read_ini_key(settings_ini_path, "Settings",
                 "gtk-application-prefer-dark-theme");

const bool gtk_dark = (prefer_dark == "true") || (prefer_dark == "1");
}
#endif

#if 0
bool get_color(css::data_t& css_data, const std::string& selector, const std::string& key, css::color_t& color)
{
    auto c_ptr = css::color::parse(css_data[selector][key]);
    if (c_ptr) {
    	color = *c_ptr;
    	return true;
	}
	return false;
}
#else
//		bool has_color = get_property<color>(css_data, ".background", "color", color);

template <typename N, typename T>
bool get_property(css::data_t& css_data, const std::string& selector, const std::string& key, T& value)
{
    auto c_ptr = N::parse(css_data[selector][key]);
    if (c_ptr) {
        value = *c_ptr;
        return true;
    }
    return false;
}

#endif

int main() {
	std::cout << "=== Starting the settings.ini generator ===" << std::endl;

	std::string gtk_theme    = get_gsettings_value("org.gnome.desktop.interface", "gtk-theme");
	if (gtk_theme.empty()) gtk_theme = "Adwaita";

	std::string font_name    = get_gsettings_value("org.gnome.desktop.interface", "font-name");
	std::string layout       = get_gsettings_value("org.gnome.desktop.wm.preferences", "button-layout");
	std::string icon_theme   = get_gsettings_value("org.gnome.desktop.interface", "icon-theme");
	std::cout << "Icon theme:\t" << icon_theme << std::endl; 

	color_scheme = get_gsettings_value("org.gnome.desktop.interface", "color-scheme");

	gtk_theme = "Adwaita";
	color_scheme = "prefer-dark";
	
	// Possible values of the "color-scheme" key:
	// - 'prefer-dark'  -> The user prefers a dark theme
	// - 'prefer-light' -> The user prefers a light theme
	// - 'default'      -> The standard theme (usually light)
	std::cout << "Color scheme:\t" << color_scheme << std::endl; 
	
	//std::string plugin_theme_path = std::string(g_get_user_config_dir()) + "/wayfire/assets/" + gtk_theme;
	std::string plugin_theme_path = std::string("/tmp/wayfire/assets/")  + gtk_theme;
    fs::ensure_directory_exists(plugin_theme_path);
	
	std::string output_path = plugin_theme_path + "/settings.ini";
	
	std::ofstream out(output_path);
	
	if (out.is_open()) {
		
		std::string selector;
		std::string key;
		std::string border_style;

		out << "; Generated automatically by the gtkdecor configurator" << std::endl;
		out << std::endl << "[main]" << std::endl;
		add_key(out, "theme-name", gtk_theme);
		add_key(out, "font-name",  font_name);
#if 0
		std::string raw_css   = get_theme_css(gtk_theme);
		std::string clean_css = preprocess_gtk_css(raw_css);
		css::data_t css_data    = css::parse_and_ungroup(clean_css);
#endif
		std::string theme_path;
		std::string resource_prefix;

		std::string css_content = get_theme_css(gtk_theme, theme_path, resource_prefix);
		if (!css_content.empty()) {
			std::cout << "CSS for theme " << gtk_theme << " successfully loaded into memory!" << std::endl;
		}		
		css::data_t css_data    = css::parse_and_ungroup(css_content);
		 // fallbacks
		const css::color_t white_color{ 255,255,255,1.0 };
		const css::color_t black_color{   0,  0,  0,1.0 };

		css::color_t color = (color_scheme == "prefer-dark" ? white_color : black_color);
		bool has_color = get_property<css::color>(css_data, ".background", "color", color);
		std::cout << "color" << " = " << css::color::format(color) << std::endl;
		css::color_t background_color = (color_scheme == "prefer-dark" ? black_color : white_color);
		bool has_background_color = get_property<css::color>(css_data, ".background", "background-color", background_color);
		std::cout << "background-color" << " = " << css::color::format(background_color) << std::endl;

		css::color_t color_backdrop = color;
		bool has_color_backdrop = get_property<css::color>(css_data, ".background:backdrop", "color", color_backdrop);
		std::cout << "color:backdrop" << " = " << css::color::format(color_backdrop) << std::endl;
		css::color_t background_color_backdrop = background_color;
		bool has_background_color_backdrop = get_property<css::color>(css_data, ".background:backdrop", "background-color", background_color_backdrop);
		std::cout << "background-color:backdrop" << " = " << css::color::format(background_color_backdrop) << std::endl;

		css::background_t bg;
		bool has_bg;

		out << std::endl << "[titlebar]" << std::endl;
    	add_key(out, "button-layout", layout);
		//selector = ".titlebar"; // headerbar, .titlebar, headerbar label.title
		selector = "headerbar";
		if (css_data.count(selector)) {
			add_key(out, css_data, selector, "font-family");
			add_key(out, css_data, selector, "font-size");
			add_key(out, css_data, selector, "font-weight");
    	    add_key(out, "text-align", "center"); // Standarg GTK alinment couldn't be changed
			std::string title_selector{"headerbar .title"};
	        if (css_data.count(title_selector)) {
			    add_padding_key(out, css_data, title_selector, "text-padding");
	        }
			const std::string key{ "color" };
			if (add_color_key(out, css_data, selector, key)) {
				auto color_ptr = css::color::parse(css_data[selector][key]);
				color = *color_ptr;
			} else {
			    add_color_key(out, "color", color);
			    if (!has_color) {
				    std::cout << "Warning: titlebar color is no defined, using: " << css::color::format(color) << "\n";
				}
			}
			if (!has_color_backdrop) color_backdrop = color;

		    has_bg = get_property<css::background>(css_data, selector, "background", bg);
			if (!has_bg) {
				bg.with_gradient = false;
		        if (!get_property<css::color>(css_data, selector, "background-color", bg.color)) {
				    std::cout << "Warning: titlebar background color is no defined, using: " << css::color::format(background_color) << "\n";
					bg.color = background_color;
				}
			}
			add_background_key(out, "background", bg);

			if (!has_background_color_backdrop) background_color_backdrop = background_color;
			if (css_data[selector].count("border")) {
				std::string border_style = css_data[selector]["border"];
				add_border_keys(out, css_data, selector);
				add_key(out, css_data, selector, "border-radius");
			} else {
				add_color_key(out, css_data, selector, "border-color");
				add_key(out, css_data, selector, "border-style");
				add_key(out, css_data, selector, "border-width");
			}
			add_key(out, css_data, selector, "box-shadow");
			std::string csd_selector{".csd headerbar.default-decoration"};
			if (css_data.count(csd_selector)) {
				if (!add_key(out, css_data, csd_selector, "min-height")) {
					add_key(out, css_data, selector, "min-height");
				}
				if (!add_edge_insets_key(out, css_data, csd_selector, "padding")) {
					add_edge_insets_key(out, css_data, selector, "padding");
				}
			}
		}
		out << std::endl << "[titlebar:backdrop]" << std::endl;
		selector = "headerbar:backdrop";
		if (css_data.count(selector)) {
			add_key(out, css_data, selector, "font-family");
			add_key(out, css_data, selector, "font-size");
			if (!add_color_key(out, css_data, selector, "color")) {
			    add_color_key(out, "color", color_backdrop);
			    if (!has_color_backdrop) {
				    std::cout << "Warning: titlebar:backdrop color is no defined, using: " << css::color::format(color_backdrop) << "\n";
				}
			}

		    has_bg = get_property<css::background>(css_data, selector, "background", bg);
			if (!has_bg) {
				bg.with_gradient = false;
		        if (!get_property<css::color>(css_data, selector, "background-color", bg.color)) {
				    std::cout << "Warning: titlebar:backdrop background color is no defined, using: " << css::color::format(bg.color) << "\n";
				}
			}
			add_background_key(out, "background", bg);

			if (!add_color_key(out, css_data, selector, "background-color")) {
			}
  			add_key(out, css_data, selector, "transition"); // transition: 200ms
			if (css_data[selector].count("border")) {
				std::string border_style = css_data[selector]["border"];
				add_border_keys(out, css_data, selector);
				add_key(out, css_data, selector, "border-radius");
			} else {
				add_color_key(out, css_data, selector, "border-color");
				add_key(out, css_data, selector, "border-style");
				add_key(out, css_data, selector, "border-width");
			}
			add_key(out, css_data, selector, "box-shadow");
		}
		selector = "decoration";
		if (css_data.count(selector)) {
			out << std::endl << "[decoration]" << std::endl;
			if (css_data.count(selector)) {
				if (css_data[selector].count("border")) {
					std::string border_style = css_data[selector]["border"];
					add_border_keys(out, css_data, selector);
				} else {
					add_key(out, css_data, selector, "border-width");
					add_key(out, css_data, selector, "border-style");
					add_color_key(out, css_data, selector, "border-color");
				}
				add_key(out, css_data, selector, "border-radius");
				add_shadow_key(out, css_data, selector);
			}
		}
		selector = "decoration:backdrop";
		if (css_data.count(selector)) {
			out << std::endl << "[decoration:backdrop]" << std::endl;
			if (css_data.count(selector)) {
				if (css_data[selector].count("border")) {
					std::string border_style = css_data[selector]["border"];
					add_border_keys(out, css_data, selector);
				} else {
					add_color_key(out, css_data, selector, "border-color");
				}
				add_shadow_key(out, css_data, selector);
			}
		}
		
		auto image_size{ 16 }; // TODO
		std::string suffix;

        // Open your INI stream to write the [IconPaths] section
        out << std::endl << "[icons]" << std::endl;
        add_key(out, "spacing", "6px");
        add_key(out, "image-size", std::to_string(image_size) + "px");

        // 1. Extract button spacing from padding
  		//selector = "headerbar.default-decoration button.titlebutton";
        bool has_icon_color            = false;
        bool has_icon_background_color = false;
        selector = "headerbar button.titlebutton";
		if (css_data.count(selector)) {
			add_key(out, css_data, selector, "min-width");
		    add_edge_insets_key(out, css_data, selector, "padding");
			has_icon_color = add_color_key(out, css_data, selector, "color");
			has_icon_background_color = add_color_key(out, css_data, selector, "background-color");
		}
		if (!has_icon_color)
		    add_color_key(out, "color", color);
		if (!has_icon_background_color)
		    add_color_key(out, "background-color", background_color);
		
		// add_color_key(out, css_data, selector, "border-radius");
		suffix = ":hover";
        has_icon_color            = false;
        has_icon_background_color = false;
        selector = "headerbar button.titlebutton" + suffix;
		if (css_data.count(selector)) {
			has_icon_color = add_color_key(out, css_data, selector, "color", suffix);
			has_icon_background_color = add_color_key(out, css_data, selector, "background-color", suffix);
		}
		if (!has_icon_color)
		    add_color_key(out, "color" + suffix, color);
		if (!has_icon_background_color)
		    add_color_key(out, "background-color" + suffix, background_color);

		suffix = ":active";
        has_icon_color            = false;
        has_icon_background_color = false;
        selector = "headerbar button.titlebutton" + suffix;
		if (css_data.count(selector)) {
			has_icon_color = add_color_key(out, css_data, selector, "color", suffix);
			has_icon_background_color = add_color_key(out, css_data, selector, "background-color", suffix);
		}
		if (!has_icon_color)
		    add_color_key(out, "color" + suffix, color);
		if (!has_icon_background_color)
		    add_color_key(out, "background-color" + suffix, background_color);
#if 0
        // 2. Extract the window shadow margin from margin
        std::string margin_raw = css_tree["decoration"]["margin"]; // For example, "10px" or "10px 10px 10px 10px"
        edge_insets_t window_margin = css_utils::parse_edge_insets(margin_raw);
        int shadow_margin = static_cast<int>(window_margin.top.value); // This extracts the raw 10
#endif
        // 1. Declare the matrix metadata strings
        std::vector<std::string> buttons = {"close", "minimize", "maximize", "restore"};

        // We map our logical INI suffix to the actual GTK/CSS pseudo-class suffixes
        struct state_mapping_t {
            std::string ini_suffix;
            std::string css_suffix;
        };

        const std::vector<state_mapping_t> states = {
            {"active_normal",    ""},
            {"active_hovered",   ":hover"},
            {"active_clicked",   ":active"},
            {"inactive_normal",  ":backdrop"},
            {"inactive_hovered", ":backdrop:hover"},
            {"inactive_clicked", ":backdrop:active"}
        };
		selector = "button.titlebutton";
		std::cout << "Selector: " << selector << std::endl;
		auto properties = css_data[selector];
        for (const auto& [prop, val] : properties) {
            std::cout << "  " << prop << " : " << val << ";\n";
		}
		selector += ":not(.appmenu)";
		std::cout << "Selector: " << selector << std::endl;
		properties = css_data[selector];
        for (const auto& [prop, val] : properties) {
            std::cout << "  " << prop << " : " << val << ";\n";
        }
		selector = "button.flat";
		std::cout << "Selector: " << selector << std::endl;
		properties = css_data[selector];
        for (const auto& [prop, val] : properties) {
            std::cout << "  " << prop << " : " << val << ";\n";
        }
		selector += ":hover";
		std::cout << "Selector: " << selector << std::endl;
		properties = css_data[selector];
        for (const auto& [prop, val] : properties) {
            std::cout << "  " << prop << " : " << val << ";\n";
        }
		selector += ":active";
		std::cout << "Selector: " << selector << std::endl;
		properties = css_data[selector];
        for (const auto& [prop, val] : properties) {
            std::cout << "  " << prop << " : " << val << ";\n";
        }

        // 2. Loop through every button type
        for (const auto& btn : buttons) {
            std::cout << "\n=======================================" << std::endl;
            std::cout << "Processing button: "                       << btn << std::endl;
            std::cout << "======================================="   << std::endl;

            // We will accumulate resolved states to perform cross-state fallbacks later if needed
            std::map<std::string, icon_paths_t> resolved_button_matrix;
			std::string fallback_icon      = get_theme_icon_path_portable(icon_theme, btn, 24);
			std::string fallback_icon_hdpi = get_theme_icon_path_portable(icon_theme, btn, 24, 2);

            // 3. Loop through every state for this specific button
            for (const auto& st : states) {
                // Construct the full GTK selectors used by themes
                std::string selector_primary    = "headerbar button.titlebutton." + btn + st.css_suffix;
                std::string selector_secondary  = ".titlebar button.titlebutton." + btn + st.css_suffix;
				std::string selector_tertiary   = "button.titlebutton:not(.appmenu)";
				std::string selector_quaternary = "button.titlebutton";
                
                std::string target_selector{};
                
                // Safely check which selector pattern exists in the parsed css_tree
                if (css_data.count(selector_primary)) {
                    target_selector = selector_primary;
                } else if (css_data.count(selector_secondary)) {
                    target_selector = selector_secondary;
				}
                if (!target_selector.empty()) {
                    std::cout << "Found target selector: " << target_selector << std::endl;
                    auto properties = css_data[target_selector];
                    
                    for (const auto& [prop, val] : properties) {
                        // std::cout << "  " << prop << " : " << val << ";" << std::endl;
                        // Search inside the matched CSS properties for background-image
                        if (prop.rfind("image") != std::string::npos) {
			                icon_paths_t icon_paths{};
                            if (extract_theme_icon(theme_path, resource_prefix, val, plugin_theme_path, icon_paths)) {
                                // Save resolved (could be empty strings if CSS properties were missing)
                                resolved_button_matrix[st.ini_suffix] = icon_paths;
                                break;
                            }
                        }
                    }
                } else {
                }
            }
			if (resolved_button_matrix["active_normal"].normal.empty()) {
				resolved_button_matrix["active_normal"].normal = fallback_icon;
			}
            // 4. APPLY THE FALLBACK MATRIX RULES
            // Handle Active window fallbacks
            if (resolved_button_matrix["active_hovered"].normal.empty()) {
                resolved_button_matrix["active_hovered"] = resolved_button_matrix["active_normal"];
            }
            if (resolved_button_matrix["active_clicked"].normal.empty()) {
                resolved_button_matrix["active_clicked"] = resolved_button_matrix["active_hovered"];
            }

            // Handle Inactive (backdrop) window fallbacks (YOUR custom cross-state fallback rule!)
            if (resolved_button_matrix["inactive_normal"].normal.empty()) {
                resolved_button_matrix["inactive_normal"] = resolved_button_matrix["active_normal"];
            }
            if (resolved_button_matrix["inactive_hovered"].normal.empty()) {
                resolved_button_matrix["inactive_hovered"] = resolved_button_matrix["active_hovered"];
            }
            if (resolved_button_matrix["inactive_clicked"].normal.empty()) {
                resolved_button_matrix["inactive_clicked"] = resolved_button_matrix["active_clicked"];
            }

            // 5. WRITE DIRECTLY TO THE CONFIG FILE
            // Write 1x or 2x files paths sequentially for this button
            for (const auto& state : states) {
                std::string key = btn + "_" + state.ini_suffix;
                if ((image_size == 32) && !resolved_button_matrix[state.ini_suffix].hidpi.empty()) {
					add_key(out, key, resolved_button_matrix[state.ini_suffix].hidpi);
				} else {
					add_key(out, key, resolved_button_matrix[state.ini_suffix].normal);
				}
            }
        }        
		
		std::cout << "Configuration successfully saved to: " << output_path << std::endl;
	} else {
		std::cerr << "Error creating the file!" << std::endl;
	}
	return 0;
}
