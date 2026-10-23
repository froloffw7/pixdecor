#include <gtk/gtk.h>
#include <cairo.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <vector>
#include <tuple>
#include <map>
#include <vector>
#include <filesystem>

class gtk_renderer_t {

    struct theme_info_t {
        bool        user;
        bool        dark;
    };

  public:

    static void init() {
        if (!gtk_init_check(nullptr, nullptr)) {
            std::cerr << "Нет дисплея — нужен X11/Wayland или: xvfb-run -a ./app\n";
            std::exit(1);
        }
    }

    // CSS строкой (самодостаточные фрагменты)
    static bool bake_css(const std::string& css,
                         const std::string& icon_name, const std::string& out,
                         GtkStateFlags state, int w, int h, int scale) {
        GError* err = nullptr;
        GtkCssProvider* provider = gtk_css_provider_new();
        if (!gtk_css_provider_load_from_data(provider, css.c_str(), -1, &err)) {
            std::cerr << "CSS не разобран: " << (err ? err->message : "?") << "\n";
            g_clear_error(&err); g_object_unref(provider);
            return false;
        }
        bool ok = bake(provider, icon_name, out, state, w, h, scale);
        g_object_unref(provider);

        return ok;
    }

    // Файл темы — правильный путь: @import/url() резолвятся,
    // и там есть hover/active/backdrop-правила, которых нет во фрагменте.
    static bool bake_file(const std::string& path,
                          const std::string& icon_name, const std::string& out,
                          GtkStateFlags state, int w, int h, int scale) {
        GError* err = nullptr;
        GtkCssProvider* provider = gtk_css_provider_new();
        if (!gtk_css_provider_load_from_path(provider, path.c_str(), &err)) {
            std::cerr << "Не удалось загрузить " << path << ": "
                      << (err ? err->message : "?") << "\n";
            g_clear_error(&err); g_object_unref(provider);
            return false;
        }
        bool ok = bake(provider, icon_name, out, state, w, h, scale);
        g_object_unref(provider);

        return ok;
    }

    // Режим 1: системная тема, CSS не трогаем
    static bool bake_system(const std::string& icon, const std::string& out,
                            GtkStateFlags state, int w, int h, int scale) {
        return bake(nullptr, icon, out, state, w, h, scale);
    }

    // Режим 2: тема из GResource libgtk, без распаковки
    static bool bake_builtin(const std::string& theme, bool dark, const std::string& icon,
                             const std::string& out, GtkStateFlags state, int w, int h, int scale) {
    
        const std::string base = "/org/gtk/libgtk/theme/" + theme + "/";
        std::string candidates[2];
        if (dark) {
            candidates[0] = base + "gtk-contained-dark.css";  // полный (как у вас, 2404)
            candidates[1] = base + "gtk-dark.css";            // старые layout'ы: там это полный файл
        } else {
            candidates[0] = base + "gtk-contained.css";
            candidates[1] = base + "gtk.css";
        }
        std::string chosen;
        for (const std::string& c : candidates)
            if (GBytes* probe = g_resources_lookup_data(c.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr)) {
                chosen = c;
                g_bytes_unref(probe);
                break;
            }
        if (chosen.empty()) {
            std::cerr << "Тема " << theme << " не вшита в этот libgtk\n";
            return false;
        }

        GtkCssProvider* provider = gtk_css_provider_new();
        gtk_css_provider_load_from_resource(provider, chosen.c_str());
        bool ok = bake(provider, icon, out, state, w, h, scale);
        g_object_unref(provider);

        return ok;
    }

    // Ручки темы для режима 1 (вызывать сразу после init, до создания виджетов)
    static void set_theme(const char* name, bool prefer_dark) {
        GtkSettings* s = gtk_settings_get_default();
        if (name && *name)
            g_object_set(s, "gtk-theme-name", name, nullptr);
        g_object_set(s, "gtk-application-prefer-dark-theme", (gboolean)prefer_dark, nullptr);
    }

    static std::string current_theme() {
        char* n = nullptr; gboolean d = FALSE;
        g_object_get(gtk_settings_get_default(),
                     "gtk-theme-name", &n, "gtk-application-prefer-dark-theme", &d, nullptr);
        std::string r = n ? n : "?"; if (d) r += " :dark";
        g_free(n);
        return r;
    }

