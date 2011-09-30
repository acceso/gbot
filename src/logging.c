#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if OS == SUN
#include <strings.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_PTHREAD
#include <sys/file.h>
#endif /* HAVE_PTHREAD */

#include "server.h"
#include "lib.h"
#include "logging.h"



void
do_log (const struct servermsg *msg)
{
	int fd = STDOUT_FILENO;
	char path[PATH_MAX_LEN] = "";
	char obuf[MSGBUF];
	char fdate[DATELEN];
	char *obufptr;
	const char *file = NULL, statusfile[] = "status";

	path[0] = '\0';

	time_str (fdate, DATE_FORMAT);

	switch (msg->action_n) {
	case PRIVMSG:
		if (!is_channel (*msg->channel))
			file = msg->nick;
		else
			file = msg->channel;
		snprintf (obuf, MSGBUF, M_MESSAGE, fdate, msg->nick, msg->message);
		break;
	case MODE:
		file = msg->channel;
		if (*msg->message == '+') {
			if (*(msg->message + 1) == 'o' || *(msg->message + 1) == 'O')
				snprintf (obuf, MSGBUF, M_GAIN_OP,
					  fdate, msg->nick, msg->message + 3);
			else if (*(msg->message + 1) == 'v' || *(msg->message + 1) == 'V')
				snprintf (obuf, MSGBUF, M_GAIN_VOICE,
					  fdate, msg->nick, msg->message + 3);
			else if (*(msg->message + 1) == 'b' || *(msg->message + 1) == 'B')
				snprintf (obuf, MSGBUF, M_GAIN_BAN,
					  fdate, msg->nick, msg->message + 3);
			else
				snprintf (obuf, MSGBUF, M_MODE_CHANGE_CHAN,
					  fdate, msg->nick, msg->message, msg->channel);
		} else if (*msg->message == '-') {
			if (*(msg->message + 1) == 'o' || *(msg->message + 1) == 'O')
				snprintf (obuf, MSGBUF, M_LOOSE_OP,
					  fdate, msg->nick, msg->message + 3);
			else if (*(msg->message + 1) == 'v' || *(msg->message + 1) == 'V')
				snprintf (obuf, MSGBUF, M_LOOSE_VOICE,
					  fdate, msg->nick, msg->message + 3);
			else if (*(msg->message + 1) == 'b' || *(msg->message + 1) == 'B')
				snprintf (obuf, MSGBUF, M_LOOSE_BAN,
					  fdate, msg->nick, msg->message + 3);
			else
				snprintf (obuf, MSGBUF, M_MODE_CHANGE_CHAN,
					  fdate, msg->nick, msg->message, msg->channel);
		} else {
			file = msg->nick;
			snprintf (obuf, MSGBUF, M_MODE_CHANGE,
				  fdate, msg->nick, msg->message);
		}
		break;
	case JOIN:
		file = msg->channel;
		snprintf (obuf, MSGBUF, M_JOINS,
			  fdate, msg->nick, msg->ident, msg->host, msg->channel);
		break;
	case NICK:
		file = msg->nick;
		snprintf (obuf, MSGBUF, M_NICK_CHANGE, fdate, msg->nick, msg->channel);
		break;
	case QUIT:
		file = msg->nick;
		snprintf (obuf, MSGBUF, M_QUITS, fdate, msg->nick, msg->message);
		break;
	case TOPIC:
		file = msg->channel;
		snprintf (obuf, MSGBUF, M_TOPIC_CHANGE, fdate, msg->nick, msg->message);
		break;
	case KICK:
		file = msg->channel;
		snprintf (obuf, MSGBUF, M_KICKS,
			  fdate, msg->nick, msg->target, msg->channel, msg->message);
		break;
	case NOTICE:
		if (msg->nick[0] == '\0')
			file = statusfile;
		else
			file = msg->nick;

		snprintf (obuf, MSGBUF, M_NOTICE, fdate, msg->nick, msg->message);
		break;
	case PART:
		file = msg->channel;
		if (!*msg->message)
			snprintf (obuf, MSGBUF, M_PARTS_NO_REASON, fdate, msg->nick,
				  msg->ident, msg->host, msg->channel);
		else
			snprintf (obuf, MSGBUF, M_PARTS, fdate, msg->nick, msg->ident,
				  msg->host, msg->channel, msg->message);
		break;
	case INVITE:
		break;
	case KILL:
		break;
	case WALLOPS:
		break;
	case PING:
		/* file = STATUSFILE <-- needs a const char *STATUSFILE="...etc..." */
		break;
	case SNOTICE:
		break;
	case ERROR:
		break;
	case OTHER:
		break;
	case CTCP:
		/* FIXME: temporal hack to improve logging for the so common ACTION messages */
		if (!is_channel (*msg->channel))
			file = msg->nick;
		else
			file = msg->channel;

		if (strncasecmp (msg->message, "ACTION", 6) == 0)
			snprintf (obuf, MSGBUF, M_CTCP, fdate, msg->nick,
				  strchr (msg->message, ' ') + 1);
		else
			return;

		break;
	default:
		if (!msg->message)
			break;
		file = statusfile;
		snprintf (obuf, MSGBUF, M_OTHER, fdate, msg->action, msg->message);
		break;
	}


	if (file != NULL && path[0] == '\0') {
		char *env_ret = getenv ("HOME");
		if (env_ret != NULL) {
			char lwfile[CHAN_MAX_LENGTH + 1];

			strncpy (lwfile, file, strlen (file));
			lwfile[strlen (file)] = '\0';

			snprintf (path, PATH_MAX_LEN, "%s/." M_DIR "%s", env_ret,
				  strtolower (lwfile));
		}
	}

	/* If there is no path return without do_log */
	/* FIXME: I should add a check to see if the file does not open because the 
	 * directory does not exist, in that case it could be created and the file 
	 * should be tried to be opened again */
	if (path[0] == '\0' || (fd = open (path, O_WRONLY | O_CREAT | O_APPEND, 0640)) == -1)
		return;

	obufptr = strip_color (obuf);


#ifdef HAVE_PTHREAD
	lock_file (fd);
#endif /* HAVE_PTHREAD */

	write (fd, obufptr, strlen ((const char *) obufptr) * sizeof (char));

#ifdef HAVE_PTHREAD
	unlock_file (fd);
#endif /* HAVE_PTHREAD */


	free (obufptr);

	close (fd);

	return;

}


