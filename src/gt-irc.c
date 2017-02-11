#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include "gt-irc.h"
#include "gt-win.h"
#include "gt-app.h"
#include "utils.h"

#define TAG "GtIrc"
#include "gnome-twitch/gt-log.h"

#define CHAT_RPL_STR_WELCOME    "001"
#define CHAT_RPL_STR_YOURHOST   "002"
#define CHAT_RPL_STR_CREATED    "003"
#define CHAT_RPL_STR_MYINFO     "004"
#define CHAT_RPL_STR_MOTDSTART  "375"
#define CHAT_RPL_STR_MOTD       "372"
#define CHAT_RPL_STR_ENDOFMOTD  "376"
#define CHAT_RPL_STR_NAMEREPLY  "353"
#define CHAT_RPL_STR_ENDOFNAMES "366"

#define CHAT_CMD_STR_PING         "PING"
#define CHAT_CMD_STR_PONG         "PONG"
#define CHAT_CMD_STR_PASS_OAUTH   "PASS oauth:"
#define CHAT_CMD_STR_NICK         "NICK"
#define CHAT_CMD_STR_JOIN         "JOIN"
#define CHAT_CMD_STR_PART         "PART"
#define CHAT_CMD_STR_PRIVMSG      "PRIVMSG"
#define CHAT_CMD_STR_CAP_REQ      "CAP REQ"
#define CHAT_CMD_STR_CAP          "CAP"
#define CHAT_CMD_STR_NOTICE       "NOTICE"
#define CHAT_CMD_STR_CHANNEL_MODE "MODE"
#define CHAT_CMD_STR_USERSTATE    "USERSTATE"
#define CHAT_CMD_STR_ROOMSTATE    "ROOMSTATE"
#define CHAT_CMD_STR_CLEARCHAT    "CLEARCHAT"

#define TWITCH_IRC_HOSTNAME "irc.twitch.tv"
#define TWITC_IRC_PORT      6667

#define CR_LF "\r\n"

#define GT_IRC_ERROR g_quark_from_static_string("gt-irc-error")

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

    GtChannel* chan;

    GtIrcState state;
    gboolean recv_logged_in;
    gboolean send_logged_in;

    GMutex mutex;
} GtIrcPrivate;

struct _GtTwitchChatSource
{
    GSource parent_instance;
    GAsyncQueue* queue;
    gboolean resetting_queue;
};

typedef struct
{
    GtIrc* self;
    GDataInputStream* istream;
    GOutputStream* ostream;
} ChatThreadData;

G_DEFINE_TYPE_WITH_PRIVATE(GtIrc, gt_irc, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_STATE,
    NUM_PROPS
};

enum
{
    SIG_ERROR_ENCOUNTERED,
    NUM_SIGS
};

static GParamSpec* props[NUM_PROPS];

static guint sigs[NUM_SIGS];

static const GEnumValue gt_irc_state_enum_values[] =
{
    {GT_IRC_STATE_DISCONNECTED, "GT_IRC_STATE_DISCONNECTED", "disconnected"},
    {GT_IRC_STATE_CONNECTING, "GT_IRC_STATE_CONNECTING", "connecting"},
    {GT_IRC_STATE_CONNECTED, "GT_IRC_STATE_CONNECTED", "connected"},
    {GT_IRC_STATE_LOGGED_IN, "GT_IRC_STATE_LOGGED_IN", "logged-in"},
    {GT_IRC_STATE_JOINED, "GT_IRC_STATE_JOINED", "joined"},
};

