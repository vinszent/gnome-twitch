#ifndef GT_GAME_CONTAINER_VIEW_H
#define GT_GAME_CONTAINER_VIEW_H

#include <glib-object.h>
#include "gt-container-view.h"
#include "gt-game.h"

G_BEGIN_DECLS

#define GT_TYPE_GAME_CONTAINER_VIEW gt_game_container_view_get_type()

G_DECLARE_FINAL_TYPE(GtGameContainerView, gt_game_container_view, GT, GAME_CONTAINER_VIEW, GtContainerView);

struct _GtGameContainerView
{
    GtContainerView parent_instance;
};

void gt_game_container_view_open_game(GtGameContainerView* self, GtGame* game);

G_END_DECLS

#endif
