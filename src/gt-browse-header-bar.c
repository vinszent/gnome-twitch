#include "gt-browse-header-bar.h"
#include "gt-win.h"
#include "utils.h"
#include "gt-channels-view.h"

typedef struct
{
    GtChannelsView* channels_view;

    GtkWidget* nav_buttons_revealer;
    GtkWidget* nav_buttons_stack;
    GtkWidget* search_button;
    GtkWidget* refresh_button;
    GtkWidget* refresh_revealer;
} GtBrowseHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtBrowseHeaderBar, gt_browse_header_bar, GTK_TYPE_HEADER_BAR)

enum 
{
    PROP_0,
    PROP_CHANNELS_VIEW,
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
search_button_cb(GtkToggleButton* button,
                 gpointer udata)
{

    if (gtk_toggle_button_get_active(button))
        gt_win_start_search(GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(button))));
    else
        gt_win_stop_search(GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(button))));
}

static void
home_button_cb(GtkButton* button,
               gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    gt_channels_view_clear_game_channels(priv->channels_view);
}

static void
refresh_button_cb(GtBrowseHeaderBar* self,
                  GtkButton* button)
{
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);
    GtWin* win = GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(self)));

    gt_win_refresh_view(win);
}

static void
favourites_button_cb(GtkButton* button,
                     gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self); 

    gt_win_show_favourites(GT_WIN_TOPLEVEL(button));
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
            if (priv->channels_view)
                g_object_unref(priv->channels_view);
            priv->channels_view = g_value_ref_sink_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
showing_top_channels_converter(GBinding* bind,
                               const GValue* from,
                               GValue* to,
                               gpointer udata)
{
    if (g_value_get_boolean(from))
        g_value_set_string(to, "favourites");
    else
        g_value_set_string(to, "home");
}

static void
show_refresh_button_cb(GObject* source,
                       GParamSpec* pspec,
                       gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);
    gboolean showing_channels = FALSE;
    gboolean showing_favourites = FALSE;
     
    g_object_get(GT_WIN_TOPLEVEL(self), "showing-channels", &showing_channels, NULL);
    g_object_get(priv->channels_view, "showing-favourites", &showing_favourites, NULL);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->refresh_revealer), !(showing_channels && showing_favourites));
}

static void
realize(GtkWidget* widget,
         gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    g_object_bind_property_full(priv->channels_view,
                                "showing-top-channels",
                                priv->nav_buttons_stack,
                                "visible-child-name",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) showing_top_channels_converter,
                                NULL, NULL, NULL);
    g_object_bind_property(GT_WIN_TOPLEVEL(widget), "showing-channels",
                           priv->nav_buttons_revealer, "reveal-child",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_signal_connect(GT_WIN_TOPLEVEL(widget), "notify::showing-channels", G_CALLBACK(show_refresh_button_cb), self);
    g_signal_connect(priv->channels_view, "notify::showing-favourites", G_CALLBACK(show_refresh_button_cb), self);
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
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_button_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), refresh_button_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), home_button_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), favourites_button_cb);
}

static void
gt_browse_header_bar_init(GtBrowseHeaderBar* self)
{
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    g_signal_connect(self, "realize", G_CALLBACK(realize), self);

    gtk_widget_init_template(GTK_WIDGET(self));
}

void
gt_browse_header_bar_stop_search(GtBrowseHeaderBar* self)
{
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->search_button), FALSE);
}
