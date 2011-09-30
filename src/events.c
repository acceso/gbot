#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fnmatch.h>
#include <regex.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "events.h"
#include "server.h"
#include "lib.h"
#include "defaults.h"
#include "logging.h"
#include "net.h"
#include "main.h"
#include "readcfg.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif /* HAVE_PTHREAD */

struct _event *events[ev_LAST];


#define raise_event_servermsg(__msg)		\
	look_up_event (__msg)


/* When an event is matched, raise_event() is called with
 * the event, this is because I want to treat bot actions
 * like events comming from the outside. The main use of this
 * is do_log () of messages from the bot. 
 */
static void
raise_event (struct _server *serv, const unsigned int action_n,
	     const char *channel, const char *message)
{
	struct servermsg msg;

	msg.server = serv;
	strncpy (msg.nick, serv->nick, NICK_MAX_LENGTH);
	*msg.ident = '\0';
	*msg.host = '\0';
	snprintf (msg.action, 20 + 1, "%d", action_n);
	msg.action_n = action_n;
	strncpy (msg.channel, channel, CHAN_MAX_LENGTH);
	*msg.target = '\0';
	strncpy (msg.message, message, MSGBUF);
	msg.last_matched = NULL;

	look_up_event (&msg);
}



static void
free_list (struct _event *ev)
{
	if (ev->next != NULL)
		free_list (ev->next);
	else {
#ifndef USE_FNMATCH
		regfree (&ev->regex);
#endif
		free (ev);
	}
}


#define reload_events() 			\
do { 						\
	int i = 0;				\
						\
	for ( ; i < ev_LAST; i++) {		\
		if (events[i] != NULL)		\
			free_list (events[i]);	\
		events[i] = NULL;		\
	}					\
						\
	read_events (NULL);			\
} while (0)

#define RELOAD_MSG "Reloading..."

static int
do_reload (const struct servermsg *msg)
{
	char sendbuf[EV_BUF];

	snprintf (sendbuf, EV_BUF, "NOTICE %s :" RELOAD_MSG SEPARATOR, msg->nick);

	reload_events ();

	botsend (msg->server, sendbuf);

	raise_event (msg->server, OTHER, msg->channel, RELOAD_MSG SEPARATOR);

	return 0;

}


void
do_sig_reload (int signal)
{
	reload_events ();
}


static struct _locals *locals;


