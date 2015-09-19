#include "gt-channels-view.h"
#include "gt-game.h"
#include "gt-app.h"
#include "gt-win.h"
#include "utils.h"
#include <string.h>
#include <json-glib/json-glib.h>

#define FAV_CHANNELS_FILE g_build_filename(g_get_user_data_dir(), "gnome-twitch", "favourite-channels.json", NULL);

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

    GCancellable* cancel;

    GList* favourite_channels;
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
    PROP_SHOWING_GAME_CHANNELS,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsView*
gt_channels_view_new(void)
{
    return g_object_new(GT_TYPE_CHANNELS_VIEW, 
                        NULL);
}

static void
save_favourite_channels(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);
    JsonArray* jarr = json_array_new();
    JsonGenerator* gen = json_generator_new();
    JsonNode* final = json_node_new(JSON_NODE_ARRAY);
    gchar* fp = FAV_CHANNELS_FILE;

    for (GList* l = priv->favourite_channels; l != NULL; l = l->next)
    {
        JsonNode* node = json_gobject_serialize(l->data);
        json_array_add_element(jarr, node);
    }

    final = json_node_init_array(final, jarr);
    json_array_unref(jarr);

    json_generator_set_root(gen, final);
    json_generator_to_file(gen, fp, NULL);

    json_node_free(final);
    g_object_unref(gen);
    g_free(fp);
}

static void
load_favourite_channels(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);
    JsonParser* parse = json_parser_new();
    JsonNode* root;
    JsonArray* jarr;
    gchar* fp = FAV_CHANNELS_FILE;
    GError* err = NULL;

    json_parser_load_from_file(parse, fp, &err);

    if (err)
    {
        g_print("Error loading favourite channels\n%s\n", err->message); //TODO: Change this to a proper logging func
        goto finish;
    }

    root = json_parser_get_root(parse);
    jarr = json_node_get_array(root);

    for (GList* l = json_array_get_elements(jarr); l != NULL; l = l->next)
    {
        GtChannel* chan = GT_CHANNEL(json_gobject_deserialize(GT_TYPE_CHANNEL, l->data));
        g_object_set(chan, "auto-update", TRUE, NULL);
        priv->favourite_channels = g_list_append(priv->favourite_channels, chan);
    }

finish:
    g_object_unref(parse);
    g_free(fp);
}

static void
edge_reached_cb(GtkScrolledWindow* scroll,
                GtkPositionType pos,
                gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    if (pos == GTK_POS_BOTTOM)
    {
        if (strlen(priv->search_query) == 0)
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

    priv->search_query = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    priv->search_page = 0;

    if (strlen(priv->search_query) == 0)
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

static void
channel_favourited_cb(GObject* source,
                      GParamSpec* pspec,
                      gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);
    GtChannel* chan = GT_CHANNEL(source);

    gboolean favourited;

    g_object_get(chan, "favourited", &favourited, NULL);

    if (favourited)
    {
        priv->favourite_channels = g_list_append(priv->favourite_channels, chan);
        g_object_ref(chan);
    }
    else
    {
        GList* found = g_list_find_custom(priv->favourite_channels, chan, (GCompareFunc) gt_channel_compare);
        // Should never return null;

        g_object_unref(G_OBJECT(found->data));
        priv->favourite_channels = g_list_delete_link(priv->favourite_channels, found);
    }

}

static void
shutdown_cb(GApplication* app,
            gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);

    save_favourite_channels(self);
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
        case PROP_SHOWING_GAME_CHANNELS:
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

    props[PROP_SHOWING_GAME_CHANNELS] = g_param_spec_boolean("showing-game-channels",
                                                           "Showing Game Channels",
                                                           "Whether a game is set to show channels",
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

    priv->search_query = "";

    priv->channels_page = 0;
    priv->search_page = 0;
    priv->cancel = g_cancellable_new();

    gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect(priv->channels_flow, "child-activated", G_CALLBACK(child_activated_cb), self);
    g_signal_connect(priv->channels_scroll, "edge-reached", G_CALLBACK(edge_reached_cb), self);
    g_signal_connect(main_app, "shutdown", G_CALLBACK(shutdown_cb), self);

    load_favourite_channels(self);

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

        g_object_set(chan, "favourited", gt_channels_view_is_channel_favourited(self, chan), NULL);
        g_signal_connect(chan, "notify::favourited", G_CALLBACK(channel_favourited_cb), self);

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

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_GAME_CHANNELS]);

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

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_GAME_CHANNELS]);

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

    if (strlen(priv->search_query) == 0)
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

gboolean
gt_channels_view_is_channel_favourited(GtChannelsView* self, GtChannel* chan)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    return g_list_find_custom(priv->favourite_channels, chan, (GCompareFunc) gt_channel_compare) != NULL;
}
