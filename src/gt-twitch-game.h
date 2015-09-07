#ifndef GT_TWITCH_GAME_H
#define GT_TWITCH_GAME_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_TWITCH_GAME (gt_twitch_game_get_type())

G_DECLARE_FINAL_TYPE(GtTwitchGame, gt_twitch_game, GT, TWITCH_GAME, GObject)

struct _GtTwitchGame
{
    GObject parent_instance;
};

GtTwitchGame* gt_twitch_game_new(gint64 id);
void gt_twitch_game_free_list(GList* self);

G_END_DECLS

#endif
