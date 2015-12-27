#include "gt-twitch-chat-client.h"
#include "gt-app.h"
#include <string.h>
#include <glib/gprintf.h>

#define TWITCH_IRC_HOSTNAME "irc.twitch.tv"
#define TWITC_IRC_PORT 6667

#define CR_LF "\r\n"

//TODO: Need to check for 001 message before setting connected

typedef struct
{
    GSocketConnection* irc_conn_recv;
    GSocketConnection* irc_conn_send;
    GDataInputStream* istream_recv;
    GOutputStream* ostream_recv;
    GDataInputStream* istream_send;
    GOutputStream* ostream_send;

    GThread* worker_thread_recv;
    GThread* worker_thread_send;

    gchar* cur_chan;
    gboolean connected;
} GtTwitchChatClientPrivate;

struct _GtTwitchChatSource
{
    GSource parent_instance;
    GAsyncQueue* queue;
};

typedef struct
{
    GtTwitchChatClient* self;
    GDataInputStream* istream;
    GOutputStream* ostream;
} ChatThreadData;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchChatClient, gt_twitch_chat_client, G_TYPE_OBJECT)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtTwitchChatClient*
gt_twitch_chat_client_new()
{
    return g_object_new(GT_TYPE_TWITCH_CHAT_CLIENT,
                        NULL);
}

static gboolean
source_prepare(GSource* source,
               gint* timeout)
{
    GtTwitchChatSource* self = (GtTwitchChatSource*) source;
    gint len;

    g_async_queue_lock(self->queue);
    len = g_async_queue_length_unlocked(self->queue);
    g_async_queue_unlock(self->queue);

    return len > 0;
}

static gboolean
source_dispatch(GSource* source,
                GSourceFunc callback,
                gpointer udata)
{
    GtTwitchChatSource* self = (GtTwitchChatSource*) source;
    GtTwitchChatMessage* msg;

    msg = g_async_queue_try_pop(self->queue);

    if (!msg)
        return TRUE;
    if (!callback)
    {
        gt_twitch_chat_message_free(msg);
        return TRUE;
    }

    return ((GtTwitchChatSourceFunc) callback)(msg, udata);
}

static void
source_finalise(GSource* source)
{
    GtTwitchChatSource* self = (GtTwitchChatSource*) source;

    g_async_queue_unref(self->queue);

    g_print("Cleanup source\n");
}

static GSourceFuncs source_funcs =
{
    source_prepare,
    NULL,
    source_dispatch,
    source_finalise,
    NULL
};

static GtTwitchChatSource*
gt_twitch_chat_source_new()
{
    GSource* source;

    source = g_source_new(&source_funcs, sizeof(GtTwitchChatSource));

    g_source_set_name(source, "GtTwitchChatSource");

    ((GtTwitchChatSource*) source)->queue = g_async_queue_new_full((GDestroyNotify) gt_twitch_chat_message_free);

    return (GtTwitchChatSource*) source;
}

static void
send_raw_printf(GOutputStream* ostream, const gchar* format, ...)
{
    va_list args;

    va_start(args, format);
    g_output_stream_vprintf(ostream, NULL, NULL, NULL, format, args);
    va_end(args);
}

static void
send_cmd(GOutputStream* ostream, const gchar* cmd, const gchar* param)
{
    g_message("{GtTwitchChatClient} Sending command '%s' with parameter '%s'", cmd, param);

    g_output_stream_printf(ostream, NULL, NULL, NULL, "%s %s%s", cmd, param, CR_LF);
}

static void
send_cmd_printf(GOutputStream* ostream, const gchar* cmd, const gchar* format, ...)
{
    va_list args;
    gchar param[512];

    va_start(args, format);
    g_vsprintf(param, format, args);
    va_end(args);

    g_message("{GtTwitchChatClient} Sending command '%s' with parameter '%s'", cmd, param);

    g_output_stream_printf(ostream, NULL, NULL, NULL, "%s %s%s", cmd, param, CR_LF);
}

static void
parse_line(gchar* line, GtTwitchChatMessage* msg)
{
    gchar* orig = line;
    gchar* prefix = NULL;

//    g_print("%s\n", line);

    if (line[0] == '@')
    {
        line = line+1;
        msg->tags = g_strsplit_set(strsep(&line, " "), ";=", -1);
    }

    if (line[0] == ':')
    {
        line = line+1;
        prefix = strsep(&line, " ");

        if (g_strrstr(prefix, "!"))
            msg->nick = g_strdup(strsep(&prefix, "!"));
        if (g_strrstr(prefix, "@"))
            msg->user = g_strdup(strsep(&prefix, "@"));

        msg->host = g_strdup(prefix);
    }

    msg->command = g_strdup(strsep(&line, " "));
    msg->params = g_strdup(line);

    g_free(orig);
}

