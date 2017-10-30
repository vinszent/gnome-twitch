/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gt-player.h"
#include "gt-twitch.h"
#include "gt-win.h"
#include "gt-app.h"
#include "gt-enums.h"
#include "gt-chat.h"
#include "gnome-twitch/gt-player-backend.h"
#include "utils.h"
#include <libpeas-gtk/peas-gtk.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#define TAG "GtPlayer"
#include "gnome-twitch/gt-log.h"

#define FULLSCREEN_BAR_REVEAL_HEIGHT 50
#define CHAT_RESIZE_HANDLE_SIZE 10
#define CHAT_MIN_WIDTH 290
#define CHAT_MIN_HEIGHT 200

#define CHANNEL_SETTINGS_FILE g_build_filename(g_get_user_data_dir(), "gnome-twitch", "channel_settings.json", NULL);
#define CHANNEL_SETTINGS_FILE_VERSION 1

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

    gchar* quality;

    GtPlayerChannelSettings* cur_channel_settings;

    GtkWidget* player_box;
    GtkWidget* error_box;
    GtkWidget* empty_box;
    GtkWidget* player_overlay;
    GtkWidget* docking_pane;
    GtkWidget* chat_view;
    GtkWidget* controls_revealer;
    GtkWidget* buffer_revealer;
    GtkWidget* buffer_label;
    GtkWidget* player_widget;
    GtkWidget* reload_button;

    GtkWidget* volume_button;
    GtkWidget* toggle_chat_button;

    GtPlayerBackend* backend;
    PeasPluginInfo* backend_info;

    GtChannel* channel;
    GtVOD* vod;

    JsonParser* json_parser;
    GCancellable* cancel;

    gchar* vod_id;

    GList* stream_qualities;

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

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayer, gt_player, GTK_TYPE_STACK)

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
    PROP_STREAM_QUALITY,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static GHashTable* channel_settings_table;

static void
load_channel_settings()
{
    g_autofree gchar* fp = CHANNEL_SETTINGS_FILE;
    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(GError) err = NULL;
    gint version = 0;
    gint count = 0;

    g_hash_table_remove_all(channel_settings_table);

    MESSAGE("Loading chat settings");

    if (!g_file_test(fp, G_FILE_TEST_EXISTS))
    {
        INFO("Chat settings file at '%s' doesn't exist", fp);
        return;
    }

    json_parser_load_from_file(parser, fp, &err);

    if (err)
    {
        WARNING("Unable to load chat settings because: %s", err->message);
        return;
    }

    reader = json_reader_new(json_parser_get_root(parser));

/* FIXME: Propagate errors to UI once GtApp implements a startup error queue */
#define READ_JSON_VALUE(name, p)                                        \
    if (json_reader_read_member(reader, name))                          \
    {                                                                   \
        p = _Generic(p,                                                 \
            gboolean: json_reader_get_boolean_value(reader),            \
            gdouble: json_reader_get_double_value(reader),              \
            gint64: json_reader_get_int_value(reader));                 \
    }                                                                   \
    else                                                                \
    {                                                                   \
        WARNING("Couldn't read value '%s' from element '%d' in chat settings file." \
            "Will assign default value.", name, i);                     \
    }                                                                   \
    json_reader_end_member(reader);                                     \

    if (json_reader_read_member(reader, "version"))
    {
        version = json_reader_get_int_value(reader);
        INFO("Found version number '%d' for chat settings file", version);
    }
    else
        INFO("No version number found, assuming version 0 for chat settings file");
    json_reader_end_member(reader);

    if (version > 0) json_reader_read_member(reader, "chat-settings");

    count = json_reader_count_elements(reader);

    INFO("Reading '%d' elements from chat settings file", count);

    for (gint i = 0; i < count; i++)
    {
        GtPlayerChannelSettings* settings = NULL;
        gchar* id = NULL;

        if (!json_reader_read_element(reader, i))
        {
            WARNING("Couldn't read element '%d' from chat settings file."
                "This item will be removed on next save.", i);
            continue;
        }

        if (json_reader_read_member(reader, version > 0 ? "id" : "name"))
            id = g_strdup(json_reader_get_string_value(reader));
        else
        {
            WARNING("Couldn't read value '%s' from element '%d' from chat settings file."
                "This item will be removed on next save.", version > 0 ? "id" : "name", i);
            continue;
        }
        json_reader_end_member(reader);

        settings = gt_player_channel_settings_new();

        READ_JSON_VALUE("dark-theme", settings->dark_theme);
        READ_JSON_VALUE("visible", settings->visible);
        READ_JSON_VALUE("docked", settings->docked);
        READ_JSON_VALUE("opacity", settings->opacity);
        READ_JSON_VALUE("width", settings->width);
        READ_JSON_VALUE("height", settings->height);
        READ_JSON_VALUE("x-pos", settings->x_pos);
        READ_JSON_VALUE("y-pos", settings->y_pos);
        READ_JSON_VALUE("docked-handle-pos", settings->docked_handle_pos);

        json_reader_end_element(reader);

        g_hash_table_insert(channel_settings_table, id, settings);
    }

    if (version > 0) json_reader_end_member(reader);

#undef READ_JSON_VALUE
}

