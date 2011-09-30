#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "defaults.h"
#include "server.h"
#include "main.h"
#include "lib.h"
#include "events.h"



static void
store_cfg_parm (struct _server *server, char *lvalue, char *rvalue)
{
	if (strncasecmp (lvalue, "SERVER", 6) == 0)
		strncpy (server->hostname, rvalue, 128);
	else if (strncasecmp (lvalue, "PORT", 4) == 0)
		server->port = atoi (rvalue);
	else if (strncasecmp (lvalue, "SERVPASS", 8) == 0)
		strncpy (server->password, rvalue, 85);
	else if (strncasecmp (lvalue, "NICK", 4) == 0)
		strncpy (server->nick, rvalue, NICK_MAX_LENGTH);
	else if (strncasecmp (lvalue, "IDENT", 5) == 0)
		strncpy (server->ident, rvalue, 9);
	else if (strncasecmp (lvalue, "NAME", 4) == 0)
		strncpy (server->fullname, rvalue, 128);
	else if (strncasecmp (lvalue, "BOTPASS", 7) == 0)
		strncpy (botdata->pass, rvalue, 9);
	else if (strncasecmp (lvalue, "OP_PASS", 7) == 0)
		strncpy (botdata->op_pass, rvalue, 9);
	else
		fprintf (stderr, "Error in config file\n"
			 "LVALUE: %s\nRVALUE: %s\n", lvalue, rvalue);

	return;

}



static void
parse_cfg_line (char *line, char *lvalue, unsigned int lvalsz, char *rvalue,
		unsigned int rvalsz)
{
	char longval = '\0';
	unsigned int i = 0;

	/* Store the variable name in lvalue */
	while (*line != '=' && *line != ' ') {
		if (i >= lvalsz - 1)
			break;

		*(lvalue + i++) = *line++;
	}

	*(lvalue + i) = '\0';

	/* skip the separator */
	while (*line == ' ')
		line++;
	while (*line == '=')
		line++;
	while (*line == ' ')
		line++;

	i = 0;
	if (*line == '\'' || *line == '"') {
		longval = *line;
		line++;
		while (*line != longval) {
			if (*line == longval)
				break;

			if (*line == '\\' && *(line + 1) == longval)
				line++;

			if (i >= rvalsz - 1)
				break;

			*(rvalue + i) = *line;
			line++;
			i++;
		}
	} else {
		i = 0;
		while (*line != ' ' && *line != '\n') {
			if (i >= rvalsz - 1)
				break;

			*(rvalue + i) = *line;
			line++;
			i++;
		}
	}

	*(rvalue + i) = '\0';

	return;

}



/* 
 * Reads main configuration file 
 */
void
read_cfg (struct _server *server)
{
	int fd;
	char path[PATH_MAX_LEN];
	char line[LINEBUF], lvalue[VAR_LVALUE], rvalue[VAR_RVALUE];
	char *cbuf;
	size_t offset = 0;
	off_t size;

	snprintf (path, PATH_MAX_LEN, "%s/." PACKAGE "/" CONFIGFILE, getenv ("HOME"));

	if ((fd = open (path, O_RDONLY, 0640)) == -1) {
		snprintf (path, PATH_MAX_LEN, "./" CONFIGFILE);
		if ((fd = open (path, O_RDONLY, 0640)) == -1)
			return;
	}

	size = lseek (fd, 0, SEEK_END);

	cbuf = (char *) mmap (0, (size_t) size, PROT_READ, MAP_SHARED, fd, 0);
	if (cbuf == MAP_FAILED)
		eperror ("mmap");

	close (fd);

	while (readln (cbuf, line, LINEBUF, &offset, size) != EOF) {
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
			continue;

		parse_cfg_line (line, lvalue, VAR_LVALUE, rvalue, VAR_RVALUE);
		store_cfg_parm (server, lvalue, rvalue);
	}

	munmap ((void *) cbuf, (size_t) size);

	return;

}

/* -------------------- main events read below ---------------------- */


extern struct _event *events[ev_LAST];