GType
gt_irc_state_get_type()
{
    static GType type = 0;

    if (!type)
        type = g_enum_register_static("GtIrcState", gt_irc_state_enum_values);

    return type;
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
    GtIrcMessage* msg;

    msg = g_async_queue_try_pop(self->queue);

    if (!msg)
        return TRUE;
    if (!callback)
    {
        gt_irc_message_free(msg);
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

    ((GtTwitchChatSource*) source)->queue = g_async_queue_new_full((GDestroyNotify) gt_irc_message_free);

    return (GtTwitchChatSource*) source;
}

static void
send_raw_printf(GOutputStream* ostream, const gchar* format, ...)
{
    va_list args;
    gchar* param = NULL;

    va_start(args, format);
    param = g_strdup_vprintf(format, args);
    va_end(args);

    DEBUGF("Sending raw command on osteam='%s' with parameter='%s'",
           (gchar*) g_object_get_data(G_OBJECT(ostream), "type"), param);

    va_start(args, format);
    g_output_stream_printf(ostream, NULL, NULL, NULL, "%s", param);
    va_end(args);
}

static void
send_cmd(GOutputStream* ostream, const gchar* cmd, const gchar* param)
{
    DEBUGF("Sending command='%s' on ostream='%s' with parameter='%s'",
           cmd, (gchar*) g_object_get_data(G_OBJECT(ostream), "type"), param);

    g_output_stream_printf(ostream, NULL, NULL, NULL, "%s %s%s", cmd, param, CR_LF);
}

static void
send_cmd_printf(GOutputStream* ostream, const gchar* cmd, const gchar* format, ...)
{
    va_list args;
    gchar* param = NULL;

    va_start(args, format);
    param = g_strdup_vprintf(format, args);
    va_end(args);

    DEBUGF("Sending command='%s' on ostream='%s' with parameter='%s'",
           cmd, (gchar*) g_object_get_data(G_OBJECT(ostream), "type"), param);

    g_output_stream_printf(ostream, NULL, NULL, NULL, "%s %s%s", cmd, param, CR_LF);

    g_free(param);
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

static inline GtIrcCommandType
chat_cmd_str_to_enum(const gchar* str_cmd)
{
    int ret = -1;

#define IFCASE(name)                                         \
    else if (g_strcmp0(str_cmd, CHAT_CMD_STR_##name) == 0)   \
        ret = GT_IRC_COMMAND_##name;

    if (str_is_numeric(str_cmd))
        ret = GT_IRC_COMMAND_REPLY;
    IFCASE(NOTICE)
    IFCASE(PRIVMSG)
    IFCASE(CAP)
    IFCASE(JOIN)
    IFCASE(PART)
    IFCASE(PING)
    IFCASE(USERSTATE)
    IFCASE(ROOMSTATE)
    IFCASE(CHANNEL_MODE)
    IFCASE(CLEARCHAT)

#undef IFCASE

    return ret;
}

static inline const gchar*
chat_cmd_enum_to_str(GtIrcCommandType num)
{
    const gchar* ret = NULL;

#define ADDCASE(name)                            \
    case GT_IRC_COMMAND_##name:                  \
        ret = CHAT_CMD_STR_##name;               \
        break;

    switch (num)
    {
        ADDCASE(NOTICE);
        ADDCASE(PING);
        ADDCASE(PRIVMSG);
        ADDCASE(CAP);
        ADDCASE(JOIN);
        ADDCASE(PART);
        ADDCASE(CHANNEL_MODE);
        ADDCASE(USERSTATE);
        ADDCASE(ROOMSTATE);
        ADDCASE(CLEARCHAT);

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

gint
emote_compare(const GtChatEmote* a, const GtChatEmote* b)
{
    if (a->start < b->start)
        return -1;
    else if (a->start > b->start)
        return 1;
    else
        return 0;
}


static GtIrcMessage*
parse_line(GtIrc* self, gchar* line)
{
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);
    gchar* orig = line;
    gchar* prefix = NULL;
    GtIrcMessage* msg = g_new0(GtIrcMessage, 1);

    TRACEF("Received line='%s'", line);

    /* g_print("%s\n", line); */

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
        case GT_IRC_COMMAND_REPLY:
            msg->cmd.reply = g_new0(GtIrcCommandReply, 1);
            msg->cmd.reply->type = chat_reply_str_to_enum(cmd);
            msg->cmd.reply->reply = g_strdup(line);
            break;
        case GT_IRC_COMMAND_PING:
            msg->cmd.ping = g_new0(GtIrcCommandPing, 1);
            msg->cmd.ping->server = g_strdup(line);
            break;
        case GT_IRC_COMMAND_PRIVMSG:
            msg->cmd.privmsg = g_new0(GtIrcCommandPrivmsg, 1);
            msg->cmd.privmsg->target = g_strdup(strsep(&line, " "));
            strsep(&line, ":");

            if (line[0] == '\001')
            {
                strsep(&line, " ");
                line[strlen(line) - 1] = '\0';
            }

            msg->cmd.privmsg->msg = g_strdup(line);

            if (!msg->tags)
                break;

            gint user_modes = 0;

            if (atoi(utils_search_key_value_strv(msg->tags, "subscriber")))
                user_modes |= IRC_USER_MODE_SUBSCRIBER;
            if (atoi(utils_search_key_value_strv(msg->tags, "turbo")))
                user_modes |= IRC_USER_MODE_TURBO;

            const gchar* user_type = utils_search_key_value_strv(msg->tags, "user-type");
            if (g_strcmp0(user_type, "mod") == 0) user_modes |= IRC_USER_MODE_MOD;
            else if (g_strcmp0(user_type, "global_mod") == 0) user_modes |= IRC_USER_MODE_GLOBAL_MOD;
            else if (g_strcmp0(user_type, "admin") == 0) user_modes |= IRC_USER_MODE_ADMIN;
            else if (g_strcmp0(user_type, "staff") == 0) user_modes |= IRC_USER_MODE_STAFF;

            msg->cmd.privmsg->user_modes = user_modes;

            const gchar* badges = utils_search_key_value_strv(msg->tags, "badges");

            g_assert_nonnull(badges);

            gchar** badgesv = g_strsplit(badges, ",", -1);

            for (gchar** c = badgesv; *c != NULL; c++)
            {
                gchar** badgev = g_strsplit(*c, "/", -1);
                const gchar* name = *badgev;
                const gchar* version = *(badgev+1);
                GError* err = NULL;

                GtChatBadge* badge = gt_twitch_fetch_chat_badge(main_app->twitch,
                    gt_channel_get_id(priv->chan), name, version, &err);

                g_assert_null(err); //TODO: Propagate the error further

                msg->cmd.privmsg->badges = g_list_append(msg->cmd.privmsg->badges, badge);

                g_strfreev(badgev);
            }

            g_strfreev(badgesv);

            msg->cmd.privmsg->colour = g_strdup(utils_search_key_value_strv(msg->tags, "color"));
            msg->cmd.privmsg->display_name = g_strdup(utils_search_key_value_strv(msg->tags, "display-name"));

            gchar* emotes = g_strdup(utils_search_key_value_strv(msg->tags, "emotes"));
            gchar* _emotes = emotes;
            gchar* e;

            while ((e = strsep(&emotes, "/")) != NULL)
            {
                gint id;
                gchar* indexes;
                gchar* i;

                id = atoi(strsep(&e, ":"));
                indexes = strsep(&e, ":");

                while ((i = strsep(&indexes, ",")) != NULL)
                {
                    GdkPixbuf* pixbuf = NULL;
                    GtChatEmote* emp = gt_chat_emote_new();
                    emp->start = atoi(strsep(&i, "-"));
                    emp->end = atoi(strsep(&i, "-"));
                    emp->id = id;
                    emp->pixbuf = gt_twitch_download_emote(main_app->twitch, emp->id);

                    msg->cmd.privmsg->emotes = g_list_append(msg->cmd.privmsg->emotes, emp);
                }
            }

            g_free(_emotes);

            msg->cmd.privmsg->emotes = g_list_sort(msg->cmd.privmsg->emotes, (GCompareFunc) emote_compare);

            break;
        case GT_IRC_COMMAND_NOTICE:
            msg->cmd.notice = g_new0(GtIrcCommandNotice, 1);
            msg->cmd.notice->target = g_strdup(strsep(&line, " "));
            strsep(&line, ":");
            msg->cmd.notice->msg = g_strdup(strsep(&line, ":"));
            break;
        case GT_IRC_COMMAND_JOIN:
            msg->cmd.join = g_new0(GtIrcCommandJoin, 1);
            msg->cmd.join->channel = g_strdup(strsep(&line, " "));
            break;
        case GT_IRC_COMMAND_PART:
            msg->cmd.part = g_new0(GtIrcCommandPart, 1);
            msg->cmd.part->channel = g_strdup(strsep(&line, " "));
            break;
        case GT_IRC_COMMAND_CAP:
            msg->cmd.cap = g_new0(GtIrcCommandCap, 1);
            msg->cmd.cap->target = g_strdup(strsep(&line, " "));
            msg->cmd.cap->sub_command = g_strdup(strsep(&line, " ")); //TODO: Replace with enum
            msg->cmd.cap->parameter = g_strdup(strsep(&line, " "));
            break;
        case GT_IRC_COMMAND_CHANNEL_MODE:
            msg->cmd.chan_mode = g_new0(GtIrcCommandChannelMode, 1);
            msg->cmd.chan_mode->channel = g_strdup(strsep(&line, " "));
            msg->cmd.chan_mode->modes = g_strdup(strsep(&line, " "));
            msg->cmd.chan_mode->nick = g_strdup(strsep(&line, " "));
            break;
        case GT_IRC_COMMAND_USERSTATE:
            msg->cmd.userstate = g_new0(GtIrcCommandUserstate, 1);
            msg->cmd.userstate->channel = g_strdup(strsep(&line, " "));
            break;
        case GT_IRC_COMMAND_ROOMSTATE:
            msg->cmd.roomstate = g_new0(GtIrcCommandRoomstate, 1);
            msg->cmd.roomstate->channel = g_strdup(strsep(&line, " "));
            break;
        case GT_IRC_COMMAND_CLEARCHAT:
            msg->cmd.clearchat = g_new0(GtIrcCommandClearchat, 1);
            msg->cmd.clearchat->channel = g_strdup(strsep(&line, " "));
            strsep(&line, ":");
            msg->cmd.clearchat->target = g_strdup(strsep(&line, ":"));
            break;
        default:
            WARNINGF("Unhandled IRC command '%s'", cmd);
            break;
    }

    g_free(orig);

    return msg;
}


//TODO: Although clunky this would be cleaner if it's split up into
//two functions one for sending and one for receiving
static gboolean
handle_message(GtIrc* self, GOutputStream* ostream, GtIrcMessage* msg)
{
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    if (ostream == priv->ostream_recv)
    {
        if (!priv->recv_logged_in)
        {
            if (msg->cmd_type == GT_IRC_COMMAND_REPLY && msg->cmd.reply->type == GT_CHAT_REPLY_WELCOME)
            {
                priv->recv_logged_in = TRUE;

                g_mutex_lock(&priv->mutex);

                if (priv->state == GT_IRC_STATE_CONNECTED &&
                    priv->send_logged_in)
                {
                    priv->state = GT_IRC_STATE_LOGGED_IN;
                    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATE]);
                }

                g_mutex_unlock(&priv->mutex);
            }
            else
            {
                GError* err;

                err = g_error_new(GT_IRC_ERROR, ERROR_LOG_IN_FAILED,
                                  "Unable to log in on receive socket, server replied '%s'", msg->cmd.notice->msg);

                g_signal_emit(self, sigs[SIG_ERROR_ENCOUNTERED], 0, err);

                g_error_free(err);

                WARNINGF("Unable to log in on receive socket, server replied with message='%s'",
                          msg->cmd.notice->msg);

                return FALSE;
            }
        }

        if (msg->cmd_type == GT_IRC_COMMAND_PING)
        {
            send_cmd(ostream, CHAT_CMD_STR_PONG, msg->cmd.ping->server);
        }
        else
        {
            if (priv->chan) g_async_queue_push(self->source->queue, msg);
        }
    }
    else if (ostream == priv->ostream_send)
    {
        if (!priv->send_logged_in)
        {
            if (msg->cmd_type == GT_IRC_COMMAND_REPLY && msg->cmd.reply->type == GT_CHAT_REPLY_WELCOME)
            {
                priv->send_logged_in = TRUE;

                g_mutex_lock(&priv->mutex);

                if (priv->state == GT_IRC_STATE_CONNECTED &&
                    priv->recv_logged_in)
                {
                    priv->state = GT_IRC_STATE_LOGGED_IN;
                    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATE]);
                }

                g_mutex_unlock(&priv->mutex);
            }
            else
            {
                GError* err;

                err = g_error_new(GT_IRC_ERROR, ERROR_LOG_IN_FAILED,
                                  "Unable to log in on send socket, server replied '%s'", msg->cmd.notice->msg);

                g_signal_emit(self, sigs[SIG_ERROR_ENCOUNTERED], 0, err);

                g_error_free(err);

                WARNINGF("Unable to log in on send socket, server replied with message='%s'",
                          msg->cmd.notice->msg);

                return FALSE;
            }
        }

        if (msg->cmd_type == GT_IRC_COMMAND_PING)
            send_cmd(ostream, CHAT_CMD_STR_PONG, msg->cmd.ping->server);

        gt_irc_message_free(msg);
    }

    return TRUE;
}

