#ifndef NET_H
#define NET_H

#include <sys/types.h>


ssize_t botsend (struct _server *server, const char *data);

int listen_socket (const char *name);
int botrecv (struct servermsg *msg);


#endif /* NET_H */

