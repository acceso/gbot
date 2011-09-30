#ifndef MAIN_H
#define MAIN_H

#include "server.h"

/* Extern variables */
#ifdef LOCAL
#define EXTERN
#else
#define EXTERN extern
#endif

#define GET   1
#define LOOSE 0

#define TRUE  1
#define FALSE 0


struct bot_data {
	char pass[9 + 1];
	char op_pass[9 + 1];
};


EXTERN struct bot_data *botdata;


#endif /* MAIN_H */