static void
var_set (char *str)
{
	char lvalue[VAR_LVALUE];
	char operation = '\0';
	int rvalue_i, lvaluelen;
	struct _locals *thisvar = locals;

	while (*str == ' ')
		str++;
	/* a '$' in front of the variable name is not necesary but accepted */
	if (*str == '$')
		str++;

	if ((lvaluelen = strlen2sep (str, ' ')) == -1
	    && (lvaluelen = strlen2sep (str, '=')) == -1
	    && (lvaluelen = strlen2sep (str, '+')) == -1
	    && (lvaluelen = strlen2sep (str, '+')) == -1)
		return;

	strncpy (lvalue,
		 str, (size_t) ((lvaluelen < VAR_LVALUE) ? lvaluelen : VAR_LVALUE - 1));

	*(lvalue + lvaluelen) = '\0';

	str += lvaluelen;
	while (*str == ' ')
		str++;

	if (thisvar == NULL) {
		thisvar = (struct _locals *) xmalloc (sizeof (struct _locals));
		locals = thisvar;
		strcpy (thisvar->var, lvalue);
	} else if (strcmp (thisvar->var, lvalue) != 0) {
		/* In this case the variable is not the first one: */
		char found = FALSE;

		while (thisvar->next != NULL) {
			if (strcmp (thisvar->next->var, lvalue) == 0) {
				found = TRUE;
				break;
			}
			thisvar = thisvar->next;
		}

		if (found == FALSE) {
			thisvar->next = (struct _locals *) xmalloc (sizeof (struct _locals));
			thisvar = thisvar->next;
			strcpy (thisvar->var, lvalue);
		}
	}

	if (*str == '=')
		operation = '=';
	else if (*str == '+') {
		if (*(str + 1) == '+')
			operation = '+'; /* + = +1 increment */
		else if (*(str + 1) == '=')
			operation = 'i'; /* i = increment */
		str++;
	} else if (*str == '-') {
		if (*(str + 1) == '-')
			operation = '-'; /* - = -1 increment */
		else if (*(str + 1) == '=')
			operation = 'd'; /* d = decrement */
		str++;
	} else
		return;

	str++;
	while (*str == ' ')
		str++;

	/* If trying to increment-decrement a new variable ... */
	if (thisvar->time == 0 && operation != '=') {
		free (thisvar);
		return;
	}

	if (isdigit (*str))
		sscanf (str, "%i", &rvalue_i);
	/* Note: every printable 7bit char is accepted, 
	 * It can't be nor a space (see above) nor a digit 
	 */
	else if (isascii (*str) && isprint (*str)) {
		/* Just let assignation operation on strings */
		if (operation != '=')
			return;
		thisvar->val = (char *) xmalloc (VAR_RVALUE);
		strncpy (thisvar->val, str, VAR_RVALUE);

		thisvar->time = time (NULL);
		return;
	}

	if (operation == '=')
		thisvar->ival = rvalue_i;
	else if (operation == '+')
		thisvar->ival++;
	else if (operation == '-')
		thisvar->ival--;
	else if (operation == 'd')
		thisvar->ival -= rvalue_i;
	else if (operation == 'i')
		thisvar->ival += rvalue_i;

	thisvar->time = time (NULL);

	return;
}


static void
var_del (char *str)
{
	struct _locals *thisvar = locals, *prev;

	while (*str == ' ')
		str++;
	/* a '$' in front of the variable name is not necesary but accepted */
	if (*str == '$')
		str++;

	prev = thisvar;
	while (thisvar != NULL) {
		if (strcmp (thisvar->var, str) == 0) {
			if (prev == thisvar)
				locals = thisvar->next;
			else
				prev->next = thisvar->next;
			if (thisvar->val != NULL)
				free (thisvar->val);
			free (thisvar);
			return;
		}
		prev = thisvar;
		thisvar = thisvar->next;
	}
}


static void
var_print (char *str, char *rvalue)
{
	struct _locals *thisvar = locals;

	while (*str == ' ')
		str++;

	/* a '$' in front of the variable name is not necesary but accepted */
	if (*str == '$')
		str++;

	*rvalue = '\0';

	while (thisvar != NULL) {
		if (strcmp (thisvar->var, str) == 0) {
			if (thisvar->val == NULL)
				snprintf (rvalue, EV_BUF - 1, "%i", thisvar->ival);
			else
				strcpy (rvalue, str);
			return;
		}
		thisvar = thisvar->next;
	}

}


