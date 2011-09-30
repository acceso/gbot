#ifndef EVENTS_H
#define EVENTS_H

#include <regex.h>
#include <time.h>


#include "defaults.h"
#include "server.h"


#define ADD_SEP '0'
#define NOT_SEP '1'


struct _event {
	char event[8];			/* Event */
	char channel[CHAN_MAX_LENGTH];	/* Channel or nick */
	char mask[NICK_MAX_LENGTH + 9 + 128 + 3];
					/* nick + ident + host + (! + @ + \0) */
	char text[REGEX_LENGTH];	/* Text for the regex */
	regex_t regex;			/* Compiled regular expresion from "text" */
	char cond[EXTRA_LENGTH];	/* Extra condition or extra commands */
	char options[10];		/* it could be:
					 * c -> continue matching
					 * i -> ignore
					 * n -> stop matching 
					 * t -> event should be exec'ed on another thread
					 */
	char command[EV_BUF];		/* What to do on match */
	unsigned int target_ncmd;	/* 1st word in command is restricted to a set */
	struct _event *next;
};


struct _locals {
	char var[VAR_LVALUE];
	char *val;
	int ival;
	time_t time;
/* time_t timeout; ? */
	struct _locals *next;
};

enum {
	/* ev_num[0-9] are used to store numeric events,
	 * must be at the beginning, values [0-9]. */
	ev_num0 = 0,
	ev_num1,
	ev_num2,
	ev_num3,
	ev_num4,
	ev_num5,
	ev_num6,
	ev_num7,
	ev_num8,
	ev_num9,
	ev_any,
	ev_privmsg,
	ev_ping,
	ev_mode,
	ev_join,
	ev_nick,
	ev_quit,
	ev_topic,
	ev_kick,
	ev_notice,
	ev_part,
	ev_invite,
	ev_send,
	ev_ctcp,
	ev_LAST
};


enum {
/* First internal commands */
	cmd_first_internal = 0,
	cmd_null,
	cmd_log,
	cmd_reload,
	cmd_last_internal,
/* direct irc commands */
	cmd_first_irc,
	cmd_pong,
	cmd_mode,
	cmd_privmsg,
	cmd_topic,
	cmd_kick,
	cmd_notice,
	cmd_last_irc,
/* FIXME: look up the rfc and add other unimplemented commands */
	cmd_other,
	cmd_last
};

void look_up_event (struct servermsg *msg);
void expand_str (char *sendbuf, const struct servermsg *msg,
		 const char *ptr, size_t len, char tosend);
void do_sig_reload (int signal);


#endif /* EVENTS */