static void
save_channel_settings()
{
    g_autofree gchar* fp = CHANNEL_SETTINGS_FILE;
    g_autoptr(JsonBuilder) builder = json_builder_new();
    g_autoptr(JsonGenerator) gen = json_generator_new();
    g_autoptr(JsonNode) final = NULL;
    g_autoptr(GError) err = NULL;
    GHashTableIter iter;
    gchar* key;
    GtPlayerChannelSettings* settings;

    MESSAGE("Saving chat settings");

    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "version");
    json_builder_add_int_value(builder, CHANNEL_SETTINGS_FILE_VERSION);

    json_builder_set_member_name(builder, "chat-settings");
    json_builder_begin_array(builder);

    g_hash_table_iter_init(&iter, channel_settings_table);

    while (g_hash_table_iter_next(&iter, (gpointer*) &key, (gpointer*) &settings))
    {
        json_builder_begin_object(builder);

        json_builder_set_member_name(builder, "id");
        json_builder_add_string_value(builder, key);

        json_builder_set_member_name(builder, "dark-theme");
        json_builder_add_boolean_value(builder, settings->dark_theme);

        json_builder_set_member_name(builder, "visible");
        json_builder_add_boolean_value(builder, settings->visible);

        json_builder_set_member_name(builder, "docked");
        json_builder_add_boolean_value(builder, settings->docked);

        json_builder_set_member_name(builder, "opacity");
        json_builder_add_double_value(builder, settings->opacity);

        json_builder_set_member_name(builder, "width");
        json_builder_add_double_value(builder, settings->width);

        json_builder_set_member_name(builder, "height");
        json_builder_add_double_value(builder, settings->height);

        json_builder_set_member_name(builder, "x-pos");
        json_builder_add_double_value(builder, settings->x_pos);

        json_builder_set_member_name(builder, "y-pos");
        json_builder_add_double_value(builder, settings->y_pos);

        json_builder_set_member_name(builder, "docked-handle-pos");
        json_builder_add_double_value(builder, settings->docked_handle_pos);

        json_builder_end_object(builder);
    }

    json_builder_end_array(builder);

    json_builder_end_object(builder);

    final = json_builder_get_root(builder);

    json_generator_set_root(gen, final);
    json_generator_to_file(gen, fp, &err);

    if (err)
    {
        WARNING("Unable to write chat settings file to '%s' because: %s",
            fp, err->message);
    }
}

static void
finalise(GObject* obj)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    G_OBJECT_CLASS(gt_player_parent_class)->finalize(obj);

    //TODO: Unref more stuff

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

static gboolean
update_chat_undocked_position(GtkOverlay* overlay,
    GtkWidget* widget, GdkRectangle* chat_alloc, gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GtkAllocation alloc;

    if (widget != priv->chat_view) return FALSE;

    /* NOTE: This is to shut GTK up */
    gtk_widget_get_preferred_size(priv->chat_view, NULL, NULL);

    gtk_widget_get_allocation(GTK_WIDGET(self), &alloc);

    chat_alloc->width = ROUND(priv->cur_channel_settings->width*alloc.width);
    chat_alloc->height = ROUND(priv->cur_channel_settings->height*alloc.height);
    chat_alloc->x = ROUND(priv->cur_channel_settings->x_pos*(alloc.width - chat_alloc->width));
    chat_alloc->y = ROUND(priv->cur_channel_settings->y_pos*(alloc.height - chat_alloc->height));

    return TRUE;
}

