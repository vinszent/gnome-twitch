#include <libpeas-gtk/peas-gtk.h>
#include <glib/gi18n.h>
#include "gt-player.h"
#include "gt-twitch.h"
#include "gt-win.h"
#include "gt-app.h"
#include "gt-enums.h"
#include "gt-chat.h"
#include "gnome-twitch/gt-player-backend.h"
#include "utils.h"

#define TAG "GtPlayer"
#include "gnome-twitch/gt-log.h"

#define FULLSCREEN_BAR_REVEAL_HEIGHT 50
#define CHAT_RESIZE_HANDLE_SIZE 10

typedef enum
{
    MOUSE_POS_LEFT_HANDLE,
    MOUSE_POS_RIGHT_HANDLE,
    MOUSE_POS_TOP_HANDLE,
    MOUSE_POS_BOTTOM_HANDLE,
    MOUSE_POS_INSIDE,
    MOUSE_POS_OUTSIDE,
} MousePos;

typedef struct
{
    GSimpleActionGroup* action_group;

    GtTwitchStreamQuality quality;

    GtChatViewSettings* chat_settings;

    GtkWidget* empty_box;
    GtkWidget* player_overlay;
    GtkWidget* docking_pane;
    GtkWidget* chat_view;
    GtkWidget* fullscreen_bar_revealer;
    GtkWidget* fullscreen_bar;
    GtkWidget* buffer_revealer;
    GtkWidget* buffer_label;
    GtkWidget* player_widget;

    GtPlayerBackend* backend;
    PeasPluginInfo* backend_info;

    GtChannel* channel;

    gdouble volume;
    gdouble prev_volume;
    gdouble muted;
    gboolean playing;
    gboolean edit_chat;

    MousePos start_mouse_pos;
    gboolean mouse_pressed;

    guint mouse_pressed_handler_id;
    guint mouse_released_handler_id;
    guint mouse_moved_handler_id;

    guint inhibitor_cookie;
    guint mouse_source;
} GtPlayerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayer, gt_player, GTK_TYPE_EVENT_BOX)

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_MUTED,
    PROP_CHANNEL,
    PROP_PLAYING,
    PROP_CHAT_VISIBLE,
    PROP_CHAT_DOCKED,
    PROP_CHAT_DARK_THEME,
    PROP_CHAT_OPACITY,
    PROP_DOCKED_HANDLE_POSITION,
    PROP_EDIT_CHAT,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];


static void
finalise(GObject* obj)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    G_OBJECT_CLASS(gt_player_parent_class)->finalize(obj);

    //TODO: Unref stuff

    g_object_unref(priv->chat_view);
    g_object_unref(priv->backend);
    g_boxed_free(PEAS_TYPE_PLUGIN_INFO, priv->backend_info);

    g_settings_set_double(main_app->settings, "volume",
                          priv->muted ? priv->prev_volume : priv->volume);

    MESSAGE("Finalise");
}

static void
destroy_cb(GtkWidget* widget,
           gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    g_object_unref(priv->action_group);
}

static void
update_docked(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (priv->chat_settings->docked)
    {
        gdouble width;

        if (gtk_widget_get_parent(priv->chat_view) == priv->player_overlay)
            gtk_container_remove(GTK_CONTAINER(priv->player_overlay), priv->chat_view);

        gtk_paned_pack2(GTK_PANED(priv->docking_pane), priv->chat_view, FALSE, TRUE);

        width = gtk_widget_get_allocated_width(GTK_WIDGET(self));

        gtk_paned_set_position(GTK_PANED(priv->docking_pane),
            priv->chat_settings->docked_handle_pos*width);

        g_object_set(priv->chat_view,
            "opacity", 1.0,
            "width-request", -1,
            "height-request", -1,
            "margin-start", 0,
            "margin-top", 0,
            "valign", GTK_ALIGN_FILL,
            "halign", GTK_ALIGN_FILL,
            NULL);
    }
    else
    {
        GtkAllocation alloc;
        gint width_request;
        gint height_request;
        gint margin_top;
        gint margin_start;

        if (gtk_widget_get_parent(priv->chat_view) == priv->docking_pane)
            gtk_container_remove(GTK_CONTAINER(priv->docking_pane), priv->chat_view);

        gtk_overlay_add_overlay(GTK_OVERLAY(priv->player_overlay), priv->chat_view);

        gtk_widget_get_allocation(GTK_WIDGET(self), &alloc);

        width_request = ROUND(priv->chat_settings->width*alloc.width);
        height_request = ROUND(priv->chat_settings->height*alloc.height);
        margin_start = ROUND(priv->chat_settings->x_pos*(alloc.width - width_request));
        margin_top = ROUND(priv->chat_settings->y_pos*(alloc.height - height_request));

        g_object_set(priv->chat_view,
            "opacity", priv->chat_settings->opacity,
            "valign", GTK_ALIGN_START,
            "halign", GTK_ALIGN_START,
            "margin-start", margin_start,
            "margin-top", margin_top,
            "width-request", width_request,
            "height-request", height_request,
            NULL);
    }
}