static int
var_cond (char *str)
{
	char lvalue[VAR_LVALUE];
	char operation = '\0';
	int lvaluelen;

	struct _locals *thisvar = locals;

	/* First goto in my history: it makes easy to change the
	 * default policy when not found 
	 * FIXME: se puede quitar y devolver FALSE ya que sólo hago eso. 
	 * Claro que es mi primer goto.. snif :)
	 */
	if (thisvar == NULL && !strchr (str, '%'))
		return FALSE;

	while (*str == ' ')
		str++;

	/* a '$' in front of the variable name is not necesary but accepted */
	if (*str == '$')
		str++;

	lvaluelen = strlen2sep (str, ' ');
	if (lvaluelen == -1) {
		lvaluelen = strlen2sep (str, '=');
		if (lvaluelen == -1)
			return FALSE;
	}

	strncpy (lvalue, str,
		 (size_t) ((lvaluelen < VAR_LVALUE) ? lvaluelen : VAR_LVALUE - 1));

	*(lvalue + lvaluelen) = '\0';

	str += lvaluelen;
	while (*str == ' ')
		str++;

	if (*str != '%') {
		while (thisvar != NULL) {
			if (strcmp (thisvar->var, lvalue) == 0)
				break;
			thisvar = thisvar->next;
		}

		if (thisvar == NULL)
			return FALSE;
	}

	/* str++ is needed on 2 chars operators: '<=' and '>=' */
	if (*str == '=') {
		operation = 'c'; /* c from comparation */
		while (*str == '=')
			str++;
		str--;
	} else if (*str == '<') {
		if (*(str + 1) == '=') {
			operation = 'm'; /* m = Minor or equal */
			str++;
		} else
			operation = 'M'; /* Minor */
	} else if (*str == '>') {
		if (*(str + 1) == '=') {
			operation = 'g'; /* g = Greater or equal */
			str++;
		} else
			operation = 'G'; /* greater */
	} else if (*str == '%')
		operation = 'p'; /* p = pert cent */
	else
		return FALSE;

	str++;
	while (*str == ' ')
		str++;

	if (thisvar != NULL)
		thisvar->time = time (NULL);

	if (thisvar == NULL || thisvar->val == NULL) {
		int rvalue_i = 0;

		sscanf (str, "%i", &rvalue_i);
		if (operation == 'c')
			return (thisvar->ival == rvalue_i) ? TRUE : FALSE;
		else if (operation == 'm')
			return (thisvar->ival <= rvalue_i) ? TRUE : FALSE;
		else if (operation == 'M')
			return (thisvar->ival < rvalue_i) ? TRUE : FALSE;
		else if (operation == 'g')
			return (thisvar->ival >= rvalue_i) ? TRUE : FALSE;
		else if (operation == 'G')
			return (thisvar->ival > rvalue_i) ? TRUE : FALSE;
		else if (operation == 'p')
			return !(rand () % (100 / rvalue_i));
	} else {
		int cmpval;

		cmpval = strcmp (thisvar->val, str);
		if (operation == 'c')
			return (cmpval == 0) ? TRUE : FALSE;
		else if ((operation == 'm' && cmpval <= 0) ||
			 (operation == 'M' && cmpval < 0) ||
			 (operation == 'g' && cmpval >= 0) ||
			 (operation == 'G' && cmpval > 0))
			return TRUE;
		else
			return FALSE;
	}

	/* If condition is not met, default to false */
	return FALSE;

}



static void
do_system (const char *c, char *dest, int len, int strip)
{
	pid_t pid;
	int fds[2];
	int status;

	if (pipe (fds) < 0)
		return;

	if ((pid = fork ()) < 0)
		return;

	if (pid == 0) { /* Child */
		/* Just writes */
		close (fds[0]);

		/* A command line option could be added to
		 * catch stderr: */
		/* dup2 (fds[1], STDERR_FILENO); */
		dup2 (fds[1], STDOUT_FILENO);
		close (fds[1]);

		system (c);
		_exit (0);
	} else {
		char *ptr = dest;
		char *tmp = (char *) xmalloc (len + 1), *tptr = tmp;
		int count = 0;

		/* Just reads. */
		close (fds[1]);

		while (read (fds[0], tptr++, 1) > 0) {
			if (++count >= len)
				break;
		}

		*tptr = '\0';

		if (strip != 0) {
			tptr = tmp;

			do {
				if (*tptr == '\n' || *tptr == '\r') {
					tptr++;
					continue;
				}

				*ptr++ = *tptr++;
			} while (*tptr != '\0');

			*ptr = '\0';
		} else {
			/* The string has to be copied, the \r is removed
			 * because its a mess, it won't probably there anyway.. */
			tptr = tmp;

			do {
				if (*tptr == '\r') {
					tptr++;
					continue;
				}

				*ptr++ = *tptr++;
			} while (*tptr != '\0');

			*ptr = '\0';
		}

		free (tmp);

		/* A sigchild handler will be better in practice... */
		wait (&status);
	}
}


