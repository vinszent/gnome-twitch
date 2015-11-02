#ifndef GT_TWITCH_CHAT_CLIENT_H
#define GT_TWITCH_CHAT_CLIENT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TWITCH_CHAT_CMD_WELCOME "001"
#define TWITCH_CHAT_CMD_PING "PING"
#define TWITCH_CHAT_CMD_PONG "PONG"
#define TWITCH_CHAT_CMD_PASS "PASS oauth:"
#define TWITCH_CHAT_CMD_NICK "NICK"
#define TWITCH_CHAT_CMD_JOIN "JOIN"
#define TWITCH_CHAT_CMD_PART "PART"
#define TWITCH_CHAT_CMD_PRIVMSG "PRIVMSG"
#define TWITCH_CHAT_CMD_CAP_REQ "CAP REQ"

#define GT_TYPE_TWITCH_CHAT_CLIENT gt_twitch_chat_client_get_type()

G_DECLARE_FINAL_TYPE(GtTwitchChatClient, gt_twitch_chat_client, GT, TWITCH_CHAT_CLIENT, GObject)

typedef struct
{
    gchar* nick;
    gchar* user;
    gchar* host;
    gchar* command;
    gchar* params;
    gchar** tags;
} GtTwitchChatMessage;

typedef gboolean (*GtTwitchChatSourceFunc) (GtTwitchChatMessage* msg, gpointer udata);

typedef struct _GtTwitchChatSource GtTwitchChatSource;

struct _GtTwitchChatClient
{
    GObject parent_instance;

    GtTwitchChatSource* source;
};

GtTwitchChatClient* gt_twitch_chat_client_new();
void gt_twitch_chat_client_connect(GtTwitchChatClient* self);
void gt_twitch_chat_client_join(GtTwitchChatClient* self, const gchar* channel);
void gt_twitch_chat_client_part(GtTwitchChatClient* self);
GtTwitchChatMessage* gt_twitch_chat_message_new();
void gt_twitch_chat_message_free(GtTwitchChatMessage* msg);

G_END_DECLS

#endif
