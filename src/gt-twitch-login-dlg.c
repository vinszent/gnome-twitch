#include "gt-twitch-login-dlg.h"
#include <webkit2/webkit2.h>

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
gt_twitch_login_dlg_new(GtWin* win)
{
    return g_object_new(GT_TYPE_TWITCH_LOGIN_DLG,
                        "transient-for", win,
                        "use-header-bar", 1,
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
                                                "/com/gnome-twitch/ui/gt-twitch-login-dlg.ui");

    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchLoginDlg, web_view);
}

static void
uri_changed_cb(GObject* source,
               GParamSpec* pspec,
               gpointer udata)
{
    GtTwitchLoginDlg* self = GT_TWITCH_LOGIN_DLG(udata);
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);
    gchar* url;
    GMatchInfo* match_info = NULL;

    g_object_get(source, "uri", &url, NULL);

    g_info("{GtTwitchLoginDlg} Redirect url is '%s'", url);

    g_regex_match(priv->token_redirect_regex, url, 0, &match_info);
    if (g_match_info_matches(match_info))
    {
        gchar* token = g_match_info_fetch(match_info, 1);

        g_object_set(main_app, "oauth-token", token, NULL);

        g_message("{GtTwitchLoginDlg} Successfully got OAuth token '%s'", token);

        gt_win_show_info_message(GT_WIN(gtk_window_get_transient_for(GTK_WINDOW(self))),
                                 "Successfully logged in to Twitch!");

        gtk_widget_destroy(GTK_WIDGET(self));
        g_free(token);

    }
    else if (g_str_has_prefix(url, "http://localhost/?error=access_denied"))
    {
        g_message("{GtTwitchLoginDlg} Error logging or login cancelled");

        gtk_widget_destroy(GTK_WIDGET(self));
    }

    g_match_info_unref(match_info);
    g_free(url);
}

static void
submit_form_cb(WebKitWebView* web_view,
               WebKitFormSubmissionRequest* request,
               gpointer udata)
{
    GHashTable* text_fields = webkit_form_submission_request_get_text_fields(request);

    gchar* user_name = g_hash_table_lookup(text_fields, "username");

    g_message("{GtTwitchLoginDlg} Got username '%s' from form", user_name);

    g_object_set(main_app, "user-name", user_name, NULL);
}

static void
gt_twitch_login_dlg_init(GtTwitchLoginDlg* self)
{
    GtTwitchLoginDlgPrivate* priv = gt_twitch_login_dlg_get_instance_private(self);

    priv->token_redirect_regex = g_regex_new("http://localhost/#access_token=(\\S+)&scope=chat_login", 0, 0, NULL);

    gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect(priv->web_view, "notify::uri", G_CALLBACK(uri_changed_cb), self);
    g_signal_connect(priv->web_view, "submit-form", G_CALLBACK(submit_form_cb), self);

    const gchar* uri = g_strdup_printf("https://api.twitch.tv/kraken/oauth2/authorize?response_type=token&client_id=%s&redirect_uri=%s&scope=%s", "afjnp6n4ufzott4atb3xpb8l5a31aav", "http://localhost", "chat_login");

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(priv->web_view), uri);
}
