#include "gt-channels-view.h"
#include "gt-channels-view-child.h"
#include "gt-game.h"
#include "gt-app.h"
#include "gt-win.h"
#include "utils.h"
#include <string.h>

typedef struct
{
    GtkWidget* channels_scroll;
    GtkWidget* channels_flow;
    GtkWidget* search_bar;
    GtkWidget* spinner_revealer;

    gint channels_page;

    gchar* search_query;
    gint search_page;

    gchar* game_name;

    gboolean showing_top_channels;
    gboolean showing_favourites;

    GCancellable* cancel;
} GtChannelsViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsView, gt_channels_view, GTK_TYPE_BOX)

static void
top_channels_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata);
static void
search_channels_cb(GObject* source,
                   GAsyncResult* res,
                   gpointer udata);
enum 
{
    PROP_0,
    PROP_SHOWING_TOP_CHANNELS,
    PROP_SHOWING_FAVOURITES,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsView*
gt_channels_view_new(void)
{
    return g_object_new(GT_TYPE_CHANNELS_VIEW, 
                        NULL);
}

static gboolean
filter_favourites_by_name(GtkFlowBoxChild* _child,
                          gpointer udata)
{
    GtChannelsViewChild* child = GT_CHANNELS_VIEW_CHILD(_child); 
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);
    GtChannel* chan;
    gchar* name;
    gboolean ret = FALSE;

    g_object_get(child, "channel", &chan, NULL);
    g_object_get(chan, "name", &name, NULL); 

    if (!priv->search_query)
        ret = TRUE;
    else
        ret = g_strrstr(name, priv->search_query) != NULL;

    g_free(name);
    g_object_unref(chan);

    return ret;
}

static void
showing_favourites_cb(GObject* source,
                      GParamSpec* pspec,
                      gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(source);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    if (priv->showing_favourites)
    {
        gtk_flow_box_set_filter_func(GTK_FLOW_BOX(priv->channels_flow),
                                     (GtkFlowBoxFilterFunc) filter_favourites_by_name,
                                     self, NULL);
    }
    else
    {
        gtk_flow_box_set_filter_func(GTK_FLOW_BOX(priv->channels_flow),
                                     (GtkFlowBoxFilterFunc) NULL, NULL, NULL);
    }
}

static void
edge_reached_cb(GtkScrolledWindow* scroll,
                GtkPositionType pos,
                gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    //TODO: Don't do this when showing favourites
    if (pos == GTK_POS_BOTTOM)
    {
        if (!priv->search_query || strlen(priv->search_query) == 0)
        {
            gt_twitch_top_channels_async(main_app->twitch, //TODO: Calculate amount needed?
                                         MAX_QUERY,
                                         ++priv->channels_page*MAX_QUERY,
                                         priv->game_name ? priv->game_name : NO_GAME,
                                         priv->cancel,
                                         (GAsyncReadyCallback) top_channels_cb,
                                         self);
        }
        else
        {
            gt_twitch_search_channels_async(main_app->twitch,
                                            priv->search_query,
                                            MAX_QUERY,
                                            ++priv->search_page*MAX_QUERY,
                                            priv->cancel,
                                            (GAsyncReadyCallback) search_channels_cb,
                                            self);
        }
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);
}

static void
top_channels_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);
    GError* error = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        g_error_free(error);
        return;
    }

    gt_channels_view_append_channels(self, new);
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), FALSE);
}

static void
search_channels_cb(GObject* source,
                   GAsyncResult* res,
                   gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);
    GError* error = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        g_error_free(error);
        return;
    }

    gt_channels_view_append_channels(self, new);
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), FALSE);
} 

static void
child_activated_cb(GtkFlowBox* flow,
                   GtkFlowBoxChild* child,
                   gpointer udata)
{
    GtChannelsViewChild* chan = GT_CHANNELS_VIEW_CHILD(child);
    GtChannel* twitch;

    gt_channels_view_child_hide_overlay(chan);

    g_object_get(chan, "channel", &twitch, NULL);

    gt_win_open_channel(GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(flow))),
                               twitch);

    g_object_unref(twitch);

}

static void
search_entry_cb(GtkSearchEntry* entry,
                gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    g_free(priv->search_query);
    priv->search_query = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    priv->search_page = 0;

    if (priv->showing_favourites)
    {
        gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(priv->channels_flow));
    }
    else
    {
        if (!priv->search_query || strlen(priv->search_query) == 0)
        {
            priv->channels_page = 0;
            priv->search_page = 0;
            gt_twitch_top_channels_async(main_app->twitch,
                                         MAX_QUERY,
                                         priv->channels_page*MAX_QUERY,
                                         priv->game_name ? priv->game_name : NO_GAME,
                                         priv->cancel,
                                         (GAsyncReadyCallback) top_channels_cb,
                                         self);


        }
        else
        {
            gt_twitch_search_channels_async(main_app->twitch,
                                            priv->search_query,
                                            MAX_QUERY,
                                            priv->search_page*MAX_QUERY,
                                            priv->cancel,
                                            (GAsyncReadyCallback) search_channels_cb,
                                            self);
        }

        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

        gtk_container_clear(GTK_CONTAINER(priv->channels_flow));
    }


}