static void
update_muted(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (priv->muted)
    {
        priv->prev_volume = priv->volume;
        g_object_set(self, "volume", 0.0, NULL);
    }
    else
    {
        g_object_set(self, "volume", priv->prev_volume, NULL);
    }
}

static void
update_volume(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    priv->muted = !(priv->volume > 0);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_MUTED]);
}

static gboolean
motion_cb(GtkWidget* widget,
          GdkEventMotion* evt,
          gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (gt_win_is_fullscreen(GT_WIN_TOPLEVEL(widget)) && evt->y_root < FULLSCREEN_BAR_REVEAL_HEIGHT)
    {
        gtk_widget_set_visible(priv->fullscreen_bar_revealer, TRUE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->fullscreen_bar_revealer), TRUE);
    }
    else
    {
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->fullscreen_bar_revealer), FALSE);
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
player_button_press_cb(GtkWidget* widget,
                       GdkEventButton* evt,
                       gpointer udata)
{
    if (evt->button == 1 && evt->type == GDK_2BUTTON_PRESS)
        gt_win_toggle_fullscreen(GT_WIN_TOPLEVEL(widget));

    return GDK_EVENT_PROPAGATE;
}


static void
buffer_fill_cb(GObject* source,
                  GParamSpec* pspec,
                  gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gdouble perc;

    g_object_get(priv->backend, "buffer-fill", &perc, NULL);

    if (perc < 1.0)
    {
        gchar* text;

        text = g_strdup_printf(_("Buffered %d%%"), (gint) (perc*100.0));
        gtk_label_set_label(GTK_LABEL(priv->buffer_label), text);
        g_free(text);

        gtk_widget_set_visible(priv->buffer_revealer, TRUE);

        if (!gtk_revealer_get_child_revealed(GTK_REVEALER(priv->buffer_revealer)))
            gtk_revealer_set_reveal_child(GTK_REVEALER(priv->buffer_revealer), TRUE);
    }
    else
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->buffer_revealer), FALSE);

}

static void
revealer_revealed_cb(GObject* source,
                     GParamSpec* pspec,
                     gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!gtk_revealer_get_child_revealed(GTK_REVEALER(source)))
        gtk_widget_set_visible(GTK_WIDGET(source), FALSE);
}

static void
fullscreen_cb(GObject* source,
              GParamSpec* pspec,
              gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!gt_win_is_fullscreen(GT_WIN_TOPLEVEL(self)))
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->fullscreen_bar_revealer), FALSE);
}

