#include <cairo.h>
#include <librsvg/rsvg.h>
#include <stdio.h>

void draw_svg_to_cairo_surface(const char *svg_path, cairo_t *cr) {
    GError *error = NULL;

    // 1. Load the SVG file from disk (librsvg uses GLib/Gio under the hood)
    RsvgHandle *handle = rsvg_handle_new_from_file(svg_path, &error);
    
    if (error != NULL) {
        g_printerr("SVG load error: %s\n", error->message);
        g_error_free(error);
        return;
    }

    // 2. Render the SVG to the current Cairo context
    // The icon is automatically scaled to the dimensions set in the cairo context
    gboolean success = rsvg_handle_render_cairo(handle, cr);

    if (!success) {
        g_printerr("Failed to render SVG via Cairo\n");
    }

    // 3. Free the memory
    g_object_unref(handle);
}


void draw_svg_with_size(const char *svg_path, cairo_t *cr, double x, double y, double width, double height) {
    GError *error = NULL;
    RsvgHandle *handle = rsvg_handle_new_from_file(svg_path, &error);
    
    if (!handle) {
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
        return;
    }

    // Set the exact dimensions and coordinates for drawing
    RsvgRectangle viewport = {
        .x = x,
        .y = y,
        .width = width,   // For example, 24.0
        .height = height  // For example, 24.0
    };

    // Render the SVG strictly within the bounds of this rectangle
    rsvg_handle_render_document(handle, cr, &viewport, &error);

    g_object_unref(handle);
}

//-------------------------------
cairo_surface_t *image = cairo_image_surface_create_from_png(path_to_2x_png);

// Tell Cairo that 1 logical pixel corresponds to 2 physical pixels.
// After this, the surface becomes logically 24x24 but retains 48x48 sharpness!
cairo_surface_set_device_scale(image, 2.0, 2.0); 

// Now we can safely draw in UI coordinates
cairo_set_source_surface(cr, image, x, y);
cairo_paint(cr);
cairo_surface_destroy(image);