static void
update_docked(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (priv->cur_channel_settings->docked)
    {
        gdouble width;

        width = ROUND(gtk_widget_get_allocated_width(GTK_WIDGET(self)));

        if (width < 2.0)
            return;

        if (gtk_widget_get_parent(priv->chat_view) == priv->player_overlay)
            gtk_container_remove(GTK_CONTAINER(priv->player_overlay), priv->chat_view);

        if (gtk_widget_get_parent(priv->chat_view) == NULL)
            gtk_paned_pack2(GTK_PANED(priv->docking_pane), priv->chat_view, FALSE, TRUE);

        g_object_set(priv->chat_view,
            "opacity", 1.0,
            NULL);

        g_object_set(self,
            "edit-chat", FALSE,
            NULL);

        gtk_paned_set_position(GTK_PANED(priv->docking_pane),
            ROUND(priv->cur_channel_settings->docked_handle_pos*width));
    }
    else
    {
        if (gtk_widget_get_parent(priv->chat_view) == priv->docking_pane)
            gtk_container_remove(GTK_CONTAINER(priv->docking_pane), priv->chat_view);

        if (gtk_widget_get_parent(priv->chat_view) == NULL)
        {
            gtk_overlay_add_overlay(GTK_OVERLAY(priv->player_overlay), priv->chat_view);
            gtk_overlay_reorder_overlay(GTK_OVERLAY(priv->player_overlay), priv->chat_view, 0);
        }

        g_object_set(priv->chat_view,
            "opacity", priv->cur_channel_settings->opacity,
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
    GdkEventMotion* evt, gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (evt->y_root > gtk_widget_get_allocated_height(widget) - FULLSCREEN_BAR_REVEAL_HEIGHT)
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->controls_revealer), TRUE);
    else
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->controls_revealer), FALSE);

    return GDK_EVENT_PROPAGATE;
}