static void
get_n_word (const char *text, char *rvalue, int nword)
{
	int i;
	const char *ptr = text;

	for (i = 0; i < nword; i++, ptr++)
		while (*ptr != ' ')
			ptr++;

	if (rvalue == NULL)
		return;

	if ((i = strlen2sep (ptr, ' ')) < 0)
		i = strlen2sep (ptr, '\0');

	strncpy (rvalue, ptr, (size_t) max (i, 0));

	*(rvalue + i) = '\0';

	return;

}

#define N_WORD(_num)							\
do {									\
	const char *ptr = msg->message;					\
	int i;								\
									\
	if (*(lvalue + 1) == '*') {					\
		for (i = 1; i < _num ; i++, ptr++)			\
			while (*ptr != ' ' && *ptr != '\0')		\
				ptr++;					\
		strcpy (rvalue, ptr);					\
	} else 								\
		get_n_word (msg->message, rvalue, _num - 1);		\
} while (0)


static int
get_void_rvalue (char *lvalue)
{
	if (strncasecmp (lvalue, "set ", 4) == 0)
		var_set (lvalue + 4);
	else if (strncasecmp (lvalue, "del ", 4) == 0)
		var_del (lvalue + 4);
	else if (strncasecmp (lvalue, "cond ", 5) == 0)
		return var_cond (lvalue + 5);

	return TRUE;
}


/* Copies rvaluelen bytes on rvalue given a lvalue, msg is needed for some info. */
/* TODO: A table could be used to speed this up, I mean, something like:
 * cached_table [*lvalue - 'A'] = function pointer with the function for commands
 * starting with *lvalue (I think you get the idea.... this one could be inlined
 * and the correct function whould get called with a minimum amount of casecmps).
 * Oh, this function whould have to be splitted into pre-expanded and not.
 * (I should also improve or get ride of this comment :)
 */
