#include <gtk/gtk.h>
#include <clutter-gst/clutter-gst.h>
#include <clutter-gtk/clutter-gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <math.h>
#include "gt-app.h"
#include "config.h"

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#endif

GtApp* main_app;

static gint LOG_LEVEL = 2;

static GOptionEntry cli_options[] =
{
    {"log-level", 'l', 0, G_OPTION_ARG_INT, &LOG_LEVEL, "Set logging level", "Level"},
    {NULL},
};

static void
gt_log(const gchar* domain,
       GLogLevelFlags _level,
       const gchar* msg,
       gpointer udata)
{
    gchar* level;
    GDateTime* date = NULL;
    gchar* time_fmt = NULL;

    if (_level > 8*pow(2, LOG_LEVEL))
        return;

    switch (_level)
    {
        case G_LOG_LEVEL_ERROR:
            level = "Error";
            break;
        case G_LOG_LEVEL_CRITICAL:
            level = "Critical";
            break;
        case G_LOG_LEVEL_WARNING:
            level = "Warning";
            break;
        case G_LOG_LEVEL_MESSAGE:
            level = "Message";
            break;
        case G_LOG_LEVEL_INFO:
            level = "Info";
            break;
        case G_LOG_LEVEL_DEBUG:
            level = "Debug";
            break;
        case G_LOG_LEVEL_MASK:
            level = "All";
            break;
        default:
            level = "Unkown";
            break;
    }

    date = g_date_time_new_now_utc();
    time_fmt = g_date_time_format(date, "%H:%M:%S");

    g_print("[%s] %s - %s : \"%s\"\n", time_fmt, domain, level, msg);

    g_free(time_fmt);
    g_date_time_unref(date);
}

int main(int argc, char** argv)
{
    GError* err = NULL;
    GOptionContext* opt_ctxt;

    opt_ctxt = g_option_context_new(NULL);
    g_option_context_add_main_entries(opt_ctxt, cli_options, "gnome-twitch");
    if (!g_option_context_parse(opt_ctxt, &argc, &argv, &err))
    {
	g_critical("Could not parse CLI options code '%d' message '%s'", err->code, err->message);
	exit(1);
    }

#ifdef GDK_WINDOWING_X11
    XInitThreads();
#endif
    gtk_clutter_init(NULL, NULL);

    bindtextdomain("gnome-twitch", GT_LOCALE_DIR);
    bind_textdomain_codeset("gnome-twitch", "UTF-8");
    textdomain("gnome-twitch");

    g_log_set_default_handler((GLogFunc) gt_log, NULL);

    main_app = gt_app_new();

    return g_application_run(G_APPLICATION(main_app), argc, argv);
}
