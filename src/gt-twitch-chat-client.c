#include "gt-twitch-chat-client.h"
#include "gt-app.h"
#include <string.h>

#define TWITCH_IRC_HOSTNAME "irc.twitch.tv"
#define TWITC_IRC_PORT 6667

#define CR_LF "\r\n"

//TODO: Need to check for 001 message before setting connected

typedef struct
{
    GSocketConnection* irc_conn;
    GDataInputStream* istream;
    GOutputStream* ostream;

    GThread* worker_thread;

    gchar* cur_chan;
    gboolean connected;
} GtTwitchChatClientPrivate;

struct _GtTwitchChatSource
{
    GSource parent_instance;
    GAsyncQueue* queue;
};

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchChatClient, gt_twitch_chat_client, G_TYPE_OBJECT)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

enum
{
    SIG_CHANNEL_JOINED,
    NUM_SIGS
};

static guint sigs[NUM_SIGS];

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

    return g_async_queue_length(self->queue) > 0;
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
send_raw_printf(GtTwitchChatClient* self, const gchar* format, ...)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);
    va_list args;

    va_start(args, format);
    g_output_stream_vprintf(priv->ostream, NULL, NULL, NULL, format, args);
    va_end(args);
}

static void
send_cmd(GtTwitchChatClient* self, const gchar* cmd, const gchar* param)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    g_debug("{GtTwitchChatClient} Sending command %s with parameter %s", cmd, param);

    g_output_stream_printf(priv->ostream, NULL, NULL, NULL, "%s %s%s", cmd, param, CR_LF);
}

static void
parse_line(gchar* line, GtTwitchChatMessage* msg)
{
    gchar* orig = line;
    gchar* prefix = NULL;

    if (line[0] == '@')
    {
        line = line+1;
        msg->tags = g_strsplit_set(line, ";=", -1);
        strsep(&line, " ");
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
handle_message(GtTwitchChatClient* self, GtTwitchChatMessage* msg)
{
    if (g_strcmp0(msg->command, TWITCH_CHAT_CMD_PING) == 0)
    {
        send_cmd(self, TWITCH_CHAT_CMD_PONG, msg->params);
        gt_twitch_chat_message_free(msg);
    }
    else
    {
        if (self->source)
            g_async_queue_push(self->source->queue, msg);
    }
}

static void
read_lines(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    gchar buf[512];
    gsize count = 512;
    gsize read = 0;
    gsize read1 = 0;
    GError* err = NULL;

//    send_cmd(self, "JOIN", "#bobross");

    g_print("Running\n");

    for (gchar* line = g_data_input_stream_read_line(priv->istream, &read, NULL, &err); !err;
         line = g_data_input_stream_read_line(priv->istream, &read, NULL, &err))
    {
        g_print("%s\n", line);
        if (line)
        {
            GtTwitchChatMessage* msg = gt_twitch_chat_message_new();

            /* g_print("Line %s\n\n", line); */

            parse_line(line, msg);
            /* g_print("Nick %s\n", msg->nick); */
            /* g_print("User %s\n", msg->user); */
            /* g_print("Host %s\n", msg->host); */
            /* g_print("Command %s\n", msg->command); */
            /* g_print("Params %s\n", msg->params); */

            handle_message(self, msg);
        }
    }

    g_print("Exited\n");
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

    sigs[SIG_CHANNEL_JOINED] = g_signal_new("channel-joined",
                                            GT_TYPE_TWITCH_CHAT_CLIENT,
                                            G_SIGNAL_RUN_LAST,
                                            0, NULL, NULL,
                                            g_cclosure_marshal_VOID__STRING,
                                            G_TYPE_NONE,
                                            1, G_TYPE_STRING);

}


static void
gt_twitch_chat_client_init(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    priv->connected = FALSE;
}

void
gt_twitch_chat_client_connect(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    GSocketConnectable* addr;
    GSocketClient* sock_client;
    GError* err = NULL;
    gchar* oauth_token;
    gchar* nick;

    addr = g_network_address_new(TWITCH_IRC_HOSTNAME, TWITC_IRC_PORT);
    sock_client = g_socket_client_new();
    priv->irc_conn = g_socket_client_connect(sock_client, addr, NULL, &err);

    if (err)
    {
        //TODO: Error handling
        g_print("Error\n");
        goto cleanup;
    }

    priv->istream = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(priv->irc_conn)));
    g_data_input_stream_set_newline_type(priv->istream, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    priv->ostream = g_io_stream_get_output_stream(G_IO_STREAM(priv->irc_conn));

    g_object_get(main_app,
                 "oauth-token", &oauth_token,
                 "user-name", &nick,
                 NULL);
    send_raw_printf(self, "%s%s%s", TWITCH_CHAT_CMD_PASS, oauth_token, CR_LF);
    send_cmd(self, TWITCH_CHAT_CMD_NICK, nick);
    send_cmd(self, TWITCH_CHAT_CMD_CAP_REQ, ":twitch.tv/tags");

    g_message("{GtTwitchChatClient} Connected to chat");
    priv->connected = TRUE;

    priv->worker_thread = g_thread_new("gnome-twitch-chat-worker",
                                       (GThreadFunc) read_lines, self);

    g_free(oauth_token);
    g_free(nick);
cleanup:
    g_object_unref(sock_client);
    g_object_unref(addr);
}

void
gt_twitch_chat_client_disconnect(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    priv->connected = FALSE;

    g_io_stream_close(G_IO_STREAM(priv->irc_conn), NULL, NULL); //TODO: Error handling
    g_clear_object(&priv->irc_conn);
    g_clear_object(&priv->istream);
    g_clear_object(&priv->ostream);
    g_thread_exit(priv->worker_thread);
    g_thread_unref(priv->worker_thread);
}

void
gt_twitch_chat_client_join(GtTwitchChatClient* self, const gchar* channel)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);
    gchar* chan = NULL;

    if (!priv->connected)
        return;

    gt_twitch_chat_client_part(self);

    self->source = gt_twitch_chat_source_new();
    g_source_attach((GSource*) self->source, g_main_context_default());

    if (channel[0] != '#')
        chan = g_strdup_printf("#%s", channel);
    else
        chan = g_strdup(channel);

    g_message("{GtTwitchChatClient} Joining channel '%s'", chan);

    send_cmd(self, TWITCH_CHAT_CMD_JOIN, chan);

    g_free(priv->cur_chan);
    priv->cur_chan = chan;

    g_signal_emit(self, sigs[SIG_CHANNEL_JOINED], 0, priv->cur_chan);
}

void
gt_twitch_chat_client_part(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    if (!priv->connected)
        return;

    if (priv->cur_chan)
    {
        g_message("{GtTwitchChatClient} Parting channel '%s'", priv->cur_chan);
        send_cmd(self, TWITCH_CHAT_CMD_PART, priv->cur_chan);
    }

    g_clear_pointer(&priv->cur_chan, (GDestroyNotify) g_free);
    g_source_destroy((GSource*) self->source);
    g_clear_pointer(&self->source, (GDestroyNotify) g_source_unref);
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
