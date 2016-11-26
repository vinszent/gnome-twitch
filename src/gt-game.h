#ifndef GT_GAME_H
#define GT_GAME_H

#include "gt-twitch.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_GAME (gt_game_get_type())

G_DECLARE_FINAL_TYPE(GtGame, gt_game, GT, GAME, GObject)

struct _GtGame
{
    GObject parent_instance;
};

GtGame* gt_game_new(const gchar* name, gint64 id);
void gt_game_update_from_raw_data(GtGame* self, GtGameRawData* data);
void gt_game_list_free(GList* self);
const gchar* gt_game_get_name(GtGame* self);
gboolean gt_game_get_updating(GtGame* self);

G_END_DECLS

#endif
