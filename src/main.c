/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <math.h>
#include <locale.h>
#include "gt-app.h"
#include "config.h"
#include "utils.h"

#define TAG "Main"
#include "gnome-twitch/gt-log.h"

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#endif

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

GtApp* main_app;
gchar* ORIGINAL_LOCALE;

static void
gt_log(const gchar* domain,
       gint _level,
       const gchar* msg,
       gpointer udata)
{
    gchar* level;
    GDateTime* date = NULL;
    gchar* time_fmt = NULL;
    gchar* colour = WHT;

    if ((_level >= 1 << G_LOG_LEVEL_USER_SHIFT //GT levels
         && _level > LOG_LEVEL) ||
        (_level < 1 << G_LOG_LEVEL_USER_SHIFT //GLib levels
         && _level > G_LOG_LEVEL_WARNING))
    {
        return;
    }

    switch (_level)
    {
        case G_LOG_LEVEL_ERROR:
        case GT_LOG_LEVEL_ERROR:
            level = "Error";
            colour = RED;
            break;
        case G_LOG_LEVEL_CRITICAL:
        case GT_LOG_LEVEL_CRITICAL:
            level = "Critical";
            colour = YEL;
            break;
        case G_LOG_LEVEL_WARNING:
        case GT_LOG_LEVEL_WARNING:
            level = "Warning";
            colour = MAG;
            break;
        case GT_LOG_LEVEL_MESSAGE:
            level = "Message";
            colour = GRN;
            break;
        case G_LOG_LEVEL_INFO:
        case GT_LOG_LEVEL_INFO:
            level = "Info";
            colour = CYN;
            break;
        case GT_LOG_LEVEL_DEBUG:
            level = "Debug";
            colour = BLU;
            break;
        case GT_LOG_LEVEL_TRACE:
            level = "Trace";
            break;
        default:
            level = "Unknown";
            break;
    }

    date = g_date_time_new_now_utc();
    time_fmt = g_date_time_format(date, "%H:%M:%S");

    if (NO_FANCY_LOGGING)
        g_print("[%s] %s - %s : %s\n",
                time_fmt, level, domain ? domain : "GNOME-Twitch", msg);
    else
        g_print("\e[1m[%s]\e[0m %s%s%s - %s : \e[3m%s\e[0m\n",
                time_fmt, colour, level, RESET, domain ? domain : "GNOME-Twitch", msg);


    g_free(time_fmt);
    g_date_time_unref(date);

}

int main(int argc, char** argv)
{
#ifdef GDK_WINDOWING_X11
    XInitThreads();
#endif

    bindtextdomain("gnome-twitch", GT_LOCALE_DIR);
    bind_textdomain_codeset("gnome-twitch", "UTF-8");
    textdomain("gnome-twitch");

    g_log_set_default_handler((GLogFunc) gt_log, NULL);

    ORIGINAL_LOCALE = g_strdup(setlocale(LC_NUMERIC, NULL));

    main_app = gt_app_new();

    gint ret = g_application_run(G_APPLICATION(main_app), argc, argv);

    g_object_unref(main_app);

    return ret;
}