static MousePos
get_mouse_pos(GtPlayer* self, gint x, gint y)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GtkAllocation alloc;

    gtk_widget_get_allocation(priv->chat_view, &alloc);

    gtk_widget_translate_coordinates(priv->chat_view, GTK_WIDGET(self),
        alloc.x, alloc.y, &alloc.x, &alloc.y);

    gboolean inside_horizontally = alloc.x < x && x < alloc.x + alloc.width;
    gboolean inside_vertically = alloc.y < y && y < alloc.y + alloc.height;

    if (alloc.x - CHAT_RESIZE_HANDLE_SIZE < x &&
        x < alloc.x + CHAT_RESIZE_HANDLE_SIZE &&
        inside_vertically)
    {
        return MOUSE_POS_LEFT_HANDLE;
    }
    else if (alloc.x + alloc.width - CHAT_RESIZE_HANDLE_SIZE < x &&
        x < alloc.x + alloc.width + CHAT_RESIZE_HANDLE_SIZE &&
        inside_vertically)
    {
        return MOUSE_POS_RIGHT_HANDLE;
    }
    else if (alloc.y - CHAT_RESIZE_HANDLE_SIZE < y &&
        y < alloc.y + CHAT_RESIZE_HANDLE_SIZE &&
        inside_horizontally)
    {
        return MOUSE_POS_TOP_HANDLE;
    }
    else if (alloc.y + alloc.height - CHAT_RESIZE_HANDLE_SIZE < y &&
        y < alloc.y + alloc.height + CHAT_RESIZE_HANDLE_SIZE &&
        inside_horizontally)
    {
        return MOUSE_POS_BOTTOM_HANDLE;
    }
    else if (inside_horizontally && inside_vertically)
    {
        return MOUSE_POS_INSIDE;
    }
    else
    {
        return MOUSE_POS_OUTSIDE;
    }
}

static gboolean
mouse_pressed_cb(GtkWidget* widget,
    GdkEvent* evt, gpointer udata)
{
    g_assert(GT_IS_PLAYER(udata));

    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    GtkAllocation alloc;

    gtk_widget_get_allocation(priv->chat_view, &alloc);

    priv->start_mouse_pos = get_mouse_pos(self, evt->button.x, evt->button.y);

    if (priv->start_mouse_pos != MOUSE_POS_OUTSIDE)
        priv->mouse_pressed = TRUE;

    return GDK_EVENT_PROPAGATE;
}

static gboolean
mouse_released_cb(GtkWidget* widget,
    GdkEvent* evt, gpointer udata)
{
    g_assert(GT_IS_PLAYER(udata));

    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    priv->mouse_pressed = FALSE;
    priv->start_mouse_pos = MOUSE_POS_OUTSIDE;

    return GDK_EVENT_PROPAGATE;
}

static gboolean
mouse_moved_cb(GtkWidget* widget,
    GdkEvent* evt, gpointer udata)
{
    g_assert(GT_IS_PLAYER(udata));

    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    MousePos pos;
    gint x, y;

    x = ROUND(evt->button.x);
    y = ROUND(evt->button.y);

    pos = get_mouse_pos(self, x, y);

    g_print("Position %d %d\n", pos, priv->start_mouse_pos);

    if (priv->mouse_pressed)
    {
        GtkAllocation alloc;
        gint margin_top;
        gint margin_start;
        gint width_request;
        gint height_request;

        g_object_get(priv->chat_view,
            "margin-top", &margin_top,
            "margin-start", &margin_start,
            "width-request", &width_request,
            "height-request", &height_request,
            NULL);

        gtk_widget_get_allocation(GTK_WIDGET(self), &alloc);

        //TODO: Implement dragging
        if (priv->start_mouse_pos == MOUSE_POS_LEFT_HANDLE && x >= 0)
        {
            width_request += margin_start - x;
            margin_start = x;

            g_object_set(priv->chat_view,
                "width-request", width_request,
                "margin-start", margin_start,
                NULL);
        }
        else if (priv->start_mouse_pos == MOUSE_POS_RIGHT_HANDLE &&
            x <= gtk_widget_get_allocated_width(GTK_WIDGET(self)))
        {
            width_request += x - margin_start - width_request;

            g_object_set(priv->chat_view,
                "width-request", width_request,
                NULL);
        }
        else if (priv->start_mouse_pos == MOUSE_POS_TOP_HANDLE && y >= 0)
        {
            height_request += margin_top - y;
            margin_top = y;

            g_object_set(priv->chat_view,
                "height-request", height_request,
                "margin-top", y,
                NULL);
        }
        else if (priv->start_mouse_pos == MOUSE_POS_BOTTOM_HANDLE &&
            y <= gtk_widget_get_allocated_height(GTK_WIDGET(self)))
        {
            height_request += y - margin_top - height_request;

            g_object_set(priv->chat_view,
                "height-request", height_request,
                NULL);
        }
        else if (priv->start_mouse_pos == MOUSE_POS_INSIDE)
        {
            margin_start = MIN(gtk_widget_get_allocated_width(GTK_WIDGET(self)) - width_request,
                MAX(0, x - width_request / 2));

            margin_top = MIN(gtk_widget_get_allocated_height(GTK_WIDGET(self)) - height_request,
                MAX(0, y - height_request / 2));

            g_object_set(priv->chat_view,
                "margin-start", margin_start,
                "margin-top", margin_top,
                NULL);
        }

        priv->chat_settings->x_pos = margin_start / (gdouble) (alloc.width - width_request);
        priv->chat_settings->y_pos = margin_top / (gdouble) (alloc.height - height_request);
        priv->chat_settings->width = width_request / (gdouble) alloc.width;
        priv->chat_settings->height = height_request / (gdouble) alloc.height;

        /* g_print("%d %d %d %d %d %d\n", x, y, priv->chat_margin_start, priv->chat_margin_top, priv->chat_width_request, priv->chat_height_request); */
    }
    else
    {
        g_autoptr(GdkCursor) cursor = NULL;

        switch (pos)
        {
            case MOUSE_POS_LEFT_HANDLE:
                cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_LEFT_SIDE);
                break;
            case MOUSE_POS_RIGHT_HANDLE:
                cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_RIGHT_SIDE);
                break;
            case MOUSE_POS_TOP_HANDLE:
                cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_TOP_SIDE);
                break;
            case MOUSE_POS_BOTTOM_HANDLE:
                cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_BOTTOM_SIDE);
                break;
            case MOUSE_POS_INSIDE:
                cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_FLEUR);
                break;
            case MOUSE_POS_OUTSIDE:
                cursor = NULL;
                break;
            default:
                g_assert_not_reached();
        }

        gdk_window_set_cursor(evt->any.window, cursor);
    }

    return GDK_EVENT_PROPAGATE;
}

