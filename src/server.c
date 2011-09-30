#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#if (OS == SUN)
#include <strings.h>
#endif

#include "server.h"
#include "defaults.h"
#include "net.h"
#include "lib.h"
#include "main.h"


static void
connectto (struct _server *server)
{
	struct addrinfo hints, *res = NULL;
	struct in6_addr serveraddr;
	char port[6];
	int rc;

	memset (&hints, 0, sizeof (hints));
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (inet_pton (AF_INET, server->hostname, &serveraddr) == 1) {
		hints.ai_family = AF_INET;
		hints.ai_flags |= AI_NUMERICHOST;
	} else if (inet_pton (AF_INET6, server->hostname, &serveraddr) == 1) {
		hints.ai_family = AF_INET6;
		hints.ai_flags |= AI_NUMERICHOST;
	}

	snprintf (port, 6, "%d", server->port);

	rc = getaddrinfo (server->hostname, port, &hints, &res);
	if (rc != 0) {
		fprintf (stderr, "Host not found: %s\n", gai_strerror (rc));
		exit (EXIT_FAILURE);
	}

	server->sock = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
	if (server->sock < 0)
		eperror ("socket");

	if (connect (server->sock, res->ai_addr, res->ai_addrlen) < 0)
		eperror ("connect");

	if (res != NULL)
		freeaddrinfo (res);

	return;
}



/* Open a connection to an irc server */
void
open_irc_server (struct _server *server)
{
	char sndbuf[MSGBUF];

	connectto (server);

	/* FIXME: let select mask in config for user modes */
	snprintf (sndbuf, MSGBUF, "PASS %s%sNICK %s%sUSER %s bla * :%s%s", 
			 server->password, SEPARATOR, /* PASS */
			 server->nick, SEPARATOR, /* NICK */
			 server->ident, 
			 server->fullname, SEPARATOR); /* USER */

	botsend (server, sndbuf);

	return;

}

#define JUMP_COLON()			\
do {					\
	while (*(data + offset) == ' ')	\
		offset++;		\
	if (*(data + offset) == ':')	\
		offset++;		\
} while (0)


static void
process_named_cmd (struct servermsg *msg, char *data)
{
	int offset = 0;

	/* #channel :text here */
	if (strcasecmp ("PRIVMSG", msg->action) == 0) {
		get_token (data, ' ', msg->channel, &offset, CHAN_MAX_LENGTH);
		JUMP_COLON ();
		/* CTCP detected */
		if (*(data + offset) == 1 && data[strlen (data) - SEPARATOR_LEN - 1] == 1) {
			msg->action_n = CTCP;
			offset++;
			get_token (data, 1, msg->message, &offset, MSGBUF);
		} else {
			msg->action_n = PRIVMSG;
			get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
		}
		return;
	}
	if (strcasecmp ("MODE", msg->action) == 0) {
		get_token (data, ' ', msg->channel, &offset, CHAN_MAX_LENGTH);
		get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
		msg->action_n = MODE;
		return;
	}
	if (strcasecmp ("JOIN", msg->action) == 0) {
		JUMP_COLON ();
		get_token_str (data, SEPARATOR, msg->channel, &offset, CHAN_MAX_LENGTH);
		*msg->message = '\0';
		msg->action_n = JOIN;
		return;
	}
	if (strcasecmp ("NICK", msg->action) == 0) {
		JUMP_COLON ();
		get_token_str (data, SEPARATOR, msg->channel, &offset, NICK_MAX_LENGTH);
		*msg->message = '\0';
		msg->action_n = NICK;
		return;
	}
	if (strcasecmp ("QUIT", msg->action) == 0) {
		*msg->channel = '\0';
		JUMP_COLON ();
		get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
		msg->action_n = QUIT;
		return;
	}
	if (strcasecmp ("TOPIC", msg->action) == 0) {
		get_token (data, ' ', msg->channel, &offset, CHAN_MAX_LENGTH);
		JUMP_COLON ();
		get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
		msg->action_n = TOPIC;
		return;
	}
	if (strcasecmp ("KICK", msg->action) == 0) {
		get_token (data, ' ', msg->channel, &offset, CHAN_MAX_LENGTH);
		get_token (data, ' ', msg->target, &offset, NICK_MAX_LENGTH);
		JUMP_COLON ();
		get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
		msg->action_n = KICK;
		return;
	}
	if (strcasecmp ("NOTICE", msg->action) == 0) {
		/* Note: rfc defines "target", but I think msg->channel is ok */
		get_token (data, ' ', msg->channel, &offset, CHAN_MAX_LENGTH);
		JUMP_COLON ();
		get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
		msg->action_n = NOTICE;
		return;
	}
	if (strcasecmp ("PART", msg->action) == 0) {
		/* Part reason is optional */
		if (strchr (data, ' ') == NULL) {
			get_token_str (data, SEPARATOR, msg->channel, &offset,
				       CHAN_MAX_LENGTH);
			*msg->message = '\0';
		} else {
			get_token (data, ' ', msg->channel, &offset, CHAN_MAX_LENGTH);
			JUMP_COLON ();
			get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
		}
		msg->action_n = PART;
		return;
	}
	/* FIXME: not yet done */
	if (strcasecmp ("INVITE", msg->action) == 0) {
		*msg->channel = '\0';
		*msg->message = '\0';
		msg->action_n = INVITE;
		return;
	}
	if (strcasecmp ("KILL", msg->action) == 0) {
		*msg->channel = '\0';
		*msg->message = '\0';
		msg->action_n = KILL;
		return;
	}
	if (strcasecmp ("WALLOPS", msg->action) == 0) {
		*msg->channel = '\0';
		*msg->message = '\0';
		msg->action_n = WALLOPS;
		return;
	}

	msg->action_n = OTHER;

	*msg->message = *msg->channel = '\0';

}


