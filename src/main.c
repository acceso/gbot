#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LOCAL
#include "defaults.h"
#include "server.h"
#include "main.h"
#include "readcfg.h"
#include "lib.h"
#include "net.h"
#include "events.h"


static struct bot_data bdata;


static int
start_bot (struct _server *server)
{
	struct servermsg msg;

	msg.server = server;

	/********** Bot's main loop **********/
	for (;;) {
		if (botrecv (&msg) == 0)
			break;

/* I keep this just in case events stop working while coding something new */
/*		if (!strncasecmp (msg.action, "PING", 4)) {
			char cad[100];
			sprintf (cad, "PONG :%s" SEPARATOR, msg.message);
			botsend (server, cad);
		}
*/

		look_up_event (&msg);
	}
	/*************************************/

	return 0;

}



#if (OS != BSD) && (OS != SUN)
/* Try to create the necesary directories */
static int
make_dirs (void)
{
	char path[PATH_MAX_LEN];

	snprintf (path, PATH_MAX_LEN, "%s/." PACKAGE "/", getenv ("HOME"));

	if (mkdir (path, 0777) == -1) /* rwxr-xr-- & ~umask */
		if (errno != EEXIST)
			return 1;

	snprintf (path, PATH_MAX_LEN, "%s/." PACKAGE "/" LOGDIR "/", getenv ("HOME"));

	if (mkdir (path, 0777) == -1) /* rwxr-xr-- & ~umask */
		if (errno != EEXIST)
			return 1;

	return 0;

}



/* FIXME: invalid lvalue in assignment, 3 lines below in *BSD and SUN */
/* Reopen streams properly */
static void
reopen_streams (void)
{
	char statusfile[PATH_MAX_LEN];

	stdin = freopen (STDIFILE, "r", stdin);

	/* FIXME: use server->servername instead of status */
	snprintf (statusfile, PATH_MAX_LEN, "%s/." PACKAGE "/" LOGDIR "/status",
		  getenv ("HOME"));

	stdout = freopen (statusfile, "a", stdout);
	if (stdout == NULL && make_dirs ())
		if ((stdout = freopen (statusfile, "a", stdout)) == NULL)
			stdout = freopen (STDOFILE, "r", stdout);

	stderr = freopen (statusfile, "a", stderr);
	if (stderr == NULL && make_dirs ())
		if ((stderr = freopen (statusfile, "a", stderr)) == NULL)
			stderr = freopen (STDEFILE, "r", stderr);

	return;

}
#endif



#ifndef __STRICT_ANSI__
/* Prints a banner when called with -h */
static void
prn_ban (void)
{
	printf ("Usage: " PACKAGE " [OPTIONS] server\n"
		"  -n Nick       Nick\n"
		"  -i Ident      Use ident \"ident\"\n"
		"  -f Fullname   Fullname for the bot\n"
		"  -o passwOrd   Connection password\n"
		"  -r poRt       Server port\n"
		"  -p Pass       Bot password\n"
		"  -a Adminpass  Bot admin password\n"
		"  -e Eventsfile Events file\n"
		"\n");
	return;

}


static void
parse_args (struct _server *server, char *file, int argc, char **argv)
{
	int c;

	/* FIXME: user getopt_long() */
	while ((c = getopt (argc, argv, "n:i:f:o:r:p:a:e:h")) != -1)
		switch (c) {
		case 'n':
			strncpy (server->nick, optarg, NICK_MAX_LENGTH);
			break;
		case 'i':
			strncpy (server->ident, optarg, 9);
			break;
		case 'f':
			strncpy (server->fullname, optarg, 128);
			break;
		case 'o':
			strncpy (server->password, optarg, 85);
			break;
		case 'r':
			server->port = atoi (optarg);
			break;
		case 'p':
			strncpy (botdata->pass, optarg, 9);
			break;
		case 'a':
			strncpy (botdata->op_pass, optarg, 9);
			break;
		case 'e':
			strncpy (file, optarg, PATH_MAX_LEN);
			break;
		case 'h':
			prn_ban ();
			exit (EXIT_SUCCESS);
		case '?':
			if (isprint (optopt))
				fprintf (stderr, "Unknown option or wrong argument number:"
					 " '-%c'.\n", optopt);
			else
				fprintf (stderr, "Caracter desconocido: '\\x%x'.\n", optopt);
			break;
		default:
			exit (EXIT_FAILURE);
		}

	return;

}
#endif /* __STRICT_ANSI__ */




/* Load default values */
static void
defl_val (struct _server *server)
{
	strncpy (server->hostname, DEFAULT_HOST, 128);
	server->port = DEFAULT_PORT;
	strncpy (server->nick, DEFAULT_NICK, NICK_MAX_LENGTH);
	strncpy (server->ident, DEFAULT_IDENT, 9);

	strncpy (server->vhost, DEFAULT_VHOST, 128);
	strncpy (server->fullname, DEFAULT_FULLNAME, 128);

	return;

}



int
main (int argc, char *argv[])
{
	struct _server srv, *server = &srv;
	char ev_file[PATH_MAX_LEN] = "";
	char *ev_file_ptr = ev_file;
#ifdef RUN_AS_DAEMON
	pid_t pid;
	struct sigaction act;
#endif

	botdata = &bdata;

	defl_val (server);
	read_cfg (server);

	/* FIXME: use enviroment to get server, nick, etc. here */

	/* Parse argv */
#ifndef __STRICT_ANSI__ /* Defined when -ansi is used */
	parse_args (server, ev_file, argc, argv);

	if (optind == argc - 1)
		strncpy (server->hostname, *(argv + argc), 128);
#endif /* __STRICT_ANSI__ */

	read_events (&ev_file_ptr);


	printf (PACKAGE " " VERSION " aka " VERSIONSTR "\n"
		"built on " __DATE__ " " __TIME__ " with GNU C " __VERSION__ "\n");

	/* Try to connect to the server */
	printf ("Connecting to server... ");
	open_irc_server (server);
	printf ("OK!\n");

	/* Some irc servers accept nicks such in nick:password,
	 * without this, the nick gets logged with the pass. */
	if (strchr (server->nick, ':'))
		*strchr (server->nick, ':') = '\0';

	srand ((unsigned int) time (NULL));

#ifdef RUN_AS_DAEMON
	printf ("Forking... OK!\n");

	switch (pid = fork ()) {
	case 0: /* Child */
		setsid ();
		switch (pid = fork ()) {
		case 0: /* GrandChild xDD */
			/* Reopen standard streams */
#if (OS != BSD) && (OS != SUN)
			reopen_streams ();
#endif
			chdir ("/");
			umask (0);
			/* line buffered output */
			setlinebuf (stdout);
			setlinebuf (stderr);

			/* just in case it does't get the standar streams */
			if (fileno (stdin) != STDIN_FILENO)
				dup2 (fileno (stdin), STDIN_FILENO);
			if (fileno (stdout) != STDOUT_FILENO)
				dup2 (fileno (stdout), STDOUT_FILENO);
			if (fileno (stderr) != STDERR_FILENO)
				dup2 (fileno (stdout), STDERR_FILENO);

			NEW_HANDLER (SIGHUP, &act, do_sig_reload);

			start_bot (server);
		}
		break;
	case -1: /* error */
	default: /* parent */
		_exit (EXIT_SUCCESS);
	}

#else /* RUN_AS_DAEMON */

	start_bot (server);

#endif

	return EXIT_SUCCESS;

}


