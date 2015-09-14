#include <gtk/gtk.h>
#include <gst/gst.h>
#include <glib/gi18n.h>
#include "gt-app.h"

GtApp* main_app;

int main(int argc, char** argv)
{
    bindtextdomain("gnome-twitch", "/usr/share/locale"); //FIXME: Wait for a reply from meson people on how to get the locale dir.
    bind_textdomain_codeset("gnome-twitch", "UTF-8");
    textdomain("gnome-twitch");

    gst_init(0, NULL);

    main_app = gt_app_new();

    return g_application_run(G_APPLICATION(main_app), argc, argv);
}