static void
update_edit_chat(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    g_object_set(self, "above-child", priv->edit_chat, NULL);

    if (priv->edit_chat)
    {
        priv->mouse_pressed_handler_id = g_signal_connect(self, "button-press-event",
            G_CALLBACK(mouse_pressed_cb), self);

        priv->mouse_released_handler_id = g_signal_connect(self, "button-release-event",
            G_CALLBACK(mouse_released_cb), self);

        priv->mouse_moved_handler_id = g_signal_connect(self, "motion-notify-event",
            G_CALLBACK(mouse_moved_cb), self);
    }
    else
    {
        g_signal_handler_disconnect(self, priv->mouse_pressed_handler_id);
        g_signal_handler_disconnect(self, priv->mouse_released_handler_id);
        g_signal_handler_disconnect(self, priv->mouse_moved_handler_id);
    }
}

static void
set_property(GObject* obj,
             guint prop,
             const GValue* val,
             GParamSpec* pspec)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            update_volume(self);
            break;
        case PROP_MUTED:
            priv->muted = g_value_get_boolean(val);
            update_muted(self);
            break;
        case PROP_PLAYING:
            priv->playing = g_value_get_boolean(val);
            break;
        case PROP_CHANNEL:
            g_clear_object(&priv->channel);
            priv->channel = g_value_dup_object(val);
            break;
        case PROP_CHAT_DOCKED:
            priv->chat_settings->docked = g_value_get_boolean(val);
            update_docked(self);
            break;
        case PROP_CHAT_OPACITY:
            priv->chat_settings->opacity = g_value_get_double(val);
            break;
        case PROP_CHAT_VISIBLE:
            priv->chat_settings->visible = g_value_get_boolean(val);
            break;
        case PROP_CHAT_DARK_THEME:
            priv->chat_settings->dark_theme = g_value_get_boolean(val);
            break;
        case PROP_DOCKED_HANDLE_POSITION:
            priv->chat_settings->docked_handle_pos = g_value_get_double(val);
            break;
        case PROP_EDIT_CHAT:
            priv->edit_chat = g_value_get_boolean(val);
            update_edit_chat(self);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);

    }
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            g_value_set_double(val, priv->volume);
            break;
        case PROP_MUTED:
            g_value_set_boolean(val, priv->muted);
            break;
        case PROP_CHANNEL:
            g_value_set_object(val, priv->channel);
            break;
        case PROP_PLAYING:
            g_value_set_boolean(val, priv->playing);
            break;
        case PROP_CHAT_DOCKED:
            g_value_set_boolean(val, priv->chat_settings->docked);
            break;
        case PROP_CHAT_OPACITY:
            g_value_set_double(val, priv->chat_settings->opacity);
            break;
        case PROP_CHAT_VISIBLE:
            g_value_set_boolean(val, priv->chat_settings->visible);
            break;
        case PROP_CHAT_DARK_THEME:
            g_value_set_boolean(val, priv->chat_settings->dark_theme);
            break;
        case PROP_DOCKED_HANDLE_POSITION:
            g_value_set_double(val, priv->chat_settings->docked_handle_pos);
            break;
        case PROP_EDIT_CHAT:
            g_value_set_boolean(val, priv->edit_chat);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