static void
read_lines(ChatThreadData* data)
{
    GtIrc* self = GT_IRC(data->self);
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    gsize read = 0;
    GError* err = NULL;

    if (data->istream == priv->istream_recv)
        INFO("Running chat worker thread for receive");
    else if (data->istream == priv->istream_send)
        INFO("{GtIrc} Running chat worker thread for send");

    for (gchar* line = g_data_input_stream_read_line(data->istream, &read, NULL, &err); !err;
         line = g_data_input_stream_read_line(data->istream, &read, NULL, &err))
    {
        if (priv->state < GT_IRC_STATE_CONNECTED)
            break;

        if (line)
        {
            GtIrcMessage* msg = parse_line(self, line);

            if (!handle_message(self, data->ostream, msg))
                break;
        }
    }

    if (data->istream == priv->istream_recv)
        INFO("Stopping chat worker thread for receive");
    else if (data->istream == priv->istream_send)
        INFO("Stopping chat worker thread for send");
}

static void
error_encountered_cb(GtIrc* self,
                     GError* error,
                     gpointer udata)
{
    gt_irc_disconnect(self);
}

static void
logged_in_cb(GObject* source,
    GParamSpec* pspec, gpointer udata)
{
    g_assert(GT_IS_IRC(source));

    GtIrc* self = GT_IRC(source);
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    if (priv->state == GT_IRC_STATE_LOGGED_IN)
    {
        gt_irc_join(self, gt_channel_get_name(priv->chan));

        g_signal_handlers_disconnect_by_func(self, logged_in_cb, self);
    }
}