static gboolean
player_button_press_cb(GtkWidget* widget,
                       GdkEventButton* evt,
                       gpointer udata)
{
    if (evt->button == 1 && evt->type == GDK_2BUTTON_PRESS)
        gt_win_toggle_fullscreen(GT_WIN_TOPLEVEL(widget));

    gtk_widget_grab_focus(widget);

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
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->controls_revealer), FALSE);
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

    if (((GdkEventButton*) evt)->type == GDK_2BUTTON_PRESS)
    {
        priv->mouse_pressed = FALSE;

        g_object_set(self, "edit-chat", FALSE, NULL);
    }
    else
    {
        GtkAllocation alloc;

        gtk_widget_get_allocation(priv->chat_view, &alloc);

        priv->start_mouse_pos = get_mouse_pos(self, evt->button.x, evt->button.y);

        if (priv->start_mouse_pos != MOUSE_POS_OUTSIDE)
            priv->mouse_pressed = TRUE;
    }

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

    if (priv->mouse_pressed)
    {
        GtkAllocation alloc;

        gtk_widget_get_allocation(GTK_WIDGET(self), &alloc);

        gint width_request = ROUND(priv->cur_channel_settings->width*alloc.width);
        gint height_request = ROUND(priv->cur_channel_settings->height*alloc.height);
        gint margin_start = ROUND(priv->cur_channel_settings->x_pos*(alloc.width - width_request));
        gint margin_top = ROUND(priv->cur_channel_settings->y_pos*(alloc.height - height_request));

        if (priv->start_mouse_pos == MOUSE_POS_LEFT_HANDLE && x >= 0)
        {
            width_request += margin_start - x;

            width_request = MAX(width_request, CHAT_MIN_WIDTH);

            margin_start = width_request == CHAT_MIN_WIDTH ? margin_start : x;
        }
        else if (priv->start_mouse_pos == MOUSE_POS_RIGHT_HANDLE &&
            x <= gtk_widget_get_allocated_width(GTK_WIDGET(self)))
        {
            width_request += x - margin_start - width_request;

            width_request = MAX(width_request, CHAT_MIN_WIDTH);
        }
        else if (priv->start_mouse_pos == MOUSE_POS_TOP_HANDLE && y >= 0)
        {
            height_request += margin_top - y;

            height_request = MAX(height_request, CHAT_MIN_HEIGHT);

            margin_top = height_request == CHAT_MIN_HEIGHT ? margin_top : y;
        }
        else if (priv->start_mouse_pos == MOUSE_POS_BOTTOM_HANDLE &&
            y <= gtk_widget_get_allocated_height(GTK_WIDGET(self)))
        {
            height_request += y - margin_top - height_request;

            height_request = MAX(height_request, CHAT_MIN_HEIGHT);
        }
        else if (priv->start_mouse_pos == MOUSE_POS_INSIDE)
        {
            margin_start = MIN(gtk_widget_get_allocated_width(GTK_WIDGET(self)) - width_request,
                MAX(0, x - width_request / 2));

            margin_top = MIN(gtk_widget_get_allocated_height(GTK_WIDGET(self)) - height_request,
                MAX(0, y - height_request / 2));
        }

        priv->cur_channel_settings->x_pos = margin_start / (gdouble) (alloc.width - width_request);
        priv->cur_channel_settings->y_pos = margin_top / (gdouble) (alloc.height - height_request);
        priv->cur_channel_settings->width = width_request / (gdouble) alloc.width;
        priv->cur_channel_settings->height = height_request / (gdouble) alloc.height;

        gtk_widget_queue_resize_no_redraw(priv->chat_view);

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

    g_object_set(priv->player_box, "above-child", priv->edit_chat, NULL);

    if (priv->edit_chat)
    {
        priv->mouse_pressed_handler_id = g_signal_connect(self, "button-press-event",
            G_CALLBACK(mouse_pressed_cb), self);

        priv->mouse_released_handler_id = g_signal_connect(self, "button-release-event",
            G_CALLBACK(mouse_released_cb), self);

        priv->mouse_moved_handler_id = g_signal_connect(self, "motion-notify-event",
            G_CALLBACK(mouse_moved_cb), self);

        ADD_STYLE_CLASS(self, "edit-chat");
    }
    else
    {
        if (priv->mouse_pressed_handler_id > 0)
        {
            g_signal_handler_disconnect(self, priv->mouse_pressed_handler_id);
            priv->mouse_pressed_handler_id = 0;
        }

        if (priv->mouse_released_handler_id > 0)
        {
            g_signal_handler_disconnect(self, priv->mouse_released_handler_id);
            priv->mouse_pressed_handler_id = 0;
        }

        if (priv->mouse_moved_handler_id > 0)
        {
            g_signal_handler_disconnect(self, priv->mouse_moved_handler_id);
            priv->mouse_moved_handler_id = 0;
        }

        REMOVE_STYLE_CLASS(self, "edit-chat");
    }
}

