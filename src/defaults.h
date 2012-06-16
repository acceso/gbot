#ifndef DEFAULTS_H
#define DEFAULTS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Server ----------------------------------------------------*/
#define DEFAULT_PORT		6667
#define DEFAULT_IDENT		"myident"
#define DEFAULT_FULLNAME	"Lost freak"
#define DEFAULT_HOST		"irc.irc-hispano.org"
#define DEFAULT_VHOST		"x.x.x.x"
#define DEFAULT_NICK		"Mrdyc"

/* miscs -----------------------------------------------------*/

#define CONFIGFILE PACKAGE ".conf"
#define EVENTS_FILE "events.conf"

/* Where to store logs within the dot dir */
#define LOGDIR "logs"

/* Default value for standard streams,
   just used when every other choice fails */
#define STDIFILE "/dev/null"
#define STDOFILE "/dev/null"
#define STDEFILE "/dev/null"

/* RFC says it is \r\n */
#define SEPARATOR "\r\n"
#define SEPARATOR_LEN (strlen (SEPARATOR))

#define EVENT_SEPARATOR ':'

/* lengths ---------------------------------------------------*/

#define PATH_MAX_LEN 512

#define NICK_MAX_LENGTH 64
#define CHAN_MAX_LENGTH 202
#define TOPIC_MAX_LENGTH 512	/* Couldn't find a better number */
#define REGEX_LENGTH 250	/* Beware, if this is set to low some regexes won't be compiled */
#define EXTRA_LENGTH 150

#define LINEBUF 512
#define MSGBUF 520
#define EV_BUF 640

/* Number of significative chars for variables in events file */
#define VAR_LVALUE 50 /* It has to be this long for things like ${system ....} */
#define VAR_RVALUE 512

#ifdef DEBUG
#define eperror(fcn) {	fprintf (stderr, "%s %d: ", __FILE__, __LINE__); \
			perror (fcn); \
			exit (EXIT_FAILURE); }
#else
#define eperror(fcn) {	perror (fcn); \
			exit (EXIT_FAILURE); }
#endif /* DEBUG */

#endif /* DEFAULTS_H */

