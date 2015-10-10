#include <gtk/gtk.h>
#include <clutter-gst/clutter-gst.h>
#include <clutter-gtk/clutter-gtk.h>
#include <glib/gi18n.h>
#include "gt-app.h"
#include "config.h"

GtApp* main_app;

int main(int argc, char** argv)
{
    bindtextdomain("gnome-twitch", GT_LOCALE_DIR);
    bind_textdomain_codeset("gnome-twitch", "UTF-8");
    textdomain("gnome-twitch");

    /* gst_init(0, NULL); */
    /* gtk_clutter_init(0, NULL); */
    clutter_gst_init(0, NULL);

    main_app = gt_app_new();

    return g_application_run(G_APPLICATION(main_app), argc, argv);
}
