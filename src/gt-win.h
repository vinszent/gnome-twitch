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

#ifndef GT_WIN_H
#define GT_WIN_H

#include <gtk/gtk.h>
#include "gt-app.h"
#include "gt-channel.h"
#include "gt-vod.h"
#include "gt-player.h"

#define GT_WIN_TOPLEVEL(w) GT_WIN(gtk_widget_get_ancestor(GTK_WIDGET(w), GTK_TYPE_WINDOW))
#define GT_WIN_ACTIVE GT_WIN(gtk_application_get_active_window(GTK_APPLICATION(main_app)))

G_BEGIN_DECLS

#define GT_TYPE_WIN (gt_win_get_type())

G_DECLARE_FINAL_TYPE(GtWin, gt_win, GT, WIN, GtkApplicationWindow)

struct _GtWin
{
    GtkApplicationWindow parent_instance;

    GtPlayer* player;
    GtChannel* open_channel;
};

GtWin* gt_win_new(GtApp* app);
void gt_win_open_channel(GtWin* self, GtChannel* chan);
void gt_win_play_vod(GtWin* self, GtVOD* vod);
gboolean gt_win_is_fullscreen(GtWin* self);
void gt_win_toggle_fullscreen(GtWin* self);
void gt_win_show_info_message(GtWin* self, const gchar* msg_fmt, ...);
void gt_win_show_error_message(GtWin* self, const gchar* secondary, const gchar* details_fmt, ...);
void gt_win_show_error_dialogue(GtWin* self, const gchar* secondary, const gchar* details_fmt, ...);
void gt_win_ask_question(GtWin* self, const gchar* msg, GCallback cb, gpointer udata);

G_END_DECLS

#endif
