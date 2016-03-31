#ifndef GT_IRC_H
#define GT_IRC_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_IRC gt_irc_get_type()

G_DECLARE_FINAL_TYPE(GtIrc, gt_irc, GT, IRC, GObject)

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

struct _GtIrc
{
    GObject parent_instance;

    GtTwitchChatSource* source;
};

GtIrc*  gt_irc_new();
void                 gt_irc_connect(GtIrc* self, const gchar* host, int port, const gchar* oauth_token, const gchar* nick);
void                 gt_irc_disconnect(GtIrc* self);
void                 gt_irc_join(GtIrc* self, const gchar* channel);
void                 gt_irc_connect_and_join(GtIrc* self, const gchar* chan);
void                 gt_irc_connect_and_join_async(GtIrc* self, const gchar* chan, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
void                 gt_irc_part(GtIrc* self);
void                 gt_irc_privmsg(GtIrc* self, const gchar* msg);
gboolean             gt_irc_is_connected(GtIrc* self);
gboolean             gt_irc_is_logged_in(GtIrc* self);
GtTwitchChatMessage* gt_twitch_chat_message_new();
void                 gt_twitch_chat_message_free(GtTwitchChatMessage* msg);

G_END_DECLS

#endif