static void
finalise(GObject* obj)
{
    GtIrc* self = GT_IRC(obj);
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    G_OBJECT_CLASS(gt_irc_parent_class)->finalize(obj);

    gt_irc_disconnect(self);

    //TODO: Free other stuff
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtIrc* self = GT_IRC(obj);
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    switch (prop)
    {
        case PROP_STATE:
            g_value_set_enum(val, priv->state);
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
    GtIrc* self = GT_IRC(obj);
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_irc_class_init(GtIrcClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    sigs[SIG_ERROR_ENCOUNTERED] = g_signal_new("error-encountered",
                                               GT_TYPE_IRC,
                                               G_SIGNAL_RUN_LAST,
                                               0, NULL, NULL,
                                               g_cclosure_marshal_VOID__OBJECT,
                                               G_TYPE_NONE,
                                               1, G_TYPE_ERROR);

    props[PROP_STATE] = g_param_spec_enum("state", "State", "Current state",
        GT_TYPE_IRC_STATE, GT_IRC_STATE_DISCONNECTED, G_PARAM_READABLE);

    g_object_class_install_properties(obj_class, NUM_PROPS, props);
}

static void
gt_irc_init(GtIrc* self)
{
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    priv->recv_logged_in = FALSE;
    priv->send_logged_in = FALSE;
    priv->state = GT_IRC_STATE_DISCONNECTED;

    g_mutex_init(&priv->mutex);

    self->source = gt_twitch_chat_source_new();
    g_source_attach((GSource*) self->source, g_main_context_default());

    g_signal_connect_after(self, "error-encountered", G_CALLBACK(error_encountered_cb), NULL);
}

void
gt_irc_connect(GtIrc* self,
    const gchar* host, int port,
    const gchar* oauth_token, const gchar* nick)
{
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    GSocketConnectable* addr;
    GSocketClient* sock_client;
    GError* err = NULL;
    ChatThreadData* recv_data;
    ChatThreadData* send_data;

    MESSAGEF("Connecting with nick='%s', host='%s' and port='%d'",
             nick, host, port);

    addr = g_network_address_new(host, port);
    sock_client = g_socket_client_new();

    priv->irc_conn_recv = g_socket_client_connect(sock_client, addr, NULL, &err);
    if (err)
    {
        //TODO: Error handling
        g_print("Error\n");
        goto cleanup;
    }
    g_clear_error(&err); // Probably unnecessary
    priv->irc_conn_send = g_socket_client_connect(sock_client, addr, NULL, &err);
    if (err)
    {
        //TODO: Error handling
        g_print("Error\n");
        goto cleanup;
    }

    priv->state = GT_IRC_STATE_CONNECTED;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATE]);

    priv->istream_recv = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(priv->irc_conn_recv)));
    g_data_input_stream_set_newline_type(priv->istream_recv, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    priv->ostream_recv = g_io_stream_get_output_stream(G_IO_STREAM(priv->irc_conn_recv));
    g_object_set_data(G_OBJECT(priv->ostream_recv), "type", "receive");

    priv->istream_send = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(priv->irc_conn_send)));
    g_data_input_stream_set_newline_type(priv->istream_send, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    priv->ostream_send = g_io_stream_get_output_stream(G_IO_STREAM(priv->irc_conn_send));
    g_object_set_data(G_OBJECT(priv->ostream_send), "type", "send");

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

    if (utils_str_empty(oauth_token))
    {
        gchar* _nick = g_strdup_printf("justinfan%d", g_random_int_range(1, 9999999));
        send_cmd(priv->ostream_recv, CHAT_CMD_STR_NICK, _nick);
        send_cmd(priv->ostream_send, CHAT_CMD_STR_NICK, _nick);
        g_free(_nick);
    }
    else
    {
        send_raw_printf(priv->ostream_recv, "%s%s%s", CHAT_CMD_STR_PASS_OAUTH, oauth_token, CR_LF);
        send_cmd(priv->ostream_recv, CHAT_CMD_STR_NICK, nick);
        send_raw_printf(priv->ostream_send, "%s%s%s", CHAT_CMD_STR_PASS_OAUTH, oauth_token, CR_LF);
        send_cmd(priv->ostream_send, CHAT_CMD_STR_NICK, nick);
    }

    send_cmd(priv->ostream_recv, CHAT_CMD_STR_CAP_REQ, ":twitch.tv/tags");
    send_cmd(priv->ostream_recv, CHAT_CMD_STR_CAP_REQ, ":twitch.tv/membership");
    send_cmd(priv->ostream_recv, CHAT_CMD_STR_CAP_REQ, ":twitch.tv/commands");

cleanup:
    g_object_unref(sock_client);
    g_object_unref(addr);
}

