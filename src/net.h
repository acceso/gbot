#ifndef NET_H
#define NET_H

#include <sys/types.h>

#ifdef DEAD_CODE
int get_count_multi (const char *data);
#endif

ssize_t botsend (struct _server *server, const char *data);
int botrecv (struct servermsg *msg);


#endif /* NET_H */