static void
add_event (struct _event *root, struct _event *event, int evnum)
{
	if (root == NULL)
		root = events[evnum] = (struct _event *) xmalloc (sizeof (struct _event));
	else {
		while (root->next != NULL)
			root = root->next;
		root->next = (struct _event *) xmalloc (sizeof (struct _event));
		root = root->next;
	}

	strncpy (root->event, event->event, 8);
	strncpy (root->channel, event->channel, CHAN_MAX_LENGTH);
	strncpy (root->mask, event->mask, NICK_MAX_LENGTH + 9 + 128 + 3);

	if (*event->text != '\0') {
		expand_str (root->text, NULL, event->text, REGEX_LENGTH, NOT_SEP);
		if (regcomp (&root->regex, root->text, REG_EXTENDED | REG_ICASE | REG_NOSUB)
		    != 0) {
			/* This is to exit when can't compile regex: */
			/* eperror (cad); */
			/* I think is safer skip the line */
			free (root);
			return;
		}
	}

	strncpy (root->cond, event->cond, EXTRA_LENGTH);

	/* It's faster if we store [a-z] here instead of comparing to
	 * [a-z] _and_ [A-Z] afterwards, TODO: this could be sorted to
	 * make it even faster. */
	strncpy (root->options, strtolower (event->options), 10);

	strncpy (root->command, event->command, EV_BUF);

	root->target_ncmd = event->target_ncmd;

	return;

}


static void
store_event (struct _event *event)
{
	if (strcasecmp (event->event, "ANY") == 0)
		add_event (events[ev_any], event, ev_any);
	else if (strcasecmp (event->event, "PRIVMSG") == 0)
		add_event (events[ev_privmsg], event, ev_privmsg);
	else if (strcasecmp (event->event, "PING") == 0)
		add_event (events[ev_ping], event, ev_ping);
	else if (strcasecmp (event->event, "MODE") == 0)
		add_event (events[ev_mode], event, ev_mode);
	else if (strcasecmp (event->event, "JOIN") == 0)
		add_event (events[ev_join], event, ev_join);
	else if (strcasecmp (event->event, "NICK") == 0)
		add_event (events[ev_nick], event, ev_quit);
	else if (strcasecmp (event->event, "QUIT") == 0)
		add_event (events[ev_quit], event, ev_quit);
	else if (strcasecmp (event->event, "TOPIC") == 0)
		add_event (events[ev_topic], event, ev_topic);
	else if (strcasecmp (event->event, "KICK") == 0)
		add_event (events[ev_kick], event, ev_kick);
	else if (strcasecmp (event->event, "NOTICE") == 0)
		add_event (events[ev_notice], event, ev_notice);
	else if (strcasecmp (event->event, "PART") == 0)
		add_event (events[ev_part], event, ev_part);
	else if (strcasecmp (event->event, "INVITE") == 0)
		add_event (events[ev_invite], event, ev_invite);
	else if (strcasecmp (event->event, "CTCP") == 0)
		add_event (events[ev_ctcp], event, ev_ctcp);
	else {
		int num;

		if (sscanf (event->event, "%d", &num) <= 0) {
			fprintf (stderr, "Unknown event: %s\n", event->event);
			return;
		}

		num %= 10;
		add_event (events[num], event, num);
	}

	return;

}


static unsigned int
is_void_func (const char *str)
{
	while (*str == ' ')
		if (*str++ == '\0')
			return FALSE;

	if (*str++ != '(')
		return FALSE;

	while (*str == ' ')
		if (*str++ == '\0')
			return FALSE;

	if (*str != ')')
		return FALSE;

	return TRUE;
}


