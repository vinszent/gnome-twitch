#ifndef GT_CHANNELS_VIEW_H
#define GT_CHANNELS_VIEW_H

#include <gtk/gtk.h>
#include "gt-game.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_VIEW (gt_channels_view_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsView, gt_channels_view, GT, CHANNELS_VIEW, GtkBox)

typedef enum
{
    GT_CHANNELS_CONTAINER_TYPE_TOP,
    GT_CHANNELS_CONTAINER_TYPE_FAVOURITE,
    GT_CHANNELS_CONTAINER_TYPE_SEARCH,
    GT_CHANNELS_CONTAINER_TYPE_GAME
} GtChannelsContainerType;

struct _GtChannelsView
{
    GtkBox parent_instance;
};

GtChannelsView* gt_channels_view_new(void);
void gt_channels_view_refresh(GtChannelsView* self);
void gt_channels_view_show_type(GtChannelsView* self, GtChannelsContainerType type);
void gt_channels_view_set_game(GtChannelsView* self, GtGame* game);

G_END_DECLS

#endif
