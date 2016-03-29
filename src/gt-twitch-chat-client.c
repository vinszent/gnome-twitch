#include "gt-twitch-chat-client.h"
#include "gt-app.h"
#include <string.h>
#include <stdio.h>
#include <glib/gprintf.h>

#define CHAT_RPL_STR_WELCOME    "001"
#define CHAT_RPL_STR_YOURHOST   "002"
#define CHAT_RPL_STR_CREATED    "003"
#define CHAT_RPL_STR_MYINFO     "004"
#define CHAT_RPL_STR_MOTDSTART  "375"
#define CHAT_RPL_STR_MOTD       "372"
#define CHAT_RPL_STR_ENDOFMOTD  "376"
#define CHAT_RPL_STR_NAMEREPLY  "353"
#define CHAT_RPL_STR_ENDOFNAMES "366"

#define CHAT_CMD_STR_PING    "PING"
#define CHAT_CMD_STR_PONG    "PONG"
#define CHAT_CMD_STR_PASS    "PASS oauth:"
#define CHAT_CMD_STR_NICK    "NICK"
#define CHAT_CMD_STR_JOIN    "JOIN"
#define CHAT_CMD_STR_PART    "PART"
#define CHAT_CMD_STR_PRIVMSG "PRIVMSG"
#define CHAT_CMD_STR_CAP_REQ "CAP REQ"
#define CHAT_CMD_STR_CAP     "CAP"
#define CHAT_CMD_STR_NOTICE  "NOTICE"
#define CHAT_CMD_STR_MODE    "MODE"

#define TWITCH_IRC_HOSTNAME "irc.twitch.tv"
#define TWITC_IRC_PORT 6667

#define CR_LF "\r\n"

#define GT_TWITCH_CHAT_CLIENT_ERROR g_quark_from_static_string("gt-twitch-chat-client-error")

enum
{
    ERROR_LOG_IN_FAILED,
};

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
    gboolean recv_logged_in;
    gboolean send_logged_in;
} GtTwitchChatClientPrivate;

struct _GtTwitchChatSource
{
    GSource parent_instance;
    GAsyncQueue* queue;
    gboolean resetting_queue;
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
    PROP_LOGGED_IN,
    NUM_PROPS
};

enum
{
    SIG_ERROR_ENCOUNTERED,
    NUM_SIGS
};

static GParamSpec* props[NUM_PROPS];

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
    gint len = 0;

    if (!self->resetting_queue)
        len = g_async_queue_length_unlocked(self->queue);

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
    g_info("{GtTwitchChatClient} Sending command '%s' with parameter '%s'", cmd, param);

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

    g_info("{GtTwitchChatClient} Sending command '%s' with parameter '%s'", cmd, param);

    g_output_stream_printf(ostream, NULL, NULL, NULL, "%s %s%s", cmd, param, CR_LF);
}

static gboolean
str_is_numeric(const gchar* str)
{
    char c = str[0];
    for (int i = 0; c != '\0'; i++, c = str[i])
    {
        if (!g_ascii_isdigit(c))
            return FALSE;
    }

    return TRUE;
}

