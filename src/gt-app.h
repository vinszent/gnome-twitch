#ifndef GT_APP_H
#define GT_APP_H

#include <gtk/gtk.h>
#include "gt-twitch.h"
#include "gt-favourites-manager.h"
#include "gt-twitch-chat-client.h"

G_BEGIN_DECLS

#define GT_TYPE_APP (gt_app_get_type())

G_DECLARE_FINAL_TYPE(GtApp, gt_app, GT, APP, GtkApplication)

struct _GtApp
{
    GtkApplication parent_instance;

    GtTwitch* twitch;
    GtFavouritesManager* fav_mgr;
    GSettings* settings;
};

GtApp* gt_app_new(void);

extern GtApp* main_app;

const gchar* gt_app_get_user_name(GtApp* self);
const gchar* gt_app_get_oauth_token(GtApp* self);

G_END_DECLS

#endif