static void
app_shutdown_cb(GApplication* app,
    gpointer udata)
{
    g_assert_null(udata);

    save_channel_settings();
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
            priv->cur_channel_settings->docked = g_value_get_boolean(val);
            update_docked(self);
            break;
        case PROP_CHAT_OPACITY:
            priv->cur_channel_settings->opacity = g_value_get_double(val);
            break;
        case PROP_CHAT_VISIBLE:
            priv->cur_channel_settings->visible = g_value_get_boolean(val);
            break;
        case PROP_CHAT_DARK_THEME:
            priv->cur_channel_settings->dark_theme = g_value_get_boolean(val);
            break;
        case PROP_DOCKED_HANDLE_POSITION:
            priv->cur_channel_settings->docked_handle_pos = g_value_get_double(val);
            break;
        case PROP_EDIT_CHAT:
            priv->edit_chat = g_value_get_boolean(val);
            update_edit_chat(self);
            break;
        case PROP_STREAM_QUALITY:
            gt_player_set_quality(self, g_value_get_string(val));
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
            g_value_set_boolean(val, priv->cur_channel_settings->docked);
            break;
        case PROP_CHAT_OPACITY:
            g_value_set_double(val, priv->cur_channel_settings->opacity);
            break;
        case PROP_CHAT_VISIBLE:
            g_value_set_boolean(val, priv->cur_channel_settings->visible);
            break;
        case PROP_CHAT_DARK_THEME:
            g_value_set_boolean(val, priv->cur_channel_settings->dark_theme);
            break;
        case PROP_DOCKED_HANDLE_POSITION:
            g_value_set_double(val, priv->cur_channel_settings->docked_handle_pos);
            break;
        case PROP_EDIT_CHAT:
            g_value_set_boolean(val, priv->edit_chat);
            break;
        case PROP_STREAM_QUALITY:
            g_value_set_string(val, priv->quality);
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
    const GtTwitchStreamData* stream_data;
    g_autoptr(GError) err = NULL;

    priv->stream_qualities = gt_twitch_all_streams_finish(GT_TWITCH(source), res, &err);

    if (err)
    {
        GtWin* win = GT_WIN_TOPLEVEL(self);

        RETURN_IF_FAIL(GT_IS_WIN(win));

        if (g_error_matches(err, GT_TWITCH_ERROR, GT_TWITCH_ERROR_SOUP_NOT_FOUND))
        {
            /* Translators: %s will be filled with the channel name */
            gt_win_show_info_message(win, _("Unable to open channel %s because it’s offline"),
                gt_channel_get_name(priv->channel));
        }
        else
            gt_win_show_error_message(win, _("Error opening stream"), err->message);

        gtk_stack_set_visible_child(GTK_STACK(self), priv->error_box);

        gt_chat_disconnect(GT_CHAT(priv->chat_view));

        return;
    }

    stream_data = gt_twitch_stream_list_filter_quality(priv->stream_qualities, priv->quality);

    //NOTE: Incase we get back a different quality from what we asked for
    //eg. when then quality doesn't exist, so we get the first one (normally source)
    g_free(priv->quality);
    priv->quality = g_strdup(stream_data->quality);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STREAM_QUALITY]);

    g_object_set(self, "playing", FALSE, NULL);
    g_object_set(priv->backend, "uri", stream_data->url, NULL);
    g_object_set(self, "playing", TRUE, NULL);

    priv->inhibitor_cookie = gtk_application_inhibit(GTK_APPLICATION(main_app),
        GTK_WINDOW(GTK_WINDOW(GT_WIN_TOPLEVEL(self))), GTK_APPLICATION_INHIBIT_IDLE, "Playing a stream");
}

static void
reload_button_cb(GtkButton* button,
    GtPlayer* self)
{
    g_assert(GT_IS_PLAYER(self));

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    //NOTE: Need to do this because of the way open_channel works
    //it will dereference channel before referencing it again
    gt_player_open_channel(self, g_object_ref(priv->channel));

    g_object_unref(priv->channel);
}

static void
realize_cb(GtkWidget* widget,
            gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);

    g_assert(GT_IS_WIN(win));

    gtk_widget_insert_action_group(GTK_WIDGET(win), "player",
        G_ACTION_GROUP(priv->action_group));

    g_signal_connect(win, "notify::fullscreen", G_CALLBACK(fullscreen_cb), self);
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
            GT_TYPE_PLAYER_BACKEND, NULL);

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

        gtk_stack_set_visible_child(GTK_STACK(self), priv->player_box);

        priv->player_widget = gt_player_backend_get_widget(priv->backend);
        gtk_widget_add_events(priv->player_widget, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
        gtk_widget_set_can_focus(priv->player_widget, TRUE);
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

        gtk_stack_set_visible_child(GTK_STACK(self), priv->empty_box);

        priv->player_widget = NULL;

        g_clear_object(&priv->backend);

        g_boxed_free(PEAS_TYPE_PLUGIN_INFO, priv->backend_info);
        priv->backend_info = NULL;
    }
}

#define SHOW_ERROR(primary, secondary, err)             \
    G_STMT_START                                        \
    {                                                   \
        GtWin* win = GT_WIN_TOPLEVEL(self);             \
        RETURN_IF_FAIL(GT_IS_WIN(win));                 \
                                                        \
        WARNING(secondary " because %s", err->message); \
                                                        \
        gt_win_show_error_message(win, _(primary),      \
            secondary " because: %s", err->message);    \
                                                        \
        return;                                         \
    } G_STMT_END

