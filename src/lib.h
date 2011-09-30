#ifndef LIB_H
#define LIB_H

#include <stdio.h>
#include <unistd.h>


#define DATELEN 30

void *xmalloc (size_t size);
int readln (char *src, char *buf, size_t len, size_t *offset, off_t maxsize);
int strlen2sep (const char *str, const char sep);
int strlen2sepstr (const char *str, const char *sep);
void get_token (char *data, char sep, char *dest, int *offset, size_t len);
void get_token_str (char *data, char *sep, char *dest, int *offset, size_t len);
char *strip_color (const char *text);
char *strtolower (char *text);
void delim_every_line (char *s, const char *prepend, const char *postpend,
	char sep, int len);
void lock_file (int fd);
void unlock_file (int fd);


#define STR_TIME 	"%H:%M"
#define STR_TIME_12 	"%I:%M%p"
#define STR_DATE 	"%d/%m"
#define DATE_FORMAT 	"%b %d %H:%M:%S"
#define DATE_FORMAT_L 	"%a %b %d %H:%M:%S %Y"

#define time_str(_s, _format) 						\
	do {								\
		time_t result = time (NULL);				\
		strftime (_s, DATELEN, _format, localtime (&result));	\
	} while (0)


#define NEW_HANDLER(_sign, _sigact_ptr, _fcn) 				\
	do { 								\
		sigset_t emptyset;					\
					    				\
		if (sigemptyset (&emptyset) == -1) {			\
			perror ("sigemptyset");				\
			exit (EXIT_FAILURE);				\
		}							\
		act.sa_handler = _fcn;					\
		act.sa_mask = emptyset;					\
		act.sa_flags = 0;					\
		if (sigaction (_sign, _sigact_ptr, NULL) == -1) {	\
			perror ("sigaction");				\
			exit (EXIT_FAILURE);				\
		}							\
	} while (0)

#define ERR_LONGERLINE (-2)

/* True if a character is a valid start of a channel string */
#define is_channel(__c)							\
	((__c) == '#' || (__c) == '&' || (__c) == '+' || (__c) == '!')

/* Classical macros */
#define max(__a, __b)	\
        ((__a) > (__b)) ? (__a) : (__b)

#define min(__a, __b)\
	((__a) > (__b)) ? (__b) : (__a)


#endif /* LIB_H */