void
gt_irc_disconnect(GtIrc* self)
{
    g_assert(GT_IS_IRC(self));

    MESSAGE("Disconnecting");

    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    if (priv->state < GT_IRC_STATE_CONNECTED)
    {
        WARNING("Trying to disconnect when not connected");

        return;
    }

    if (priv->state >= GT_IRC_STATE_LOGGED_IN)
        gt_irc_part(self);

    priv->state = GT_IRC_STATE_DISCONNECTED;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATE]);

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

    g_object_unref(priv->chan);

    self->source->resetting_queue = TRUE;
    g_async_queue_unref(self->source->queue);
    self->source->queue = g_async_queue_new_full((GDestroyNotify) gt_irc_message_free);
    self->source->resetting_queue = FALSE;

    priv->recv_logged_in = FALSE;
    priv->send_logged_in = FALSE;

}

void
gt_irc_join(GtIrc* self, const gchar* channel)
{
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);
    gchar* chan = NULL;

    if (priv->state != GT_IRC_STATE_LOGGED_IN)
    {
        WARNING("Trying to join channel with name '%s' when not logged in", channel);

        return;
    }

    if (channel[0] != '#')
        chan = g_strdup_printf("#%s", channel);
    else
        chan = g_strdup(channel);

    MESSAGEF("Joining with channel='%s'", chan);

    send_cmd(priv->ostream_recv, CHAT_CMD_STR_JOIN, chan);
    send_cmd(priv->ostream_send, CHAT_CMD_STR_JOIN, chan);

    priv->state = GT_IRC_STATE_JOINED;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATE]);
}

