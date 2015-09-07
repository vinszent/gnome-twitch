#ifndef GT_STREAMS_VIEW_H
#define GT_STREAMS_VIEW_H

#include <gtk/gtk.h>
#include "gt-streams-view-child.h"
#include "gt-twitch-game.h"

G_BEGIN_DECLS

#define GT_TYPE_STREAMS_VIEW (gt_streams_view_get_type())

G_DECLARE_FINAL_TYPE(GtStreamsView, gt_streams_view, GT, STREAMS_VIEW, GtkBox)

struct _GtStreamsView
{
    GtkBox parent_instance;
};

GtStreamsView* gt_streams_view_new(void);
void gt_streams_view_append_streams(GtStreamsView* self, GList* streams);
void gt_streams_view_start_search(GtStreamsView* self);
void gt_streams_view_stop_search(GtStreamsView* self);
void gt_streams_view_show_game_streams(GtStreamsView* self, GtTwitchGame* game);
void gt_streams_view_clear_game_streams(GtStreamsView* self);
void gt_streams_view_refresh(GtStreamsView* self);

G_END_DECLS

#endif