static void
process_playlist_data_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(G_IS_INPUT_STREAM(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtPlayer) self = g_weak_ref_get(ref);

    if (!self)
    {
        DEBUG("Unreffed");
        return;
    }

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    g_autoptr(GtPlaylistEntryList) entries = NULL;
    g_autoptr(GError) err = NULL;
    g_autofree gchar* data = NULL;

    g_input_stream_read_all_finish(G_INPUT_STREAM(source), res, NULL, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
        SHOW_ERROR("Unable to open VOD", "Unable to load playlist data", err);

    data = g_object_steal_data(G_OBJECT(self), "playlist-data");

    entries = utils_parse_playlist(data, &err);

    if (err)
        SHOW_ERROR("Unable to open VOD", "Unable to parse playlist data", err);

    g_object_set(G_OBJECT(priv->backend), "playing", FALSE, NULL);
    g_object_set(G_OBJECT(priv->backend), "uri", ((GtPlaylistEntry*) entries->data)->uri, NULL);
    g_object_set(G_OBJECT(priv->backend), "playing", TRUE, NULL);
}

static void
handle_vod_playlist_response_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(SOUP_IS_SESSION(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtPlayer) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Unreffed while waiting");
        return;
    }

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    g_autoptr(GInputStream) istream = NULL;
    g_autoptr(GError) err = NULL;

    istream = soup_session_send_finish(SOUP_SESSION(source), res, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
        SHOW_ERROR("Unable to open VOD", "Unable to handle VOD playlist response", err);

    #define BUFFER_SIZE 4096*2

    g_object_set_data_full(G_OBJECT(self), "playlist-data", g_malloc0(BUFFER_SIZE), NULL);

    g_input_stream_read_all_async(istream, g_object_get_data(G_OBJECT(self), "playlist-data"), BUFFER_SIZE,
        G_PRIORITY_DEFAULT, priv->cancel, process_playlist_data_cb, g_steal_pointer(&ref));
}

static void
process_vod_json_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(JSON_IS_PARSER(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtPlayer) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Unreffed while waiting");
        return;
    }

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(GError) err = NULL;
    g_autofree gchar* uri = NULL;
    g_autofree gchar* token = NULL;
    g_autofree gchar* sig = NULL;
    const gchar* vod_id = NULL;

    json_parser_load_from_stream_finish(JSON_PARSER(source), res, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
        SHOW_ERROR("Unable to open VOD", "Unable to process VOD access token JSON", err);

    reader = json_reader_new(json_parser_get_root(JSON_PARSER(source)));

    if (!json_reader_read_member(reader, "token"))
        SHOW_ERROR("Unable to open VOD", "Unable to process VOD access token JSON", json_reader_get_error(reader));
    token = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "sig"))
        SHOW_ERROR("Unable to open VOD", "Unable to process VOD access token JSON", json_reader_get_error(reader));
    sig = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    vod_id = gt_vod_get_id(priv->vod);

    if (g_ascii_isalpha(vod_id[0]))
        vod_id = vod_id + 1;

    uri = g_strdup_printf("https://usher.ttvnw.net/vod/%s.m3u8?nauth=%s&nauthsig=%s&allow_source=true&player_backend=html5",
        vod_id, token, sig);

    msg = soup_message_new(SOUP_METHOD_GET, uri);

    gt_app_queue_soup_message(main_app, "gt-player", msg, priv->cancel,
        handle_vod_playlist_response_cb, g_steal_pointer(&ref));
}

static void
handler_vod_access_token_response_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(SOUP_IS_SESSION(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtPlayer) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Unreffed while waiting");
        return;
    }

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    g_autoptr(GInputStream) istream = NULL;
    g_autoptr(GError) err = NULL;

    istream = soup_session_send_finish(SOUP_SESSION(source), res, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
        SHOW_ERROR("Unable to open VOD", "Unable to handle VOD access token response", err);

    json_parser_load_from_stream_async(priv->json_parser, istream,
        priv->cancel, process_vod_json_cb, g_steal_pointer(&ref));
}

