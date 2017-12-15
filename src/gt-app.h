/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GT_APP_H
#define GT_APP_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

#define GT_TYPE_APP (gt_app_get_type())

G_DECLARE_FINAL_TYPE(GtApp, gt_app, GT, APP, GtkApplication)

#include "gt-follows-manager.h"
#include "gt-irc.h"
#include "gt-http.h"

typedef struct
{
    gchar* id;
    gchar* name;
    gchar* oauth_token;
    gchar* display_name;
    gchar* bio;
    gchar* logo_url;
    gchar* type;
    gchar* email;
    gboolean email_verified;
    gboolean partnered;
    gboolean twitter_connected;
    GDateTime* created_at;
    GDateTime* updated_at;

    struct
    {
        gboolean email;
        gboolean push;
    } notifications;
} GtUserInfo;

typedef struct
{
    gboolean valid;
    gchar* oauth_token;
    gchar* user_name;
    gchar* user_id;
    gchar* client_id;
    GStrv scopes;
    GDateTime* created_at;
    GDateTime* updated_at;
} GtOAuthInfo;

#include "gt-twitch.h"

typedef struct _GtAppPrivate GtAppPrivate;

struct _GtApp
{
    GtkApplication parent_instance;

    GtTwitch* twitch;
    GtFollowsManager* fav_mgr;
    GSettings* settings;

    PeasEngine* players_engine;

    SoupSession* soup;

    GtHTTP* http;
};

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
extern const gchar* TWITCH_AUTH_SCOPES[];

gboolean gt_app_is_logged_in(GtApp* self);
void gt_app_set_oauth_info(GtApp* self, GtOAuthInfo* info);
const GtUserInfo* gt_app_get_user_info(GtApp* self);
const GtOAuthInfo* gt_app_get_oauth_info(GtApp* self);
const gchar* gt_app_get_language_filter(GtApp* self);
gboolean gt_app_should_show_notifications(GtApp* self);
void gt_app_queue_soup_message(GtApp* self, const gchar* category, SoupMessage* msg, GCancellable* cancel, GAsyncReadyCallback callback, gpointer udata);

GtUserInfo*       gt_user_info_new();
void              gt_user_info_free(GtUserInfo* info);
GtOAuthInfo*      gt_oauth_info_new();
void              gt_oauth_info_free(GtOAuthInfo* info);

G_END_DECLS

#endif
