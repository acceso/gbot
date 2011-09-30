#ifndef LOGGING_H
#define LOGGING_H

#include "server.h"

#define M_MESSAGE		"%s <%s> %s\n"
#define M_CTCP			"%s *\t%s %s\n"
#define M_GAIN_OP 		"%s --- %s gives channel operator status to %s\n"
#define M_LOOSE_OP		"%s --- %s removes channel operator status from %s\n"
#define M_GAIN_VOICE 		"%s --- %s gives voice to %s\n"
#define M_LOOSE_VOICE		"%s --- %s removes voice from %s\n"
#define M_GAIN_BAN 		"%s --- %s sets ban on %s\n"
#define M_LOOSE_BAN		"%s --- %s removes ban on %s\n"
#define M_MODE_CHANGE		"%s --- %s changes mode to %s\n"
#define M_MODE_CHANGE_CHAN	"%s --- %s changes mode %s %s\n"
#define M_JOINS			"%s --> %s (%s@%s) has joined %s\n"
#define M_QUITS			"%s <-- %s has quit (%s)\n"
#define M_TOPIC_CHANGE		"%s --- %s has changed the topic to: %s\n"
#define M_PARTS			"%s <-- %s (%s@%s) has left %s (%s)\n"
#define M_PARTS_NO_REASON	"%s <-- %s (%s@%s) has left %s\n"
#define M_OTHER			"%s --- (%s) %s\n"
#define M_NICK_CHANGE		"%s --- %s is now known as %s\n"
#define M_QUITS			"%s <-- %s has quit (%s)\n"
#define M_TOPIC_CHANGE		"%s --- %s has changed the topic to: %s\n"
#define M_KICKS			"%s <-- %s has kicked to %s from %s (%s)\n"
#define M_NOTICE		"%s -%s- %s\n"


#define M_DIR  PACKAGE "/" LOGDIR "/"


void do_log (const struct servermsg *msg);


#endif /* LOGGING */


