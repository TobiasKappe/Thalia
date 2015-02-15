#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <glib/gprintf.h>

#include "libthalia/thalia_gb.h"
#include "libthalia/thalia_keypad.h"
#include "libthalia/thalia_gpu.h"

static GtkWidget* menu_bar = NULL;
static GtkWidget* file_menu = NULL;
static GtkWidget* file_item = NULL;
static GtkWidget* open_item = NULL;
static GtkWidget* screen = NULL;
static GtkWidget* window = NULL;
static GtkWidget* vbox = NULL;
static ThaliaGB* gb = NULL;

static gpointer thalia_gui_bg_thread(gpointer args)
{
    thalia_gb_run(gb);
    return NULL;
}

static gint thalia_gui_render_pixbuf(gpointer data)
{
    // Make sure we draw to a secondary buffer, avoid screen tearing.
    GdkRegion* region = gdk_drawable_get_clip_region(screen->window);
    gdk_window_begin_paint_region(screen->window, region);

    // We should be in the main thread now, we can render the pixbuf.
    cairo_t* context = gdk_cairo_create(screen->window);
    gdk_cairo_set_source_pixbuf(context, gb->gpu.screen, 0, 0);
    cairo_paint(context);
    cairo_destroy(context);

    // Indicate to GDK that the buffer may be swapped back for display.
    gdk_window_end_paint(screen->window);

    // Tell the emulation thread that screen rendering is safe again.
    thalia_gpu_unlock(gb);
    return 0;
}

static void thalia_gui_render_screen()
{
    // Make sure rendering happens on the GTK thread.
    thalia_gpu_lock(gb);
    gtk_idle_add(thalia_gui_render_pixbuf, NULL);
}

static void thalia_gui_make_menu_bar(GtkWidget* container)
{
    open_item = gtk_menu_item_new_with_mnemonic("_Open");
    file_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);

    // TODO: Actually do something when this menu is used.
    file_item = gtk_menu_item_new_with_mnemonic("_File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);

    menu_bar = gtk_menu_bar_new();
    gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), file_item);

    gtk_container_add(GTK_CONTAINER(container), menu_bar);
}

static void thalia_gui_update_key(guint16 keyval, gboolean value)
{
    thalia_keypad_lock(gb);

    // TODO: Maintain these separately with regard to double presses.
    switch(keyval) {
    case GDK_Up:
        gb->keypad.key_up = value;
        if(gb->keypad.key_down)
            gb->keypad.key_down = FALSE;
        break;
    case GDK_Down:
        gb->keypad.key_down = value;
        if(gb->keypad.key_up)
            gb->keypad.key_up = FALSE;
        break;
    case GDK_Right:
        gb->keypad.key_right = value;
        if(gb->keypad.key_left)
            gb->keypad.key_left = FALSE;
        break;
    case GDK_Left:
        gb->keypad.key_left = value;
        if(gb->keypad.key_right)
            gb->keypad.key_right = FALSE;
        break;
    case GDK_z:
        gb->keypad.key_a = value;
        break;
    case GDK_x:
        gb->keypad.key_b = value;
        break;
    case GDK_Return:
        gb->keypad.key_start = value;
        break;
    case GDK_BackSpace:
        gb->keypad.key_select = value;
        break;
    }
    thalia_keypad_unlock(gb);
}

static void thalia_gui_key_pressed(GtkWidget* widget, GdkEventKey* event)
{
    thalia_gui_update_key(event->keyval, TRUE);
}

static void thalia_gui_key_released(GtkWidget* widget, GdkEventKey* event)
{
    thalia_gui_update_key(event->keyval, FALSE);
}

static void thalia_gui_make_screen_area(GtkWidget* container)
{
    // Create a drawing area to draw our screen on.
    screen = gtk_drawing_area_new();
    gtk_widget_set_size_request(
        screen,
        THALIA_GPU_SCREEN_WIDTH,
        THALIA_GPU_SCREEN_HEIGHT
    );

    // Add it to the container we're filling.
    gtk_widget_add_events(screen, GDK_BUTTON_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(container), screen);
}

static void thalia_gui_make_window()
{
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    vbox = gtk_vbox_new(FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_window_set_title(GTK_WINDOW(window), "Thalia");
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    g_signal_connect(
        G_OBJECT(window),
        "key-press-event",
        G_CALLBACK(thalia_gui_key_pressed),
        NULL
    );
    g_signal_connect(
        G_OBJECT(window),
        "key-release-event",
        G_CALLBACK(thalia_gui_key_released),
        NULL
    );
}

static void thalia_gui_exit(gint code)
{
    if(window)
        gtk_widget_destroy(window); // This should free children, too.
    if(gb)
        thalia_gb_destroy(gb);

    gtk_exit(code);
}

static void thalia_gui_fatal_error(const gchar* intro, GError* error)
{
    GtkWidget* dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "%s: %s.",
        intro,
        error->message
    );
    gtk_window_set_title(GTK_WINDOW(dialog), "Thalia: Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    g_error_free(error);

    thalia_gui_exit(-1);
}

int main(int argc, char *argv[])
{
    GError* error = NULL;

    gtk_init(&argc, &argv);

    if(argc < 2) {
        g_printf("Usage: %s romfile.gb\r\n", argv[0]);
        gtk_exit(0); // TODO: Proper file loading
    }

    // Start up the gameboy and load the ROM.
    gb = thalia_gb_new();
    thalia_gb_load_rom(gb, argv[1], &error);
    if(error)
        thalia_gui_fatal_error("Could not load ROM file", error);

    // Signals us that the screen is ready to be rendered.
    g_signal_connect(
        G_OBJECT(gb),
        "thalia-render-screen",
        G_CALLBACK(thalia_gui_render_screen),
        NULL
    );

    // Build the GUI.
    thalia_gui_make_window();
    thalia_gui_make_menu_bar(vbox);
    thalia_gui_make_screen_area(vbox);
    gtk_widget_show_all(window);

    // Initialize the GLib and GDK threading systems.
    gdk_threads_init();

    // Spawn a background thread to do execution in, so we don't block the GUI.
    g_thread_try_new("emulation", thalia_gui_bg_thread, NULL, &error);
    if(error)
        thalia_gui_fatal_error("Could not spawn background thread", error);

    // Make sure the main event loop executes holding the GDK lock.
    gdk_threads_enter();
    gtk_main ();
    gdk_threads_leave();
    thalia_gui_exit(0);
    return 0; // Keep GCC happy
}
