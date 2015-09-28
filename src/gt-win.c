#include "gt-win.h"
#include <gdk/gdkx.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include "gt-twitch.h"
#include "gt-player.h"
#include "gt-player-header-bar.h"
#include "gt-browse-header-bar.h"
#include "gt-channels-view.h"
#include "gt-games-view.h"
#include "gt-settings-dlg.h"
#include "gt-enums.h"
#include "utils.h"
#include "config.h"

typedef struct
{
    GtkWidget* main_stack;
    GtkWidget* channels_view;
    GtkWidget* games_view;
    GtkWidget* player;
    GtkWidget* header_stack;
    GtkWidget* browse_stack;
    GtkWidget* player_header_bar;
    GtkWidget* browse_header_bar;
    GtkWidget* browse_stack_switcher;

    gboolean fullscreen;
    gboolean showing_channels;
} GtWinPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtWin, gt_win, GTK_TYPE_APPLICATION_WINDOW)

enum 
{
    PROP_0,
    PROP_CHANNELS_VIEW,
    PROP_GAMES_VIEW,
    PROP_FULLSCREEN,
    PROP_SHOWING_CHANNELS,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtWin*
gt_win_new(GtApp* app)
{
    return g_object_new(GT_TYPE_WIN, 
                        "application", app,
                        NULL);
}

static void
player_set_quality_cb(GSimpleAction* action,
                      GVariant* par,
                      gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    GEnumClass* eclass;

    eclass = g_type_class_ref(GT_TYPE_TWITCH_STREAM_QUALITY);

    GEnumValue* qual = g_enum_get_value_by_nick(eclass, 
                                                g_variant_get_string(par, NULL));

    gt_player_set_quality(GT_PLAYER(priv->player), qual->value);

    g_simple_action_set_state(action, par);

    g_type_class_unref(eclass);
}

static void
show_about_cb(GSimpleAction* action,
              GVariant* par,
              gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    char* authors[] = {"Vincent Szolnoky", NULL};

    gtk_show_about_dialog(GTK_WINDOW(self),
                          "version", GT_VERSION,
                          "program-name", "GNOME Twitch",
                          "authors", &authors,
                          "license-type", GTK_LICENSE_GPL_3_0,
                          "copyright", "Copyright © 2015 Vincent Szolnoky",
                          "comments", _("Enjoy Twitch on your GNOME desktop"),
                          "logo-icon-name", "gnome-twitch",
                          "website", "https://github.com/Ippytraxx/gnome-twitch",
                          "website-label", "Github",
                          NULL);
}

static void
show_settings_cb(GSimpleAction* action,
                 GVariant* par,
                 gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    GtSettingsDlg* settings_dlg = gt_settings_dlg_new(self);

    gtk_window_present(GTK_WINDOW(settings_dlg));
}

static GActionEntry win_actions[] = 
{
    {"player_set_quality", player_set_quality_cb, "s", "'source'", NULL},
    {"show_about", show_about_cb, NULL, NULL, NULL},
    {"show_settings", show_settings_cb, NULL, NULL, NULL}
};

static void
window_state_cb(GtkWidget* widget,
                GdkEventWindowState* evt,
                gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (evt->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if (evt->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
            priv->fullscreen = TRUE;
        else
            priv->fullscreen = FALSE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_FULLSCREEN]);
    }

}

static void
browse_view_changed_cb(GtkWidget* child,
                       GParamSpec* pspec,
                       gpointer udata)
{
    GtWin* self = GT_WIN(udata);

    gt_win_stop_search(self);
}

static gboolean
visible_child_converter(GBinding* bind,
                        const GValue* from,
                        GValue* to,
                        gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (g_value_get_object(from) == priv->channels_view)
        g_value_set_boolean(to, TRUE);
    else    
        g_value_set_boolean(to, FALSE);

    return TRUE;
}

