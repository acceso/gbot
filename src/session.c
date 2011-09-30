
#include <string.h>


#include "lib.h"
#include "session.h"
#include "server.h"



static struct _session *main_session;




/* Adds a session, for example when WE join a channel */
static void
session_new (struct servermsg *msg, struct _session *session)
{
	session->server = msg->server;

	strncpy (session->name, msg->channel, CHAN_MAX_LENGTH);


}



/* Removes a session, for example when WE leave a channel */
static void
session_rm (struct servermsg *msg, struct _session *session)
{


}




/* Adds a nick to a session */
static void
session_in (struct servermsg *msg, struct _session *session)
{


}



/* Removes a nick from a session */
static void
session_out (struct servermsg *msg, struct _session *session)
{


}



/* Updates topic in a channel */
static void
session_upd_topic (struct servermsg *msg, struct _session *session)
{

}



/* Updates info about a nick in a session */
static void
session_upd (struct servermsg *msg, struct _session *session)
{


}



/*
struct _server *server;
char nick[NICK_MAX_LENGTH];
char ident[9 + 1];
char host[128 + 1];
char action[20 + 1];
unsigned int action_n;
char channel[CHAN_MAX_LENGTH];
char target[CHAN_MAX_LENGTH];
char message[MSGBUF];
struct _event *last_matched;
*/


#define msg_is_mine() (strncmp (msg->nick, msg->server->nick, NICK_MAX_LENGTH) == 0)


void
session_chk (struct servermsg *msg)
{
	struct _session *this_session = main_session;

	switch (msg->action_n) {
	case MODE:
		session_upd (msg, this_session);
		return;
	case JOIN:
		if (!msg_is_mine ())
			session_in (msg, this_session);
		return;
	case NICK:
		session_upd (msg, this_session);
		return;
	case QUIT:
		/* Own quit messages won't reach this code */
		session_out (msg, this_session);
		return;
	case TOPIC:
		session_upd_topic (msg, this_session);
		return;
	case KICK:
		session_out (msg, this_session);
		return;
	case PART:
		if (msg_is_mine ())
			session_rm (msg, this_session);
		else
			session_out (msg, this_session);
		return;
	case KILL:
		session_out (msg, this_session);
		return;
	case RPL_NAMREPLY:
		/* Just happens with own messages */
		if (this_session->next != NULL)
			while (this_session->next != NULL)
				this_session = this_session->next;
		this_session->next = xmalloc (sizeof (struct _session));
		session_new (msg, this_session->next);
		return;
	}

	return;

}


void
session_init (struct _server *server)
{
	struct _session *this_session = main_session;

	if (this_session != NULL)
		while (this_session->next != NULL)
			this_session = this_session->next;
	
	this_session = xmalloc (sizeof (struct _session));

	this_session->server = server;
	strcpy (this_session->name, "MAIN");

	return;
}