void
gt_irc_part(GtIrc* self)
{
    g_assert(GT_IS_IRC(self));

    GtIrcPrivate* priv = gt_irc_get_instance_private(self);
    const gchar* name;

    if (priv->state < GT_IRC_STATE_JOINED)
    {
        WARNING("Trying to part channel when not joined");

        return;
    }

    name = gt_channel_get_name(priv->chan);

    MESSAGEF("Parting with channel='%s'", name);

    send_cmd(priv->ostream_recv, CHAT_CMD_STR_PART, name);
    send_cmd(priv->ostream_send, CHAT_CMD_STR_PART, name);

    priv->state = GT_IRC_STATE_LOGGED_IN;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATE]);
}

void
gt_irc_connect_and_join_channel(GtIrc* self, GtChannel* chan)
{
    g_assert(GT_IS_IRC(self));
    g_assert(GT_IS_CHANNEL(chan));

    GtIrcPrivate* priv = gt_irc_get_instance_private(self);
    GList* servers = NULL;
    gint pos = 0;
    gchar host[20];
    gint port;
    const GtUserInfo* info = NULL;
    GError* err = NULL;

    if (priv->state != GT_IRC_STATE_DISCONNECTED)
    {
        WARNING("Trying to connect before being disconnected");

        return;
    }

    priv->state = GT_IRC_STATE_CONNECTING;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATE]);

    priv->chan = g_object_ref(chan);

    gt_twitch_load_chat_badge_sets_for_channel(main_app->twitch, gt_channel_get_id(priv->chan), &err);

    g_assert_no_error(err); //TODO: Propagate error further

    servers = gt_twitch_chat_servers(main_app->twitch, gt_channel_get_name(chan), &err);

    g_assert_no_error(err); //TODO: Propagate error further

    pos = g_random_int() % g_list_length(servers);

    sscanf((gchar*) g_list_nth(servers, pos)->data, "%[^:]:%d", host, &port);

    g_signal_connect(self, "notify::state", G_CALLBACK(logged_in_cb), self);

    info = gt_app_get_user_info(main_app);

    gt_irc_connect(self, host, port,
        info ? info->oauth_token : NULL,
        info ? info->name : NULL);

    g_list_free_full(servers, g_free);
}