static void
finalize(GObject* object)
{
    GtWin* self = (GtWin*) object;
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    G_OBJECT_CLASS(gt_win_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtWin* self = GT_WIN(obj);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNELS_VIEW:
            g_value_set_object(val, priv->channels_view);
            break;
        case PROP_GAMES_VIEW:
            g_value_set_object(val, priv->games_view);
            break;
        case PROP_FULLSCREEN:
            g_value_set_boolean(val, priv->fullscreen);
            break;
        case PROP_SHOWING_CHANNELS:
            g_value_set_boolean(val, priv->showing_channels);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject*      obj,
             guint         prop,
             const GValue* val,
             GParamSpec*   pspec)
{
    GtWin* self = GT_WIN(obj);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    switch (prop)
    {
        case PROP_SHOWING_CHANNELS:
            priv->showing_channels = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_win_class_init(GtWinClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_CHANNELS_VIEW] = g_param_spec_object("channels-view",
                           "Channels View",
                           "Channels View",
                           GT_TYPE_CHANNELS_VIEW,
                           G_PARAM_READABLE);

    props[PROP_GAMES_VIEW] = g_param_spec_object("games-view",
                           "Games View",
                           "Games View",
                           GT_TYPE_GAMES_VIEW,
                           G_PARAM_READABLE);
    props[PROP_FULLSCREEN] = g_param_spec_boolean("fullscreen",
                                                  "Fullscreen",
                                                  "Whether window is fullscreen",
                                                  FALSE,
                                                  G_PARAM_READABLE);
    props[PROP_SHOWING_CHANNELS] = g_param_spec_boolean("showing-channels",
                                                        "Showing Channels",
                                                        "Whether showing channels",
                                                        TRUE,
                                                        G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/com/gnome-twitch/ui/gt-win.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, main_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, channels_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, games_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, player);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, header_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, player_header_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_header_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_stack_switcher);
}

static void
gt_win_init(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    GT_TYPE_PLAYER; // Hack to load GtPlayer into the symbols table
    GT_TYPE_PLAYER_HEADER_BAR;
    GT_TYPE_BROWSE_HEADER_BAR;
    GT_TYPE_CHANNELS_VIEW;
    GT_TYPE_GAMES_VIEW;

    gtk_widget_init_template(GTK_WIDGET(self));

    g_object_set(self, "application", main_app, NULL); // Another hack because GTK is bugged and resets the app menu when using custom widgets

    gtk_widget_realize(GTK_WIDGET(priv->player));

    g_signal_connect(priv->browse_stack, "notify::visible-child", G_CALLBACK(browse_view_changed_cb), self);
    g_object_bind_property(priv->channels_view, "showing-favourites",
                           priv->browse_stack_switcher, "sensitive",
                           G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN);

    g_object_bind_property_full(priv->browse_stack, "visible-child",
                                self, "showing-channels",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) visible_child_converter,
                                NULL, self, NULL);

    GdkScreen* screen = gdk_screen_get_default();
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/com/gnome-twitch/gnome-twitch-style.css");
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(self, "window-state-event", G_CALLBACK(window_state_cb), self);

    g_action_map_add_action_entries(G_ACTION_MAP(self),
                                    win_actions,
                                    G_N_ELEMENTS(win_actions),
                                    self);
}

void
gt_win_open_channel(GtWin* self, GtChannel* chan)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gchar* status; gchar* display_name; gchar* name; gchar* token; gchar* sig;
    g_object_get(chan, 
                 "display-name", &display_name,
                 "name", &name,
                 "status", &status,
                 NULL);

    g_object_set(priv->player_header_bar,
                 "name", display_name,
                 "status", status,
                 NULL);


    gt_player_open_channel(GT_PLAYER(priv->player), chan);

    g_free(name);
    g_free(status);
    g_free(display_name);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack),
                                     "player");
    gtk_stack_set_visible_child_name(GTK_STACK(priv->header_stack),
                                     "player");
}

void
gt_win_browse_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->header_stack),
                                     "browse");
    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack),
                                     "browse");
}

void
gt_win_browse_channels_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gt_win_browse_view(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->browse_stack),
                                     "channels");
}

void
gt_win_browse_games_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gt_win_browse_view(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->browse_stack),
                                     "games");
}

void
gt_win_start_search(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)) == priv->channels_view)
        gt_channels_view_start_search(GT_CHANNELS_VIEW(priv->channels_view));
    else
        gt_games_view_start_search(GT_GAMES_VIEW(priv->games_view));
}

void
gt_win_stop_search(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)) == priv->channels_view)
        gt_channels_view_stop_search(GT_CHANNELS_VIEW(priv->channels_view));
    else
        gt_games_view_stop_search(GT_GAMES_VIEW(priv->games_view));

    gt_browse_header_bar_stop_search(GT_BROWSE_HEADER_BAR(priv->browse_header_bar));
}

void
gt_win_refresh_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)) == priv->channels_view)
        gt_channels_view_refresh(GT_CHANNELS_VIEW(priv->channels_view));
    else
        gt_games_view_refresh(GT_GAMES_VIEW(priv->games_view));
}

void
gt_win_show_favourites(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)) == priv->channels_view)
        gt_channels_view_show_favourites(GT_CHANNELS_VIEW(priv->channels_view));
    //TODO: Favouriting games?
}

GtChannelsView*
gt_win_get_channels_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    return GT_CHANNELS_VIEW(priv->channels_view);
}

GtGamesView*
gt_win_get_games_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    return GT_GAMES_VIEW(priv->games_view);
}
