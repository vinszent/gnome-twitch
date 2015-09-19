#ifndef GT_GAME_H
#define GT_GAME_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_GAME (gt_game_get_type())

G_DECLARE_FINAL_TYPE(GtGame, gt_game, GT, GAME, GObject)

struct _GtGame
{
    GObject parent_instance;
};

GtGame* gt_game_new(gint64 id);
void gt_game_free_list(GList* self);

G_END_DECLS

#endif