static void
process_numeric_cmd (struct servermsg *msg, char *data)
{
	char *data_ptr;
	int offset;

	if (sscanf (msg->action, "%ui", &msg->action_n) <= 0) {
		msg->action_n = ERROR;
		return;
	}

	if ((data_ptr = strchr (data, ' ')) == NULL)
		return;

	offset = data_ptr - data;

/* FIXME: look the rfc, more numerics will need some parse here */
	switch (msg->action_n) {
	case RPL_NAMREPLY: /* Sample: 353 mynick * #chan :mynick @opped_nick +voiced_nick */
		/* @ = secret, * = private, = = other */
		/* I will use ->target to store this: */
		get_token (data, ' ', msg->target, &offset, CHAN_MAX_LENGTH);
		get_token (data, ' ', msg->channel, &offset, CHAN_MAX_LENGTH);
		offset++;
		/* ->message will store the nick list, will parse later */
		get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
		break;
	default:
		/* *msg-> target = *msg->channel = *msg->message = '\0'; */
		get_token (data, ' ', msg->channel, &offset, CHAN_MAX_LENGTH);
		offset++;
		get_token_str (data, SEPARATOR, msg->message, &offset, MSGBUF);
	}


	return;
}


/* 
 * MODE #chan ...
 * +ws nick -xjk *!*@* -s +l 10
 */
static void
get_multi__mode_chan (char *mode, unsigned int modes_left)
{
	/* +oo-ov yo tu el tururu */
	char *ptr = mode;
	unsigned short int n = 0;
	short int len = 0;
	char sign = '\0', flag = '\0';

	while (*ptr != '\0') {
		if (*ptr == '+' || *ptr == '-')
			sign = *ptr;
		else {
			/* This modes don't have parameters */
			if (*ptr == 'a' || *ptr == 'i' || *ptr == 'm' ||
			    *ptr == 'n' || *ptr == 'q' || *ptr == 'p' ||
			    *ptr == 'r' || *ptr == 's' || *ptr == 't') {
				n++;
				if (n == modes_left) {
				    	/* flag = *ptr; */
					*mode = sign;
					*(mode + 1) = *ptr;
					*(mode + 2) = '\0';
					return;
				}
			/* This modes do have parameters */
			} else if (*ptr == 'O' || *ptr == 'o' || *ptr == 'v' 
				    || *ptr == 'l' || *ptr == 'k' || *ptr == 'b' 
				    || *ptr == 'e' || *ptr == 'i') {
				n++;
				if (n == modes_left)
					flag = *ptr;
				ptr++;
				/* The actual parameter */
				while (*ptr == ' ')
					ptr++;
				len = strlen2sep (ptr, ' ');
				if (len == -1) {
					len = strlen2sep (ptr, '\0');
					/* Won't happen */
					if (len == -1)
						return;
				}
				break;
			} /* else
				the mode is ignored */
			if (n > modes_left)
				return;
		}
		ptr++;
	}
	if (ptr - mode > 2) {
		*mode = sign;
		*(mode + 1) = flag;
		/* ' ' + len + \0 = len + 2 */
		snprintf (mode + 2, len + 2, " %s", ptr);
	}
}


