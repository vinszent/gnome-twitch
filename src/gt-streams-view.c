#include "gt-streams-view.h"
#include "gt-twitch-game.h"
#include "gt-app.h"
#include "gt-win.h"
#include "utils.h"
#include <string.h>

#define MAX_QUERY 50
#define NO_GAME ""

typedef struct
{
    GtkWidget* streams_scroll;
    GtkWidget* streams_flow;
    GtkWidget* search_bar;
    GtkWidget* spinner_revealer;

    gint streams_page;

    gchar* search_query;
    gint search_page;

    gchar* game_name;

    GCancellable* cancel;

} GtStreamsViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtStreamsView, gt_streams_view, GTK_TYPE_BOX)

static void
top_streams_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata);
static void
search_streams_cb(GObject* source,
                   GAsyncResult* res,
                   gpointer udata);
enum 
{
    PROP_0,
    PROP_SHOWING_GAME_STREAMS,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtStreamsView*
gt_streams_view_new(void)
{
    return g_object_new(GT_TYPE_STREAMS_VIEW, 
                        NULL);
}

static void
edge_reached_cb(GtkScrolledWindow* scroll,
                GtkPositionType pos,
                gpointer udata)
{
    GtStreamsView* self = GT_STREAMS_VIEW(udata);
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    if (pos == GTK_POS_BOTTOM)
    {
        if (strlen(priv->search_query) == 0)
        {
            gt_twitch_top_streams_async(main_app->twitch, //TODO: Calculate amount needed?
                                         MAX_QUERY,
                                         ++priv->streams_page*MAX_QUERY,
                                         priv->game_name ? priv->game_name : NO_GAME,
                                         priv->cancel,
                                         (GAsyncReadyCallback) top_streams_cb,
                                         self);
        }
        else
        {
            gt_twitch_search_streams_async(main_app->twitch,
                                            priv->search_query,
                                            MAX_QUERY,
                                            ++priv->search_page*MAX_QUERY,
                                            priv->cancel,
                                            (GAsyncReadyCallback) search_streams_cb,
                                            self);
        }
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);
}

static void
top_streams_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata)
{
    GtStreamsView* self = GT_STREAMS_VIEW(udata);
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);
    GError* error = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        g_error_free(error);
        return;
    }

    gt_streams_view_append_streams(self, new);
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), FALSE);
}

static void
search_streams_cb(GObject* source,
                   GAsyncResult* res,
                   gpointer udata)
{
    GtStreamsView* self = GT_STREAMS_VIEW(udata);
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);
    GError* error = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        g_error_free(error);
        return;
    }

    gt_streams_view_append_streams(self, new);
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), FALSE);
} 

static void
child_activated_cb(GtkFlowBox* flow,
                   GtkFlowBoxChild* child,
                   gpointer udata)
{
    GtStreamsViewChild* chan = GT_STREAMS_VIEW_CHILD(child);
    GtTwitchStream* twitch;

    gt_streams_view_child_hide_overlay(chan);

    g_object_get(chan, "twitch-stream", &twitch, NULL);

    gt_win_open_twitch_stream(GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(flow))),
                               twitch);

    g_object_unref(twitch);

}

static void
search_entry_cb(GtkSearchEntry* entry,
                gpointer udata)
{
    GtStreamsView* self = GT_STREAMS_VIEW(udata);
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    priv->search_query = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    priv->search_page = 0;

    if (strlen(priv->search_query) == 0)
    {
        priv->streams_page = 0;
        priv->search_page = 0;
        gt_twitch_top_streams_async(main_app->twitch,
                                     MAX_QUERY,
                                     priv->streams_page*MAX_QUERY,
                                     priv->game_name ? priv->game_name : NO_GAME,
                                     priv->cancel,
                                     (GAsyncReadyCallback) top_streams_cb,
                                     self);


    }
    else
    {
        gt_twitch_search_streams_async(main_app->twitch,
                                        priv->search_query,
                                        MAX_QUERY,
                                        priv->search_page*MAX_QUERY,
                                        priv->cancel,
                                        (GAsyncReadyCallback) search_streams_cb,
                                        self);
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    gtk_container_clear(GTK_CONTAINER(priv->streams_flow));
}

static void
finalize(GObject* object)
{
    GtStreamsView* self = (GtStreamsView*) object;
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_streams_view_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtStreamsView* self = GT_STREAMS_VIEW(obj);
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SHOWING_GAME_STREAMS:
            g_value_set_boolean(val, priv->game_name != NULL);
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
    GtStreamsView* self = GT_STREAMS_VIEW(obj);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_streams_view_class_init(GtStreamsViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_SHOWING_GAME_STREAMS] = g_param_spec_boolean("showing-game-streams",
                                                           "Showing Game Streams",
                                                           "Whether a game is set to show streams",
                                                           FALSE,
                                                           G_PARAM_READABLE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/org/gnome/gnome-twitch/ui/gt-streams-view.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtStreamsView, streams_scroll);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtStreamsView, streams_flow);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtStreamsView, search_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtStreamsView, spinner_revealer);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_entry_cb);
}

