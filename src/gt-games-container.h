#ifndef GT_GAMES_CONTAINER_H
#define GT_GAMES_CONTAINER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_GAMES_CONTAINER (gt_games_container_get_type())

G_DECLARE_DERIVABLE_TYPE(GtGamesContainer, gt_games_container, GT, GAMES_CONTAINER, GtkStack)

typedef enum
{
    GT_GAMES_CONTAINER_TYPE_TOP,
    GT_GAMES_CONTAINER_TYPE_SEARCH
} GtGamesContainerType;

struct _GtGamesContainerClass
{
    GtkStackClass parent_class;

    void (*check_empty) (GtGamesContainer* self);
    void (*set_empty_info) (GtGamesContainer* self, const gchar* icon_name, const gchar* title, const gchar* subtitle);
    void (*show_load_spinner) (GtGamesContainer* self, gboolean show);
    void (*append_games) (GtGamesContainer* self, GList* games);
    GtkFlowBox* (*get_games_flow) (GtGamesContainer* self);

    void (*bottom_edge_reached) (GtGamesContainer* self);
    void (*refresh) (GtGamesContainer* self);
    void (*filter) (GtGamesContainer* self, const gchar* query);
};

GtGamesContainer* gt_games_container_new(void);

void gt_games_container_refresh(GtGamesContainer* self);
void gt_games_container_set_filter_query(GtGamesContainer* self, const gchar* query);

G_END_DECLS

#endif