static void
handle_message(GtTwitchChatClient* self, GOutputStream* ostream, GtTwitchChatMessage* msg)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    if (ostream == priv->ostream_recv)
    {
        if (g_strcmp0(msg->command, TWITCH_CHAT_CMD_PING) == 0)
            send_cmd(ostream, TWITCH_CHAT_CMD_PONG, msg->params);
        else
        {
            g_async_queue_push(self->source->queue, msg);
        }
    }
    else if (ostream == priv->ostream_send)
    {
        if (g_strcmp0(msg->command, TWITCH_CHAT_CMD_PING) == 0)
            send_cmd(ostream, TWITCH_CHAT_CMD_PONG, msg->params);

        gt_twitch_chat_message_free(msg);
    }
}

static void
read_lines(ChatThreadData* data)
{
    GtTwitchChatClient* self = GT_TWITCH_CHAT_CLIENT(data->self);
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    gchar buf[512];
    gsize count = 512;
    gsize read = 0;
    gsize read1 = 0;
    GError* err = NULL;

    g_message("{GtTwitchChatClient} Running chat worker thread");

    for (gchar* line = g_data_input_stream_read_line(data->istream, &read, NULL, &err); !err;
         line = g_data_input_stream_read_line(data->istream, &read, NULL, &err))
    {
        if (!priv->connected)
            break;

        if (line)
        {
            GtTwitchChatMessage* msg = gt_twitch_chat_message_new();

            parse_line(line, msg);
            /* g_print("Nick %s\n", msg->nick); */
            /* g_print("User %s\n", msg->user); */
            /* g_print("Host %s\n", msg->host); */
            /* g_print("Command %s\n", msg->command); */
            /* g_print("Params %s\n", msg->params); */

            handle_message(self, data->ostream, msg);
        }
    }

    g_message("{GtTwitchChatClient} Stopping chat worker thread");
}

static void
finalise(GObject* obj)
{
    GtTwitchChatClient* self = GT_TWITCH_CHAT_CLIENT(obj);
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    G_OBJECT_CLASS(gt_twitch_chat_client_parent_class)->finalize(obj);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtTwitchChatClient* self = GT_TWITCH_CHAT_CLIENT(obj);
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

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
    GtTwitchChatClient* self = GT_TWITCH_CHAT_CLIENT(obj);
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_twitch_chat_client_class_init(GtTwitchChatClientClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;
}

static void
gt_twitch_chat_client_init(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    priv->connected = FALSE;

    self->source = gt_twitch_chat_source_new();
    g_source_attach((GSource*) self->source, g_main_context_default());
}

void
gt_twitch_chat_client_connect(GtTwitchChatClient* self, const gchar* oauth_token, const gchar* nick)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    GSocketConnectable* addr;
    GSocketClient* sock_client;
    GError* err = NULL;
    ChatThreadData* recv_data;
    ChatThreadData* send_data;

    g_assert_nonnull(oauth_token);
    g_assert_nonnull(nick);

    g_message("{GtTwitchChatClient} Connecting");

    addr = g_network_address_new(TWITCH_IRC_HOSTNAME, TWITC_IRC_PORT);
    sock_client = g_socket_client_new();

    priv->irc_conn_recv = g_socket_client_connect(sock_client, addr, NULL, &err);
    if (err)
    {
        //TODO: Error handling
        g_print("Error\n");
        goto cleanup;
    }
    g_clear_error(&err); // Probably unecessary
    priv->irc_conn_send = g_socket_client_connect(sock_client, addr, NULL, &err);
    if (err)
    {
        //TODO: Error handling
        g_print("Error\n");
        goto cleanup;
    }

    priv->istream_recv = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(priv->irc_conn_recv)));
    g_data_input_stream_set_newline_type(priv->istream_recv, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    priv->ostream_recv = g_io_stream_get_output_stream(G_IO_STREAM(priv->irc_conn_recv));

    priv->istream_send = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(priv->irc_conn_send)));
    g_data_input_stream_set_newline_type(priv->istream_send, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    priv->ostream_send = g_io_stream_get_output_stream(G_IO_STREAM(priv->irc_conn_send));

    send_raw_printf(priv->ostream_recv, "%s%s%s", TWITCH_CHAT_CMD_PASS, oauth_token, CR_LF);
    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_NICK, nick);
    send_raw_printf(priv->ostream_send, "%s%s%s", TWITCH_CHAT_CMD_PASS, oauth_token, CR_LF);
    send_cmd(priv->ostream_send, TWITCH_CHAT_CMD_NICK, nick);

    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_CAP_REQ, ":twitch.tv/tags");
    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_CAP_REQ, ":twitch.tv/membership");
    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_CAP_REQ, ":twitch.tv/commands");

    priv->connected = TRUE;

    send_data = g_new(ChatThreadData, 2);
    recv_data = send_data + 1;

    recv_data->self = self;
    recv_data->istream = priv->istream_recv;
    recv_data->ostream = priv->ostream_recv;

    send_data->self = self;
    send_data->istream = priv->istream_send;
    send_data->ostream = priv->ostream_send;

    priv->worker_thread_recv = g_thread_new("gnome-twitch-chat-worker-recv",
                                       (GThreadFunc) read_lines, recv_data);
    priv->worker_thread_send = g_thread_new("gnome-twitch-chat-worker-send",
                                       (GThreadFunc) read_lines, send_data);

