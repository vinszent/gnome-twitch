#include "gt-browse-header-bar.h"
#include "gt-win.h"
#include "utils.h"
#include "gt-streams-view.h"

typedef struct
{
    GtStreamsView* streams_view;

    GtkWidget* home_button_revealer;
    GtkWidget* search_button;
} GtBrowseHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtBrowseHeaderBar, gt_browse_header_bar, GTK_TYPE_HEADER_BAR)

enum 
{
    PROP_0,
    PROP_STREAMS_VIEW,
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

    gt_streams_view_clear_game_streams(priv->streams_view);
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
        case PROP_STREAMS_VIEW:
            g_value_set_object(val, priv->streams_view);
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
        case PROP_STREAMS_VIEW:
            if (priv->streams_view)
                g_object_unref(priv->streams_view);
            priv->streams_view = g_value_ref_sink_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
realize(GtkWidget* widget,
         gpointer udata)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(udata);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    g_object_bind_property(priv->streams_view,
                           "showing-game-streams",
                           priv->home_button_revealer,
                           "reveal-child",
                            G_BINDING_DEFAULT);

}

static void
gt_browse_header_bar_class_init(GtBrowseHeaderBarClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_STREAMS_VIEW] = g_param_spec_object("streams-view",
                           "Streams View",
                           "Streams View",
                           GT_TYPE_STREAMS_VIEW,
                           G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/org/gnome/gnome-twitch/ui/gt-browse-header-bar.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, home_button_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, search_button);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_button_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), refresh_button_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), home_button_cb);
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