streams_list_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata)
{
    g_assert(GT_IS_PLAYER(udata));
    g_assert(G_IS_TASK(res));
    g_assert(GT_IS_TWITCH(source));

    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GList* streams = NULL;
    GtTwitchStreamData* stream;
    GtTwitchStreamQuality default_quality;
    GError* err = NULL;

    streams = gt_twitch_all_streams_finish(GT_TWITCH(source), res, &err);

    //TODO: Replace player with some kind of error image
    if (err)
    {
        GtWin* win = GT_WIN_TOPLEVEL(self);

        g_assert(GT_IS_WIN(win));

        gt_win_show_error_message(win, "Error opening stream", err->message);

        g_error_free(err);

        return;
    }

    stream = gt_twitch_stream_list_filter_quality(streams, priv->quality);

    g_object_set(self, "playing", FALSE, NULL);
    g_object_set(priv->backend, "uri", stream->url, NULL);
    g_object_set(self, "playing", TRUE, NULL);

    priv->inhibitor_cookie = gtk_application_inhibit(GTK_APPLICATION(main_app),
                                                     GTK_WINDOW(GTK_WINDOW(GT_WIN_TOPLEVEL(self))),
                                                     GTK_APPLICATION_INHIBIT_IDLE,
                                                     "Playing a stream");
}

static void
realise_cb(GtkWidget* widget,
            gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);

    gtk_widget_insert_action_group(GTK_WIDGET(win), "player",
                                   G_ACTION_GROUP(priv->action_group));

    //Hack to get the bar connected properly

    gtk_widget_realize(priv->fullscreen_bar);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_VISIBLE]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_DOCKED_HANDLE_POSITION]);

    g_signal_connect(win, "notify::fullscreen", G_CALLBACK(fullscreen_cb), self);
}

static void
set_quality_action_cb(GSimpleAction* action,
                      GVariant* arg,
                      gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GEnumClass* eclass;
    GEnumValue* eval;

    eclass = g_type_class_ref(GT_TYPE_TWITCH_STREAM_QUALITY);
    eval = g_enum_get_value_by_nick(eclass, g_variant_get_string(arg, NULL));

    if (eval->value != priv->quality)
        gt_player_set_quality(self, eval->value);

    g_simple_action_set_state(action, arg);

    g_type_class_unref(eclass);
}

static gboolean
hide_cursor_cb(gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GdkCursor* cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_BLANK_CURSOR);

    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(priv->player_widget)), cursor);

    priv->mouse_source = 0;

    return G_SOURCE_REMOVE;
}

static gboolean
motion_event_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GdkCursor* cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_LEFT_PTR);

    gdk_window_set_cursor(gtk_widget_get_window(widget), cursor);

    if (!gt_win_is_fullscreen(GT_WIN_TOPLEVEL(self)))
        return G_SOURCE_REMOVE;

    if (priv->mouse_source)
        g_source_remove(priv->mouse_source);

    priv->mouse_source = g_timeout_add(1000, hide_cursor_cb, self);

    return G_SOURCE_REMOVE;
}

