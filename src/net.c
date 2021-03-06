#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "server.h"
#include "lib.h"


/* used to send */
ssize_t
botsend (struct _server *server, const char *data)
{

	/* FIXME: flood control here */
	server->lastmsg_time = time (NULL);

#ifdef DEBUG_VERBOSE
	printf ("\n-->%s", data);
#endif /* DEBUG */

	/* The gnu libc manual says: "The write function is the underlying 
	 * primitive for all of the functions that write to streams", and the
	 * stdio man pages say flock and friends use locks, so a lock shouldn't
	 * be necessary, BEWARE: for now. */
	return write (server->sock, data, strlen (data));

}


/* FIXME: hack? */
/* data == nick!ident@mask MODE #canal +oo-v tal cual pascual */
static void
count_multi (const char *data, unsigned int *modes_left)
{
	if (data == NULL)
		goto out;

	while (*data == ' ')
		data++;

	data = strchr (data, ' ');
	if (data == NULL)
		goto out;

	while (*data == ' ')
		data++;

	if (strncasecmp (data, "MODE ", 5) == 0) {
		/* Descriptive examples: "MODE #chan...
		 * +o nick -b *!*@*"; +oo-v tal cual pascual"; 
		 * -s +l 10"; +iwo-Or+j"; -s"; +o nick" 
		 */
		*modes_left = 0;
		data += 5;

		/* We are just counting, so it's enough knowing how many chars 
		 * are between _every_ '+'/'-' sign and ' ' minus '+'s and '-'s.
		 */
		data = strchr (data, (int) ' ');
		if (data == NULL)
			return;
		while (*data == ' ')
			data++;
		if (data == NULL || (*data != '+' && *data != '-')) /*Bogus mode*/
			goto out;
		while (*data != '\0') {
			if (*data == '+' || *data == '-') { /* A mode string starts */
				/* while *data != ' ' count chars diferent from +/-, 
				 * the '\0' if for lines like: mode nick -i\0 
				 */
				while (*data != ' ' && *data != '\0') {
					if (*data != '+' && *data != '-')
						++*modes_left;
					data++;
				}
			} else /* no mode sub-string, get next char until +/-/\0 */
				while (*data != '+' && *data != '-' && *data != '\0')
					data++;
		}
	}
	return;

out:
	*modes_left = 1;
	return;
}



int
listen_socket (const char *name)
{
}


/* measure the length till SEPARATOR, and PEEK that length */
int
botrecv (struct servermsg *msg)
{
	int bytes, offset;
	char data[MSGBUF];
	static unsigned int modes_left = 1;

	/* Receive MSGBUF bytes but PEEKing */
	bytes = recv (msg->server->sock, data, MSGBUF, MSG_PEEK);

	/* The offset till SEPARATOR */
	if (strstr (data, SEPARATOR) != NULL)
		offset = strstr (data, SEPARATOR) - data + SEPARATOR_LEN;
	else
		offset = strchr (data, '\0') - data;

	data[offset] = '\0';

	/* If finished with previous mode changes, check modes for this line */
	if (modes_left <= 1)
		count_multi (data, &modes_left);
	else /* If not, check next */
		modes_left--;

	/* Really receive, but just one line till SEPARATOR */
	if (modes_left <= 1) {
		if ((bytes = recv (msg->server->sock, data, offset, 0)) < 1)
			return bytes;

		/* The separator is kept to mark the end of string */
		data[bytes] = '\0';

		/* Just in case we got an empty line or NULL */
		if (data[0] == '\0')
			return bytes;
	}

	/* If the data received starts with ':' ... */
	if (data[0] == ':') /* Most commands: */
		parse_server (&data[1], msg, modes_left);
	else /* PING, NOTICE, etc. */
		parse_server2 (data, msg);

#ifdef DEBUG_VERBOSE
	DUMP_MSG (msg);
#endif /* DEBUG */


	return bytes;

}


