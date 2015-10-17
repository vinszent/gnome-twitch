#ifndef GT_PLAYER_H
#define GT_PLAYER_H

#include <gtk/gtk.h>
#include "gt-channel.h"

G_BEGIN_DECLS

#define GT_TYPE_PLAYER (gt_player_get_type())

G_DECLARE_INTERFACE(GtPlayer, gt_player, GT, PLAYER, GObject)

struct _GtPlayerInterface
{
    GTypeInterface parent_iface;

    void (*set_uri) (GtPlayer* self, const gchar* uri);
    void (*play) (GtPlayer* self);
    void (*stop) (GtPlayer* self);
};

GtPlayer* gt_player_new(void);

void gt_player_open_channel(GtPlayer* self, GtChannel* chan);
void gt_player_play(GtPlayer* self);
void gt_player_stop(GtPlayer* self);
void gt_player_set_quality(GtPlayer* self, GtTwitchStreamQuality q);

G_END_DECLS

#endif