static void
gt_player_class_init(GtPlayerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalise;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_VOLUME] = g_param_spec_double("volume", "Volume", "Current volume",
        0, 1.0, 0.3, G_PARAM_READWRITE);

    props[PROP_MUTED] = g_param_spec_boolean("muted", "Muted", "Whether muted",
        FALSE, G_PARAM_READWRITE);

    props[PROP_CHANNEL] = g_param_spec_object("channel", "Channel", "Current channel",
        GT_TYPE_CHANNEL, G_PARAM_READWRITE);

    props[PROP_PLAYING] = g_param_spec_boolean("playing", "Playing", "Whether playing",
        FALSE, G_PARAM_READWRITE);

    props[PROP_CHAT_VISIBLE] = g_param_spec_boolean("chat-visible", "Chat Visible", "Whether chat visible",
        TRUE, G_PARAM_READWRITE);

    props[PROP_CHAT_DOCKED] = g_param_spec_boolean("chat-docked", "Chat Docked", "Whether chat docked",
        TRUE, G_PARAM_READWRITE);

    props[PROP_CHAT_DARK_THEME] = g_param_spec_boolean("chat-dark-theme", "Chat Dark Theme", "Whether chat dark theme",
        TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    props[PROP_CHAT_OPACITY] = g_param_spec_double("chat-opacity", "Chat Opacity", "Current chat opacity",
        0, 1.0, 1.0, G_PARAM_READWRITE);

    props[PROP_DOCKED_HANDLE_POSITION] = g_param_spec_double("docked-handle-position", "Docked Handle Position", "Current docked handle position",
        0, 1.0, 0, G_PARAM_READWRITE);

    props[PROP_EDIT_CHAT] = g_param_spec_boolean("edit-chat", "Edit chat", "Whether to edit chat",
        FALSE, G_PARAM_READWRITE);

    props[PROP_STREAM_QUALITY] = g_param_spec_string("stream-quality", "Stream quality", "Current stream quality",
        NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/vinszent/GnomeTwitch/ui/gt-player.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, empty_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, docking_pane);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, player_overlay);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, controls_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, buffer_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, buffer_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, player_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, error_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, reload_button);

    channel_settings_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) gt_player_channel_settings_free);

    load_channel_settings();
}

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

    priv->json_parser = json_parser_new();
    priv->cancel = NULL;

    priv->mouse_pressed = FALSE;
    priv->cur_channel_settings = gt_player_channel_settings_new();

    g_object_set(self, "volume",
                 g_settings_get_double(main_app->settings, "volume"),
                 NULL);

    g_object_ref(priv->empty_box);

    priv->chat_view = GTK_WIDGET(gt_chat_new());
    g_object_ref(priv->chat_view); //TODO: Unref in finalise

    priv->action_group = g_simple_action_group_new();

    action = g_property_action_new("toggle_playing", self, "playing");
    g_action_map_add_action(G_ACTION_MAP(priv->action_group), G_ACTION(action));
    g_object_unref(action);

    action = g_property_action_new("toggle_chat", self, "chat-visible");
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

    action = g_property_action_new("set_stream_quality", self, "stream-quality");
    g_action_map_add_action(G_ACTION_MAP(priv->action_group), G_ACTION(action));
    g_object_unref(action);

    g_object_bind_property_full(self, "docked-handle-position",
        priv->docking_pane, "position",
        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
        handle_position_to, handle_position_from, self, NULL);

    utils_signal_connect_oneshot(self, "realize", G_CALLBACK(realize_cb), self);
    utils_signal_connect_oneshot_swapped(priv->docking_pane, "size-allocate", G_CALLBACK(update_docked), self);
    g_signal_connect(priv->fullscreen_bar_revealer, "notify::child-revealed", G_CALLBACK(revealer_revealed_cb), self);
    g_signal_connect(priv->buffer_revealer, "notify::child-revealed", G_CALLBACK(revealer_revealed_cb), self);
    g_signal_connect(priv->player_box, "motion-notify-event", G_CALLBACK(motion_cb), self);
    g_signal_connect_after(main_app->players_engine, "load-plugin", G_CALLBACK(plugin_loaded_cb), self);
    g_signal_connect(main_app->players_engine, "unload-plugin", G_CALLBACK(plugin_unloaded_cb), self);
    g_signal_connect(self, "destroy", G_CALLBACK(destroy_cb), self);
    g_signal_connect(priv->reload_button, "clicked", G_CALLBACK(reload_button_cb), self);
    g_signal_connect(main_app, "shutdown", G_CALLBACK(app_shutdown_cb), NULL);
    g_signal_connect_object(priv->player_overlay, "get-child-position", G_CALLBACK(update_chat_undocked_position), self, 0);

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
    const gchar* id = gt_channel_get_id(chan);
    g_autofree gchar* default_quality = NULL;

    g_object_set(self, "channel", chan, NULL);

    if (!priv->backend)
    {
        MESSAGE("Can't open channel, no backend loaded");

        gtk_stack_set_visible_child(GTK_STACK(self), priv->empty_box);

        return;
    }

    gtk_stack_set_visible_child(GTK_STACK(self), priv->player_box);

    gtk_label_set_label(GTK_LABEL(priv->buffer_label), _("Loading stream"));

    gtk_widget_set_visible(priv->buffer_revealer, TRUE);

    if (!gtk_revealer_get_child_revealed(GTK_REVEALER(priv->buffer_revealer)))
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->buffer_revealer), TRUE);

    gt_chat_connect(GT_CHAT(priv->chat_view), chan);

    default_quality = g_settings_get_string(main_app->settings, "default-quality");

    g_object_set(self, "stream-quality", default_quality, NULL);

    MESSAGEF("Opening stream '%s' with quality '%s'",
        gt_channel_get_name(chan), priv->quality);

    priv->cur_channel_settings = g_hash_table_lookup(channel_settings_table, id);
    if (!priv->cur_channel_settings)
    {
        priv->cur_channel_settings = gt_player_channel_settings_new();
        g_hash_table_insert(channel_settings_table,
            g_strdup(id), priv->cur_channel_settings);
    }

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_VISIBLE]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_OPACITY]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_DOCKED_HANDLE_POSITION]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_DOCKED]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_DARK_THEME]);

    update_docked(self);
}

