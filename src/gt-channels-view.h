#ifndef GT_CHANNELS_VIEW_H
#define GT_CHANNELS_VIEW_H

#include <gtk/gtk.h>
#include "gt-channels-view-child.h"
#include "gt-twitch-game.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_VIEW (gt_channels_view_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsView, gt_channels_view, GT, CHANNELS_VIEW, GtkBox)

struct _GtChannelsView
{
    GtkBox parent_instance;
};

GtChannelsView* gt_channels_view_new(void);
void gt_channels_view_append_channels(GtChannelsView* self, GList* channels);
void gt_channels_view_start_search(GtChannelsView* self);
void gt_channels_view_stop_search(GtChannelsView* self);
void gt_channels_view_show_game_channels(GtChannelsView* self, GtTwitchGame* game);
void gt_channels_view_clear_game_channels(GtChannelsView* self);
void gt_channels_view_refresh(GtChannelsView* self);

G_END_DECLS

#endif
