#ifndef GT_PLAYER_H
#define GT_PLAYER_H

#include <gtk/gtk.h>
#include "gt-twitch.h"
#include "gt-channel.h"

G_BEGIN_DECLS

#define GT_TYPE_PLAYER (gt_player_get_type())

G_DECLARE_FINAL_TYPE(GtPlayer, gt_player, GT, PLAYER, GtkOverlay)

struct _GtPlayer
{
    GtkOverlay parent_instance;
};

GtPlayer* gt_player_new(void);
void gt_player_open_channel(GtPlayer* self, GtChannel* channel);
void gt_player_play(GtPlayer* self);
void gt_player_pause(GtPlayer* self);
void gt_player_stop(GtPlayer* self);
void gt_player_set_quality(GtPlayer* self, GtTwitchStreamQuality qual);

G_END_DECLS

#endif