static void
finalize(GObject* object)
{
    GtChannelsView* self = (GtChannelsView*) object;
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_channels_view_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(obj);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SHOWING_TOP_CHANNELS:
            g_value_set_boolean(val, priv->showing_top_channels);
            break;
        case PROP_SHOWING_FAVOURITES:
            g_value_set_boolean(val, priv->showing_favourites);
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
    GtChannelsView* self = GT_CHANNELS_VIEW(obj);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(obj);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);


    G_OBJECT_CLASS(gt_channels_view_parent_class)->constructed(obj);
}


static void
gt_channels_view_class_init(GtChannelsViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_SHOWING_TOP_CHANNELS] = g_param_spec_boolean("showing-top-channels",
                                                            "Showing Top Channels",
                                                            "Whether top channels are being shown",
                                                            TRUE,
                                                            G_PARAM_READABLE);
    props[PROP_SHOWING_FAVOURITES] = g_param_spec_boolean("showing-favourites",
                                                          "Showing Favourites",
                                                          "Whether showing favourites",
                                                          FALSE,
                                                          G_PARAM_READABLE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/com/gnome-twitch/ui/gt-channels-view.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsView, channels_scroll);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsView, channels_flow);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsView, search_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsView, spinner_revealer);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_entry_cb);
}

static void
gt_channels_view_init(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    priv->search_query = NULL;

    priv->channels_page = 0;
    priv->search_page = 0;
    priv->cancel = g_cancellable_new();
    priv->showing_top_channels = TRUE;

    gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect(priv->channels_flow, "child-activated", G_CALLBACK(child_activated_cb), self);
    g_signal_connect(priv->channels_scroll, "edge-reached", G_CALLBACK(edge_reached_cb), self);
    g_signal_connect(self, "notify::showing-favourites", G_CALLBACK(showing_favourites_cb), NULL);

    gt_twitch_top_channels_async(main_app->twitch,
                                 MAX_QUERY,
                                 priv->channels_page,
                                 NO_GAME,
                                 priv->cancel,
                                 (GAsyncReadyCallback) top_channels_cb,
                                 self);

}

void
gt_channels_view_append_channels(GtChannelsView* self, GList* channels)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    for (GList* l = channels; l != NULL; l = l->next)
    {
        GtChannel* chan = GT_CHANNEL(l->data);

        GtChannelsViewChild* child = gt_channels_view_child_new(chan);
        gtk_widget_show_all(GTK_WIDGET(child));
        gtk_container_add(GTK_CONTAINER(priv->channels_flow), GTK_WIDGET(child));
    }
}

void
gt_channels_view_show_game_channels(GtChannelsView* self, GtGame* game)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    gtk_container_clear(GTK_CONTAINER(priv->channels_flow));
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    g_free(priv->game_name);
    g_object_get(game, "name", &priv->game_name, NULL);

    priv->showing_top_channels = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_TOP_CHANNELS]);

    gt_twitch_top_channels_async(main_app->twitch,
                                 MAX_QUERY,
                                 priv->channels_page,
                                 priv->game_name,
                                 priv->cancel,
                                 (GAsyncReadyCallback) top_channels_cb,
                                 self);
}

void
gt_channels_view_clear_game_channels(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    if (priv->game_name)
        g_free(priv->game_name);
    priv->game_name = NULL;

    priv->showing_top_channels = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_TOP_CHANNELS]);
    priv->showing_favourites = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_FAVOURITES]);

    gtk_container_clear(GTK_CONTAINER(priv->channels_flow));
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    gt_twitch_top_channels_async(main_app->twitch,
                                 MAX_QUERY,
                                 priv->channels_page,
                                 NO_GAME,
                                 priv->cancel,
                                 (GAsyncReadyCallback) top_channels_cb,
                                 self);
}

void
gt_channels_view_start_search(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(priv->search_bar), TRUE);
}

void
gt_channels_view_stop_search(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(priv->search_bar), FALSE);
}

void 
gt_channels_view_refresh(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    if (!priv->search_query || strlen(priv->search_query) == 0)
    {
        priv->channels_page = 0;
        priv->search_page = 0;
        gt_twitch_top_channels_async(main_app->twitch,
                                     MAX_QUERY,
                                     priv->channels_page*MAX_QUERY,
                                     priv->game_name ? priv->game_name : NO_GAME,
                                     priv->cancel,
                                     (GAsyncReadyCallback) top_channels_cb,
                                     self);


    }
    else
    {
        priv->search_page = 0;
        gt_twitch_search_channels_async(main_app->twitch,
                                        priv->search_query,
                                        MAX_QUERY,
                                        priv->search_page*MAX_QUERY,
                                        priv->cancel,
                                        (GAsyncReadyCallback) search_channels_cb,
                                        self);
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    gtk_container_clear(GTK_CONTAINER(priv->channels_flow));
}

void
gt_channels_view_show_favourites(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), FALSE);
    gtk_container_clear(GTK_CONTAINER(priv->channels_flow));

    gt_channels_view_append_channels(self, main_app->fav_mgr->favourite_channels);

    priv->showing_top_channels = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_TOP_CHANNELS]);
    priv->showing_favourites = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_FAVOURITES]);
}

