#ifndef GT_WIN_H
#define GT_WIN_H

#include <gtk/gtk.h>
#include "gt-app.h"
#include "gt-channel.h"
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
};

GtWin* gt_win_new(GtApp* app);
void gt_win_open_channel(GtWin* self, GtChannel* chan);
void gt_win_start_search(GtWin* self);
void gt_win_stop_search(GtWin* self);
void gt_win_refresh_view(GtWin* self);
void gt_win_show_follows(GtWin* self);
gboolean gt_win_is_fullscreen(GtWin* self);
void gt_win_toggle_fullscreen(GtWin* self);
void gt_win_show_info_message(GtWin* self, const gchar* msg);
void gt_win_show_error_message(GtWin* self, const gchar* secondary, const gchar* details);
void gt_win_ask_question(GtWin* self, const gchar* msg, GCallback cb, gpointer udata);

G_END_DECLS

#endif