void
gt_player_open_vod(GtPlayer* self, GtVOD* vod)
{
    RETURN_IF_FAIL(GT_IS_PLAYER(self));
    RETURN_IF_FAIL(GT_IS_VOD(vod));

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    g_autoptr(SoupMessage) msg = NULL;
    const gchar* vod_id = NULL;

    g_clear_object(&priv->vod);
    priv->vod = g_object_ref(vod);

    utils_refresh_cancellable(&priv->cancel);

    vod_id = gt_vod_get_id(priv->vod);

    if (g_ascii_isalpha(vod_id[0]))
        vod_id = vod_id + 1;

    msg = utils_create_twitch_request_v("https://api.twitch.tv/api/vods/%s/access_token",
        vod_id);

    gt_app_queue_soup_message(main_app, "gt-player", msg, priv->cancel,
        handler_vod_access_token_response_cb, utils_weak_ref_new(self));
}

void
gt_player_close_channel(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gt_chat_disconnect(GT_CHAT(priv->chat_view));

    g_object_set(self,
        "channel", NULL,
        "edit-chat", FALSE,
        NULL);

    gt_player_stop(self);

    if (priv->inhibitor_cookie)
    {
        gtk_application_uninhibit(GTK_APPLICATION(main_app), priv->inhibitor_cookie);
        priv->inhibitor_cookie = 0;
    }
}

void
gt_player_set_quality(GtPlayer* self, const gchar* quality)
{
    g_assert(GT_IS_PLAYER(self));
    g_assert_nonnull(quality);

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    const gchar* name;

    name = gt_channel_get_name(priv->channel);

    g_free(priv->quality);

    priv->quality = g_strdup(quality);

    gt_twitch_all_streams_async(main_app->twitch, name, NULL, (GAsyncReadyCallback) streams_list_cb, self);
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

GList*
gt_player_get_available_stream_qualities(GtPlayer* self)
{
    g_assert(GT_IS_PLAYER(self));

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    return priv->stream_qualities;
}

gboolean
gt_player_is_playing(GtPlayer* self)
{
    g_assert(GT_IS_PLAYER(self));

    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    return priv->playing;
}

GtPlayerChannelSettings*
gt_player_channel_settings_new()
{
    GtPlayerChannelSettings* settings =  g_slice_new(GtPlayerChannelSettings);

    settings->dark_theme = TRUE;
    settings->opacity = 1.0;
    settings->visible = TRUE;
    settings->docked = TRUE;
    settings->width = 0.2;
    settings->height = 0.7;
    settings->x_pos = 1.0;
    settings->y_pos = 0.5;
    settings->docked_handle_pos = 0.75;

    return settings;
}

void
gt_player_channel_settings_free(GtPlayerChannelSettings* settings)
{
    if (!settings)
        return;

    g_slice_free(GtPlayerChannelSettings, settings);
}