static void
gt_streams_view_init(GtStreamsView* self)
{
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    priv->search_query = "";

    priv->streams_page = 0;
    priv->search_page = 0;
    priv->cancel = g_cancellable_new();

    gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect(priv->streams_flow, "child-activated", G_CALLBACK(child_activated_cb), self);
    g_signal_connect(priv->streams_scroll, "edge-reached", G_CALLBACK(edge_reached_cb), self);

    gt_twitch_top_streams_async(main_app->twitch,
                                 MAX_QUERY,
                                 priv->streams_page,
                                 NO_GAME,
                                 priv->cancel,
                                 (GAsyncReadyCallback) top_streams_cb,
                                 self);
}

void
gt_streams_view_append_streams(GtStreamsView* self, GList* streams)
{
    GList* l = NULL;
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    for (l = streams; l != NULL; l = l->next)
    {
        GtStreamsViewChild* child = gt_streams_view_child_new(GT_TWITCH_STREAM(l->data));
        gtk_widget_show_all(GTK_WIDGET(child));
        gtk_container_add(GTK_CONTAINER(priv->streams_flow), GTK_WIDGET(child));
    }
}

void
gt_streams_view_show_game_streams(GtStreamsView* self, GtTwitchGame* game)
{
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    gtk_container_clear(GTK_CONTAINER(priv->streams_flow));
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    g_free(priv->game_name);
    g_object_get(game, "name", &priv->game_name, NULL);

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_GAME_STREAMS]);

    gt_twitch_top_streams_async(main_app->twitch,
                                 MAX_QUERY,
                                 priv->streams_page,
                                 priv->game_name,
                                 priv->cancel,
                                 (GAsyncReadyCallback) top_streams_cb,
                                 self);
}

void
gt_streams_view_clear_game_streams(GtStreamsView* self)
{
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    if (priv->game_name)
        g_free(priv->game_name);
    priv->game_name = NULL;

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_GAME_STREAMS]);

    gtk_container_clear(GTK_CONTAINER(priv->streams_flow));
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    gt_twitch_top_streams_async(main_app->twitch,
                                 MAX_QUERY,
                                 priv->streams_page,
                                 NO_GAME,
                                 priv->cancel,
                                 (GAsyncReadyCallback) top_streams_cb,
                                 self);
}

void
gt_streams_view_start_search(GtStreamsView* self)
{
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(priv->search_bar), TRUE);
}

void
gt_streams_view_stop_search(GtStreamsView* self)
{
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(priv->search_bar), FALSE);
}

void 
gt_streams_view_refresh(GtStreamsView* self)
{
    GtStreamsViewPrivate* priv = gt_streams_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    if (strlen(priv->search_query) == 0)
    {
        priv->streams_page = 0;
        priv->search_page = 0;
        gt_twitch_top_streams_async(main_app->twitch,
                                     MAX_QUERY,
                                     priv->streams_page*MAX_QUERY,
                                     priv->game_name ? priv->game_name : NO_GAME,
                                     priv->cancel,
                                     (GAsyncReadyCallback) top_streams_cb,
                                     self);


    }
    else
    {
        priv->search_page = 0;
        gt_twitch_search_streams_async(main_app->twitch,
                                        priv->search_query,
                                        MAX_QUERY,
                                        priv->search_page*MAX_QUERY,
                                        priv->cancel,
                                        (GAsyncReadyCallback) search_streams_cb,
                                        self);
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    gtk_container_clear(GTK_CONTAINER(priv->streams_flow));
}