    // Проверка одного имени по тем же правилам (для валидации --theme=…)
    static bool theme_exists(const std::string& name) {
        if (name.empty() || name.find('/') != std::string::npos) return false;
        if (builtin_has(name.c_str(), false)) return true;
        std::vector<std::string> dirs = {
            std::string(g_get_user_data_dir()) + "/themes",
            std::string(g_get_home_dir())     + "/.themes" };
        for (const gchar* const* p = g_get_system_data_dirs(); *p; ++p) {
            std::string path = *p;
            if (!path.empty() && path.back() != '/') path += '/';
            path += std::string("themes");
            dirs.emplace_back(path);
        }
        for (const std::string& d : dirs) {
            std::string path = d + '/' + name + "/gtk-3.0/gtk.css";
            // std::cout << path + '\n';
            if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR))
                return true;
        }
        return false;
    }

    // Что вообще вшито: перечисляет Adwaita/, HighContrast/…
    static void list_builtin_themes() {
        GError* err = nullptr;
        gchar** dirs = g_resources_enumerate_children("/org/gtk/libgtk/theme",
                                                      G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
        if (!dirs) { g_clear_error(&err); return; }
        std::cout << "Встроено в libgtk:";
        for (gchar** d = dirs; *d; ++d) {
            std::string theme = *d;
            if (!theme.empty() && theme.back() == '/') theme.pop_back();// Удаляем последний символ '/'
            std::cout << "\n    " << theme;
        }
        std::cout << "\n(файлы внутри: g_resources_enumerate_children(\"/org/gtk/libgtk/theme/\", …))\n";
        g_strfreev(dirs);
    }

    // имя → есть ли тёмный вариант
    static std::vector<std::pair<std::string,theme_info_t>> list_themes() {
        std::map<std::string,theme_info_t> out;

        collect_from_dir(std::string(g_get_user_data_dir()) + "/themes", true, out); // ~/.local/share/themes
        collect_from_dir(std::string(g_get_home_dir())     + "/.themes", true, out); // легаси, GTK его тоже смотрит
        for (const gchar* const* p = g_get_system_data_dirs(); *p; ++p)              // /usr/local/share, /usr/share, …
            collect_from_dir(std::string(*p) + "/themes", false, out);
#if 0
        for (const char* b : {"Adwaita", "HighContrast"})     // встроенные: на диске их нет
            if (builtin_has(b, false) && !out.count(b))
                out.emplace(b, builtin_has(b, true));
#endif
        return { out.begin(), out.end() };                    // map => уже отсортировано
    }

  private:
    static void collect_from_dir(const std::string& dir, bool user, std::map<std::string,theme_info_t>& out) {
        GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
        if (!d) return;
        const gchar* name;
        while ((name = g_dir_read_name(d)) != nullptr) {
            const std::string base = dir + "/" + name + "/gtk-3.0";
            if (!g_file_test((base + "/gtk.css").c_str(), G_FILE_TEST_IS_REGULAR))
                continue;                                   // не GTK3-тема — мимо
            const bool has_dark =
                g_file_test((base + "/gtk-dark.css").c_str(), G_FILE_TEST_IS_REGULAR);
            out[name] = {user, has_dark};                   // дедуп по имени
        }
        g_dir_close(d);
    }

    static bool builtin_has(const char* theme, bool dark) { // для встроенных Adwaita/HighContrast
        const std::string p = std::string("/org/gtk/libgtk/theme/") + theme +
                              (dark ? "/gtk-dark.css" : "/gtk.css");
        GBytes* b = g_resources_lookup_data(p.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
        if (b) { g_bytes_unref(b); return true; }
        return false;
    }

    static bool bake(GtkCssProvider* provider,
                     const std::string& icon_name, const std::string& out,
                     GtkStateFlags state, int w, int h, int scale) {
        GdkScreen* screen = gdk_screen_get_default();

        if (provider)
            // (3) Провайдер — на весь экран, поверх темы (APPLICATION=600 > THEME=200)
            gtk_style_context_add_provider_for_screen(
                screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        // Дерево как у настоящего CSD-окна:
        // window.background.csd > headerbar.titlebar > button.titlebutton.close
        GtkWidget* window = gtk_offscreen_window_new();
        GtkWidget* header = gtk_header_bar_new();
        GtkWidget* button = gtk_button_new();

        gtk_style_context_add_class(gtk_widget_get_style_context(window), "background");
        gtk_style_context_add_class(gtk_widget_get_style_context(window), "csd");
        if (getenv("BAKERY_DEBUG")) {
            GtkStyleContext* window_ctx = gtk_widget_get_style_context(window);
            std::cout << gtk_style_context_to_string(window_ctx, GTK_STYLE_CONTEXT_PRINT_RECURSE) << std::endl;
        }

        gtk_style_context_add_class(gtk_widget_get_style_context(header), "titlebar");

        GtkStyleContext* ctx = gtk_widget_get_style_context(button);
        gtk_style_context_add_class(ctx, "titlebutton"); // это добавляет GtkWindow
        // gtk_style_context_add_class(ctx, "close");       // и это тоже — для close
        gtk_style_context_add_class(ctx, icon_name.c_str());
        //gtk_style_context_add_class(ctx, "flat");        // GTK сам НЕ добавляет; нужен,
                                                          // только если ваш CSS пишет button.flat

        gtk_container_add(GTK_CONTAINER(window), header);
        gtk_header_bar_pack_start(GTK_HEADER_BAR(header), button);
        gtk_widget_set_size_request(button, w, h);

        // (1) ГЛАВНОЕ: состояние — ДО первого вычисления стиля.
        // Никаких show_all / gtk_style_context_set_state / invalidate.
        gtk_widget_set_state_flags(button, state, TRUE);

        // «Нативный» размер кнопки по CSS (если понадобится):
        // gint mn, nat;
        // gtk_widget_get_preferred_width(button, &mn, &nat);

        cairo_surface_t* surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, w * scale, h * scale);
        cairo_t* cr = cairo_create(surface);
        cairo_scale(cr, scale, scale);

        gtk_render_background(ctx, cr, 0, 0, w, h);
        gtk_render_frame(ctx, cr, 0, 0, w, h);

        // Отладка: что GTK реально насчитал по CSS
        // char* dump = gtk_style_context_to_string(ctx, GTK_STYLE_CONTEXT_PRINT_SHOW_STYLE);
        // std::cerr << dump << "\n"; g_free(dump);

        // Глиф: размер — из CSS-паддингов, цвет — из контекста (state-aware)
        GtkBorder pad;
        gtk_style_context_get_padding(ctx, state, &pad);
        int icon_size = std::min(w - pad.left - pad.right, h - pad.top - pad.bottom);
        if (icon_size < 8) icon_size = 16;  // как GtkWindow (GTK_ICON_SIZE_MENU)

        const std::string symbolic_name = "window-" + icon_name;
        auto icon_theme = gtk_icon_theme_get_default();

        // A. Ищем PNG вариант
    	GtkIconInfo *info = gtk_icon_theme_lookup_icon_for_scale(
    	    icon_theme, 
    	    symbolic_name.c_str(), 
    	    icon_size,             // Исходный базовый размер в пикселях (unscaled)
    	    scale,                 // Масштаб: 2 для @2 (HiDPI), 1 для обычных экранов
            GtkIconLookupFlags(GTK_ICON_LOOKUP_NO_SVG | GTK_ICON_LOOKUP_FORCE_SYMBOLIC)
    	    //GTK_ICON_LOOKUP_FORCE_SIZE  // Флаг принудительного ограничения размера
    	);
        // B. Ищем векторный SVG вариант
        if (!info)
    		info = gtk_icon_theme_lookup_icon_for_scale(
    		    icon_theme, 
    		    symbolic_name.c_str(), 
    		    icon_size,             // Исходный базовый размер в пикселях (unscaled)
    		    scale,                 // Масштаб: 2 для @2 (HiDPI), 1 для обычных экранов
    	        GtkIconLookupFlags(GTK_ICON_LOOKUP_FORCE_SVG | GTK_ICON_LOOKUP_FORCE_SYMBOLIC)
    		    //GTK_ICON_LOOKUP_FORCE_SIZE  // Флаг принудительного ограничения размера
    		);
        // Дамп вычисленного стиля — только под флагом: на каждый bake это тысячи строк
        if (getenv("BAKERY_DEBUG"))
            std::cout << gtk_style_context_to_string(ctx, GTK_STYLE_CONTEXT_PRINT_RECURSE) << std::endl;
        // Иконка не нашлась — молча писать PNG с одним фоном нельзя:
        // шейдер получит «пустую» кнопку. Считаем баком FAIL.
        bool icon_ok = info != nullptr;
        if (info) {
           GError* ierr = nullptr;
           gboolean was_symbolic = FALSE;
           GdkPixbuf* pixbuf = gtk_icon_info_load_symbolic_for_context(info, ctx, &was_symbolic, &ierr);
           if (pixbuf) {
               double iw = gdk_pixbuf_get_width(pixbuf)  / (double)scale;
               double ih = gdk_pixbuf_get_height(pixbuf) / (double)scale;
#if 0
                // gtk_render_icon рисует сам и применяет -gtk-icon-shadow из темы
                // (в Adwaita: 0 1px rgba(255,255,255,0.77), у :backdrop — none)
                gtk_render_icon(ctx, cr, pixbuf, (w - iw) / 2.0, (h - ih) / 2.0);
#else
               // 1:1 физические пиксели для HiDPI — как было;
               // если вернёте :disabled — apply_icon_effect(pixbuf, ...) ЗДЕСЬ, до surface
               cairo_surface_t* icon_surface = gdk_cairo_surface_create_from_pixbuf(pixbuf, scale, nullptr);

               // …но отрисовку отдаём GTK: применит -gtk-icon-shadow и -gtk-icon-transform из CSS
               gtk_render_icon_surface(ctx, cr, icon_surface, (w - iw) / 2.0, (h - ih) / 2.0);
               cairo_surface_destroy(icon_surface);
#endif
               g_object_unref(pixbuf);
           } else {
               icon_ok = false;
           }
           g_clear_error(&ierr);
           g_object_unref(info);
        } else {
            std::cerr << symbolic_name << "-symbolic не найден (нет значковой темы?)\n";
        }

        cairo_status_t status = cairo_surface_write_to_png(surface, out.c_str());
        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        // (2) Контекст кнопки принадлежит кнопке — НЕ unref-им его.
        gtk_widget_destroy(window);  // освобождает и header, и button, и их контексты
        if (provider)
            gtk_style_context_remove_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider));

        return status == CAIRO_STATUS_SUCCESS && icon_ok;
    }
};