static inline GtChatCommandType
chat_cmd_str_to_enum(const gchar* str_cmd)
{
    int ret = -1;

#define IFCASE(name)                                         \
    else if (g_strcmp0(str_cmd, CHAT_CMD_STR_##name) == 0)   \
        ret = GT_CHAT_COMMAND_##name;

    if (str_is_numeric(str_cmd))
        ret = GT_CHAT_COMMAND_REPLY;
    IFCASE(NOTICE)
    IFCASE(PRIVMSG)
    IFCASE(CAP)
    IFCASE(JOIN)
    IFCASE(PING)

#undef IFCASE

    return ret;
}

static inline const gchar*
chat_cmd_enum_to_str(GtChatCommandType num)
{
    const gchar* ret = NULL;

#define ADDCASE(name)                             \
    case GT_CHAT_COMMAND_##name:                  \
        ret = CHAT_CMD_STR_##name;                \
        break;

    switch (num)
    {
        ADDCASE(NOTICE);
        ADDCASE(PING);
        ADDCASE(PRIVMSG);
        ADDCASE(CAP);
        ADDCASE(JOIN);

        default:
            break;
    }

#undef ADDCASE

    return ret;
}

static inline GtChatReplyType
chat_reply_str_to_enum(const gchar* str_reply)
{
    int ret = -1;

#define ADDCASE(name)                                            \
    else if (g_strcmp0(str_reply, CHAT_RPL_STR_##name) == 0)    \
        ret = GT_CHAT_REPLY_##name;

    if (g_strcmp0(str_reply, CHAT_RPL_STR_WELCOME) == 0)
        ret = GT_CHAT_REPLY_WELCOME;
    ADDCASE(YOURHOST)
    ADDCASE(CREATED)
    ADDCASE(MYINFO)
    ADDCASE(MOTDSTART)
    ADDCASE(MOTD)
    ADDCASE(ENDOFMOTD)
    ADDCASE(NAMEREPLY)
    ADDCASE(ENDOFNAMES)

#undef ADDCASE

    return ret;
}


static void
parse_line(gchar* line, GtTwitchChatMessage* msg)
{
    gchar* orig = line;
    gchar* prefix = NULL;

    g_print("%s\n", line);

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

    gchar* cmd = strsep(&line, " ");
    msg->cmd_type = chat_cmd_str_to_enum(cmd);

    switch (msg->cmd_type)
    {
        case GT_CHAT_COMMAND_REPLY:
            msg->cmd.reply = g_new0(GtChatCommandReply, 1);
            msg->cmd.reply->type = chat_reply_str_to_enum(cmd);
            msg->cmd.reply->reply = g_strdup(line);
            break;
        case GT_CHAT_COMMAND_PING:
            msg->cmd.ping = g_new0(GtChatCommandPing, 1);
            msg->cmd.ping->server = g_strdup(line);
            break;
        case GT_CHAT_COMMAND_PRIVMSG:
            msg->cmd.privmsg = g_new0(GtChatCommandPrivmsg, 1);
            msg->cmd.privmsg->target = g_strdup(strsep(&line, " "));
            strsep(&line, ":");
            msg->cmd.privmsg->msg = g_strdup(strsep(&line, ":"));
            break;
        case GT_CHAT_COMMAND_NOTICE:
            msg->cmd.notice = g_new0(GtChatCommandNotice, 1);
            msg->cmd.notice->target = g_strdup(strsep(&line, " "));
            strsep(&line, ":");
            msg->cmd.notice->msg = g_strdup(strsep(&line, ":"));
            break;
        case GT_CHAT_COMMAND_JOIN:
            msg->cmd.join = g_new0(GtChatCommandJoin, 1);
            msg->cmd.join->channel = g_strdup(strsep(&line, " "));
            break;
        case GT_CHAT_COMMAND_CAP:
            msg->cmd.cap = g_new0(GtChatCommandCap, 1);
            msg->cmd.cap->target = g_strdup(strsep(&line, " "));
            msg->cmd.cap->sub_command = g_strdup(strsep(&line, " ")); //TODO: Replace with enum
            msg->cmd.cap->parameter = g_strdup(strsep(&line, " "));
            break;
        case GT_CHAT_COMMAND_CHANNEL_MODE:
            msg->cmd.chan_mode = g_new0(GtChatCommandChannelMode, 1);
            msg->cmd.chan_mode->channel = g_strdup(strsep(&line, " "));
            msg->cmd.chan_mode->modes = g_strdup(strsep(&line, " "));
            msg->cmd.chan_mode->nick = g_strdup(strsep(&line, " "));
            break;
        default:
            g_warning("{GtTwitchChatClient} Unhandled irc command '%s'\n", line);
            break;
    }

    g_free(orig);
}

static gboolean
handle_message(GtTwitchChatClient* self, GOutputStream* ostream, GtTwitchChatMessage* msg)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    if (ostream == priv->ostream_recv)
    {
        if (!priv->recv_logged_in)
        {
            if (msg->cmd_type == GT_CHAT_COMMAND_REPLY && msg->cmd.reply->type == GT_CHAT_REPLY_WELCOME)
            {
                priv->recv_logged_in = TRUE;

                if (priv->send_logged_in)
                    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOGGED_IN]);
            }
            else
            {
                GError* err;

                err = g_error_new(GT_TWITCH_CHAT_CLIENT_ERROR, ERROR_LOG_IN_FAILED,
                                  "Unable to log in on receive socket, server replied '%s'", msg->cmd.notice->msg);

                g_signal_emit(self, sigs[SIG_ERROR_ENCOUNTERED], 0, err);

                g_error_free(err);

                g_warning("{GtTwitchChatClient} Unable to log in on recive socket, server replied '%s'",
                          msg->cmd.notice->msg);

                return FALSE;
            }
        }

        if (msg->cmd_type == GT_CHAT_COMMAND_PING)
        {
            send_cmd(ostream, TWITCH_CHAT_CMD_PONG, msg->cmd.ping->server);
        }
        else
        {
            if (priv->cur_chan) g_async_queue_push(self->source->queue, msg);
        }
    }
    else if (ostream == priv->ostream_send)
    {
        if (!priv->send_logged_in)
        {
            if (msg->cmd_type == GT_CHAT_COMMAND_REPLY && msg->cmd.reply->type == GT_CHAT_REPLY_WELCOME)
            {
                priv->send_logged_in = TRUE;

                if (priv->recv_logged_in)
                    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOGGED_IN]);
            }
            else
            {
                GError* err;

                err = g_error_new(GT_TWITCH_CHAT_CLIENT_ERROR, ERROR_LOG_IN_FAILED,
                                  "Unable to log in on send socket, server replied '%s'", msg->cmd.notice->msg);

                g_signal_emit(self, sigs[SIG_ERROR_ENCOUNTERED], 0, err);

                g_error_free(err);

                g_warning("{GtTwitchChatClient} Unable to log in on send socket, server replied '%s'",
                          msg->cmd.notice->msg);

                return FALSE;
            }
        }

        if (msg->cmd_type == GT_CHAT_COMMAND_PING)
            send_cmd(ostream, TWITCH_CHAT_CMD_PONG, msg->cmd.ping->server);

        gt_twitch_chat_message_free(msg);
    }

    return TRUE;
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

    if (data->istream == priv->istream_recv)
        g_message("{GtTwitchChatClient} Running chat worker thread for receive");
    else if (data->istream == priv->istream_send)
        g_message("{GtTwitchChatClient} Running chat worker thread for send");

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

            if (!handle_message(self, data->ostream, msg))
                break;
        }
    }
    if (data->istream == priv->istream_recv)
        g_message("{GtTwitchChatClient} Stopping chat worker thread for receive");
    else if (data->istream == priv->istream_send)
        g_message("{GtTwitchChatClient} Stopping chat worker thread for send");
}

