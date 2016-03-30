#ifndef GT_TWITCH_CHAT_CLIENT_H
#define GT_TWITCH_CHAT_CLIENT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_TWITCH_CHAT_CLIENT gt_twitch_chat_client_get_type()

G_DECLARE_FINAL_TYPE(GtTwitchChatClient, gt_twitch_chat_client, GT, TWITCH_CHAT_CLIENT, GObject)

typedef enum
{
    GT_CHAT_COMMAND_NOTICE,
    GT_CHAT_COMMAND_PRIVMSG,
    GT_CHAT_COMMAND_PING,
    GT_CHAT_COMMAND_REPLY,
    GT_CHAT_COMMAND_CHANNEL_MODE,
    GT_CHAT_COMMAND_CAP,
    GT_CHAT_COMMAND_JOIN,
    GT_CHAT_COMMAND_USERSTATE,
    GT_CHAT_COMMAND_ROOMSTATE,
    GT_CHAT_COMMAND_CLEARCHAT,
} GtChatCommandType;

typedef enum
{
    GT_CHAT_REPLY_WELCOME    = 1,
    GT_CHAT_REPLY_YOURHOST   = 2,
    GT_CHAT_REPLY_CREATED    = 3,
    GT_CHAT_REPLY_MYINFO     = 4,
    GT_CHAT_REPLY_MOTDSTART  = 375,
    GT_CHAT_REPLY_MOTD       = 372,
    GT_CHAT_REPLY_ENDOFMOTD  = 376,
    GT_CHAT_REPLY_NAMEREPLY  = 353,
    GT_CHAT_REPLY_ENDOFNAMES = 366,
} GtChatReplyType;

typedef struct
{
    gchar* target;
    gchar* msg;
} GtChatCommandNotice;

typedef struct
{
    gchar* target;
    gchar* msg;
    gint user_modes;
    gchar* display_name;
    gchar** emotes;
    gchar* colour;
} GtChatCommandPrivmsg;

typedef struct
{
    gchar* server;
} GtChatCommandPing;

typedef struct
{
    gchar* channel;
} GtChatCommandJoin;

typedef enum
{
    GT_CHAT_CAP_SUB_COMMAND_ACK,
    GT_CHAT_CAP_SUB_COMMAND_LS,
    GT_CHAT_CAP_SUB_COMMAND_LIST,
    GT_CHAT_CAP_SUB_COMMAND_NAK,
    GT_CHAT_CAP_SUB_COMMAND_END,
    GT_CHAT_CAP_SUB_COMMAND_REQ
} GtChatCapSubCommandType;

typedef struct
{
    gchar* target;
    gchar* sub_command; //TODO: Change this to enum
    gchar* parameter;
} GtChatCommandCap;

typedef struct
{
    GtChatReplyType type;
    gchar* reply; // Might need another union for more complex replies
} GtChatCommandReply;

typedef struct
{
    gchar* channel;
    gchar* modes; //TODO: Turn this into bitmask of modes
    gchar* nick;
} GtChatCommandChannelMode;

typedef struct
{
    gchar* channel;
} GtChatCommandUserstate;

typedef struct
{
    gchar* channel;
} GtChatCommandRoomstate;

typedef struct
{
    gchar* channel;
    gchar* target;
} GtChatCommandClearchat;

typedef struct
{
    gchar* nick;
    gchar* user;
    gchar* host;
    GtChatCommandType cmd_type;
    gchar** tags;
    union
    {
        GtChatCommandNotice* notice;
        GtChatCommandPrivmsg* privmsg;
        GtChatCommandPing* ping;
        GtChatCommandJoin* join;
        GtChatCommandCap* cap;
        GtChatCommandReply* reply;
        GtChatCommandChannelMode* chan_mode;
        GtChatCommandUserstate* userstate;
        GtChatCommandRoomstate* roomstate;
        GtChatCommandClearchat* clearchat;
    } cmd;
} GtTwitchChatMessage;

typedef gboolean (*GtTwitchChatSourceFunc) (GtTwitchChatMessage* msg, gpointer udata);

typedef struct _GtTwitchChatSource GtTwitchChatSource;

struct _GtTwitchChatClient
{
    GObject parent_instance;

    GtTwitchChatSource* source;
};

GtTwitchChatClient*  gt_twitch_chat_client_new();
void                 gt_twitch_chat_client_connect(GtTwitchChatClient* self, const gchar* host, int port, const gchar* oauth_token, const gchar* nick);
void                 gt_twitch_chat_client_disconnect(GtTwitchChatClient* self);
void                 gt_twitch_chat_client_join(GtTwitchChatClient* self, const gchar* channel);
void                 gt_twitch_chat_client_connect_and_join(GtTwitchChatClient* self, const gchar* chan);
void                 gt_twitch_chat_client_connect_and_join_async(GtTwitchChatClient* self, const gchar* chan, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
void                 gt_twitch_chat_client_part(GtTwitchChatClient* self);
void                 gt_twitch_chat_client_privmsg(GtTwitchChatClient* self, const gchar* msg);
gboolean             gt_twitch_chat_client_is_connected(GtTwitchChatClient* self);
gboolean             gt_twitch_chat_client_is_logged_in(GtTwitchChatClient* self);
GtTwitchChatMessage* gt_twitch_chat_message_new();
void                 gt_twitch_chat_message_free(GtTwitchChatMessage* msg);

G_END_DECLS

#endif
