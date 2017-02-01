#ifndef GT_GAME_H
#define GT_GAME_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_GAME (gt_game_get_type())

G_DECLARE_FINAL_TYPE(GtGame, gt_game, GT, GAME, GInitiallyUnowned)

struct _GtGame
{
    GInitiallyUnowned parent_instance;
};

typedef struct
{
    gint64 id;
    gchar* name;
    gchar* preview_url;
    gchar* logo_url;
    gint64 viewers;
    gint64 channels;
} GtGameData;

GtGame*      gt_game_new(const gchar* name, gint64 id);
void         gt_game_update_from_raw_data(GtGame* self, GtGameData* data);
void         gt_game_list_free(GList* self);
const gchar* gt_game_get_name(GtGame* self);
gboolean     gt_game_get_updating(GtGame* self);
GtGameData*  gt_game_data_new();
void         gt_game_data_free(GtGameData* data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtGameData, gt_game_data_free);

G_END_DECLS

#endif
