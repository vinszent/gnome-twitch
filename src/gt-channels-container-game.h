#ifndef GT_CHANNELS_CONTAINER_GAME_H
#define GT_CHANNELS_CONTAINER_GAME_H

#include <gtk/gtk.h>
#include "gt-channels-container.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_CONTAINER_GAME (gt_channels_container_game_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsContainerGame, gt_channels_container_game, GT, CHANNELS_CONTAINER_GAME, GtChannelsContainer)

struct _GtChannelsContainerGame
{
    GtChannelsContainer parent_instance;
};

GtChannelsContainerGame* gt_channels_container_game_new(void);

G_END_DECLS

#endif
