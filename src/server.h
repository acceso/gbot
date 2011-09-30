#ifndef SERVER_H
#define SERVER_H

#include <time.h>

#include "defaults.h"

/* numeric replies from the server */
#define RPL_NAMREPLY 353

struct _server {
	char hostname[128 + 1];		/* Internet name or ip */
	char servername[128 + 1];	/* IRC Server name */
	int port;
	int sock;
	char password[85 + 1];
	char nick[NICK_MAX_LENGTH + 1];
	char ident[9 + 1];
	char vhost[128 + 1];		/* FIXME: fill this in anyhow */
	char fullname[128 + 1];
	char *nick_modes;		/* "iw"... */
	time_t lastmsg_time;		/* When last message was sent */
	/*struct _server *next; */
};


struct servermsg {
	struct _server *server;
	char nick[NICK_MAX_LENGTH + 1];
	char ident[9 + 1];
	char host[128 + 1]; 		/* or vhost */
	char action[20 + 1];
	unsigned int action_n;
	char channel[CHAN_MAX_LENGTH];
	char target[CHAN_MAX_LENGTH];
	char message[MSGBUF];
	struct _event *last_matched;	/* If a msg from the server has matched
					 * a event this line is a pointer to it,
					 * otherwise is NULL */
};


#ifdef DEBUG
#define DUMP_MSG(_smsg)					\
printf ("------------------------------------->\n"	\
	"mask: %s!%s@%s\n"				\
	"action: %s\n"					\
	"channel: %s\n"					\
	"target: %s\n"					\
	"message: %s\n",				\
	_smsg->nick, _smsg->ident, _smsg->host,		\
	_smsg->action, _smsg->channel,			\
	 _smsg->target,	_smsg->message);
#endif /* DEBUG */

enum {
	NIL,
	PRIVMSG,
	MODE,
	JOIN,
	NICK,
	QUIT,
	TOPIC,
	KICK,
	NOTICE,
	PART,
	INVITE,
	KILL,
	WALLOPS,
	PING = 720,
	PONG,
	SNOTICE,
	ERROR,
	OTHER = 730,
	CTCP = 800
};


void open_irc_server (struct _server *server);
void parse_server (char *data, struct servermsg *msg, unsigned int modes_left);
void parse_server2 (const char *data, struct servermsg *msg);


#endif /* SERVER_H */

