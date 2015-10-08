#ifndef GT_GAMES_VIEW_H
#define GT_GAMES_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_GAMES_VIEW (gt_games_view_get_type())

G_DECLARE_FINAL_TYPE(GtGamesView, gt_games_view, GT, GAMES_VIEW, GtkBox)

typedef enum
{
    GT_GAMES_CONTAINER_TYPE_TOP,
    GT_GAMES_CONTAINER_TYPE_SEARCH
} GtGamesContainerType;

struct _GtGamesView
{
    GtkBox parent_instance;
};

GtGamesView* gt_games_view_new(void);
void gt_games_view_refresh(GtGamesView* self);
void gt_games_view_show_type(GtGamesView* self, GtGamesContainerType type);

G_END_DECLS

#endif