static void
error_encountered_cb(GtTwitchChatClient* self,
                     GError* error,
                     gpointer udata)
{
    gt_twitch_chat_client_disconnect(self);
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
        case PROP_LOGGED_IN:
            g_value_set_boolean(val, priv->recv_logged_in && priv->send_logged_in);
            break;
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

    sigs[SIG_ERROR_ENCOUNTERED] = g_signal_new("error-encountered",
                                               GT_TYPE_TWITCH_CHAT_CLIENT,
                                               G_SIGNAL_RUN_LAST,
                                               0, NULL, NULL,
                                               g_cclosure_marshal_VOID__OBJECT,
                                               G_TYPE_NONE,
                                               1, G_TYPE_ERROR);

    props[PROP_LOGGED_IN] = g_param_spec_boolean("logged-in",
                                                 "Logged In",
                                                 "Whether logged in",
                                                 FALSE,
                                                 G_PARAM_READABLE);

    g_object_class_install_properties(obj_class, NUM_PROPS, props);
}

static void
gt_twitch_chat_client_init(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    priv->connected = FALSE;
    priv->recv_logged_in = FALSE;
    priv->send_logged_in = FALSE;

    self->source = gt_twitch_chat_source_new();
    g_source_attach((GSource*) self->source, g_main_context_default());

    g_signal_connect_after(self, "error-encountered", G_CALLBACK(error_encountered_cb), NULL);
}

void
gt_twitch_chat_client_connect(GtTwitchChatClient* self,
                              const gchar* host, int port,
                              const gchar* oauth_token, const gchar* nick)
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

    addr = g_network_address_new(host, port);
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

    priv->connected = TRUE;

    priv->istream_recv = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(priv->irc_conn_recv)));
    g_data_input_stream_set_newline_type(priv->istream_recv, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    priv->ostream_recv = g_io_stream_get_output_stream(G_IO_STREAM(priv->irc_conn_recv));

    priv->istream_send = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(priv->irc_conn_send)));
    g_data_input_stream_set_newline_type(priv->istream_send, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    priv->ostream_send = g_io_stream_get_output_stream(G_IO_STREAM(priv->irc_conn_send));

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

    send_raw_printf(priv->ostream_recv, "%s%s%s", TWITCH_CHAT_CMD_PASS, oauth_token, CR_LF);
    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_NICK, nick);
    send_raw_printf(priv->ostream_send, "%s%s%s", TWITCH_CHAT_CMD_PASS, oauth_token, CR_LF);
    send_cmd(priv->ostream_send, TWITCH_CHAT_CMD_NICK, nick);

    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_CAP_REQ, ":twitch.tv/tags");
    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_CAP_REQ, ":twitch.tv/membership");
    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_CAP_REQ, ":twitch.tv/commands");

