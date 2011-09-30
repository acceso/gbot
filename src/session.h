#ifndef SESSION_H 
#define SESSION_H


#include <time.h>


#include "defaults.h"
#include "server.h"


struct _user {
	char nick[NICK_MAX_LENGTH];
	char voiced;
	char oped;
	/* BIG FIXME: this is temporal, I won't use a linked list for nicks in
	 * a channel! */
	struct _user *next;
};


struct _session {
	struct _server *server;
	char name[CHAN_MAX_LENGTH];
	char topic[TOPIC_MAX_LENGTH];
	char chan_key[64];

	time_t lastmsg_time;

	struct _user *user;

	struct _session *next; /* FIXME my master: should find sessions quickly */

	struct _session *next_server;
};




void session_chk (struct servermsg *msg);
void session_init (struct _server *server);



#endif /* SESSION_H */


