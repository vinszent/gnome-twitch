#ifndef GT_WIN_H
#define GT_WIN_H

#include <gtk/gtk.h>
#include "gt-app.h"
#include "gt-twitch-stream.h"
#include "gt-games-view.h"
#include "gt-streams-view.h"

G_BEGIN_DECLS

#define GT_TYPE_WIN (gt_win_get_type())

G_DECLARE_FINAL_TYPE(GtWin, gt_win, GT, WIN, GtkApplicationWindow)

struct _GtWin
{
    GtkApplicationWindow parent_instance;
};

GtWin* gt_win_new(GtApp* app);
void gt_win_open_twitch_stream(GtWin* self, GtTwitchStream* chan);
void gt_win_browse_view(GtWin* self);
void gt_win_browse_streams_view(GtWin* self);
void gt_win_browse_games_view(GtWin* self);
void gt_win_start_search(GtWin* self);
void gt_win_stop_search(GtWin* self);
void gt_win_refresh_view(GtWin* self);
GtGamesView* gt_win_get_games_view(GtWin* self);
GtStreamsView* gt_win_get_streams_view(GtWin* self);

G_END_DECLS

#endif