static void
plugin_loaded_cb(PeasEngine* engine,
                 PeasPluginInfo* info,
                 gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (peas_engine_provides_extension(engine, info, GT_TYPE_PLAYER_BACKEND))
    {
        PeasExtension* ext;

        MESSAGEF("Loaded player backend '%s'", peas_plugin_info_get_name(info));

        if (priv->backend_info)
        {
            peas_engine_unload_plugin(main_app->players_engine,
                                      priv->backend_info);
        }

        ext = peas_engine_create_extension(engine, info,
                                           GT_TYPE_PLAYER_BACKEND,
                                           NULL);

        priv->backend = GT_PLAYER_BACKEND(ext);
        priv->backend_info = g_boxed_copy(PEAS_TYPE_PLUGIN_INFO,
                                          info);

        g_signal_connect(priv->backend, "notify::buffer-fill",
                         G_CALLBACK(buffer_fill_cb), self);

        g_object_bind_property(self, "volume",
                               priv->backend, "volume",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
        g_object_bind_property(self, "playing",
                               priv->backend, "playing",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_bind_property(self, "chat-opacity",
                               priv->chat_view, "opacity",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_bind_property(self, "chat-dark-theme",
                               priv->chat_view, "dark-theme",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_bind_property(self, "chat-visible",
                               priv->chat_view, "visible",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

        gtk_container_remove(GTK_CONTAINER(priv->player_overlay), priv->empty_box);

        priv->player_widget = gt_player_backend_get_widget(priv->backend);
        gtk_widget_add_events(priv->player_widget, GDK_POINTER_MOTION_MASK);
        gtk_container_add(GTK_CONTAINER(priv->player_overlay), priv->player_widget);
        gtk_widget_show_all(priv->player_overlay);

        g_signal_connect(priv->player_widget, "button-press-event", G_CALLBACK(player_button_press_cb), self);
        g_signal_connect(priv->player_widget, "motion-notify-event", G_CALLBACK(motion_event_cb), self);

        if (priv->channel)
            gt_player_open_channel(self, priv->channel);
    }
}

static void
plugin_unloaded_cb(PeasEngine* engine,
                   PeasPluginInfo* info,
                   gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (peas_engine_provides_extension(engine, info, GT_TYPE_PLAYER_BACKEND))
    {
        MESSAGEF("Unloaded player backend '%s'", peas_plugin_info_get_name(info));

        gtk_container_remove(GTK_CONTAINER(priv->player_overlay),
                             gt_player_backend_get_widget(priv->backend));
        gtk_container_add(GTK_CONTAINER(priv->player_overlay),
                          priv->empty_box);

        priv->player_widget = NULL;

        g_clear_object(&priv->backend);

        g_boxed_free(PEAS_TYPE_PLUGIN_INFO, priv->backend_info);
        priv->backend_info = NULL;
    }
}

static void
gt_player_class_init(GtPlayerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalise;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Current volume",
                                             0, 1.0, 0.3,
                                             G_PARAM_READWRITE);
    props[PROP_MUTED] = g_param_spec_boolean("muted",
                                             "Muted",
                                             "Whether muted",
                                             FALSE,
                                             G_PARAM_READWRITE);
    props[PROP_CHANNEL] = g_param_spec_object("channel",
                                              "Channel",
                                              "Current channel",
                                              GT_TYPE_CHANNEL,
                                              G_PARAM_READWRITE);
    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "Whether playing",
                                               FALSE,
                                               G_PARAM_READWRITE);
    props[PROP_CHAT_VISIBLE] = g_param_spec_boolean("chat-visible",
                                                    "Chat Visible",
                                                    "Whether chat visible",
                                                    TRUE,
                                                    G_PARAM_READWRITE);
    props[PROP_CHAT_DOCKED] = g_param_spec_boolean("chat-docked",
                                                   "Chat Docked",
                                                   "Whether chat docked",
                                                   TRUE,
                                                   G_PARAM_READWRITE);
    props[PROP_CHAT_DARK_THEME] = g_param_spec_boolean("chat-dark-theme",
                                                       "Chat Dark Theme",
                                                       "Whether chat dark theme",
                                                       TRUE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_CHAT_OPACITY] = g_param_spec_double("chat-opacity",
                                                   "Chat Opacity",
                                                   "Current chat opacity",
                                                   0, 1.0, 1.0,
                                                   G_PARAM_READWRITE);
    props[PROP_DOCKED_HANDLE_POSITION] = g_param_spec_double("docked-handle-position",
                                                             "Docked Handle Position",
                                                             "Current docked handle position",
                                                             0, 1.0, 0,
                                                             G_PARAM_READWRITE);

    props[PROP_EDIT_CHAT] = g_param_spec_boolean("edit-chat", "Edit chat", "Whether to edit chat",
        FALSE, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/vinszent/GnomeTwitch/ui/gt-player.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, empty_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, docking_pane);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, player_overlay);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, fullscreen_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, fullscreen_bar_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, buffer_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, buffer_label);
}

static GActionEntry actions[] =
{
    //TODO: Make these GPropertyAction?
    {"set_quality", NULL, "s", "'source'", set_quality_action_cb},
};

//Target -> source
static gboolean
handle_position_from(GBinding* binding,
                     const GValue* from,
                     GValue* to,
                     gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gint width = gtk_widget_get_allocated_width(priv->docking_pane);
    gint pos = g_value_get_int(from);

    g_value_set_double(to, (gdouble) pos / (gdouble) width);

    return TRUE;
}


//Source -> target
static gboolean
handle_position_to(GBinding* binding,
                   const GValue* from,
                   GValue* to,
                   gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gint width = gtk_widget_get_allocated_width(priv->docking_pane);
    gdouble mult = g_value_get_double(from);

    g_value_set_int(to, (gint) (width*mult));

    return TRUE;
}

static void
gt_player_init(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GPropertyAction* action;

    gtk_widget_init_template(GTK_WIDGET(self));

    gtk_widget_add_events(GTK_WIDGET(self), GDK_POINTER_MOTION_MASK);

    priv->mouse_pressed = FALSE;
    priv->chat_settings = gt_chat_view_settings_new();

    g_object_set(self, "volume",
                 g_settings_get_double(main_app->settings, "volume"),
                 NULL);

    g_object_ref(priv->empty_box);

    priv->chat_view = GTK_WIDGET(gt_chat_new());
    g_object_ref(priv->chat_view); //TODO: Unref in finalise

    priv->action_group = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(priv->action_group), actions,
                                    G_N_ELEMENTS(actions), self);

    action = g_property_action_new("show_chat", self, "chat-visible");
    g_action_map_add_action(G_ACTION_MAP(priv->action_group), G_ACTION(action));
    g_object_unref(action);

    action = g_property_action_new("dock_chat", self, "chat-docked");
    g_action_map_add_action(G_ACTION_MAP(priv->action_group), G_ACTION(action));
    g_object_unref(action);

    action = g_property_action_new("dark_theme_chat", self, "chat-dark-theme");
    g_action_map_add_action(G_ACTION_MAP(priv->action_group), G_ACTION(action));
    g_object_unref(action);

    action = g_property_action_new("edit_chat", self, "edit-chat");
    g_action_map_add_action(G_ACTION_MAP(priv->action_group), G_ACTION(action));
    g_object_unref(action);

    g_object_bind_property_full(self, "docked-handle-position",
                                priv->docking_pane, "position",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                                handle_position_to, handle_position_from,
                                self, NULL);

    utils_signal_connect_oneshot(self, "realize", G_CALLBACK(realise_cb), self);
    g_signal_connect(priv->fullscreen_bar_revealer, "notify::child-revealed", G_CALLBACK(revealer_revealed_cb), self);
    g_signal_connect(priv->buffer_revealer, "notify::child-revealed", G_CALLBACK(revealer_revealed_cb), self);
    g_signal_connect(self, "motion-notify-event", G_CALLBACK(motion_cb), self);
    g_signal_connect_after(main_app->players_engine, "load-plugin", G_CALLBACK(plugin_loaded_cb), self);
    g_signal_connect(main_app->players_engine, "unload-plugin", G_CALLBACK(plugin_unloaded_cb), self);
    g_signal_connect(self, "destroy", G_CALLBACK(destroy_cb), self);

    gchar** c;
    gchar** _c;
    for (_c = c = peas_engine_get_loaded_plugins(main_app->players_engine); *c != NULL; c++)
    {
        PeasPluginInfo* info = peas_engine_get_plugin_info(main_app->players_engine, *c);

        if (peas_engine_provides_extension(main_app->players_engine, info, GT_TYPE_PLAYER_BACKEND))
            plugin_loaded_cb(main_app->players_engine, info, self);
    }

    g_strfreev(_c);
}

