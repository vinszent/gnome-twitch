#ifndef GT_APP_H
#define GT_APP_H

#include <gtk/gtk.h>
#include "gt-twitch.h"
#include "gt-favourites-manager.h"

G_BEGIN_DECLS

#define GT_TYPE_APP (gt_app_get_type())

G_DECLARE_FINAL_TYPE(GtApp, gt_app, GT, APP, GtkApplication)

struct _GtApp
{
    GtkApplication parent_instance;

    GtTwitch* twitch;
    GtFavouritesManager* fav_mgr;
};

GtApp* gt_app_new(void);

extern GtApp* main_app;

G_END_DECLS

#endif