static void
connect_and_join_channel_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(G_IS_TASK(task));
    g_assert(GT_IS_IRC(source));
    g_assert(GT_IS_CHANNEL(task_data));

    GtIrc* self = GT_IRC(source);
    GtChannel* chan = task_data;

    gt_irc_connect_and_join_channel(self, chan);

    if (g_task_return_error_if_cancelled(task))
        gt_irc_disconnect(self);

    g_object_unref(chan);
}

void
gt_irc_connect_and_join_channel_async(GtIrc* self, GtChannel* chan,
    GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata)
{
    g_assert(GT_IS_IRC(self));
    g_assert(GT_IS_CHANNEL(chan));

    GTask* task = NULL;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    g_task_set_task_data(task, g_object_ref(chan), (GDestroyNotify) g_object_unref);

    g_task_run_in_thread(task, connect_and_join_channel_async_cb);
}

void
gt_irc_privmsg(GtIrc* self, const gchar* msg)
{
    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    if (priv->state < GT_IRC_STATE_JOINED)
    {
        WARNING("Trying to privmsg when not joined");

        return;
    }

    send_cmd_printf(priv->ostream_send, CHAT_CMD_STR_PRIVMSG, "%s :%s",
        gt_channel_get_name(priv->chan), msg);
}

GtIrcState
gt_irc_get_state(GtIrc* self)
{
    g_assert(GT_IS_IRC(self));

    GtIrcPrivate* priv = gt_irc_get_instance_private(self);

    return priv->state;
}