cleanup:
    g_object_unref(sock_client);
    g_object_unref(addr);
}

void
gt_twitch_chat_client_connect_simple(GtTwitchChatClient* self)
{
    gchar* oauth_token;
    gchar* nick;

    g_object_get(main_app,
                 "oauth-token", &oauth_token,
                 "user-name", &nick,
                 NULL);

    gt_twitch_chat_client_connect(self, oauth_token, nick);
}

void
gt_twitch_chat_client_disconnect(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    if (priv->connected)
    {
        gt_twitch_chat_client_part(self);

        priv->connected = FALSE;

        g_message("{GtTwitchChatClient} Disconnecting");

//        g_io_stream_close(G_IO_STREAM(priv->irc_conn_recv), NULL, NULL); //TODO: Error handling
//        g_io_stream_close(G_IO_STREAM(priv->irc_conn_send), NULL, NULL); //TODO: Error handling
        g_clear_object(&priv->irc_conn_recv);
//        g_clear_object(&priv->istream_recv);
//        g_clear_object(&priv->ostream_recv);
        g_clear_object(&priv->irc_conn_send);
//        g_clear_object(&priv->istream_send);
//        g_clear_object(&priv->ostream_send);
        g_thread_unref(priv->worker_thread_recv);
        g_thread_unref(priv->worker_thread_send);
    }
}

void
gt_twitch_chat_client_join(GtTwitchChatClient* self, const gchar* channel)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);
    gchar* chan = NULL;

    g_return_if_fail(priv->connected);

    if (channel[0] != '#')
        chan = g_strdup_printf("#%s", channel);
    else
        chan = g_strdup(channel);

    g_message("{GtTwitchChatClient} Joining channel '%s'", chan);

    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_JOIN, chan);
    send_cmd(priv->ostream_send, TWITCH_CHAT_CMD_JOIN, chan);

    priv->cur_chan = chan;
}

void
gt_twitch_chat_client_part(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);
    gint len;

    g_return_if_fail(priv->connected);
    g_return_if_fail(priv->cur_chan != NULL);

    g_message("{GtTwitchChatClient} Parting channel '%s'", priv->cur_chan);
    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_PART, priv->cur_chan);
    send_cmd(priv->ostream_send, TWITCH_CHAT_CMD_PART, priv->cur_chan);

    g_clear_pointer(&priv->cur_chan, (GDestroyNotify) g_free);

    g_async_queue_lock(self->source->queue);
    len = g_async_queue_length_unlocked(self->source->queue);
    if (len > 0)
    {
        for (int i = 0; i < len; i++)
            gt_twitch_chat_message_free(g_async_queue_pop_unlocked(self->source->queue));

    }
    g_async_queue_unlock(self->source->queue);

    g_clear_pointer(&priv->cur_chan, (GDestroyNotify) g_free);
}

void
gt_twitch_chat_client_privmsg(GtTwitchChatClient* self, const gchar* msg)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    g_return_if_fail(priv->connected);

    send_cmd_printf(priv->ostream_send, TWITCH_CHAT_CMD_PRIVMSG, "%s :%s", priv->cur_chan, msg);
}

gboolean
gt_twitch_chat_client_is_connected(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    return priv->connected;
}

GtTwitchChatMessage*
gt_twitch_chat_message_new()
{
    return g_new0(GtTwitchChatMessage, 1);
}

void
gt_twitch_chat_message_free(GtTwitchChatMessage* msg)
{
    g_free(msg->nick);
    g_free(msg->user);
    g_free(msg->host);
    g_free(msg->command);
    g_free(msg->params);
    g_strfreev(msg->tags);
    g_free(msg);
}