#ifdef DEAD_CODE
/* 
 * MODE nick ...
 * +iwo-Or+j
 */
/* I think servers don't send more than one user mode change for every message,
 * anyway, this code is UNTESTED 
 */
static void
get_multi__mode_nick (char *mode, unsigned int modes_left)
{
	/* +oo-ov yo tu el tururu */
	char *ptr = mode;
	unsigned short int n = 0;
	char sign, flag;

	while (*ptr != '\0') {
		if (*ptr == '+' || *ptr == '-')
			sign = *ptr;
		else {
			if (++n == modes_left) {
				flag = *ptr;
				break;
			} else if (n > modes_left)
				return;
		}
		ptr++;
	}

	/* If the difference is == 2, the copy is for nothing,
	 * if the difference is <2 there is no room..for ex. a borken message 
	 */
	if (ptr - mode > 2) {
		*mode = sign;
		*(mode + 1) = flag;
		*(mode + 2) = '\0';
	}
}


/* 
#foo,#bar one,two <- joins #foo with pass one and #bar with pass two
*/
/* Same for this function: servers send separate messages for every channel */
static void
get_multi__join (char *mode, unsigned int modes_left)
{
	char *ptr = mode;
	char dest[MSGBUF], *destptr = dest;
	int modes = 1, i = 1;

	while (*ptr != ' ' && *ptr != '\0')
		if (*ptr == ',')
			modes++;

	/* Normal */
	if (modes == 1)
		return;

	/* Ok! is a join for more than one channel,
	 * I think this will be almost-dead code but let's do it right 
	 */
	ptr = mode;
	while (1) {
		if (i == modes_left) {
			while (*ptr != ',' && *ptr != ' ' && *ptr != '\0')
				*destptr++ = *ptr++;
			break;
		} else
			while (*ptr != ',')
				ptr++;
		ptr++;
		i++;
	}

	while (*ptr == ' ')
		ptr++;

	/* Just a safety check, for evil servers */
	if (*ptr == '\0') {
		*destptr = '\0';
		return;
	}

	/* Start over again, but now is different, 
	 * one channel could have a key, but not the other 
	 */
	i = 1;
	while (1) {
		if (i == modes_left) {
			*destptr++ = ' ';
			while (*ptr != ',' && *ptr != ' ' && *ptr != '\0')
				*destptr++ = *ptr++;
			break;
		} else
			while (*ptr != ',' && *ptr != '\0')
				ptr++;
			/* Note: it was indentated like this, the indentation
			 * is wrong __or__ else needs {} around. */
			if (*ptr == '\0') { /* No key for this channel */
				*destptr = '\0';
				return;
			}
			ptr++;
			i++;
	}

	*destptr = '\0';

	return;
}


/*
 * Sample:
 * KICK		#chan		nick
 * KICK		#chan1[,#chan2]	nick1[,nick2[,nick3]] [:comment]
 */
static void
get_multi__kick (char *mode, unsigned int modes_left)
{
	short int nchans = 1, nnicks, has_reason;
	unsigned short int i;
	char *ptr;
	char nick[NICK_MAX_LENGTH], chan[CHAN_MAX_LENGTH], reason[MSGBUF];

	ptr = strchr (mode, ' ');
	if (ptr == NULL)
		return;
	while (*ptr == ' ')
		ptr++;

	while (*++ptr != ' ')
		if (*ptr == ',')
			nchans++;
		else if (*ptr == '\0')
			return;
	while (*ptr == ' ')
		ptr++;

	while (*++ptr != ' ')
		if (*ptr == ',')
			nnicks++;
		else if (*ptr == '\0')
			return;

	/* Shouldn't be necessary, but could have a bad counted modes_left */
	if (modes_left > nchans * nnicks)
		return;

	while (*ptr == ' ')
		ptr++;

	if (*ptr == ':')
		has_reason = TRUE;
	else
		has_reason = FALSE;

	/* Nicks and channels are counted, 
	 * now it's only take mode: modes_left 
	 */

	ptr = strchr (mode, ' ');
	while (*ptr == ' ')
		ptr++;

	/* get to know what channel get: (every nnicks nicks, channel++) */
	i = (int) ((modes_left / nnicks) - 0.001);
	while (i > 0) {
		ptr++;
		if (*ptr == ',') {
			i--;
			ptr++;
		}
	}

	while (*(ptr + i) != ' ' && *(ptr + i) != ',')
		i++;
	strncpy (nick, ptr, min (i, NICK_MAX_LENGTH));

	ptr += i;
	while (*ptr != ' ')
		ptr++;
	while (*ptr == ' ')
		ptr++;

	/* Now the same for the nick */
	i = (int) ((modes_left / nchans) - 0.001);
	while (i > 0) {
		ptr++;
		if (*ptr == ',') {
			i--;
			ptr++;
		}
	}

	while (*(ptr + i) != ' ' && *(ptr + i) != ',')
		i++;
	strncpy (chan, ptr, min (i, CHAN_MAX_LENGTH));

	if (has_reason) {
		ptr += i;
		while (*ptr != ':')
			ptr++;
		ptr++;
		snprintf (mode, MSGBUF, "KICK %s %s :%s", chan, nick, reason);
	} else
		snprintf (mode, MSGBUF, "KICK %s %s", chan, nick);

}
#endif