static unsigned short int
getrvalue (char *rvalue, char *lvalue, size_t rvaluelen, const struct servermsg *msg)
{
	*rvalue = '\0';

	if (strcasecmp (lvalue, "pass") == 0) {
		strncpy (rvalue, botdata->pass, rvaluelen);
		return TRUE;
	} else if (strcasecmp (lvalue, "op_pass") == 0) {
		strncpy (rvalue, botdata->op_pass, rvaluelen);
		return TRUE;
	} else if (strcasecmp (lvalue, "date") == 0) {
		time_str (rvalue, STR_DATE);
		return TRUE;
	} else if (strcasecmp (lvalue, "time") == 0) {
		time_str (rvalue, STR_TIME_12);
		return TRUE;
	} else if (strcasecmp (lvalue, "sep") == 0) {
		strncpy (rvalue, SEPARATOR, rvaluelen);
		return TRUE;
	} else if (strncasecmp (lvalue, "print", 5) == 0) {
		var_print (lvalue + 5, rvalue);
		return TRUE;
	}

	get_void_rvalue (lvalue);

	/* Some events have variables pre-expanded (to speed up),
	 * this cases don't have a "msg struct", so we've ended up here.
	 */
	if (msg == NULL)
		return FALSE;

	if (strcasecmp (lvalue, "chan") == 0) {
		strncpy (rvalue, msg->channel, rvaluelen);
		return TRUE;
	} else if ((strcasecmp (lvalue, "me") == 0)
		   || (strcasecmp (lvalue, "botnick") == 0)) {
		strncpy (rvalue, msg->server->nick, rvaluelen);
		return TRUE;
	} else if (strcasecmp (lvalue, "nick") == 0) {
		strncpy (rvalue, msg->nick, rvaluelen);
		return TRUE;
	} else if (strcasecmp (lvalue, "query") == 0) {
		if (strcasecmp (msg->channel, msg->server->nick) == 0)
			strncpy (rvalue, msg->nick, rvaluelen);
		else
			strncpy (rvalue, msg->channel, rvaluelen);

		return TRUE;
	} else if (strcasecmp (lvalue, "ident") == 0) {
		strncpy (rvalue, msg->ident, rvaluelen);
		return TRUE;
	} else if (strcasecmp (lvalue, "ip") == 0) {
		strncpy (rvalue, msg->host, rvaluelen);
		return TRUE;
	} else if (strcasecmp (lvalue, "action") == 0) {
		strncpy (rvalue, msg->action, rvaluelen);
		return TRUE;
	} else if (strncasecmp (lvalue, "system ", 7) == 0) {
		char s[255];
		int offset = 6;

		while (isblank (*(lvalue + offset)))
			offset++;

		strcpy (s, lvalue + offset);

		do_system (s, rvalue, rvaluelen, TRUE);

		return TRUE;
	} else if (strncasecmp (lvalue, "lsystem ", 8) == 0) {
		/* I don't know if this is a hack, the lsystem function fully 
		 * expands itself to privsmg #channel :etc.etc. */
		char s[255];
		char prepend[100];
		int offset = 7;

		while (isblank (*(lvalue + offset)))
			offset++;

		strcpy (s, lvalue + offset);

		do_system (s, rvalue, rvaluelen, FALSE);

		sprintf (prepend, "PRIVMSG ");
		if (strcasecmp (msg->channel, msg->server->nick) == 0)
			strcat (prepend, msg->nick);
		else
			strcat (prepend, msg->channel);

		strcat (prepend, " :");

		delim_every_line (rvalue, prepend, "\r\n", '\n', rvaluelen);

		return TRUE;
	} else if (strcasecmp (lvalue, "*") == 0) {
		strncpy (rvalue, msg->message, rvaluelen);

		return TRUE;
	} else if (*lvalue == '0') {
		get_n_word (msg->action, rvalue, 0);
		return TRUE;
	} else if (*lvalue == '1') {
		N_WORD (1);
		return TRUE;
	} else if (*lvalue == '2') {
		N_WORD (2);
		return TRUE;
	} else if (*lvalue == '3') {
		N_WORD (3);
		return TRUE;
	} else if (*lvalue == '4') {
		N_WORD (4);
		return TRUE;
	} else if (*lvalue == '5') {
		N_WORD (5);
		return TRUE;
	} else if (*lvalue == '6') {
		N_WORD (6);
		return TRUE;
	} else if (*lvalue == '7') {
		N_WORD (7);
		return TRUE;
	} else if (*lvalue == '8') {
		N_WORD (8);
		return TRUE;
	} else if (*lvalue == '9') {
		N_WORD (9);
		return TRUE;
	}

	return FALSE;

}


static int
getlvalue (char *lvalue, const char *ptr)
{
	int i, limit;

	if (strchr (ptr, ' ') != NULL)
		/* This should be enought, if not, lines like:
		 * ${set var=sample text} will be truncated: "sample te"
		 * Note: it has to be changed also on every caller function */
		limit = 2 * VAR_LVALUE + VAR_RVALUE + 20;
	else
		limit = VAR_LVALUE;

	for (i = 0; i < limit; i++) {
		if (*(ptr + i + 1) /*beware the sign! */ != '\\' && *(ptr + i + 2) == '}')
			break;
		if (*(ptr + i + 2) == '\\' && *(ptr + i + 3) == '}')
			i++;
		*(lvalue + i) = *(ptr + i + 2);
	}

	*(lvalue + i) = '\0';

	return i;

}