static unsigned int
get_ncmd (const char *str)
{

	while (*str == ' ')
		str++;

	/* Ordered from more to less common commands,
	 * it's a little bit messy but also a little bit faster :) 
	 */
	if (strncasecmp (str, "PRIVMSG", 7) == 0)
		return cmd_privmsg;

	if (strncasecmp (str, "MODE", 4) == 0)
		return cmd_mode;

	if (strncasecmp (str, "PONG", 4) == 0)
		return cmd_pong;

	if (strncasecmp (str, "TOPIC", 5) == 0)
		return cmd_topic;

	if (strncasecmp (str, "KICK", 4) == 0)
		return cmd_kick;

	if (strncasecmp (str, "NOTICE", 6) == 0)
		return cmd_notice;

	if (strncasecmp (str, "null", 4) == 0 && is_void_func (str + 4))
		return cmd_null;

	if (strncasecmp (str, "log", 3) == 0 && is_void_func (str + 3))
		return cmd_log;

	if (strncasecmp (str, "reload", 6) == 0 && is_void_func (str + 6))
		return cmd_reload;


	return cmd_other;

}


/* Parse one line of the events file and store in an event structure,
   nice method this of using offsets :) */
static struct _event *
parse_event_line (char *line, struct _event *this_event)
{
	int offset = 0;

	if (*line == EVENT_SEPARATOR) 
		return NULL; /* event without event */

	/* Event */
	get_token (line, EVENT_SEPARATOR, this_event->event, &offset, 8);

	/* Channel */
	get_token (line, EVENT_SEPARATOR, this_event->channel, &offset, CHAN_MAX_LENGTH);

	/* mask */
	get_token (line, EVENT_SEPARATOR, this_event->mask, &offset, (NICK_MAX_LENGTH +
		   9 /* ident */ + 128 /* server */ + 3 /* '!' + '@' + '\0' */ ));

	/* Text to match */
	get_token (line, EVENT_SEPARATOR, this_event->text, &offset, REGEX_LENGTH);

	/* Extra condition */
	get_token (line, EVENT_SEPARATOR, this_event->cond, &offset, EXTRA_LENGTH);

	/* Options */
	get_token (line, EVENT_SEPARATOR, this_event->options, &offset, 10);

	/* Command */
	get_token (line, '\n', this_event->command, &offset, EV_BUF);

	/* Numeric value for the first word in the command field */
	this_event->target_ncmd = (this_event->command != NULL) ?
	    get_ncmd (this_event->command) : cmd_other;

	return this_event;

}



/* Reads events file */
void
read_events (char **thispath)
{
	char line[1024];
	int fd = -1, ret;
	struct _event one_event;
	static char *path;
	char *mbuf;
	/* This is really funny :) */
	size_t offset = 0;
	off_t size;

	/* When called with NULL uses previous path */
	if (thispath != NULL)
		path = *thispath;

	if (*path != '\0') /* events file specificated */
		/* error checking is below: if (fd == -1) ..etc.. */
		fd = open (path, O_RDONLY, 0640);

	if (fd == -1) { /* User file fails to open or there is no user file */
		snprintf (path, PATH_MAX_LEN, "%s/." PACKAGE "/" EVENTS_FILE,
			  getenv ("HOME"));

		if ((fd = open (path, O_RDONLY, 0640)) == -1) {
			/* can't continue without events, let's see in current path */
			snprintf (path, PATH_MAX_LEN, "./" EVENTS_FILE);
			if ((fd = open (path, O_RDONLY, 0640)) == -1)
				eperror ("can't open events file");
		}
	}

	size = lseek (fd, 0, SEEK_END);

	mbuf = (void *) mmap (0, (size_t) size, PROT_READ, MAP_SHARED, fd, 0);
	if (mbuf == MAP_FAILED)
		eperror ("mmap");

	close (fd);

	while ((ret = readln (mbuf, line, 1024, &offset, size)) != EOF) {
		/* In lines longer than 1024 discard everything till the EOL */
		if (ret == ERR_LONGERLINE) {
			while (*(mbuf + offset++) != '\n')
				if ((off_t) offset >= size)
					goto eof_reached;
			offset++;
		}
		
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
			continue;

		if (parse_event_line (line, &one_event) == NULL)
			continue;

		store_event (&one_event);
	}
eof_reached:
	munmap ((void *) mbuf, (size_t) size);

	return;

}


