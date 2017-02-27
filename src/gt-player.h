#ifndef GT_PLAYER_H
#define GT_PLAYER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER (gt_player_get_type())

G_DECLARE_FINAL_TYPE(GtPlayer, gt_player, GT, PLAYER, GtkStack)

#include "gt-channel.h"
#include "gt-twitch.h"

struct _GtPlayer
{
    GtkStack parent_instance;
};

void gt_player_open_channel(GtPlayer* self, GtChannel* chan);
void gt_player_close_channel(GtPlayer* self);
void gt_player_play(GtPlayer* self);
void gt_player_stop(GtPlayer* self);
void gt_player_set_quality(GtPlayer* self, const gchar* quality);
void gt_player_toggle_muted(GtPlayer* self);
GtChannel* gt_player_get_channel(GtPlayer* self);
GList* gt_player_get_available_stream_qualities(GtPlayer* self);
gboolean gt_player_is_playing(GtPlayer* self);

G_END_DECLS

#endif