bool ensure_directory_exists(const std::string& path) {
    std::filesystem::path dir(path);

    // Метод .parent_path() автоматически отрежет имя файла, если вы передали путь к файлу
    // Например, из "~/.config/wayfire/assets/close.png" он сделает "~/.config/wayfire/assets/"
    if (dir.has_filename() && dir.extension() != "") {
        dir = dir.parent_path();
    }

    try {
        // create_directories сама проверяет существование.
        // Она вернет true, если создала новые папки, и false, если они уже были.
        std::filesystem::create_directories(dir);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Ошибка создания директории: " << e.what() << std::endl;
        return false;
    }
}


void print_help(const char *prg) {
    std::cout << "usage: " << prg << "\n";
    std::cout << "       --help          - print this text\n";
    std::cout << "       --builtin       - use GTK builtin theme\n";
    std::cout << "       --builtin-dark  - use dark theme\n";
    std::cout << "       --list          - list builtin themes\n";
    std::cout << "       --themes        - list installed themes\n";
    std::cout << "       --theme=<theme> - use theme\n";
    std::cout << "       --icon-size=<n> - create icons with size n/n@2\n";
    std::cout << "       --out=<dir>     - store files in directory, will be created if required";
    std::cout << std::endl;
}

#if 0
// фрагмент — только для демонстрации (hover/active в нём «плоские» — это ожидаемо):
const std::string fragment = R"CSS(
button.titlebutton, notebook > header > tabs > arrow, button { min-height: 24px; min-width: 16px; padding: 4px 9px; border: 1px solid; border-radius: 5px; transition: all 200ms cubic-bezier(0.25, 0.46, 0.45, 0.94); color: #eeeeec; outline-color: rgba(238, 238, 236, 0.3); border-color: #1b1b1b; border-bottom-color: #070707; background-image: linear-gradient(to top, #323232 2px, #353535); text-shadow: 0 -1px rgba(0, 0, 0, 0.834353); -gtk-icon-shadow: 0 -1px rgba(0, 0, 0, 0.834353); box-shadow: inset 0 1px rgba(255, 255, 255, 0.02), 0 1px 2px rgba(0, 0, 0, 0.07); }
button.titlebutton, button.sidebar-button, notebook > header > tabs > arrow, notebook > header > tabs > arrow.flat, button.flat { border-color: transparent; background-color: transparent; background-image: none; box-shadow: inset 0 1px rgba(255, 255, 255, 0); text-shadow: none; -gtk-icon-shadow: none; transition: none; }
button.titlebutton:hover, button.sidebar-button:hover, notebook > header > tabs > arrow:hover, button.flat:hover { transition: all 200ms cubic-bezier(0.25, 0.46, 0.45, 0.94); transition-duration: 500ms; }
button.titlebutton:hover:active, button.sidebar-button:hover:active, notebook > header > tabs > arrow:hover:active, button.flat:hover:active { transition: all 200ms cubic-bezier(0.25, 0.46, 0.45, 0.94); }
)CSS";
#endif