void
gt_irc_message_free(GtIrcMessage* msg)
{
    g_free(msg->nick);
    g_free(msg->user);
    g_free(msg->host);
    g_strfreev(msg->tags);

    switch (msg->cmd_type)
    {
        case GT_IRC_COMMAND_NOTICE:
            g_free(msg->cmd.notice->msg);
            g_free(msg->cmd.notice->target);
            g_free(msg->cmd.notice);
            break;
        case GT_IRC_COMMAND_PING:
            g_free(msg->cmd.ping->server);
            g_free(msg->cmd.ping);
            break;
        case GT_IRC_COMMAND_PRIVMSG:
            g_free(msg->cmd.privmsg->msg);
            g_free(msg->cmd.privmsg->target);
            g_free(msg->cmd.privmsg->colour);
            g_free(msg->cmd.privmsg->display_name);
            gt_chat_emote_list_free(msg->cmd.privmsg->emotes);
            g_free(msg->cmd.privmsg);
            break;
        case GT_IRC_COMMAND_REPLY:
            g_free(msg->cmd.reply->reply);
            g_free(msg->cmd.reply);
            break;
        case GT_IRC_COMMAND_JOIN:
            g_free(msg->cmd.join->channel);
            g_free(msg->cmd.join);
            break;
        case GT_IRC_COMMAND_PART:
            g_free(msg->cmd.part->channel);
            g_free(msg->cmd.part);
            break;
        case GT_IRC_COMMAND_CAP:
            g_free(msg->cmd.cap->parameter);
            g_free(msg->cmd.cap->target);
            g_free(msg->cmd.cap->sub_command);
            g_free(msg->cmd.cap);
            break;
        case GT_IRC_COMMAND_CHANNEL_MODE:
            g_free(msg->cmd.chan_mode->channel);
            g_free(msg->cmd.chan_mode->modes);
            g_free(msg->cmd.chan_mode->nick);
            g_free(msg->cmd.chan_mode);
            break;
        case GT_IRC_COMMAND_USERSTATE:
            g_free(msg->cmd.userstate->channel);
            g_free(msg->cmd.userstate);
            break;
        case GT_IRC_COMMAND_ROOMSTATE:
            g_free(msg->cmd.roomstate->channel);
            g_free(msg->cmd.roomstate);
            break;
        case GT_IRC_COMMAND_CLEARCHAT:
            g_free(msg->cmd.clearchat->channel);
            g_free(msg->cmd.clearchat->target);
            g_free(msg->cmd.clearchat);
            break;
        default:
            break;
    }

    g_free(msg);
}

GtIrc*
gt_irc_new()
{
    return g_object_new(GT_TYPE_IRC,
                        NULL);
}