static int
cond_true (const char *str)
{
	char lvalue[2 * VAR_LVALUE + VAR_RVALUE + 20] = "";
	int i = 0;

	if (*str == '\0')
		return TRUE;

	while (*str != '\0') {
		if (*(str + 1) == '{') {
			i = getlvalue (lvalue, str);
			if (get_void_rvalue (lvalue) == FALSE)
				/* Stop expansion when a cond is false,
				 * if this is not desired set cond to 
				 * false and return cond below
				 */
				/* cond = FALSE; */
				return FALSE;
			str += i + 3 - 1; /* strlen ("${}") == 3 */
		}
		str++;
	}

	return TRUE;
	/* return cond; */
}


/* Store in sendbuf, ptr expanded, needs msg to get rvalues,
 * last argv is whether add or not the separator 
 */
void
expand_str (char *sendbuf, const struct servermsg *msg, const char *ptr, size_t len,
	    char tosend)
{
	unsigned short int no_scape_flag = FALSE;

	if (len > EV_BUF)
		return;

	if (ptr == NULL)
		return;

	while (*ptr != '\0') {
		if (--len == 0)
			break;

		if (*ptr == '\\' && *(ptr + 1) == '\\') {
			ptr++;

			if (*ptr != '\0')
				*sendbuf++ = *ptr++;

			no_scape_flag = TRUE;
			continue;
		}

		if (*ptr == '$' && (*(ptr - 1) != '\\' || no_scape_flag)
		    && *(ptr + 1) == '{') {
			char lvalue[VAR_LVALUE];
			char rvalue[VAR_RVALUE];

			getlvalue (lvalue, ptr);

			/* If getrvalue returns FALSE there has been no conversion,
			 * it jums to the part where the char is copied and the next
			 * time there won't be a starting $ so it'll continue copying. */
			if (getrvalue (rvalue, lvalue, VAR_RVALUE, msg) == TRUE) {
				size_t rvaluelen;

				strncpy (sendbuf, rvalue, len);

				/* rvaluelen is set to the numer of bytes copied into sendbuf */
				rvaluelen = strlen (rvalue);
				if (rvaluelen > len)
					rvaluelen = len;

				/* TODO:change this to p = strchr (lvalue, * ’\0’); 
				 * should be faster according to the docs. I need a test 
				 * environment before commit :), oh, and there is another
				 * one into the lib.c file */
				ptr += strlen (lvalue) + 2; /* strlen ("${") = 2 */

				/* There is something like: ${var args...}, 
				 * let's search for the matching '}' */
				if (*ptr != '}') {
					while (*ptr++ != '}')
						if (*ptr == '\0')
							break;
				} else
					ptr++;

				sendbuf += rvaluelen;

				if (len < rvaluelen) {
					len = 0;
					break;
				}

				len -= rvaluelen;

				/* This make something like ${x}${y} work */
				if (*ptr == '$')
					continue;
			}
		}

		if (no_scape_flag)
			no_scape_flag = FALSE;

		if (*ptr == '\0')
			break;

		*sendbuf++ = *ptr++;

	}

	if (tosend == ADD_SEP) {
		if (len < SEPARATOR_LEN)
			sendbuf -= SEPARATOR_LEN + len;

		strncpy (sendbuf, SEPARATOR, SEPARATOR_LEN);
		*(sendbuf + SEPARATOR_LEN) = '\0';
	} else
		*sendbuf = '\0';

	return;
}


/* This function expands the command in the events file and sends it to the server,
 * besides, it makes up a line with the command and calls the internal functions 
 * like if the line would have been received from the server
 */
