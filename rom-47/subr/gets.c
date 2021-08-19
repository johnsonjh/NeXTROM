/*	@(#)gets.c	1.0	10/10/86	(c) 1986 NeXT	*/

#include <sys/types.h>

#ifdef gcc
#define	void	int
#endif gcc

gets (bp, max, echo)
	register u_char *bp;
{
	register int cc = 0, c;
	register u_char *limit = bp + max - 1;

	while (1) {

	switch (c = mon_getc()) {
		case -1:
			return (-1);	/* mouse down */
		case '\b':	/* erase char */
		case '\177':
			if (cc) {
				if (echo)
					printf ("\b \b");
				cc--;  bp--;
			}
			break;
		case '\025':	/* erase line (^U) */
			while (cc) {
				if (echo)
					printf ("\b \b");
				cc--;  bp--;
			}
			break;
		case '\027':	/* erase word (^W) */
			while (cc && *(bp-1) == ' ') {
				if (echo)
					printf ("\b \b");
				cc--;  bp--;
			}
			while (cc && *(bp-1) != ' ' && *(bp-1) != '/') {
				if (echo)
					printf ("\b \b");
				cc--;  bp--;
			}
			break;
		case '\r':	/* newline */
		case '\n':
			if (echo)
				putchar ('\n');
			*bp = 0;
			return (++cc);
		default:
			if (bp < limit) {
				if (echo)
					putchar (c);
				*bp++ = (u_char) c;
				cc++;
			} else
				putchar ('\007');
			break;
	}
	}
}
