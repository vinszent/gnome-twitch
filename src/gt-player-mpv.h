#ifndef GT_PLAYER_MPV_H
#define GT_PLAYER_MPV_H

#include "gt-player.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_MPV gt_player_mpv_get_type()

G_DECLARE_FINAL_TYPE(GtPlayerMpv, gt_player_mpv, GT, PLAYER_MPV, GtPlayer)

struct _GtPlayerMpv
{
    GtPlayer parent_instance;
};

GtPlayerMpv* gt_player_mpv_new();

G_END_DECLS

#endif