/* Fill the servermsg structure, in data there is a line from server */
/* nick, ident, ip, action, channel, message */
void
parse_server (char *data, struct servermsg *msg, const unsigned int modes_left)
{
	int offset = 0;

	if (data == NULL)
		return;

	/* if the message we get, contains '@'... 
	 * (used to distinguish between server and user command) 
	 */
	if (memchr ((void *) data, (int) '@', (size_t) (strchr (data, ' ') - data)) != NULL) {
		get_token (data, '!', msg->nick, &offset, NICK_MAX_LENGTH);
		get_token (data, '@', msg->ident, &offset, 9);
	} else
		*msg->nick = *msg->ident = '\0';

	get_token (data, ' ', msg->host, &offset, 128);

	get_token (data, ' ', msg->action, &offset, 20);

	msg->target[0] = '\0';

	/* FIXME/NOTE: msg->action_n could be pre-cached in check for mode maybe,
	 * then this two functions could be merged:
	 * advantages: simplicity here, fewer comparisons, somewhat faster 
	 * disadvantages: hack on top of hack */
	if (isdigit (*msg->action))
		process_numeric_cmd (msg, data + offset);
	else
		process_named_cmd (msg, data + offset);

	msg->last_matched = NULL;

	/* FIXME: add other potential messages */
	/* Note: these functions have _also_ to be called when modes_left == 1,
	 * otherwise multi-modes will be wrong when modes_left == 1 (the last mode)
	 * maybe FIXME: add comunication with net.c::count_multi() and if 
	 * modes_left == 1 return before 
	 */
	/* Note2: modes will work like a heap, last comed, first served */
	if (msg->action_n == MODE) {
		if (is_channel (*msg->channel)) /* Is an user mode message */
			get_multi__mode_chan (msg->message, modes_left);
#ifndef DEAD_CODE
	}
#else
		else
			get_multi__mode_nick (msg->message, modes_left);
	} else if (msg->action_n == JOIN)
		get_multi__join (msg->message, modes_left);
	else if (msg->action_n == KICK)
		get_multi__kick (msg->message, modes_left);
#endif

	return;
}



/* At this point we have: PING, NOTICE, ERROR,...
 * This one parse this kind of traffic 
 */
void
parse_server2 (const char *data, struct servermsg *msg)
{
	size_t size;

	size = (size_t) (strchr (data, ' ') - data);

	strncpy (msg->action, data, size);

	msg->action[size] = '\0';

	data += size + 1;
	while (*data == ' ')
		data++;

	*msg->nick = *msg->ident = *msg->host = *msg->channel = *msg->message = '\0';

	if (strncasecmp ("PING", msg->action, 4) == 0) {
		int len;

		data++;
		len = strlen2sepstr (data, SEPARATOR);
		strncpy (msg->message, data, len);
		*(msg->message + len) = '\0';
		msg->action_n = PING;
	/* FIXME: notice and error are currently UNIMPLEMENTED */
	} else if (strncasecmp ("NOTICE", msg->action, 6) == 0) {
		strncpy (msg->message, data, MSGBUF);
		msg->action_n = SNOTICE;
	} else if (strncasecmp ("ERROR", msg->action, 5) == 0) {
		strncpy (msg->message, data, MSGBUF);
		msg->action_n = ERROR;
	} else
		msg->action_n = OTHER;

	msg->last_matched = NULL;

	return;
}


