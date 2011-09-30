/* 
 * Used to test regexes.
 */

#include <sys/types.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

/* #define PATT		"^a[[:alnum:]]" */
/* #define STR 	"abc" */
#define PATT	argv[1]
#define STR 	argv[2]

int
main (int argc, char *argv[])
{
	regex_t regex;
	int ret;

	if (argc < 3) {
		fprintf (stderr, "argv[1] -> pattern\n"
				 "argv[2] -> text\n");
		return EXIT_FAILURE;
	}

	if (regcomp (&regex, PATT, REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
		fprintf (stderr, "Could not compile regex\n");
		return EXIT_FAILURE;
	}

	ret = regexec (&regex, STR, 0, NULL, 0);
	if (!ret)
		puts ("Match");
	else if (ret == REG_NOMATCH)
		puts ("No match");
	else {
		char msgbuf[100];

		regerror (ret, &regex, msgbuf, sizeof (msgbuf));
		fprintf (stderr, "Regex match failed: %s\n", msgbuf);
		return EXIT_FAILURE;
	}

	regfree (&regex);

	return EXIT_SUCCESS;
}


