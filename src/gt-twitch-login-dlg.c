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
} GtTwitchLoginDlgPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchLoginDlg, gt_twitch_login_dlg, GTK_TYPE_DIALOG)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

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
finalise(GObject* obj)
{
    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(obj);
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);

    G_OBJECT_CLASS(gt_twitch_login_dlg_parent_class)->finalize(obj);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(obj);
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj,
             guint prop,
             const GValue* val,
             GParamSpec* pspec)
{
    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(obj);
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_twitch_login_dlg_class_init(GtTwitchLoginDlgClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    WEBKIT_TYPE_WEB_VIEW;

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(widget_class,
                                                "/com/vinszent/GnomeTwitch/ui/gt-twitch-login-dlg.ui");

    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchLoginDlg, web_view);
}

static void
oauth_info_cb(GObject* source,
             GAsyncResult* res,
             gpointer udata)
{
    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(udata);
    GError* error = NULL;

    GtTwitchOAuthInfo* oauth_info = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        gtk_widget_destroy(GTK_WIDGET(self));

        return; //TODO: Show error to user
    }

    MESSAGEF("Successfully got username '%s'", oauth_info->user_name);

    g_object_set(main_app, "user-name", oauth_info->user_name, NULL);

    gt_win_show_info_message(GT_WIN(gtk_window_get_transient_for(GTK_WINDOW(self))),
                             _("Successfully logged in to Twitch!"));

    g_free(oauth_info);
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
        WebKitNavigationPolicyDecision* navigation = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitURIRequest* request = webkit_navigation_policy_decision_get_request(navigation);

        uri = webkit_uri_request_get_uri(request);
    }
#endif

    if (uri == NULL || strlen(uri) == 0) return FALSE;

    INFOF("Redirect uri is '%s'", uri);

    g_regex_match(priv->token_redirect_regex, uri, 0, &match_info);
    if (g_match_info_matches(match_info))
    {
        gchar* token = g_match_info_fetch(match_info, 1);

        g_object_set(main_app, "oauth-token", token, NULL);

        MESSAGEF("Successfully got OAuth token '%s'", token);

        gt_twitch_oauth_info_async(main_app->twitch, oauth_info_cb, self);

        g_free(token);

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

    priv->token_redirect_regex = g_regex_new("http://localhost/#access_token=(\\S+)&scope=chat_login", 0, 0, NULL);

    gtk_widget_init_template(GTK_WIDGET(self));

#ifdef USE_DEPRECATED_WEBKIT
    g_signal_connect(priv->web_view, "navigation-policy-decision-requested", G_CALLBACK(redirect_cb), self);
#else
    g_signal_connect(priv->web_view, "decide-policy", G_CALLBACK(redirect_cb), self);
#endif

    const gchar* uri = g_strdup_printf("https://api.twitch.tv/kraken/oauth2/authorize?response_type=token&client_id=%s&redirect_uri=%s&scope=%s", "afjnp6n4ufzott4atb3xpb8l5a31aav", "http://localhost", "chat_login+user_follows_edit");

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(priv->web_view), uri);
}