static void
do_irc (const struct _event *current, const struct servermsg *msg)
{
	char sendbuf[EV_BUF] = "", *send_ptr = sendbuf;
	char fakemsg[MSGBUF], *p;
	struct servermsg msg2;
	int end = 0, len = 0;
#ifdef DEAD_CODE
	modes_left;
#endif

	expand_str (sendbuf, msg, current->command, EV_BUF, ADD_SEP);

	botsend (msg->server, sendbuf);

	/* After send () response, raise an internal event */
	while (TRUE) {
		send_ptr += len;

		/* This deserves a comment. When there are more than one command in one line,
		 * we have: command1{SEP}command2, I don't want to give parse_server() a 
		 * string with no '\0' where it should, but I can't do: 
		 * *(send_ptr + strlen2sepstr(etc.) = '\0'; because parse_server() expects 
		 * a separator after the data. 
		 * The solution here is do a malloc but just when there are more than one command,
		 * and copy the string into other buffer 
		 */
		p = send_ptr;

		/* This one is tricky: if there is a '\0' after sep. mark the end of the loop */
		len = strlen2sepstr (send_ptr, SEPARATOR);
		if (len == -1)
			return;

		len += SEPARATOR_LEN;

		if (*(send_ptr + len) == '\0')
			end = TRUE;

		if (strncasecmp (send_ptr, "PONG", 4) == 0)
			continue;

		if (!end) {
			p = (char *) xmalloc (len + 1);
			strncpy (p, send_ptr, len + 1);
		}

		msg2.server = msg->server;

		/* Note: use send_ptr seems a little dangerous to me, 
		 * anyway being careful in events file shouldn't be a problem at all */
		/* parse_server() wants a buffer without the leading ':' 
		 * so it's not printf'ed here 
		 */
		snprintf (fakemsg, MSGBUF, "%s!%s@%s %s", msg->server->nick,
			  msg->server->ident, msg->server->vhost, p);

		if (p != send_ptr)
			free (p);

/* Just let PRIVMSG event is raised, so it'll always be one mode */
#ifdef DEAD_CODE
		modes_left = get_count_multi (fakemsg);
		/* This can be improved, I can parse once, save msg->message 
		 * and call get_multi__mode_* code afterwards, but there won't be a lot of own messages
		 * neither a lot of modes_left > 1 
		 */
		for (; modes_left > 0; modes_left--)
			parse_server (fakemsg, &msg2, modes_left);
#else
		parse_server (fakemsg, &msg2, 1);
#endif

		/* IMPORTANT!: Just let propagate some events: */
		/* FIXME: add other events here */
		if (msg2.action_n == PRIVMSG)
			raise_event_servermsg (&msg2);

		if (end)
			break;

	}

}

#ifdef HAVE_PTHREAD
/* TODO: esto deberia ser static, no lo pongo porque no tengo tiempo de probarlo y nunca se sabe.. */
struct _do_irc {
	const struct _event *one;
	const struct servermsg *two;
};

static void *
do_irc_another_thread (void *t)
{
	do_irc (((struct _do_irc *) t)->one, ((struct _do_irc *) t)->two);

	return NULL;
}
#endif /* HAVE_PTHREAD */


#ifndef FNM_CASEFOLD
#  define FNM_CASEFOLD 0
#endif /* FNM_CASEFOLD */

#define match_str(_ms, _ev)	\
	((*_ev == '\0') ? 1 : ((fnmatch (_ev, _ms, FNM_CASEFOLD) == 0) ? TRUE : FALSE))

#ifdef USE_FNMATCH

#define match_ev_str(_ms, _ev, _regex_null)	match_str(_ms, _ev)

#else /* USE_FNMATCH not defined: using regexes */

/* ms is the string with the regex,
 * regex is the compiled regex, 
 * msg is needed for the expansion. */
static int
match_ev_str (const char *ms, const char *ev, regex_t *regex, const struct servermsg *msg)
{
	int ret;
	/* It could get a little longer once expanded... */
	/*char s[REGEX_LENGTH + 50]; */

	/*expand_str (s, msg, ms, REGEX_LENGTH + 50, NOT_SEP); */

	//ret = regexec (regex, s, 0, NULL, 0);
	ret = regexec (regex, ms, 0, NULL, 0);
	if (ret == 0) /* 0 for a successful match */
		return 1;
/*	else if (ret == REG_NOMATCH)
		return 0;
	else {
		char errmsg[100];

		regerror (ret, regex, errmsg, sizeof (errmsg));
		eperror (errmsg);
	}
*/
	return 0;
}