int main(int argc, char* argv[]) {

    gtk_renderer_t::init();

    std::string css_file;
    std::string theme = "Adwaita";
    bool use_builtin = false, dark = false;
    std::string out_dir;
    int icon_size = 16;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--help" || a == "-h") { print_help(argv[0]); return 0; }
        else if (a == "--builtin")      { use_builtin = true; }
        else if (a == "--builtin-dark") { use_builtin = true; dark = true; }
        else if (a == "--list")         { gtk_renderer_t::list_builtin_themes(); return 0; }
        else if (a.rfind("--out=") == 0) {
            out_dir = a.substr(6);
            size_t first = out_dir.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                out_dir = std::string{};
            } else {
                size_t last  = out_dir.find_last_not_of("/\t\r\n");
                out_dir = out_dir.substr(first, (last - first + 1));
             }
             if (out_dir.empty() || !ensure_directory_exists(out_dir)) {
                 return 1;
             }
             out_dir += '/';
        }
        else if (a == "--themes") {
            for (const auto& [name, info] : gtk_renderer_t::list_themes())
                std::cout << (info.dark ? "[dark+] " : "        ") << name << (info.user ? "[user]\n" : "\n");
            return 0;
        }
        else if (a.rfind("--icon-size=", 0) == 0) {
            std::string size_str = a.substr(12);
		    try {
		        // std::stoi превратит "12pt" в 12, а "1px" в 1
		        icon_size = std::stoi(size_str);
		    } catch (...) {
		        // return 0; // На случай, если строка была пустой или некорректной
		    }
			icon_size = std::clamp(icon_size, 16, 128);
        }
        else if (a.rfind("--theme=", 0) == 0) {
            theme = a.substr(8);
            // … прежний парсинг, теперь с валидацией:
            if (theme.length() > 5 && theme.rfind(":dark") == theme.length() - 5) {
                dark = true; theme.erase(theme.length() - 5);
            }
            if (!gtk_renderer_t::theme_exists(theme)) {
                std::cerr << "Тема \"" << theme << "\" не найдена (см. --themes)\n";
                return 1;
            }
            gtk_renderer_t::set_theme(theme.c_str(), dark);
        } else {
            css_file = a;
            if (!g_file_test(css_file.c_str(), G_FILE_TEST_EXISTS)) {
    			std::cout << "Файл: '" << css_file << "' недосупен\n";
                return 1;
            }
        }
    }
    std::cout << "Тема: " << gtk_renderer_t::current_theme() << "\n";
    std::cout << "Размер иконок: " << icon_size << ',' << icon_size*2 << "\n";

    auto bake_one = [&](const std::string& icon, const std::string& out,
                        GtkStateFlags st, int w, int h, int s) {
        if (use_builtin)       return gtk_renderer_t::bake_builtin(theme, dark, icon, out, st, w, h, s);
        if (!css_file.empty()) return gtk_renderer_t::bake_file(css_file, icon, out, st, w, h, s);
        return gtk_renderer_t::bake_system(icon, out, st, w, h, s);   // новый дефолт
    };
    // … циклы по кнопкам/состояниям/масштабам зовут bake_one …
    std::vector<std::string> buttons = {"close", "minimize", "maximize", "restore"};

    const struct { const char* name; GtkStateFlags state; } states[] = {
        {"normal",   GTK_STATE_FLAG_NORMAL},
        {"hover",    GTK_STATE_FLAG_PRELIGHT},
        // Реальное нажатие в GTK — :hover:active (указатель на кнопке + кнопка зажата),
        // в CSS есть отдельные правила button:hover:active — печём именно эту комбинацию
        {"active",   GtkStateFlags(GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE)},
        {"backdrop", GTK_STATE_FLAG_BACKDROP},
    };

    // Loop through every button type
    for (const auto& btn : buttons) {
        std::cout << "=======================================" << std::endl;
        std::cout << "Processing button: "                       << btn << std::endl;
        std::cout << "======================================="   << std::endl;

        for (const auto& j : states)
            for (int scale = 1; scale <= 2; ++scale) {
                std::string out = std::string(btn) + '_' + j.name + (scale > 1 ? "@2" : "") + ".png";
                bool ok = bake_one(btn, out_dir + out, j.state, icon_size, icon_size, scale);
                std::cout << (ok ? "OK   " : "FAIL ") << out << "\n";
            }
	}
    std::cout << "Все текстуры успешно обновлены под вашу текущую тему!" << std::endl;

    return 0;
}
