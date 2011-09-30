#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "defaults.h"
#include "lib.h"
#include "main.h"


/* Replacement for malloc */
void *
xmalloc (size_t size)
{
	void *value = malloc (size);

	if (value == NULL)
		eperror ("malloc");

	bzero (value, size);

	return value;

}


/* Like fgets, buts read till gets a '\n' */
int
readln (char *src, char *buf, size_t len, size_t *offset, off_t maxsize)
{
	if ((off_t) *offset >= maxsize)
		return EOF;

	while ((off_t) *offset < maxsize) {
		if (len-- == 0) {
			*buf = '\n';
			return ERR_LONGERLINE;
		}
		if (*(src + *offset) != '\n') {
			*buf = *(src + *offset);
			buf++;
			(*offset)++;
			continue;
		} else
			break;
	}

	*buf = '\n';
	*++buf = '\0';
	(*offset)++;

	return 0;
}


/* Returns the offset till a separator or -1 if there is no such separator */
int
strlen2sep (const char *s, const char sep)
{
	int i = 0;
	const char *str = s;

	while (*str != sep && *str != '\0')
		++i, str++;

	if (*str == '\0' && sep != '\0')
		return -1;

	return i;

}


/* Returns the offset till a separator or -1 if there is no such separator 
 * (string as separator version) 
 */
int
strlen2sepstr (const char *str, const char *sep)
{
	unsigned int i, found;
	size_t len;
	const char *orig = str;

	len = strlen (sep);

	for (;;) {
		while (*str != *sep && *str != '\0')
			str++;
		if (*str == '\0')
			return -1;
		found = TRUE;
		for (i = 1; i < (unsigned int) len; i++)
			if (*(str + i) != *(sep + i)) {
				found = FALSE;
				++str;
			}
		if (found == TRUE)
			return (str - orig);
	}
}


/*
 * Copies 'data' into 'dest' until sep, len bytes max
 * offset is just an offset to the start of the string
 */
void
get_token (char *data, char sep, char *dest, int *offset, size_t len)
{
	size_t tmpoffset = 0;

	*dest = '\0';

	if (*(data + *offset) == sep) {
		(*offset)++;
		return;
	}

	while (tmpoffset++ < len && *(data + *offset) != sep) {
/* If any bug shows off, this was the previous line (it was changed to consider "\\$" and "\$" different):
		if (*(data + *offset) == '\\' && (*(data + *offset + 1) == sep || *(data + *offset + 1) == '\\')) */
		if (*(data + *offset) == '\\' && (*(data + *offset + 1) == sep))
			(*offset)++;

		*dest++ = *(data + (*offset)++);
	}

	(*offset)++;
	*dest = '\0';

	return;

}


void
get_token_str (char *data, char *sep, char *dest, int *offset, size_t len)
{
	int tmpoffset;

	*dest = '\0';

	if (strncmp (data + *offset, sep, strlen (sep)) == 0) {
		*offset += strlen (sep);
		return;
	}

	tmpoffset = strlen2sepstr (data + *offset, sep);
	if (tmpoffset == -1) /* Should not happen */
		tmpoffset = 0;

	strncpy (dest, data + *offset, len);

	dest[min (tmpoffset, (int) len)] = '\0';

	*offset += tmpoffset + 1;

	return;

}


/* Thanks to x-chat */
char *
strip_color (const char *text)
{
	int nc = 0, i = 0, col = FALSE;
	size_t len = strlen (text);
	char *new_str = (char *) xmalloc (len + 2);

	while (len > 0) {
		if ((col == TRUE && isdigit ((int) *text) && nc < 2) ||
		    (col == TRUE && *text == ',' && isdigit ((int) *(text + 1)) && nc < 3)) {
			nc++;
			if (*text == ',')
				nc = 0;
		} else {
			col = FALSE;
			switch (*text) {
			case '\003':			  /* ATTR_COLOR: */
				col = TRUE;
				nc = 0;
				break;
			case '\007':			  /* ATTR_BEEP: */
			case '\017':			  /* ATTR_RESET: */
			case '\026':			  /* ATTR_REVERSE: */
			case '\002':			  /* ATTR_BOLD: */
			case '\037':			  /* ATTR_UNDERLINE: */
				break;
			default:
				new_str[i] = *text;
				i++;
			}
		}
		text++;
		len--;
	}

	new_str[i] = '\0';

	return new_str;

}


char *
strtolower (char *str)
{
	char *s = str;

	for (; *s != '\0'; s++)
		*s = (char) tolower ((int) *s);

	return str;
}


/* Prepends and "Postpends" strings to every line on 's' (it doesn't has to 
 * be a line, "sep" is the marker), len is the len of the buffer 's'. */
void
delim_every_line (char *s, const char *prepend, const char *postpend, char sep, int len)
{
	char *orig = s;
	char temp[len], *dest = temp;
	int step = 0;
	int prelen = strlen (prepend);
	int poslen = strlen (postpend);

	while (TRUE) {
		if (len - prelen < 0)
			break;

		step = strlen2sep (s, sep);
		if (step > len)
			break;

		if (step == -1) {
			step = strlen2sep (s, '\0');
			if (step == -1)
				break;
		}

		strcpy (dest, prepend);
		len -= prelen;
		dest += prelen;


		if (*s == '\0')
			break;

		strncpy (dest, s, step);

		s += step;
		dest += step;
		len -= step;

		if (*s == '\0')
			break;

		if (len - poslen < 0)
			break;

		strcpy (dest, postpend);
		len -= poslen;
		dest += poslen;

		s++;
	}

	strcpy (orig, temp);

	return;
}


void
lock_file (int fd)
{
#if OS == SUN
	lockf (fd, F_LOCK, 0);
#else
	/* FIXME: check return value */
	flock (fd, LOCK_EX);
#endif /* OS == SUN */
}


void
unlock_file (int fd)
{
#if OS == SUN
	lockf (fd, F_ULOCK, 0);
#else
	flock (fd, LOCK_UN);
#endif /* OS == SUN */
}



