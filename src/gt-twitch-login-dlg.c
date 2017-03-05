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

#include "gt-twitch-login-dlg.h"
#include "config.h"
#include <glib/gi18n.h>

#define TAG "GtTwitchLoginDlg"
#include "gnome-twitch/gt-log.h"

#ifdef USE_DEPRECATED_WEBKIT
#include <webkit/webkit.h>
#else
#include <webkit2/webkit2.h>
#endif

typedef struct
{
    GtkWidget* web_view;
    GRegex* token_redirect_regex;

    GCancellable* cancel;
} GtTwitchLoginDlgPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchLoginDlg, gt_twitch_login_dlg, GTK_TYPE_DIALOG)

GtTwitchLoginDlg*
gt_twitch_login_dlg_new(GtkWindow* parent)
{
    return g_object_new(GT_TYPE_TWITCH_LOGIN_DLG,
                        "transient-for", parent,
                        "use-header-bar", 1,
                        "title", _("Login to Twitch"),
                        NULL);
}

static void
destroy_cb(GtkWidget* widget,
    gpointer udata)
{
    g_assert(GT_IS_TWITCH_LOGIN_DLG(widget));

    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(widget);
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
}

static void
dispose(GObject* obj)
{
    g_assert(GT_IS_TWITCH_LOGIN_DLG(obj));

    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(obj);
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);

    g_clear_pointer(&priv->token_redirect_regex, (GDestroyNotify) g_regex_unref);
    g_clear_pointer(&priv->cancel, (GDestroyNotify) g_object_unref);

    G_OBJECT_CLASS(gt_twitch_login_dlg_parent_class)->dispose(obj);
}

static void
gt_twitch_login_dlg_class_init(GtTwitchLoginDlgClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    WEBKIT_TYPE_WEB_VIEW;

    obj_class->dispose = dispose;

    gtk_widget_class_set_template_from_resource(widget_class,
        "/com/vinszent/GnomeTwitch/ui/gt-twitch-login-dlg.ui");

    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchLoginDlg, web_view);
}

static void
fetch_oauth_info_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(udata);
    GError* err = NULL;

    GtOAuthInfo* oauth_info = gt_twitch_fetch_oauth_info_finish(main_app->twitch, res, &err);

    if (err)
    {
        GtkWindow* win = gtk_window_get_transient_for(GTK_WINDOW(self));

        g_assert(GT_IS_WIN(win));

        gt_win_show_error_message(GT_WIN(win), "Unable to get user info", err->message);

        g_error_free(err);
    }
    else
    {
        MESSAGEF("Successfully got username '%s' and id '%s'", oauth_info->user_name, oauth_info->user_id);

        gt_app_set_oauth_info(main_app, oauth_info);

        gt_win_show_info_message(GT_WIN(gtk_window_get_transient_for(GTK_WINDOW(self))),
            _("Successfully logged in to Twitch!"));
    }

    gtk_widget_destroy(GTK_WIDGET(self));
}

static gboolean
#ifdef USE_DEPRECATED_WEBKIT
redirect_cb(WebKitWebView* web_view,
            WebKitWebFrame* web_frame,
            WebKitNetworkRequest* request,
            WebKitWebNavigationAction* action,
            WebKitWebPolicyDecision* decision,
            gpointer udata)
#else
redirect_cb(WebKitWebView* web_view,
            WebKitPolicyDecision* decision,
            WebKitPolicyDecisionType type,
            gpointer udata)
#endif
{
    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(udata);
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);
    GMatchInfo* match_info = NULL;
    const gchar* uri = NULL;

#ifdef USE_DEPRECATED_WEBKIT
    uri = webkit_network_request_get_uri(request);
#else
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    {
        WebKitNavigationAction* action = webkit_navigation_policy_decision_get_navigation_action(
            WEBKIT_NAVIGATION_POLICY_DECISION(decision));
        WebKitURIRequest* request = webkit_navigation_action_get_request(action);

        uri = webkit_uri_request_get_uri(request);
    }
#endif

    if (uri == NULL || strlen(uri) == 0) return FALSE;

    MESSAGE("Redirect uri is '%s'", uri);

    g_regex_match(priv->token_redirect_regex, uri, 0, &match_info);

    if (g_match_info_matches(match_info))
    {
        g_autofree gchar* token = g_match_info_fetch(match_info, 1);

        MESSAGEF("Successfully got OAuth token '%s'", token);

        gt_twitch_fetch_oauth_info_async(main_app->twitch, token, fetch_oauth_info_cb, priv->cancel, self);
    }
    else if (g_str_has_prefix(uri, "http://localhost/?error=access_denied"))
        WARNING("Error logging in or login cancelled");

    g_match_info_unref(match_info);

    return FALSE;
}

static void
gt_twitch_login_dlg_init(GtTwitchLoginDlg* self)
{
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);

    g_autofree gchar* scopes = g_strjoinv("+", (gchar**) TWITCH_AUTH_SCOPES);
    g_autofree gchar* scopes_escaped = g_strjoinv("\\+", (gchar**) TWITCH_AUTH_SCOPES);
    g_autofree gchar* regex_str = g_strdup_printf("http:\\/\\/localhost\\/#access_token=(\\S+)&scope=%s", scopes_escaped);

    priv->token_redirect_regex = g_regex_new(regex_str, 0, 0, NULL);
    priv->cancel = g_cancellable_new();

    gtk_widget_init_template(GTK_WIDGET(self));

#ifdef USE_DEPRECATED_WEBKIT
    g_signal_connect(priv->web_view, "navigation-policy-decision-requested", G_CALLBACK(redirect_cb), self);
#else
    g_signal_connect(priv->web_view, "decide-policy", G_CALLBACK(redirect_cb), self);
#endif

    g_signal_connect(self, "destroy", G_CALLBACK(destroy_cb), NULL);

    const gchar* uri = g_strdup_printf("https://api.twitch.tv/kraken/oauth2/authorize?response_type=token&client_id=%s&redirect_uri=%s&scope=%s", "afjnp6n4ufzott4atb3xpb8l5a31aav", "http://localhost", scopes);

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(priv->web_view), uri);
}