void
gt_player_play(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!priv->backend)
        MESSAGE("Can't play, no backend loaded");
    else
    {
        MESSAGE("Playing");
        g_object_set(priv->backend, "playing", TRUE, NULL);
    }
}

void
gt_player_stop(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!priv->backend)
        MESSAGE("Can't stop, no backend loaded");
    else
    {
        MESSAGE("Stopping");
        g_object_set(priv->backend, "playing", FALSE, NULL);
    }
}

void
gt_player_open_channel(GtPlayer* self, GtChannel* chan)
{
    g_assert(GT_IS_PLAYER(self));
    g_assert(GT_IS_CHANNEL(chan));

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    const gchar* name = gt_channel_get_name(chan);
    gchar* token;
    gchar* sig;
    GVariant* default_quality;
    GAction* quality_action;

    g_object_set(self, "channel", chan, NULL);

    if (!priv->backend)
    {
        MESSAGE("Can't open channel, no backend loaded");
        return;
    }

    gtk_label_set_label(GTK_LABEL(priv->buffer_label), _("Loading stream"));

    gtk_widget_set_visible(priv->buffer_revealer, TRUE);

    if (!gtk_revealer_get_child_revealed(GTK_REVEALER(priv->buffer_revealer)))
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->buffer_revealer), TRUE);

    gt_chat_connect(GT_CHAT(priv->chat_view), chan);

    default_quality = g_settings_get_value(main_app->settings, "default-quality");
    priv->quality = g_settings_get_enum(main_app->settings, "default-quality");

    MESSAGEF("Opening stream '%s' with quality '%s'", name, g_variant_get_string(default_quality, NULL));

    quality_action = g_action_map_lookup_action(G_ACTION_MAP(priv->action_group), "set_quality");
    g_action_change_state(quality_action, default_quality);

    priv->chat_settings = g_hash_table_lookup(main_app->chat_settings_table, name);
    if (!priv->chat_settings)
    {
        priv->chat_settings = gt_chat_view_settings_new();
        g_hash_table_insert(main_app->chat_settings_table,
                            g_strdup(name), priv->chat_settings);
    }

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_VISIBLE]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_OPACITY]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_DOCKED_HANDLE_POSITION]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_DOCKED]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_DARK_THEME]);

    update_docked(self);

    gt_twitch_all_streams_async(main_app->twitch, name, NULL, (GAsyncReadyCallback) streams_list_cb, self);
}

void
gt_player_close_channel(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gt_chat_disconnect(GT_CHAT(priv->chat_view));

    g_object_set(self, "channel", NULL, NULL);

    gt_player_stop(self);

    if (priv->inhibitor_cookie)
    {
        gtk_application_uninhibit(GTK_APPLICATION(main_app), priv->inhibitor_cookie);
        priv->inhibitor_cookie = 0;
    }
}

void
gt_player_set_quality(GtPlayer* self, GtTwitchStreamQuality qual)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gchar* name;
    GtTwitchStreamData* stream_data;
    GtChannel* chan;

    g_object_get(self, "channel", &chan, NULL);
    g_object_get(chan, "name", &name, NULL);

    priv->quality = qual;

    gt_twitch_all_streams_async(main_app->twitch, name, NULL, (GAsyncReadyCallback) streams_list_cb, self);

    g_free(name);
    g_object_unref(chan);
}

void
gt_player_toggle_muted(GtPlayer* self)
{
    gboolean muted;

    g_object_get(self, "muted", &muted, NULL);
    g_object_set(self, "muted", !muted, NULL);
}

GtChannel*
gt_player_get_channel(GtPlayer* self)
{
    g_assert(GT_IS_PLAYER(self));

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    return priv->channel;
}
