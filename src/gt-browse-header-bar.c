#include "gt-browse-header-bar.h"
#include "gt-win.h"
#include "utils.h"
#include "gt-channels-view.h"
#include "gt-favourites-view.h"

typedef struct
{
    GtChannelsView* channels_view;
    GtGamesView* games_view;
    GtFavouritesView* favourites_view;

    GtkWidget* nav_buttons_revealer;
    GtkWidget* nav_buttons_stack;
    GtkWidget* search_button;
    GtkWidget* refresh_button;
    GtkWidget* refresh_revealer;
    GtkWidget* search_revealer;
} GtBrowseHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtBrowseHeaderBar, gt_browse_header_bar, GTK_TYPE_HEADER_BAR)

enum
{
    PROP_0,
    PROP_CHANNELS_VIEW,
    PROP_GAMES_VIEW,
    PROP_FAVOURITES_VIEW,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtBrowseHeaderBar*
gt_browse_header_bar_new(void)
{
    return g_object_new(GT_TYPE_BROWSE_HEADER_BAR,
                        NULL);
}

static void
show_nav_buttons_cb(GObject* source,
                    GParamSpec* pspec,
                    gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);
    gboolean showing_game_channels = FALSE;
    GtkWidget* view = NULL;

    g_object_get(priv->games_view, "showing-game-channels", &showing_game_channels, NULL);
    g_object_get(win, "visible-view", &view, NULL);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->nav_buttons_revealer),
                                  showing_game_channels && view == GTK_WIDGET(priv->games_view));
}

static void
finalize(GObject* object)
{
    GtBrowseHeaderBar* self = (GtBrowseHeaderBar*) object;
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    G_OBJECT_CLASS(gt_browse_header_bar_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(obj);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNELS_VIEW:
            g_value_set_object(val, priv->channels_view);
            break;
        case PROP_GAMES_VIEW:
            g_value_set_object(val, priv->games_view);
            break;
        case PROP_FAVOURITES_VIEW:
            g_value_set_object(val, priv->favourites_view);
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
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(obj);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNELS_VIEW:
            g_clear_object(&priv->channels_view);
            priv->channels_view = g_value_dup_object(val);
            break;
        case PROP_GAMES_VIEW:
            g_clear_object(&priv->games_view);
            priv->games_view = g_value_dup_object(val);
            break;
        case PROP_FAVOURITES_VIEW:
            g_clear_object(&priv->favourites_view);
            priv->favourites_view = g_value_dup_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
show_refresh_button_cb(GObject* source,
                       GParamSpec* pspec,
                       gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);
    GtkWidget* view = NULL;

    g_object_get(GT_WIN_TOPLEVEL(self), "visible-view", &view, NULL);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->refresh_revealer), view != GTK_WIDGET(priv->favourites_view));

    g_object_unref(view);
}

static void
search_active_cb(GObject* source,
                 GParamSpec* pspec,
                 gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->search_button));

    GtkWidget* visible_view = NULL;

    g_object_get(GT_WIN_TOPLEVEL(self), "visible_view", &visible_view, NULL);

    g_object_set(visible_view, "search-active", active, NULL);

    g_object_unref(visible_view);
}

static void
visible_view_cb(GObject* source,
                 GParamSpec* pspec,
                 gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);
    GtkWidget* visible_view = NULL;
    gboolean search_active = FALSE;

    g_object_get(GT_WIN_TOPLEVEL(self), "visible_view", &visible_view, NULL);
    g_object_get(visible_view, "search-active", &search_active, NULL);

    g_signal_handlers_block_by_func(self, search_active_cb, self);
    g_object_set(priv->search_button, "active", search_active, NULL);
    g_signal_handlers_unblock_by_func(self, search_active_cb, self);

    g_object_unref(visible_view);
}

static void
realize(GtkWidget* widget,
         gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    g_object_bind_property(priv->games_view, "search-active",
                           priv->search_button, "active",
                           G_BINDING_DEFAULT);
    g_object_bind_property(priv->channels_view, "search-active",
                           priv->search_button, "active",
                           G_BINDING_DEFAULT);
    g_object_bind_property(priv->favourites_view, "search-active",
                           priv->search_button, "active",
                           G_BINDING_DEFAULT);

    g_signal_connect(GT_WIN_TOPLEVEL(widget), "notify::visible-view", G_CALLBACK(visible_view_cb), self);
    g_signal_connect(GT_WIN_TOPLEVEL(widget), "notify::visible-view", G_CALLBACK(show_nav_buttons_cb), self);
    g_signal_connect(GT_WIN_TOPLEVEL(widget), "notify::visible-view", G_CALLBACK(show_refresh_button_cb), self);
    g_signal_connect(priv->games_view, "notify::showing-game-channels", G_CALLBACK(show_nav_buttons_cb), self);
    g_signal_connect(priv->search_button, "notify::active", G_CALLBACK(search_active_cb), self);
}

static void
gt_browse_header_bar_class_init(GtBrowseHeaderBarClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_CHANNELS_VIEW] = g_param_spec_object("channels-view",
                                                    "Channels View",
                                                    "Channels View",
                                                    GT_TYPE_CHANNELS_VIEW,
                                                    G_PARAM_READWRITE);
    props[PROP_GAMES_VIEW] = g_param_spec_object("games-view",
                                                 "Games View",
                                                 "Games View",
                                                 GT_TYPE_GAMES_VIEW,
                                                 G_PARAM_READWRITE);
    props[PROP_FAVOURITES_VIEW] = g_param_spec_object("favourites-view",
                                                      "Favourites View",
                                                      "Favourites View",
                                                      GT_TYPE_FAVOURITES_VIEW,
                                                      G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/com/gnome-twitch/ui/gt-browse-header-bar.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, nav_buttons_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, nav_buttons_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, search_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, refresh_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, refresh_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, search_revealer);
}

static void
gt_browse_header_bar_init(GtBrowseHeaderBar* self)
{
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    g_signal_connect(self, "realize", G_CALLBACK(realize), self);

    gtk_widget_init_template(GTK_WIDGET(self));
}