#endif /* USE_FNMATCH */


/* Order matters */
#define match(_msg, _cur, _mask)								\
	(match_str (_msg->channel, _cur->channel) && 						\
	(*_cur->text == '\0' || match_ev_str (_msg->message, _cur->text, &_cur->regex, _msg))	\
	&& match_str (_mask, _cur->_mask)							\
	&& (*_cur->cond == '\0' || cond_true (_cur->cond)))					\


/* Try to match a msg against an event, and if matches calls do_irc () */
static int
match_event (struct _event *head, struct servermsg *msg)
{
	struct _event *current = head;
	char mask[NICK_MAX_LENGTH + 9 + 128 + 3];

	if (head == NULL)
		return 0; /* No events for msg->action */

	snprintf (mask, NICK_MAX_LENGTH + 9 + 128 + 3,
		  "%s!%s@%s", msg->nick, msg->ident, msg->host);

	do {
		/* If everythins matches... */
		if (match (msg, current, mask) == FALSE) {
			current = current->next;
			continue;
		}

		msg->last_matched = current;

		if (strchr (current->options, 'i') != NULL)
			return 0;

		switch (current->target_ncmd) {
		case cmd_null:
			break;
		case cmd_log:
			do_log (msg);
			break;
		case cmd_reload:
			return do_reload (msg);
		default:
			/* Local originated autos don't cross,
			 * this prevents infinite recursion 
			 */
			if (strncmp (msg->nick, msg->server->nick, NICK_MAX_LENGTH) == 0)
				return 0;

			/* This is to keep some control upon non-implemented commands */
			if (msg->action_n != OTHER) {
#ifdef HAVE_PTHREAD
				if (strchr (current->options, 't') == NULL) {
					do_irc (current, msg);
					break;
				}

				{
				/* If you don't use it, you don't pay for it (except for 
				 * the searching of 't' flag). */
				pthread_t thread;

				struct _do_irc d = {
					.one = current,
					.two = msg
				};

				/* Don't care for the returned value, if a message fails it won't
				 * be sent or whatever... */
				pthread_create (&thread, NULL, do_irc_another_thread, (void *) &d);

				}
#else /* HAVE_PTHREAD */
				do_irc (current, msg);
#endif /* HAVE_PTHREAD */
			}

			break;
		}

		if (strchr (current->options, 'n') != NULL)
			return 1;
		else if (current->options[0] == '\0'
			 || strchr (current->options, 'c') == NULL)
			return 0;

		current = current->next;
	} while (current != NULL);

	return 0;

}



/* Main event function, check if the server line in msg matches any event */
void
look_up_event (struct servermsg *msg)
{
	unsigned int offset;

	/* ANY events allways match */
	if (match_event (events[ev_any], msg) == 1)
		return;

	switch (msg->action_n) {
	case PRIVMSG:
		offset = ev_privmsg;
		break;
	case PING:
		offset = ev_ping;
		break;
	case MODE:
		offset = ev_mode;
		break;
	case JOIN:
		offset = ev_join;
		break;
	case NICK:
		offset = ev_nick;
		break;
	case QUIT:
		offset = ev_quit;
		break;
	case TOPIC:
		offset = ev_topic;
		break;
	case KICK:
		offset = ev_kick;
		break;
	case NOTICE:
		offset = ev_notice;
		break;
	case PART:
		offset = ev_part;
		break;
	case INVITE:
	case KILL:
	case WALLOPS:
	case SNOTICE:
	case ERROR: /* unparseable action */
		return;
	case OTHER: /* unimplemented non-numeric command */
		return;
	case CTCP:
		offset = ev_ctcp;
		break;
	default: /* numeric command */
		offset = msg->action_n % 10;
		break;
	}

	match_event (events[offset], msg);

	return;

}
