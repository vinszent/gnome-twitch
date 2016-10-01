#ifndef GT_APP_H
#define GT_APP_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include "gt-twitch.h"
#include "gt-follows-manager.h"
#include "gt-irc.h"

G_BEGIN_DECLS

#define GT_TYPE_APP (gt_app_get_type())

G_DECLARE_FINAL_TYPE(GtApp, gt_app, GT, APP, GtkApplication)

struct _GtApp
{
    GtkApplication parent_instance;

    GtTwitch* twitch;
    GtFollowsManager* fav_mgr;
    GSettings* settings;

    PeasEngine* players_engine;

    GHashTable* chat_settings_table; //TODO: Move this into GtChannelsManager when it's done
};

//TODO: Move this into GtChannelsManager when it's done
typedef struct
{
    gchar* name;
    gboolean visible;
    gboolean docked;
    gboolean dark_theme;
    gdouble opacity;
    gdouble width;
    gdouble height;
    gdouble x_pos;
    gdouble y_pos;
    gdouble docked_handle_pos;
} GtChatViewSettings;

GtChatViewSettings* gt_chat_view_settings_new();
GtApp* gt_app_new(void);

extern GtApp* main_app;
extern gchar* ORIGINAL_LOCALE;
extern gint LOG_LEVEL;
extern gboolean NO_FANCY_LOGGING;

const gchar* gt_app_get_user_name(GtApp* self);
const gchar* gt_app_get_oauth_token(GtApp* self);
gboolean gt_app_credentials_valid(GtApp* self);

G_END_DECLS

#endif