cleanup:
    g_object_unref(sock_client);
    g_object_unref(addr);
}

void
gt_twitch_chat_client_disconnect(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    if (priv->connected)
    {
        if (priv->cur_chan) gt_twitch_chat_client_part(self);

        priv->connected = FALSE;
        priv->recv_logged_in = FALSE;
        priv->send_logged_in = FALSE;

        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOGGED_IN]);

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

    self->source->resetting_queue = TRUE;
    g_async_queue_unref(self->source->queue);
    self->source->queue = g_async_queue_new_full((GDestroyNotify) gt_twitch_chat_message_free);
    self->source->resetting_queue = FALSE;

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

    priv->cur_chan = chan;

    g_message("{GtTwitchChatClient} Joining channel '%s'", chan);

    send_cmd(priv->ostream_recv, TWITCH_CHAT_CMD_JOIN, chan);
    send_cmd(priv->ostream_send, TWITCH_CHAT_CMD_JOIN, chan);
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

}

//TODO: Async version
void
gt_twitch_chat_client_connect_and_join(GtTwitchChatClient* self, const gchar* chan)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);
    GList* servers = NULL;
    int pos = 0;
    gchar host[20];
    int port;

    g_return_if_fail(!priv->connected);

    servers = gt_twitch_chat_servers(main_app->twitch, chan);

    pos = g_random_int() % g_list_length(servers);

    sscanf((gchar*) g_list_nth(servers, pos)->data, "%[^:]:%d", host, &port);

    gt_twitch_chat_client_connect(self, host, port,
                                  gt_app_get_oauth_token(main_app),
                                  gt_app_get_user_name(main_app));

    gt_twitch_chat_client_join(self, chan);

    g_list_free_full(servers, g_free);
}

static void
connect_and_join_async_cb(GTask* task,
                          gpointer source,
                          gpointer task_data,
                          GCancellable* cancel)
{
    GtTwitchChatClient* self = GT_TWITCH_CHAT_CLIENT(source);
    gchar* chan;

    if (g_task_return_error_if_cancelled(task))
        return;

    chan = task_data;

    gt_twitch_chat_client_connect_and_join(self, chan);
}

void
gt_twitch_chat_client_connect_and_join_async(GtTwitchChatClient* self, const gchar* chan,
                                             GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata)
{
    GTask* task;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    g_task_set_task_data(task, g_strdup(chan), (GDestroyNotify) g_free);

    g_task_run_in_thread(task, connect_and_join_async_cb);

    g_object_unref(task);
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

gboolean
gt_twitch_chat_client_is_logged_in(GtTwitchChatClient* self)
{
    GtTwitchChatClientPrivate* priv = gt_twitch_chat_client_get_instance_private(self);

    return priv->recv_logged_in && priv->send_logged_in;
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
    g_strfreev(msg->tags);

    switch (msg->cmd_type)
    {
        case GT_CHAT_COMMAND_NOTICE:
            g_free(msg->cmd.notice->msg);
            g_free(msg->cmd.notice->target);
            g_free(msg->cmd.notice);
            break;
        case GT_CHAT_COMMAND_PING:
            g_free(msg->cmd.ping->server);
            g_free(msg->cmd.ping);
            break;
        case GT_CHAT_COMMAND_PRIVMSG:
            g_free(msg->cmd.privmsg->msg);
            g_free(msg->cmd.privmsg->target);
            g_free(msg->cmd.privmsg);
            break;
        case GT_CHAT_COMMAND_REPLY:
            g_free(msg->cmd.reply->reply);
            g_free(msg->cmd.reply);
            break;
        case GT_CHAT_COMMAND_JOIN:
            g_free(msg->cmd.join->channel);
            g_free(msg->cmd.join);
            break;
        case GT_CHAT_COMMAND_CAP:
            g_free(msg->cmd.cap->parameter);
            g_free(msg->cmd.cap->target);
            g_free(msg->cmd.cap->sub_command);
            g_free(msg->cmd.cap);
            break;
        case GT_CHAT_COMMAND_CHANNEL_MODE:
            g_free(msg->cmd.chan_mode->channel);
            g_free(msg->cmd.chan_mode->modes);
            g_free(msg->cmd.chan_mode->nick);
            g_free(msg->cmd.chan_mode);
            break;
        default:
            break;
    }

    g_free(msg);
}
